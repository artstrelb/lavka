#pragma once

enum class CourierType {kAuto, kBike, kFoot};
namespace pg = userver::storages::postgres;

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


} // namespace lavka


template <typename Duration>
struct userver::storages::postgres::io::CppToUserPg<lavka::TimeRange<Duration>> {
    static constexpr DBTypeName postgres_name = "lavka.timerange";
};
 
template <typename Duration>
struct userver::storages::postgres::io::CppToUserPg<lavka::BoundedTimeRange<Duration>> {
    static constexpr DBTypeName postgres_name = "lavka.timerange";
};

