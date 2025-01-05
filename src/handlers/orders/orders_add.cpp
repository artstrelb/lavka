#include "orders_add.hpp"
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


#include "../../db/types.hpp"
#include "../../utils/validators.hpp"

namespace lavka {

OrdersAdd::OrdersAdd(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
	: HttpHandlerJsonBase(config, context),
    pg_cluster_(
	context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()
    ){}


userver::formats::json::Value OrdersAdd::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request, const userver::formats::json::Value& json, userver::server::request::RequestContext&) const {

  if(json.IsEmpty() || json["orders"].IsEmpty()) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    return userver::formats::json::FromString("{}");
  }

  userver::formats::json::ValueBuilder ans, data;
  ans["orders"] = {};

  for(auto item: json["orders"]) {
    bool error = false;

    float weight = item["weight"].As<float>(0);
    std::int64_t regions = item["regions"].As<std::int64_t>(0);

    if(weight == 0 || regions == 0) {
      error = true;
    }

    std::vector<BoundedTimeRange2> delivery_hours;
    std::vector<std::string> hours = item["delivery_hours"].As<std::vector<std::string>>({});
    for(std::string str: hours) {
      if(!lavka::checkInterval(str)) {
        error = true;
        break;
      }
      std::string t1 = str.substr(0, 5);
      std::string t2 = str.substr(6, 5);

      try {
        Minutes m1 = Minutes{t1};
        Minutes m2 = Minutes{t2};
        delivery_hours.push_back(BoundedTimeRange2{m1, m2, userver::storages::postgres::RangeBound::kBoth});
      } catch (const std::exception& e) {
        error = true;
        break;
      }
    }

    std::int64_t cost = item["cost"].As<std::int64_t>(0);

    if(error) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return userver::formats::json::FromString("{}");
    }

    auto result = pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster, "INSERT INTO lavka.orders (weight, regions, delivery_hours, cost) VALUES ($1, $2, $3, $4) RETURNING id;", weight, regions, delivery_hours, cost);
    auto order_id = result[0][0].As<std::int64_t>();

    userver::formats::json::ValueBuilder orderData = item;
    orderData["order_id"] = order_id;

    ans["orders"].PushBack(orderData.ExtractValue());
  }

  return ans.ExtractValue();
}

void AppendOrdersAdd(userver::components::ComponentList &component_list) {
    component_list.Append<OrdersAdd>();
}

}// namespace lavka