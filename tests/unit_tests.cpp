// unit tests for the parser/table/storage/database logic.
// these link directly against the source files (minus main.cpp) and
// exercise them without going through the repl at all.
//
// run with: ./unit_tests (built via `make test` or the unit_tests target)

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <limits>

#include "../source/database.h"
#include "../source/lexer.h"
#include "../source/parser.h"
#include "../source/storage.h"
#include "../source/table.h"

namespace fs = std::filesystem;

static int testsRun = 0;

#define RUN(fn)                        \
    do {                               \
        std::cout << "  " << #fn << std::endl; \
        fn();                          \
        testsRun++;                    \
    } while (0)

void test_lexer_basic_tokens() {
    auto tokens = tokenize("CREATE TABLE users (id INTEGER, name TEXT)");
    assert(tokens[0].type == TokenType::IDENTIFIER && tokens[0].text == "CREATE");
    assert(tokens[1].type == TokenType::IDENTIFIER && tokens[1].text == "TABLE");
    assert(tokens[2].type == TokenType::IDENTIFIER && tokens[2].text == "users");
    assert(tokens[3].type == TokenType::SYMBOL && tokens[3].text == "(");
    assert(tokens.back().type == TokenType::END);
}

void test_lexer_string_and_negative_number() {
    auto tokens = tokenize("VALUES (-5, \"hi there\")");
    // VALUES ( -5 , "hi there" ) END
    assert(tokens[2].type == TokenType::NUMBER && tokens[2].text == "-5");
    assert(tokens[4].type == TokenType::STRING && tokens[4].text == "hi there");
}

void test_lexer_escaped_quote() {
    auto tokens = tokenize("\"she said \\\"hi\\\"\"");
    assert(tokens[0].type == TokenType::STRING);
    assert(tokens[0].text == "she said \"hi\"");
}

void test_lexer_unterminated_string_throws() {
    bool threw = false;
    try {
        tokenize("\"never closed");
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

void test_parse_create_table() {
    Statement stmt = parseStatement("CREATE TABLE users (id INTEGER, name TEXT)");
    assert(stmt.type == StatementType::CREATE_TABLE);
    assert(stmt.tableName == "users");
    assert(stmt.columns.size() == 2);
    assert(stmt.columns[0].name == "id" && stmt.columns[0].type == ColumnType::INTEGER);
    assert(stmt.columns[1].name == "name" && stmt.columns[1].type == ColumnType::TEXT);
}

void test_parse_create_table_bad_type() {
    Statement stmt = parseStatement("CREATE TABLE users (id WEIRD)");
    assert(stmt.type == StatementType::PARSE_ERROR);
    assert(stmt.errorMessage.find("unknown type") != std::string::npos);
}

void test_parse_create_table_duplicate_column_rejected() {
    Statement stmt = parseStatement("CREATE TABLE users (id INTEGER, id TEXT)");
    assert(stmt.type == StatementType::PARSE_ERROR);
    assert(stmt.errorMessage.find("defined twice") != std::string::npos);
}

void test_parse_tolerates_trailing_semicolon() {
    Statement withSemi = parseStatement("SELECT * FROM users;");
    Statement withoutSemi = parseStatement("SELECT * FROM users");
    assert(withSemi.type == StatementType::SELECT);
    assert(withSemi.tableName == withoutSemi.tableName);

    // a semicolon isn't a statement separator - anything after it should
    // still be a parse error, not silently accepted as two statements.
    Statement twoStatements = parseStatement("SELECT * FROM users; SELECT * FROM other");
    assert(twoStatements.type == StatementType::PARSE_ERROR);
}

void test_parse_insert() {
    Statement stmt = parseStatement("INSERT INTO users VALUES (1, \"Prince\")");
    assert(stmt.type == StatementType::INSERT);
    assert(stmt.tableName == "users");
    assert(stmt.values.size() == 2);
    assert(stmt.values[0].type == ColumnType::INTEGER && stmt.values[0].intValue == 1);
    assert(stmt.values[1].type == ColumnType::TEXT && stmt.values[1].textValue == "Prince");
}

void test_parse_select_with_where() {
    Statement stmt = parseStatement("SELECT * FROM users WHERE id = 1");
    assert(stmt.type == StatementType::SELECT);
    assert(stmt.tableName == "users");
    assert(stmt.where.present);
    assert(stmt.where.column == "id");
    assert(stmt.where.value.intValue == 1);
}

void test_parse_select_without_where() {
    Statement stmt = parseStatement("SELECT * FROM users");
    assert(stmt.type == StatementType::SELECT);
    assert(!stmt.where.present);
}

void test_parse_delete_requires_where() {
    Statement stmt = parseStatement("DELETE FROM users");
    assert(stmt.type == StatementType::PARSE_ERROR);
    assert(stmt.errorMessage.find("WHERE") != std::string::npos);
}

void test_parse_delete_with_where() {
    Statement stmt = parseStatement("DELETE FROM users WHERE id = 2");
    assert(stmt.type == StatementType::DELETE_STMT);
    assert(stmt.where.present);
    assert(stmt.where.value.intValue == 2);
}

void test_parse_exit_and_empty() {
    assert(parseStatement("EXIT").type == StatementType::EXIT);
    assert(parseStatement("   ").type == StatementType::EMPTY);
    assert(parseStatement("").type == StatementType::EMPTY);
}

void test_parse_malformed_input_does_not_throw() {
    // garbage input should come back as a PARSE_ERROR, never an exception
    Statement stmt = parseStatement("this is not sql at all !!! ###");
    assert(stmt.type == StatementType::PARSE_ERROR);
}

void test_table_insert_and_type_checking() {
    Table table("users", {{"id", ColumnType::INTEGER}, {"name", ColumnType::TEXT}});

    std::string err = table.insertRow({Value::makeInt(1), Value::makeText("Prince")});
    assert(err.empty());
    assert(table.allRows().size() == 1);

    err = table.insertRow({Value::makeInt(2)});
    assert(!err.empty()); // wrong number of values

    err = table.insertRow({Value::makeText("nope"), Value::makeText("x")});
    assert(!err.empty()); // type mismatch
    assert(table.allRows().size() == 1); // failed inserts don't add rows
}

void test_table_select_where() {
    Table table("users", {{"id", ColumnType::INTEGER}, {"name", ColumnType::TEXT}});
    table.insertRow({Value::makeInt(1), Value::makeText("Prince")});
    table.insertRow({Value::makeInt(2), Value::makeText("Rahul")});

    WhereClause where;
    where.present = true;
    where.column = "id";
    where.value = Value::makeInt(2);

    std::vector<const Row*> matches;
    std::string err = table.selectWhere(where, matches);
    assert(err.empty());
    assert(matches.size() == 1);
    assert(matches[0]->values[1].textValue == "Rahul");

    WhereClause badCol;
    badCol.present = true;
    badCol.column = "nope";
    badCol.value = Value::makeInt(1);
    std::vector<const Row*> noMatches;
    err = table.selectWhere(badCol, noMatches);
    assert(!err.empty());
}

void test_table_delete_where() {
    Table table("users", {{"id", ColumnType::INTEGER}, {"name", ColumnType::TEXT}});
    table.insertRow({Value::makeInt(1), Value::makeText("Prince")});
    table.insertRow({Value::makeInt(2), Value::makeText("Rahul")});
    table.insertRow({Value::makeInt(3), Value::makeText("Amit")});

    WhereClause where;
    where.present = true;
    where.column = "id";
    where.value = Value::makeInt(2);

    size_t deleted = 0;
    std::string err = table.deleteWhere(where, deleted);
    assert(err.empty());
    assert(deleted == 1);
    assert(table.allRows().size() == 2);

    // make sure the index still works correctly after a delete (positions
    // shifted, so this also checks the rebuild logic)
    WhereClause findId3;
    findId3.present = true;
    findId3.column = "id";
    findId3.value = Value::makeInt(3);
    std::vector<const Row*> matches;
    table.selectWhere(findId3, matches);
    assert(matches.size() == 1);
    assert(matches[0]->values[1].textValue == "Amit");
}

void test_storage_round_trip() {
    std::string dir = "tests/tmp_data/storage_round_trip";
    fs::remove_all(dir);

    Table table("users", {{"id", ColumnType::INTEGER}, {"name", ColumnType::TEXT}});
    table.insertRow({Value::makeInt(1), Value::makeText("Prince")});
    table.insertRow({Value::makeInt(2), Value::makeText("has, a comma and \"quotes\"")});

    std::string err = saveTable(dir, table);
    assert(err.empty());

    Table loaded;
    err = loadTable(dir, "users", loaded);
    assert(err.empty());
    assert(loaded.name() == "users");
    assert(loaded.allRows().size() == 2);
    assert(loaded.allRows()[0].values[1].textValue == "Prince");
    assert(loaded.allRows()[1].values[1].textValue == "has, a comma and \"quotes\"");

    fs::remove_all(dir);
}

void test_storage_int64_boundary_values() {
    std::string dir = "tests/tmp_data/storage_int64_boundary";
    fs::remove_all(dir);

    const long long maxVal = std::numeric_limits<long long>::max();
    const long long minVal = std::numeric_limits<long long>::min();

    Table table("nums", {{"id", ColumnType::INTEGER}});
    table.insertRow({Value::makeInt(maxVal)});
    table.insertRow({Value::makeInt(minVal)});
    table.insertRow({Value::makeInt(0)});

    std::string err = saveTable(dir, table);
    assert(err.empty());

    Table loaded;
    err = loadTable(dir, "nums", loaded);
    assert(err.empty());
    assert(loaded.allRows()[0].values[0].intValue == maxVal);
    assert(loaded.allRows()[1].values[0].intValue == minVal);
    assert(loaded.allRows()[2].values[0].intValue == 0);

    fs::remove_all(dir);
}

void test_storage_missing_table() {
    std::string dir = "tests/tmp_data/storage_missing";
    fs::remove_all(dir);
    Table loaded;
    std::string err = loadTable(dir, "nope", loaded);
    assert(!err.empty());
}

void test_database_end_to_end() {
    std::string dir = "tests/tmp_data/database_e2e";
    fs::remove_all(dir);
    Database db(dir);
    db.loadFromDisk();

    std::string result = db.execute(parseStatement("CREATE TABLE users (id INTEGER, name TEXT)"));
    assert(result.find("created") != std::string::npos);

    result = db.execute(parseStatement("INSERT INTO users VALUES (1, \"Prince\")"));
    assert(result.find("inserted") != std::string::npos);

    result = db.execute(parseStatement("SELECT * FROM users"));
    assert(result.find("Prince") != std::string::npos);

    result = db.execute(parseStatement("DELETE FROM users WHERE id = 1"));
    assert(result.find("deleted") != std::string::npos);

    result = db.execute(parseStatement("SELECT * FROM users"));
    assert(result.find("Prince") == std::string::npos);

    fs::remove_all(dir);
}

void test_database_persists_across_instances() {
    std::string dir = "tests/tmp_data/database_persist";
    fs::remove_all(dir);

    {
        Database db(dir);
        db.loadFromDisk();
        db.execute(parseStatement("CREATE TABLE users (id INTEGER, name TEXT)"));
        db.execute(parseStatement("INSERT INTO users VALUES (1, \"Prince\")"));
    }

    {
        Database db(dir);
        db.loadFromDisk();
        std::string result = db.execute(parseStatement("SELECT * FROM users"));
        assert(result.find("Prince") != std::string::npos);
    }

    fs::remove_all(dir);
}

int main() {
    std::cout << "running unit tests..." << std::endl;

    RUN(test_lexer_basic_tokens);
    RUN(test_lexer_string_and_negative_number);
    RUN(test_lexer_escaped_quote);
    RUN(test_lexer_unterminated_string_throws);

    RUN(test_parse_create_table);
    RUN(test_parse_create_table_bad_type);
    RUN(test_parse_create_table_duplicate_column_rejected);
    RUN(test_parse_tolerates_trailing_semicolon);
    RUN(test_parse_insert);
    RUN(test_parse_select_with_where);
    RUN(test_parse_select_without_where);
    RUN(test_parse_delete_requires_where);
    RUN(test_parse_delete_with_where);
    RUN(test_parse_exit_and_empty);
    RUN(test_parse_malformed_input_does_not_throw);

    RUN(test_table_insert_and_type_checking);
    RUN(test_table_select_where);
    RUN(test_table_delete_where);

    RUN(test_storage_round_trip);
    RUN(test_storage_int64_boundary_values);
    RUN(test_storage_missing_table);

    RUN(test_database_end_to_end);
    RUN(test_database_persists_across_instances);

    std::cout << testsRun << " tests passed" << std::endl;
    return 0;
}
