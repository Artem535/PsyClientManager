#pragma once

#include <sqlite3.h>
#include <mutex>
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

  // Recursive because the repository layer's entry points call one another:
  // MeetingsRepository::create() reads its own row back through findByRef(),
  // InvitationsRepository::reissueForMeeting() calls invalidateAllForMeeting()
  // and create(), recordFailedPasscodeAttempt() calls invalidate(). Every one
  // of those is a public entry point that must take the lock when it is called
  // from outside, so the same thread re-enters the lock on the nested call. A
  // plain std::mutex would deadlock; duplicating each method as a private
  // already-locked twin would double the repository surface for no gain.
  using Lock = std::unique_lock<std::recursive_mutex>;

  // Serializes ALL database access. Returns a lock that is already held, so a
  // caller writes `auto guard = conn_.lock();` and gets release-on-return,
  // including when an exception unwinds the frame.
  //
  // SQLITE_OPEN_FULLMUTEX (see the constructor) only serializes individual
  // SQLite API calls. It does not make a *sequence* of them atomic, so without
  // this lock two failure modes are live on the shared connection:
  //
  //  1. A second thread issuing BEGIN IMMEDIATE while another thread's
  //     transaction is open gets SQLITE_ERROR ("cannot start a transaction
  //     within a transaction"), not SQLITE_BUSY — the busy timeout does not
  //     help, and the repository turns it into a 500.
  //  2. Worse, a statement issued outside any transaction (e.g.
  //     MeetingsRepository::create()'s INSERT) can land inside another
  //     thread's open transaction and be silently undone if that transaction
  //     later rolls back — after its own caller was told it succeeded.
  //
  // Every public repository method that touches the database therefore holds
  // this for its whole body, which makes at most one such call in flight at a
  // time and puts both failure modes structurally out of reach. The cost is
  // that database access is serialized process-wide; for a single-practitioner
  // backend that is not a meaningful constraint.
  //
  // Code that reaches the connection directly rather than through a repository
  // — runMigrations() is the only case — runs at startup before any request
  // thread exists and does not take this.
  [[nodiscard]] Lock lock() { return Lock(mutex_); }

private:
  sqlite3 *db_ = nullptr;
  std::recursive_mutex mutex_;
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
//
// The caller must already hold SqliteConnection::lock() and must keep holding
// it until this object is destroyed — otherwise another thread's statement can
// still land between this transaction's BEGIN and its COMMIT.
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
