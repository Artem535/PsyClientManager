#include "db/sqlite_connection.h"
#include "db/migrations.h"

#include <gtest/gtest.h>
#include <sqlite3.h>

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
