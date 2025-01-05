#include "couriers_add.hpp"
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

CouriersAdd::CouriersAdd(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
	: HttpHandlerJsonBase(config, context),
    pg_cluster_(
	context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()
    ){}


userver::formats::json::Value CouriersAdd::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request, const userver::formats::json::Value& json, userver::server::request::RequestContext&) const {

  if(json.IsEmpty() || json["couriers"].IsEmpty()) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    return userver::formats::json::FromString("{}");
  }

  userver::formats::json::ValueBuilder ans, data;
  ans["couriers"] = {};

  for(auto item: json["couriers"]) {
    bool error = false;
    std::string type = item["courier_type"].As<std::string>("NO");
    CourierType courier_type;
    if(type == "AUTO") courier_type = CourierType::kAuto;
    else if(type == "FOOT") courier_type = CourierType::kFoot;
    else if(type == "BIKE") courier_type = CourierType::kBike;
    else error = true;

    std::vector<std::int64_t> regions;
    try {
      regions = item["regions"].As<std::vector<std::int64_t>>({});
    } catch(const std::exception& e) {
	error = true;
    }

    if(regions.empty()) {
        error = true;
    }

    std::vector<BoundedTimeRange2> working_hours;
    std::vector<std::string> hours = item["working_hours"].As<std::vector<std::string>>({});
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
        working_hours.push_back(BoundedTimeRange2{m1, m2, userver::storages::postgres::RangeBound::kBoth});
      } catch (const std::exception& e) {
        error = true;
        break;
      }
    }

    if(hours.empty()) {
      error = true;
    }

    if(error) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return userver::formats::json::FromString("{}");
    }

    auto result = pg_cluster_->Execute(pg::ClusterHostType::kMaster, "INSERT INTO lavka.couriers (courier_type, regions, working_hours) VALUES ($1, $2, $3) RETURNING id;", courier_type, regions, working_hours);
    auto courier_id = result[0][0].As<std::int64_t>();

    userver::formats::json::ValueBuilder courierData = item;
    courierData["courier_id"] = courier_id;

    ans["couriers"].PushBack(courierData.ExtractValue());
  }

  return ans.ExtractValue();
}

void AppendCouriersAdd(userver::components::ComponentList &component_list) {
    component_list.Append<CouriersAdd>();
}

}// namespace lavka