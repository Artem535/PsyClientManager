// token-backend/src/main.cpp
#include "auth/static_token_authorizer.h"
#include "config.h"
#include "controller/health_controller.h"
#include "db/accounts_repository.h"
#include "db/migrations.h"
#include "db/sqlite_connection.h"

#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"

#include <cstdlib>
#include <iostream>
#include <sodium.h>

int main(int argc, char **argv) {
  oatpp::base::Environment::init();

  if (sodium_init() < 0) {
    std::cerr << "Fatal: libsodium initialization failed" << std::endl;
    return 1;
  }

  if (argc > 1 && std::string(argv[1]) == "--seed-account") {
    auto config = pcm::tokenbackend::Config::fromEnv();
    pcm::tokenbackend::SqliteConnection conn(config.dbPath);
    pcm::tokenbackend::runMigrations(conn);
    pcm::tokenbackend::AccountsRepository accounts(conn);
    auto credential = accounts.seedAccount();
    std::cout << "Seeded account. Bearer credential (copy this now, it will not be shown again):\n"
              << credential << std::endl;
    oatpp::base::Environment::destroy();
    return 0;
  }

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
