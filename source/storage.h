#pragma once

#include <string>
#include <vector>

#include "table.h"

// handles reading/writing tables to our own on-disk format. one file per
// table, named <table>.tbl inside the data directory. format is plain
// text so it's easy to poke at with a text editor while debugging:
//
//   TABLE users
//   COLUMNS id:INTEGER,name:TEXT
//   1,"Prince"
//   2,"Rahul"
//
// each row after the header is one line, values comma separated, text
// values double-quoted with \" and \\ escaping (same as the repl accepts).

// writes the entire table out, overwriting whatever was there before.
// returns an error message on failure, empty string on success.
std::string saveTable(const std::string& dataDir, const Table& table);

// loads a single table from <dataDir>/<tableName>.tbl.
// returns an error message on failure (including "doesn't exist"),
// empty string on success, with outTable filled in.
std::string loadTable(const std::string& dataDir, const std::string& tableName, Table& outTable);

// lists the table names found in the data directory (based on *.tbl files).
// returns an empty list if the directory doesn't exist yet - that's not
// an error, it just means this is a fresh database.
std::vector<std::string> listStoredTableNames(const std::string& dataDir);

// deletes the on-disk file for a table, if any. used when we ever need to
// drop a table's persisted data (not exposed via the repl yet, but the
// storage layer supports it).
void removeTableFile(const std::string& dataDir, const std::string& tableName);
