#include "db/sqlite_connection.h"
#include "db/migrations.h"

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <stdexcept>
#include <string>

namespace {

int countRows(pcm::tokenbackend::SqliteConnection &conn, const std::string &sql) {
  sqlite3_stmt *stmt = nullptr;
  EXPECT_EQ(sqlite3_prepare_v2(conn.raw(), sql.c_str(), -1, &stmt, nullptr), SQLITE_OK);
  int value = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    value = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return value;
}

} // namespace

TEST(SqliteConnectionTest, ExecRunsStatement) {
  pcm::tokenbackend::SqliteConnection conn(":memory:");
  conn.exec("CREATE TABLE t (id INTEGER PRIMARY KEY)");
  conn.exec("INSERT INTO t (id) VALUES (1)");

  sqlite3_stmt *stmt = nullptr;
  sqlite3_prepare_v2(conn.raw(), "SELECT COUNT(*) FROM t", -1, &stmt, nullptr);
  sqlite3_step(stmt);
  EXPECT_EQ(sqlite3_column_int(stmt, 0), 1);
  sqlite3_finalize(stmt);
}

// The connection is shared across oat++'s worker threads, so serialized mode
// is requested explicitly (SQLITE_OPEN_FULLMUTEX) rather than inherited from
// however libsqlite3 was compiled.
TEST(SqliteConnectionTest, LibraryIsThreadsafe) {
  EXPECT_NE(sqlite3_threadsafe(), 0)
      << "libsqlite3 was compiled with SQLITE_THREADSAFE=0; SQLITE_OPEN_FULLMUTEX "
         "cannot rescue that and the shared connection is unsafe";
}

TEST(SqliteTransactionTest, CommitPersistsEveryStatementInTheSequence) {
  pcm::tokenbackend::SqliteConnection conn(":memory:");
  conn.exec("CREATE TABLE t (id INTEGER PRIMARY KEY)");

  {
    pcm::tokenbackend::SqliteTransaction tx(conn);
    conn.exec("INSERT INTO t (id) VALUES (1)");
    conn.exec("INSERT INTO t (id) VALUES (2)");
    tx.commit();
  }

  EXPECT_EQ(countRows(conn, "SELECT COUNT(*) FROM t"), 2);
}

TEST(SqliteTransactionTest, RollsBackWhenScopeExitsWithoutCommit) {
  pcm::tokenbackend::SqliteConnection conn(":memory:");
  conn.exec("CREATE TABLE t (id INTEGER PRIMARY KEY)");
  conn.exec("INSERT INTO t (id) VALUES (1)");

  {
    pcm::tokenbackend::SqliteTransaction tx(conn);
    conn.exec("DELETE FROM t");
    conn.exec("INSERT INTO t (id) VALUES (2)");
    // No commit() — the destructor must undo both statements.
  }

  EXPECT_EQ(countRows(conn, "SELECT COUNT(*) FROM t"), 1);
  EXPECT_EQ(countRows(conn, "SELECT id FROM t"), 1) << "the original row must survive";
}

TEST(SqliteTransactionTest, RollsBackWhenAStatementThrowsMidSequence) {
  pcm::tokenbackend::SqliteConnection conn(":memory:");
  conn.exec("CREATE TABLE t (id INTEGER PRIMARY KEY)");
  conn.exec("INSERT INTO t (id) VALUES (1)");

  EXPECT_THROW(
      {
        pcm::tokenbackend::SqliteTransaction tx(conn);
        conn.exec("DELETE FROM t");
        conn.exec("INSERT INTO nonexistent_table (id) VALUES (2)"); // throws
        tx.commit();
      },
      std::runtime_error);

  EXPECT_EQ(countRows(conn, "SELECT COUNT(*) FROM t"), 1)
      << "the DELETE must not survive a failure later in the sequence";
}

TEST(MigrationsTest, CreatesAllThreeTables) {
  pcm::tokenbackend::SqliteConnection conn(":memory:");
  pcm::tokenbackend::runMigrations(conn);

  for (const std::string &table : {"accounts", "meetings", "invitations"}) {
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(conn.raw(),
                        "SELECT name FROM sqlite_master WHERE type='table' AND name=?", -1,
                        &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, table.c_str(), -1, SQLITE_TRANSIENT);
    EXPECT_EQ(sqlite3_step(stmt), SQLITE_ROW) << "missing table " << table;
    sqlite3_finalize(stmt);
  }
}
