#include "couriers.hpp"
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

#include <userver/utils/from_string.hpp>


#include "../../db/types.hpp"

namespace lavka {

Couriers::Couriers(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
	: HttpHandlerBase(config, context),
    pg_cluster_(
	context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()
    ){}


std::string Couriers::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
  request.GetHttpResponse().SetContentType(userver::http::content_type::kApplicationJson);

  std::int64_t courier_id;
  try {
    courier_id = userver::utils::FromString<int, std::string>(request.GetPathArg("courier_id"));
  } catch(...) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    return "";
  }

  auto result = pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kSlave, "select id, courier_type, regions, working_hours FROM lavka.couriers WHERE id = $1", courier_id);

  if(result.IsEmpty()) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
    return "";
  }

  auto iteration = result.AsSetOf<lavka::UserDbInfo>(pg::kRowTag);
  //auto data = result.AsContainer<std::vector<lavka::UserDbInfo>>();
  userver::formats::json::ValueBuilder r;
  r["courier_id"] = iteration[0].id;

  std::string type;
  switch(iteration[0].courier_type) {
    case CourierType::kAuto:
      type = "AUTO";
    break;
    case CourierType::kFoot:
      type = "FOOT";
    break;
    case CourierType::kBike:
      type = "BIKE";
  }
  r["courier_type"] = type;
  r["regions"] = iteration[0].regions;

  std::vector<std::string> hours;
  for(auto range: iteration[0].working_hours) {
    std::string str = fmt::format("{}-{}", userver::utils::UnderlyingValue(range).GetLowerBound(), userver::utils::UnderlyingValue(range).GetUpperBound());
    hours.push_back(str);
  }
  r["working_hours"] = hours;

  return userver::formats::json::ToString(r.ExtractValue());
}

void AppendCouriers(userver::components::ComponentList &component_list) {
    component_list.Append<Couriers>();
}

}// namespace lavka
