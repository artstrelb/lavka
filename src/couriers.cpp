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

class Couriers final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-couriers";

  Couriers(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);


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

struct UserDbInfo {
    std::int64_t id;
    CourierType courier_type;
    std::vector<int64_t> regions;
    std::vector<BoundedTimeRange2> working_hours;
};

Couriers::Couriers(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
	: HttpHandlerBase(config, context),
    pg_cluster_(
	context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()
    ){}


std::string Couriers::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
  request.GetHttpResponse().SetContentType(userver::http::content_type::kApplicationJson);

  std::string courier_id = request.GetPathArg("courier_id");
  bool isDigit = true;
  for(char ch: courier_id) {
    if(!std::isdigit(ch)) {
	isDigit = false;
     }
  }

  if(!isDigit) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    return "";
  }

  
  if(courier_id.empty() || !isDigit) {
    throw userver::server::handlers::ClientError(
      userver::server::handlers::ExternalBody{"No 'courier_id' query argument"}
    );
    return "";
  }

  int id = std::stoi(courier_id);

  auto result = pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kSlave, "select id, courier_type, regions, working_hours FROM lavka.couriers WHERE id = $1", id);

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

  //request.GetHttpResponse().SetContentType(userver::http::content_type::kApplicationJson);

  return userver::formats::json::ToString(r.ExtractValue());
//auto range = result[0].As<std::vector<BoundedTimeRange>>();

  //return fmt::format("{}", userver::utils::UnderlyingValue(range[0]).GetUpperBound());

 // return fmt::format("{}", userver::utils::UnderlyingValue(range).GetUpperBound());

  //EXPECT_EQ(Seconds{1s}, userver::utils::UnderlyingValue(range).GetLowerBound());
  
  /*auto result2 = pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kSlave, "SELECT courier_type FROM lavka.couriers WHERE id=$1", id);
    if(result[0][0].As<CourierType>() == CourierType::kAuto) {
	return "foo";

  } */
  
  //EXPECT_EQ(CourierType::kAuto, result[0][0].As<CourierType>());

  

  return "go";

  //userver::formats::json::ValueBuilder r;


  //auto [db_id, courier_type] = result.AsSingleRow<UserDbInfo>();
/*    if(result.AsSingleRow<CourierType> == CourierType::kAuto) {
	return "ura";
    }*/

    //return std::to_string(CourierType::kAuto); //result.AsSingleRow<std::string>();
  /*r["id"] = db_id;
  r["courier_type"] = courier_type;
  //r["working_hours"] = working_hours;

  request.GetHttpResponse().SetContentType(userver::http::content_type::kApplicationJson);

  return userver::formats::json::ToString(r.ExtractValue());*/

}

void AppendCouriers(userver::components::ComponentList &component_list) {
    component_list.Append<Couriers>();
    component_list.Append<userver::components::Postgres>("postgres-db-1");
    component_list.Append<userver::clients::dns::Component>();
}

}// namespace lavka