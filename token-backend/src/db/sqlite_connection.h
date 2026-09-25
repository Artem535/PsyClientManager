#pragma once

#include <sqlite3.h>
#include <string>

namespace pcm::tokenbackend {

class SqliteConnection {
public:
  explicit SqliteConnection(const std::string &path);
  ~SqliteConnection();

  SqliteConnection(const SqliteConnection &) = delete;
  SqliteConnection &operator=(const SqliteConnection &) = delete;

  sqlite3 *raw() const { return db_; }
  void exec(const std::string &sql);

private:
  sqlite3 *db_ = nullptr;
};

} // namespace pcm::tokenbackend
