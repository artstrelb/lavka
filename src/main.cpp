#include <userver/clients/http/component.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/utils/daemon_run.hpp>

#include "hello.hpp"
#include "couriers.hpp"
#include "couriers/couriers_add.hpp"
#include "couriers/couriers_list.hpp"
#include "couriers/couriers_meta_info.hpp"
#include "couriers/couriers_assignments.hpp"

#include "orders/orders.hpp"
#include "orders/orders_add.hpp"
#include "orders/orders_list.hpp"
#include "orders/orders_complete.hpp"
#include "orders/orders_assign.hpp"


int main(int argc, char* argv[]) {
  auto component_list = userver::components::MinimalServerComponentList()
                            .Append<userver::server::handlers::Ping>()
                            .Append<userver::components::TestsuiteSupport>()
                            .Append<userver::components::HttpClient>()
                            .Append<userver::server::handlers::TestsControl>();

  lavka::AppendCouriers(component_list);
  lavka::AppendCouriersAdd(component_list);
  lavka::AppendCouriersList(component_list);
  lavka::AppendCouriersMetaInfo(component_list);
  lavka::AppendCouriersAssignments(component_list);
  
  lavka::AppendOrders(component_list);
  lavka::AppendOrdersAdd(component_list);
  lavka::AppendOrdersList(component_list);
  lavka::AppendOrdersComplete(component_list);
  lavka::AppendOrdersAssign(component_list);

  return userver::utils::DaemonMain(argc, argv, component_list);
}
