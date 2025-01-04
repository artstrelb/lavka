#include "orders_assign.hpp"
#include <fmt/format.h>
#include <userver/formats/json/value.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/assert.hpp>

#include <userver/storages/postgres/io/enum_types.hpp>
#include <userver/utest/utest.hpp>

#include <chrono>
#include <userver/utest/utest.hpp>

#include <userver/utils/strong_typedef.hpp>
#include <userver/utils/time_of_day.hpp>
#include <userver/utils/underlying_value.hpp>
#include <userver/formats/parse/time_of_day.hpp>

#include <userver/formats/serialize/common_containers.hpp>
#include <userver/formats/parse/common_containers.hpp>

#include "../../db/types.hpp"

namespace lavka {

const int kMaxWeightFoot = 10;
const int kMaxWeightBike = 20;
const int kMaxWeightAuto = 40;

const int kMaxCntFoot = 2;
const int kMaxCntBike = 4;
const int kMaxCntAuto = 7;

const int kMaxRegionFoot = 1;
const int kMaxRegionBike = 2;
const int kMaxRegionAuto = 3;

const int kTime1Foot = 25;
const int kTime1Bike = 12;
const int kTime1Auto = 8;

const int kTime2Foot = 10;
const int kTime2Bike = 8;
const int kTime2Auto = 4;

const int kPrice1Foot = 10;
const int kPrice1Bike = 10;
const int kPrice1Auto = 10;

const int kPrice2Foot = 8;
const int kPrice2Bike = 8;
const int kPrice2Auto = 8;

//Объединяем заказы пока можем для упрощения
// В интервал пытаемся впихнуть новый заказ перебором


//Один конкретный интервал
struct IntervalDelivery {
  int from, to, t; // [from, to] t - время в минутах когда курьер освободится
  std::vector<std::vector<std::int64_t>> groupsOrders; //сгруппированные заказы
  std::vector<int> starts; // время когда курьер берёт группу заказов groupsOrders[i] starts[i]
  std::int64_t courier_id;
  std::int64_t weight = 0; // вес заказов на руках
  int region = 0; //регион курьера
  std::vector<int> usedRegions;
  int take = 0; // сколько заказов на руках
};

//Берем состояние курьера и пытаемся дать ему заказ
bool PutOrder(const std::unordered_map<std::int64_t, OrderDbInfo>& Orders, const std::unordered_map<std::int64_t, CourierDbInfo>& Couriers, IntervalDelivery &Interval, std::int64_t orderId, int &price) {
  std::int64_t courierId = Interval.courier_id;
  //сделать проверку если нет курьера то прекращаем;
  auto Courier = Couriers.at(courierId);
  auto Order = Orders.at(orderId);
  auto type = Courier.courier_type;
  auto region = Interval.region;
  auto take = Interval.take;

  int maxWeight;
  if(type == CourierType::kFoot) maxWeight = kMaxWeightFoot;
  else if(type == CourierType::kBike) maxWeight = kMaxWeightBike;
  else if(type == CourierType::kAuto) maxWeight = kMaxWeightAuto;

  int maxCnt;
  if(type == CourierType::kFoot) maxCnt = kMaxCntFoot;
  else if(type == CourierType::kBike) maxCnt = kMaxCntBike;
  else if(type == CourierType::kAuto) maxCnt = kMaxCntAuto;

  int time1; //время доставки первого заказа
  if(type == CourierType::kFoot) time1 = kTime1Foot;
  else if(type == CourierType::kBike) time1 = kTime1Bike;
  else if(type == CourierType::kAuto) time1 = kTime1Auto;

  int time2; //время второго и последующих заказов
  if(type == CourierType::kFoot) time2 = kTime2Foot;
  else if(type == CourierType::kBike) time2 = kTime2Bike;
  else if(type == CourierType::kAuto) time2 = kTime2Auto;

  int price1; //коэффициент доставки первого заказа
  if(type == CourierType::kFoot) price1 = kPrice1Foot;
  else if(type == CourierType::kBike) price1 = kPrice1Bike;
  else if(type == CourierType::kAuto) price1 = kPrice1Auto;

  int price2; //коэффициент доставки второго и следующего заказов
  if(type == CourierType::kFoot) price2 = kPrice2Foot;
  else if(type == CourierType::kBike) price2 = kPrice2Bike;
  else if(type == CourierType::kAuto) price2 = kPrice2Auto;

  //Диапазон заказа в минутах [t1, t2]
  //для упрощения беру только первый интервал заказа
  auto from = userver::utils::UnderlyingValue(Order.delivery_hours[0]).GetLowerBound(); // перевести в минуты
  Minutes m1 = Minutes{from};
  int t1 = 60*m1.Hours().count() + m1.Minutes().count();
  
  auto to = userver::utils::UnderlyingValue(Order.delivery_hours[0]).GetUpperBound(); // перевести в минуты
  Minutes m2 = Minutes{to};
  int t2 = 60*m2.Hours().count() + m2.Minutes().count();

  // error - не можем доставить заказ
  bool error = false;

  //проверим что доставка по региону осуществляется
  bool goodRegion = false;
  for(auto r: Courier.regions) {
      if(r == Order.regions) {
        goodRegion = true;
        break;
      }
  }
  if(!goodRegion) error = true;

  //проверить смену региона
  bool isChangeRegion = region && (region != Order.regions);
  
  //просто времени не хватит
  // [Order.from, Order.to] - диапазон в котором надо доставить заказ
  
  //Проверить что укладываемся во время доставки заказа и не выходим за рабочее время курьера
  int start = std::max(Interval.t, t1);
  if(start + time2 > Interval.to || start + time2 > t2) error = true;

  //вес заказов
  bool isOverloading = Order.weight + Interval.weight > maxWeight;

  if(Order.weight > maxWeight) error = true;

  //если физически не можем взять
  if(error) return false;

  //Наверное лучше так
  //мы отобрали заказы которые можем доставить
  //пытаемся сгруппировать с текущим
  //иначе доставляем отдельно

  if(take > 0 && take < maxCnt && (!isChangeRegion && !isOverloading && (Interval.t + time2 < Interval.to))) {
    //группируем с текущим
    Interval.take++;
    Interval.t += time2;
    Interval.weight += Order.weight;
    size_t i = Interval.groupsOrders.size();
    if(i > 0) i--;
    Interval.groupsOrders[i].push_back(orderId);

    price = price1;
    return true;
  } else if(start + time1 <= Interval.to) {
    Interval.take = 1;
    Interval.weight = 0;
    Interval.starts.push_back(Interval.t);
    Interval.t += time1;
    Interval.groupsOrders.push_back({orderId});

    price = price2;

    return true;
  }

  return false;
}

std::int64_t BestPrice = INT_MAX;
std::int64_t BestCountOrder = 0;
std::vector<IntervalDelivery> BestIntervals;

void PrintInterval(const IntervalDelivery& Interval) {
  userver::formats::json::ValueBuilder builder;

  builder["from"] = Interval.from;
  builder["to"] = Interval.to;
  builder["courier_id"] = Interval.courier_id;
  builder["t"] = Interval.t;
  builder["take"] = Interval.take;
  builder["groups"] = userver::formats::common::Type::kArray;
  for(auto group: Interval.groupsOrders){
    builder["groups"].PushBack(group);
  }
  builder["starts"] = Interval.starts;

  LOG_DEBUG()<<userver::formats::json::ToString(builder.ExtractValue());
}


void RecursionRun(const std::unordered_map<std::int64_t, OrderDbInfo>& Orders, const std::unordered_map<std::int64_t, CourierDbInfo>& Couriers, std::vector<IntervalDelivery> Intervals, const std::vector<std::int64_t> &OrdersIds, std::unordered_map<std::int64_t, bool> mapCompletedIds, int price) {
  //записать результат BestIntervals
  //Надо доставить максимум заказов
  if(mapCompletedIds.size() >= BestCountOrder ||  ( mapCompletedIds.size() == BestCountOrder && price < BestPrice)) {
    BestCountOrder = mapCompletedIds.size();
    BestPrice = price;
    BestIntervals = Intervals;
  }

  if(mapCompletedIds.size() == OrdersIds.size()) {
    return;
  }

  //перебираем все заказы
  for(auto orderId: OrdersIds) {

    if(mapCompletedIds[orderId]) return;

    for(size_t i = 0; i < Intervals.size(); i++) {
      auto Interval = Intervals[i];
      //если можно взять заказ для данного интервала
      IntervalDelivery oldInterval = Interval;
      int calcPrice = 0;
      if(PutOrder(Orders, Couriers, Interval, orderId, calcPrice)) {

        price += calcPrice;

        //пересчитать интервал
        Intervals[i] = Interval;
        mapCompletedIds[orderId] = true;

        RecursionRun(Orders, Couriers, Intervals, OrdersIds, mapCompletedIds, price);
        price -= calcPrice;

        Intervals[i] = oldInterval;
        mapCompletedIds.erase(orderId);
      }
    }

  }


}


OrdersAssign::OrdersAssign(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
	: HttpHandlerBase(config, context),
    pg_cluster_(
	context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()
    ){}


std::string OrdersAssign::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {

  //получить заказы
  auto resOrders = pg_cluster_->Execute(pg::ClusterHostType::kSlave, "SELECT id, weight, regions, delivery_hours, cost FROM lavka.orders");
  auto dataOrders = resOrders.AsSetOf<OrderDbInfo>(pg::kRowTag);

  //получить курьеров
  auto resCouriers = pg_cluster_->Execute(pg::ClusterHostType::kSlave, "SELECT id, courier_type, regions, working_hours FROM lavka.couriers");
  auto dataCouriers = resCouriers.AsSetOf<CourierDbInfo>(pg::kRowTag);

  std::unordered_map<std::int64_t, OrderDbInfo> mapOrders;
  std::unordered_map<std::int64_t, CourierDbInfo> mapCouriers;
  std::unordered_map<std::int64_t, bool> mapCompletedIds;
  std::vector<std::int64_t> OrdersIds;

  for(auto order: dataOrders) {
    mapOrders[order.id] = order;
    OrdersIds.push_back(order.id);
  }
  for(auto courier: dataCouriers) mapCouriers[courier.id] = courier;

  std::vector<IntervalDelivery> intervals;

  //cформировать интервалы
  for(auto courier: dataCouriers) {
    for(auto range: courier.working_hours) {

      IntervalDelivery interval;

      auto from = userver::utils::UnderlyingValue(range).GetLowerBound(); // перевести в минуты
      Minutes m1 = Minutes{from};
      int t1 = 60*m1.Hours().count() + m1.Minutes().count();

      auto to = userver::utils::UnderlyingValue(range).GetUpperBound(); // перевести в минуты
      Minutes m2 = Minutes{to};
      int t2 = 60*m2.Hours().count() + m2.Minutes().count();


      interval.from = t1;
      interval.to = t2;
      interval.t = t1;

      interval.courier_id = courier.id;

      intervals.push_back(interval);
    }
  }

  //запустить рекурсию
  //Взять все интервалы и попытаться перебором впихнуть заказы

  BestPrice = INT_MAX;
  BestCountOrder = 0;

  RecursionRun(mapOrders, mapCouriers, intervals, OrdersIds, mapCompletedIds, 0);

  /*IntervalDelivery testInterval = intervals[1];
  int calcPrice = 0;
  //PrintInterval(testInterval);
  PutOrder(mapOrders, mapCouriers, testInterval, 1, calcPrice);
  PutOrder(mapOrders, mapCouriers, testInterval, 2, calcPrice);
  PutOrder(mapOrders, mapCouriers, testInterval, 3, calcPrice);
  PutOrder(mapOrders, mapCouriers, testInterval, 4, calcPrice);
  PutOrder(mapOrders, mapCouriers, testInterval, 5, calcPrice);
  PutOrder(mapOrders, mapCouriers, testInterval, 6, calcPrice);
  PutOrder(mapOrders, mapCouriers, testInterval, 7, calcPrice);
  PutOrder(mapOrders, mapCouriers, testInterval, 8, calcPrice);
  PrintInterval(testInterval);*/

  //return fmt::format("{}", testInterval.t);



  // BestIntervals - глобальная переменная в которой содержится оптимальное распределение

   //сначала всё удалим
   auto res1 = pg_cluster_->Execute(pg::ClusterHostType::kMaster, "DELETE FROM lavka.couriers_assignments");

  for(auto Interval: BestIntervals) {
    auto courierId = Interval.courier_id;
    
    for(size_t i = 0; i < Interval.groupsOrders.size(); i++) {
       
       auto orders = Interval.groupsOrders[i];

       //Время перевести во время
       std::string hh, mm;
       int hours = Interval.starts[i] / 60;
       int minutes = Interval.starts[i] % 60;
       hh = std::to_string(hours);
       mm = std::to_string(minutes);
       
       if(hours < 10) hh = "0" + hh;
       if(minutes < 10) mm = "0" + mm;

       std::string timeStr = hh + ":" + mm;
       auto time_start = Minutes{timeStr};
       
      //потом можно завернуть всё в транзакцию
      //записывать когда начинаем новую партию
      auto res = pg_cluster_->Execute(pg::ClusterHostType::kMaster, "INSERT INTO lavka.couriers_assignments VALUES ($1, $2, $3)", courierId, time_start, orders);
    }
  }

  return "ok\n";
}

void AppendOrdersAssign(userver::components::ComponentList &component_list) {
    component_list.Append<OrdersAssign>();
}

}// namespace lavka
