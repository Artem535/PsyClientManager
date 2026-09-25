#include "db/sqlite_connection.h"

#include <stdexcept>

namespace pcm::tokenbackend {

SqliteConnection::SqliteConnection(const std::string &path) {
  if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
    std::string message = sqlite3_errmsg(db_);
    sqlite3_close(db_);
    throw std::runtime_error("failed to open sqlite database: " + message);
  }
  sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
}

SqliteConnection::~SqliteConnection() {
  if (db_) {
    sqlite3_close(db_);
  }
}

void SqliteConnection::exec(const std::string &sql) {
  char *errMsg = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
    std::string message = errMsg ? errMsg : "unknown sqlite error";
    sqlite3_free(errMsg);
    throw std::runtime_error("sqlite exec failed: " + message);
  }
}

} // namespace pcm::tokenbackend
