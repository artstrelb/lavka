#include "orders.hpp"
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

class Orders final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-orders";

  Orders(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);


  std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) 
    const override;

  private:
   userver::storages::postgres::ClusterPtr pg_cluster_;
};

} // namespace lavka


namespace lavka {

using namespace std::chrono_literals;
 
using Seconds = userver::utils::datetime::TimeOfDay<std::chrono::seconds>;
using TimeRange2 = TimeRange<std::chrono::minutes>;
using BoundedTimeRange2 = BoundedTimeRange<std::chrono::minutes>;

struct OrderDbInfo {
    std::int64_t id;
    float weight;
    std::int64_t regions;
    std::vector<BoundedTimeRange2> delivery_hours;
};

Orders::Orders(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
	: HttpHandlerBase(config, context),
    pg_cluster_(
	context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()
    ){}


std::string Orders::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
  request.GetHttpResponse().SetContentType(userver::http::content_type::kApplicationJson);

  std::string order = request.GetPathArg("order_id");
  bool isDigit = true;
  for(char ch: order) {
    if(!std::isdigit(ch)) {
	     isDigit = false;
     }
  }

  if(!isDigit) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    return "";
  }

  
  if(order.empty() || !isDigit) {
    throw userver::server::handlers::ClientError(
      userver::server::handlers::ExternalBody{"No 'order' query argument"}
    );
    return "";
  }

  int id = std::stoi(order);

  auto result = pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kSlave, "SELECT id, weight, regions, delivery_hours, cost FROM lavka.orders WHERE id = $1", id);

  if(result.IsEmpty()) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
    return "";
  }

  auto iteration = result.AsSetOf<lavka::OrderDbInfo>(pg::kRowTag);
  userver::formats::json::ValueBuilder r;
  r["order"] = iteration[0].id;

  r["regions"] = iteration[0].regions;

  std::vector<std::string> hours;
  for(auto range: iteration[0].delivery_hours) {
    std::string str = fmt::format("{}-{}", userver::utils::UnderlyingValue(range).GetLowerBound(), userver::utils::UnderlyingValue(range).GetUpperBound());
    hours.push_back(str);
  }
  r["delivery_hours"] = hours;

  return userver::formats::json::ToString(r.ExtractValue());
}

void AppendOrders(userver::components::ComponentList &component_list) {
    component_list.Append<Orders>();
}

}// namespace lavka