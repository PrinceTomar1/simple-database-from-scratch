#include "parser.h"

#include <algorithm>
#include <cctype>

#include "lexer.h"

namespace {

std::string toUpper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return std::toupper(c); });
    return out;
}

// CREATE TABLE <name> ( <col> <TYPE>, <col> <TYPE>, ... )
// tokens[0] is the CREATE keyword we already consumed conceptually.
Statement parseCreateTable(const std::vector<Token>& tokens) {
    Statement stmt;

    size_t pos = 1; // skip CREATE
    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER ||
        toUpper(tokens[pos].text) != "TABLE") {
        stmt.errorMessage = "malformed command: expected TABLE after CREATE";
        return stmt;
    }
    pos++;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER) {
        stmt.errorMessage = "malformed command: expected a table name after TABLE";
        return stmt;
    }
    stmt.tableName = tokens[pos].text;
    pos++;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::SYMBOL || tokens[pos].text != "(") {
        stmt.errorMessage = "malformed command: expected '(' after table name";
        return stmt;
    }
    pos++;

    bool expectMore = true;
    while (expectMore) {
        if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER) {
            stmt.errorMessage = "malformed command: expected a column name";
            return stmt;
        }
        std::string colName = tokens[pos].text;
        pos++;

        if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER) {
            stmt.errorMessage = "malformed command: expected a type for column '" + colName + "'";
            return stmt;
        }
        bool ok = false;
        ColumnType colType = columnTypeFromString(tokens[pos].text, ok);
        if (!ok) {
            stmt.errorMessage = "malformed command: unknown type '" + tokens[pos].text +
                                 "' (only INTEGER and TEXT are supported)";
            return stmt;
        }
        pos++;

        stmt.columns.push_back({colName, colType});

        if (pos >= tokens.size() || tokens[pos].type != TokenType::SYMBOL) {
            stmt.errorMessage = "malformed command: expected ',' or ')' after column definition";
            return stmt;
        }
        if (tokens[pos].text == ",") {
            pos++;
            continue;
        }
        if (tokens[pos].text == ")") {
            pos++;
            expectMore = false;
            continue;
        }
        stmt.errorMessage = "malformed command: expected ',' or ')' after column definition";
        return stmt;
    }

    if (stmt.columns.empty()) {
        stmt.errorMessage = "malformed command: a table needs at least one column";
        return stmt;
    }

    if (pos < tokens.size() && tokens[pos].type != TokenType::END) {
        stmt.errorMessage = "malformed command: unexpected input after ')'";
        return stmt;
    }

    stmt.type = StatementType::CREATE_TABLE;
    return stmt;
}

// INSERT INTO <name> VALUES ( <val>, <val>, ... )
Statement parseInsert(const std::vector<Token>& tokens) {
    Statement stmt;

    size_t pos = 1; // skip INSERT
    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER ||
        toUpper(tokens[pos].text) != "INTO") {
        stmt.errorMessage = "malformed command: expected INTO after INSERT";
        return stmt;
    }
    pos++;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER) {
        stmt.errorMessage = "malformed command: expected a table name after INTO";
        return stmt;
    }
    stmt.tableName = tokens[pos].text;
    pos++;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER ||
        toUpper(tokens[pos].text) != "VALUES") {
        stmt.errorMessage = "malformed command: expected VALUES after table name";
        return stmt;
    }
    pos++;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::SYMBOL || tokens[pos].text != "(") {
        stmt.errorMessage = "malformed command: expected '(' after VALUES";
        return stmt;
    }
    pos++;

    bool expectMore = true;
    while (expectMore) {
        if (pos >= tokens.size()) {
            stmt.errorMessage = "malformed command: unexpected end of input in VALUES list";
            return stmt;
        }
        if (tokens[pos].type == TokenType::NUMBER) {
            stmt.values.push_back(Value::makeInt(std::stoll(tokens[pos].text)));
            pos++;
        } else if (tokens[pos].type == TokenType::STRING) {
            stmt.values.push_back(Value::makeText(tokens[pos].text));
            pos++;
        } else {
            stmt.errorMessage = "malformed command: expected a number or a quoted string in VALUES list";
            return stmt;
        }

        if (pos >= tokens.size() || tokens[pos].type != TokenType::SYMBOL) {
            stmt.errorMessage = "malformed command: expected ',' or ')' in VALUES list";
            return stmt;
        }
        if (tokens[pos].text == ",") {
            pos++;
            continue;
        }
        if (tokens[pos].text == ")") {
            pos++;
            expectMore = false;
            continue;
        }
        stmt.errorMessage = "malformed command: expected ',' or ')' in VALUES list";
        return stmt;
    }

    if (pos < tokens.size() && tokens[pos].type != TokenType::END) {
        stmt.errorMessage = "malformed command: unexpected input after ')'";
        return stmt;
    }

    stmt.type = StatementType::INSERT;
    return stmt;
}

} // namespace

// this is still a skeleton - it just figures out which statement kind
// we're looking at based on the first keyword. the real per-statement
// parsing gets filled in as each command gets built out.
Statement parseStatement(const std::string& line) {
    Statement stmt;

    // trim whitespace to check for an empty line before tokenizing
    std::string trimmed = line;
    size_t start = trimmed.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        stmt.type = StatementType::EMPTY;
        return stmt;
    }

    std::vector<Token> tokens;
    try {
        tokens = tokenize(line);
    } catch (const std::exception& e) {
        stmt.type = StatementType::PARSE_ERROR;
        stmt.errorMessage = std::string("malformed command: ") + e.what();
        return stmt;
    }

    if (tokens.empty() || tokens[0].type == TokenType::END) {
        stmt.type = StatementType::EMPTY;
        return stmt;
    }

    if (tokens[0].type != TokenType::IDENTIFIER) {
        stmt.type = StatementType::PARSE_ERROR;
        stmt.errorMessage = "malformed command: expected a keyword to start the statement";
        return stmt;
    }

    std::string keyword = toUpper(tokens[0].text);

    if (keyword == "EXIT" || keyword == "QUIT") {
        stmt.type = StatementType::EXIT;
        return stmt;
    }

    if (keyword == "CREATE") {
        return parseCreateTable(tokens);
    }

    if (keyword == "INSERT") {
        return parseInsert(tokens);
    }

    if (keyword == "SELECT" || keyword == "DELETE") {
        // real parsing for these lands in later commits. for now we just
        // acknowledge we recognize the shape of the command.
        stmt.type = StatementType::PARSE_ERROR;
        stmt.errorMessage = "malformed command: " + keyword + " is not fully implemented yet";
        return stmt;
    }

    stmt.type = StatementType::PARSE_ERROR;
    stmt.errorMessage = "malformed command: unrecognized keyword '" + tokens[0].text + "'";
    return stmt;
}
