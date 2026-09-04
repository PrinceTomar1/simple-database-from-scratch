#pragma once

#include <string>
#include <vector>

#include "value.h"

// the kinds of statements our tiny sql dialect understands.
// PARSE_ERROR means we couldn't make sense of the input at all.
enum class StatementType {
    CREATE_TABLE,
    INSERT,
    SELECT,
    DELETE_STMT,
    EXIT,
    EMPTY,
    PARSE_ERROR
};

struct ColumnDef {
    std::string name;
    ColumnType type;
};

// this will grow as we add real fields for each statement kind.
struct Statement {
    StatementType type = StatementType::PARSE_ERROR;
    std::string errorMessage;

    // CREATE TABLE
    std::string tableName;
    std::vector<ColumnDef> columns;

    // INSERT
    std::vector<Value> values;
};

Statement parseStatement(const std::string& line);
