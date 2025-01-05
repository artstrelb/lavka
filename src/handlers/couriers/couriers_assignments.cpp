#include "couriers_assignments.hpp"
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

CouriersAssignments::CouriersAssignments(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
	: HttpHandlerBase(config, context),
    pg_cluster_(
	context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()
    ){}


std::string CouriersAssignments::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
  userver::formats::json::ValueBuilder ans;
  ans["couriers"] = {};

  auto result = pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kSlave, "SELECT courier_id, time_start, orders FROM lavka.couriers_assignments ORDER BY courier_id ASC, time_start ASC");

  //подумать как это всё прочитать
  auto dataAssignments = result.AsSetOf<AssignDbInfo>(pg::kRowTag);

  for(auto row: dataAssignments) {
    userver::formats::json::ValueBuilder item;
    // можно написать просто item = row ?
    item["courier_id"] = row.courier_id;
    item["time"] = fmt::format("{}", row.time_start);
    item["orders"] = row.orders;

    ans["couriers"].PushBack(item.ExtractValue());
  }

  return userver::formats::json::ToString(ans.ExtractValue()) + "\n";
}

void AppendCouriersAssignments(userver::components::ComponentList &component_list) {
    component_list.Append<CouriersAssignments>();
}

}// namespace lavka