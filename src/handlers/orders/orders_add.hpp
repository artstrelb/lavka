#pragma once

#include "userver/components/component.hpp"
#include "userver/components/component_list.hpp"
#include "userver/formats/json/serialize_container.hpp"
#include "userver/server/handlers/http_handler_json_base.hpp"
#include "userver/storages/postgres/cluster.hpp"
#include "userver/storages/postgres/component.hpp"

namespace lavka {

class OrdersAdd final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-orders-add";

  OrdersAdd(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);


  userver::formats::json::Value HandleRequestJsonThrow(const userver::server::http::HttpRequest& request, const userver::formats::json::Value& json, userver::server::request::RequestContext&) 
    const override;

  private:
   userver::storages::postgres::ClusterPtr pg_cluster_;
};

void AppendOrdersAdd(userver::components::ComponentList& component_list);

}  // namespace lavka
