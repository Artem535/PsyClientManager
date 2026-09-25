#pragma once

#include <sqlite3.h>
#include <string>

namespace pcm::tokenbackend {

// One connection is shared by every HTTP request handler (see main.cpp), and
// oat++'s HttpConnectionHandler dispatches connections across worker threads,
// so this handle is used concurrently. The constructor configures the
// connection for that explicitly rather than relying on how the SQLite library
// happened to be compiled — see sqlite_connection.cpp.
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

// RAII wrapper over BEGIN IMMEDIATE / COMMIT / ROLLBACK.
//
// Serialized threading mode makes each *statement* safe against concurrent
// use of the connection; it does not make a read-modify-write *sequence*
// atomic. Any repository method that issues more than one statement and whose
// intermediate state would be wrong to observe (or to leave behind on a
// failure) opens one of these.
//
// BEGIN IMMEDIATE rather than plain BEGIN: the write lock is taken up front,
// so two concurrent transactions cannot both read and then deadlock trying to
// upgrade. Rolls back on destruction unless commit() was called, so an
// exception thrown mid-sequence cannot leave a partial write behind.
class SqliteTransaction {
public:
  explicit SqliteTransaction(SqliteConnection &conn);
  ~SqliteTransaction();

  SqliteTransaction(const SqliteTransaction &) = delete;
  SqliteTransaction &operator=(const SqliteTransaction &) = delete;

  void commit();

private:
  SqliteConnection &conn_;
  bool done_ = false;
};

} // namespace pcm::tokenbackend
