#pragma once

#include <string>
#include <unordered_map>

#include "parser.h"
#include "table.h"

// Database owns all the tables and is the thing the repl talks to.
// execute() takes an already-parsed statement and returns text to print
// (either a result or a specific error message - never throws).
class Database {
public:
    explicit Database(std::string dataDir);

    std::string execute(const Statement& stmt);

private:
    std::string dataDir;
    std::unordered_map<std::string, Table> tables;

    std::string doCreateTable(const Statement& stmt);
    std::string doInsert(const Statement& stmt);
};
