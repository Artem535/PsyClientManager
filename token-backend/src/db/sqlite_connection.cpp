#include "db/sqlite_connection.h"

#include <stdexcept>

namespace pcm::tokenbackend {

namespace {
// How long a statement retries a lock held by another thread before giving up
// with SQLITE_BUSY. Contention here is between oat++ worker threads on one
// process's own connection, so waits are short; 5s is generous enough that a
// request never fails for a lock it would have got, and short enough that a
// genuinely stuck transaction surfaces as an error rather than hanging.
constexpr int kBusyTimeoutMs = 5000;
} // namespace

SqliteConnection::SqliteConnection(const std::string &path) {
  // SQLITE_OPEN_FULLMUTEX pins this connection to SQLite's "serialized"
  // threading mode. The connection is shared across oat++'s per-connection
  // worker threads (see main.cpp), and while serialized is SQLite's compiled-in
  // default, that is a build-time property of whatever libsqlite3 is linked —
  // not something this process can assume. Requesting it explicitly makes the
  // safety a property of this code instead of the build.
  const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
  if (sqlite3_open_v2(path.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
    std::string message = db_ ? sqlite3_errmsg(db_) : "out of memory";
    sqlite3_close(db_);
    db_ = nullptr;
    throw std::runtime_error("failed to open sqlite database: " + message);
  }
  // Without this, a statement that finds the write lock taken fails
  // immediately with SQLITE_BUSY instead of retrying.
  sqlite3_busy_timeout(db_, kBusyTimeoutMs);
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

SqliteTransaction::SqliteTransaction(SqliteConnection &conn) : conn_(conn) {
  conn_.exec("BEGIN IMMEDIATE;");
}

SqliteTransaction::~SqliteTransaction() {
  if (done_) {
    return;
  }
  // Destructor path: the scope is unwinding, usually because a statement threw.
  // A failed rollback must not throw out of a destructor, and there is nothing
  // useful to do about it — the connection is torn down at process exit.
  sqlite3_exec(conn_.raw(), "ROLLBACK;", nullptr, nullptr, nullptr);
  done_ = true;
}

void SqliteTransaction::commit() {
  if (done_) {
    return;
  }
  conn_.exec("COMMIT;");
  done_ = true;
}

} // namespace pcm::tokenbackend
