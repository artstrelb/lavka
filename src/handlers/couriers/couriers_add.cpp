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


namespace pg = userver::storages::postgres;

enum class CourierType {kAuto, kBike, kFoot};

template<>
struct userver::storages::postgres::io::CppToUserPg<CourierType> {
  static constexpr DBTypeName postgres_name = "lavka.courier_type";
  static constexpr userver::utils::TrivialBiMap enumerators = [](auto selector) {
    return selector()
      .Case("FOOT", CourierType::kFoot)
      .Case("AUTO", CourierType::kAuto)
      .Case("BIKE", CourierType::kBike);
  };
};

namespace lavka {

template <typename Duration>
using TimeRange = userver::utils::StrongTypedef<struct MyTimeTag, pg::Range<userver::utils::datetime::TimeOfDay<Duration>>>;

template <typename Duration>
using BoundedTimeRange = userver::utils::StrongTypedef<struct MyTimeTag, pg::BoundedRange<userver::utils::datetime::TimeOfDay<Duration>>>;

} // namespace lavka


template <typename Duration>
struct userver::storages::postgres::io::CppToUserPg<lavka::TimeRange<Duration>> {
    static constexpr DBTypeName postgres_name = "lavka.timerange";
};
 
template <typename Duration>
struct userver::storages::postgres::io::CppToUserPg<lavka::BoundedTimeRange<Duration>> {
    static constexpr DBTypeName postgres_name = "lavka.timerange";
};

namespace lavka {

class CouriersAdd final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-couriers-add";

  CouriersAdd(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);


  userver::formats::json::Value HandleRequestJsonThrow(const userver::server::http::HttpRequest& request, const userver::formats::json::Value& json, userver::server::request::RequestContext&) 
    const override;

  private:
   pg::ClusterPtr pg_cluster_;
};

} // namespace lavka


namespace lavka {

using namespace std::chrono_literals;
 
using Minutes = userver::utils::datetime::TimeOfDay<std::chrono::minutes>;
using TimeRange2 = TimeRange<std::chrono::minutes>;
using BoundedTimeRange2 = BoundedTimeRange<std::chrono::minutes>;

struct UserDbInfo {
    std::int64_t id;
    CourierType courier_type;
    std::vector<int64_t> regions;
    std::vector<BoundedTimeRange2> working_hours;
};

bool checkInterval(std::string str) {
  if(str.length() != 11) return false;
  
  std::string t1 = str.substr(0, 5);
  std::string t2 = str.substr(6, 5);

  return true;
}

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
      if(!checkInterval(str)) {
        error = true;
        break;
      }
      std::string t1 = str.substr(0, 5);
      std::string t2 = str.substr(6, 5);

      try {
        Minutes m1 = Minutes{t1};
        Minutes m2 = Minutes{t2};
        working_hours.push_back(BoundedTimeRange2{m1, m2, pg::RangeBound::kBoth});
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