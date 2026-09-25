// token-backend/src/main.cpp
#include "controller/health_controller.h"

#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"

#include <cstdlib>
#include <iostream>

int main(int argc, char **argv) {
  oatpp::base::Environment::init();

  {
    auto router = oatpp::web::server::HttpRouter::createShared();
    auto healthController = std::make_shared<pcm::tokenbackend::HealthController>();

    // Register controller endpoints on the router
    router->route(healthController->getEndpoints());

    auto connectionHandler = oatpp::web::server::HttpConnectionHandler::createShared(router);
    const char *portEnv = std::getenv("PORT");
    v_uint16 port = portEnv ? static_cast<v_uint16>(std::atoi(portEnv)) : 8080;
    auto connectionProvider =
        oatpp::network::tcp::server::ConnectionProvider::createShared({"0.0.0.0", port});

    oatpp::network::Server server(connectionProvider, connectionHandler);
    std::cout << "pcm-token-backend listening on :" << port << std::endl;
    server.run();
  }

  oatpp::base::Environment::destroy();
  return 0;
}
