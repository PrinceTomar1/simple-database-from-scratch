#pragma once

#include <string>

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

// this will grow as we add real fields for each statement kind.
// for now it's just enough to tell the repl what kind of line it saw.
struct Statement {
    StatementType type = StatementType::PARSE_ERROR;
    std::string errorMessage;
};

Statement parseStatement(const std::string& line);
