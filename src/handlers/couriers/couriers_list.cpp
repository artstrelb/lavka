#include "couriers_list.hpp"
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
#include "../../utils/validators.hpp"


namespace lavka {

CouriersList::CouriersList(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
	: HttpHandlerBase(config, context),
    pg_cluster_(
	context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()
    ){}


std::string CouriersList::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {

  const auto& offset = request.GetArg("offset");
  const auto& limit = request.GetArg("limit");

  bool error = false;

  std::int64_t dbOffset = 0;
  std::int64_t dbLimit = 10;

  if(!offset.empty() && !lavka::IsDigits(offset)) error = true;
  if(!limit.empty() && !lavka::IsDigits(limit)) error = true;

  if(!error) {
    if(!offset.empty()) dbOffset = stoi(offset);
    if(!limit.empty()) dbLimit = stoi(limit);
  }

  if(dbOffset < 0 || dbLimit < 1) error = true;

  if(error) {
    request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kBadRequest);
    return "{}";
  }

  userver::formats::json::ValueBuilder ans;
  ans["couriers"] = {};

  auto result = pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kSlave, "SELECT id, courier_type, regions, working_hours FROM lavka.couriers LIMIT $1 OFFSET $2", dbLimit, dbOffset);

  for(auto row: result) {
    userver::formats::json::ValueBuilder item;
    item["courier_id"] = row["id"].As<std::int64_t>();

    std::string courier_type;

    CourierType type = row["courier_type"].As<CourierType>();
    if(type == CourierType::kAuto) courier_type = "AUTO";
    else if(type == CourierType::kFoot) courier_type = "FOOT";
    else if(type == CourierType::kBike) courier_type = "BIKE";
    item["courier_type"] = courier_type;

    auto regions = row["regions"];
    row["regions"] = regions;

    std::vector<std::string> hours;
    auto working_hours = row["working_hours"].As<std::vector<BoundedTimeRange2>>();
    for(auto range: working_hours) {
      std::string str = fmt::format("{}-{}", userver::utils::UnderlyingValue(range).GetLowerBound(), userver::utils::UnderlyingValue(range).GetUpperBound());
      hours.push_back(str);
    }
    item["working_hours"] = hours;

    ans["couriers"].PushBack(item.ExtractValue());
  }

  return userver::formats::json::ToString(ans.ExtractValue());
}

void AppendCouriersList(userver::components::ComponentList &component_list) {
    component_list.Append<CouriersList>();
}

}// namespace lavka