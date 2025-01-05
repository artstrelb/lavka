#include "orders_complete.hpp"
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

OrdersComplete::OrdersComplete(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
	: HttpHandlerBase(config, context),
    pg_cluster_(
	context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()
    ){}


std::string OrdersComplete::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
  //все таки принимаем 3 параметра POST. Для тренировки
  bool error = false;

  const auto& courier_id = request.GetArg("courier_id");
  const auto& order_id = request.GetArg("order_id");
  const auto& complete_time = request.GetArg("complete_time");

  std::int64_t courier_id_db = stoi(courier_id);
  std::int64_t order_id_db = stoi(order_id);

  //научиться дату записывать в БД
  auto prepare_time = userver::utils::datetime::FromRfc3339StringSaturating(complete_time);
  auto complete_time_db = userver::storages::postgres::TimePointTz{prepare_time};

  if(courier_id.empty() || order_id.empty() || complete_time.empty()) {
    error = true;
  }

  if(error) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    return "";
  }

  //потом поменять на complete_time
  auto result = pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster, "UPDATE lavka.orders SET courier_id = $1, complete_date = $2 WHERE id = $3 AND (courier_id IS NULL OR courier_id = $4)", courier_id_db, complete_time_db, order_id_db, courier_id_db);

  if(result.RowsAffected() == 0) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    return "";
  }

  return fmt::format("{}\n", order_id);
}

void AppendOrdersComplete(userver::components::ComponentList &component_list) {
    component_list.Append<OrdersComplete>();
}

}// namespace lavka