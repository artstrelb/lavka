#include "couriers_meta_info.hpp"
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
bool IsDigits(const std::string& str) {
  for(char ch: str) {
    if(!std::isdigit(ch)) return false;
  }

  return true;
}
}

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

class CouriersListMetaInfo final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-couriers-meta-info";

  CouriersListMetaInfo(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);


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

struct UserDbInfo {
    std::int64_t id;
    CourierType courier_type;
    std::vector<int64_t> regions;
    std::vector<BoundedTimeRange2> working_hours;
};



CouriersListMetaInfo::CouriersListMetaInfo(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
	: HttpHandlerBase(config, context),
    pg_cluster_(
	context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()
    ){}


std::string CouriersListMetaInfo::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
  const auto& start_date = request.GetArg("startDate");
  const auto& end_date = request.GetArg("endDate");
  const auto& courier_id = request.GetPathArg("courier_id");

  std::int64_t courier_id_db = stoi(courier_id);

  bool error = false;

  auto prepare_time_1 = userver::utils::datetime::FromStringSaturating(start_date, "%Y-%m-%d");
  auto prepare_time_2 = userver::utils::datetime::FromStringSaturating(end_date, "%Y-%m-%d");
  auto time_1_db = pg::TimePointTz{prepare_time_1};
  auto time_2_db = pg::TimePointTz{prepare_time_2};

  using namespace std::literals;

  //посчитать количество часов между start_date end_date
  int hours = (prepare_time_2 - prepare_time_1) / 1h;
  if(hours == 0) hours = 24;

  if (start_date.empty() || end_date.empty() || courier_id.empty()) {
    error = true;
  }

  if(error) {
    request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kBadRequest);
    return "{}";
  }

  //тип курьера
  auto result1 = pg_cluster_->Execute(pg::ClusterHostType::kSlave, "SELECT courier_type FROM lavka.couriers WHERE id = $1", courier_id_db);

  //если не найден курьер вернуть 404
  if(result1.IsEmpty()) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
    return "{}";
  }

  CourierType courier_type = result1[0][0].As<CourierType>();

  int koeffIncome = 0, koeffRating = 0;
  if(courier_type == CourierType::kAuto) {
    koeffIncome = 4;
    koeffRating = 1;
  } 
  if(courier_type == CourierType::kFoot) {
    koeffIncome = 2;
    koeffRating = 3;
  }
  if(courier_type == CourierType::kBike) {
    koeffIncome = 4;
    koeffRating = 2;
  }

  auto result = pg_cluster_->Execute(pg::ClusterHostType::kSlave, "SELECT case when sum(cost) is not null then sum(cost) else 0 end s, count(*) count FROM lavka.orders WHERE courier_id = $1 AND complete_date > $2 AND complete_date < $3", courier_id_db, time_1_db, time_2_db);

  if(result.IsEmpty()) {
    return "{}";
  }

  auto [count, sum] = result[0].As<std::int64_t, std::int64_t>({0, 0});


  //std::int64_t count = result[0]["count"].As<std::int64_t>(0);
  //std::int64_t sum = result[0]["s"].As<std::int64_t>(0);

  std::int64_t income = sum * koeffIncome;
  std::int64_t rating = (count / hours) * koeffRating;
  
  userver::formats::json::ValueBuilder ans;
  
  if(count > 0) {
    ans["income"] = income;
    ans["rating"] = rating;
  } else return "{}";

  return userver::formats::json::ToString(ans.ExtractValue());
}

void AppendCouriersMetaInfo(userver::components::ComponentList &component_list) {
    component_list.Append<CouriersListMetaInfo>();
}

}// namespace lavka