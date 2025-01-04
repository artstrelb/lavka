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

class CouriersAssignments final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-couriers-assignments";

  CouriersAssignments(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);


  std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) 
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

struct AssignDbInfo {
    std::int64_t courier_id;
    Minutes time_start;
    std::vector<std::int64_t> orders;
};

CouriersAssignments::CouriersAssignments(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
	: HttpHandlerBase(config, context),
    pg_cluster_(
	context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()
    ){}


std::string CouriersAssignments::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
  userver::formats::json::ValueBuilder ans;
  ans["couriers"] = {};

  auto result = pg_cluster_->Execute(pg::ClusterHostType::kSlave, "SELECT courier_id, time_start, orders FROM lavka.couriers_assignments ORDER BY courier_id ASC, time_start ASC");

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