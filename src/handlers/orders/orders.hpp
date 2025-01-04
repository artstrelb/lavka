#pragma once

#include "userver/components/component.hpp"
#include "userver/components/component_list.hpp"
#include "userver/formats/json/serialize_container.hpp"
#include "userver/server/handlers/http_handler_json_base.hpp"
#include "userver/storages/postgres/cluster.hpp"
#include "userver/storages/postgres/component.hpp"

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

void AppendOrders(userver::components::ComponentList& component_list);

}  // namespace lavka
