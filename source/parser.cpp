#include "parser.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "lexer.h"

namespace {

std::string toUpper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return std::toupper(c); });
    return out;
}

// wraps stoll with a friendlier error for numbers that don't fit in a
// long long, instead of letting the raw stoll exception leak out.
long long parseIntegerLiteral(const std::string& text) {
    try {
        return std::stoll(text);
    } catch (const std::out_of_range&) {
        throw std::runtime_error("number '" + text + "' is too large");
    }
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

    for (size_t i = 0; i < stmt.columns.size(); i++) {
        for (size_t j = i + 1; j < stmt.columns.size(); j++) {
            if (stmt.columns[i].name == stmt.columns[j].name) {
                stmt.errorMessage =
                    "malformed command: column '" + stmt.columns[i].name + "' is defined twice";
                return stmt;
            }
        }
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
            stmt.values.push_back(Value::makeInt(parseIntegerLiteral(tokens[pos].text)));
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

// parses "WHERE <col> = <val>" starting at tokens[pos] (pointing at WHERE).
// advances pos past the whole clause. returns false and sets error on failure.
bool parseWhereClause(const std::vector<Token>& tokens, size_t& pos, WhereClause& where,
                       std::string& error) {
    pos++; // skip WHERE

    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER) {
        error = "malformed command: expected a column name after WHERE";
        return false;
    }
    where.column = tokens[pos].text;
    pos++;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::SYMBOL || tokens[pos].text != "=") {
        error = "malformed command: expected '=' after column name in WHERE (only equality is supported)";
        return false;
    }
    pos++;

    if (pos >= tokens.size()) {
        error = "malformed command: expected a value after '=' in WHERE";
        return false;
    }
    if (tokens[pos].type == TokenType::NUMBER) {
        where.value = Value::makeInt(parseIntegerLiteral(tokens[pos].text));
    } else if (tokens[pos].type == TokenType::STRING) {
        where.value = Value::makeText(tokens[pos].text);
    } else {
        error = "malformed command: expected a number or a quoted string after '=' in WHERE";
        return false;
    }
    pos++;

    where.present = true;
    return true;
}

// SELECT * FROM <name> [WHERE <col> = <val>]
Statement parseSelect(const std::vector<Token>& tokens) {
    Statement stmt;

    size_t pos = 1; // skip SELECT
    if (pos >= tokens.size() || tokens[pos].type != TokenType::SYMBOL || tokens[pos].text != "*") {
        stmt.errorMessage = "malformed command: expected '*' after SELECT (only SELECT * is supported)";
        return stmt;
    }
    pos++;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER ||
        toUpper(tokens[pos].text) != "FROM") {
        stmt.errorMessage = "malformed command: expected FROM after SELECT *";
        return stmt;
    }
    pos++;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER) {
        stmt.errorMessage = "malformed command: expected a table name after FROM";
        return stmt;
    }
    stmt.tableName = tokens[pos].text;
    pos++;

    if (pos < tokens.size() && tokens[pos].type == TokenType::IDENTIFIER &&
        toUpper(tokens[pos].text) == "WHERE") {
        if (!parseWhereClause(tokens, pos, stmt.where, stmt.errorMessage)) {
            return stmt;
        }
    }

    if (pos < tokens.size() && tokens[pos].type != TokenType::END) {
        stmt.errorMessage = "malformed command: unexpected input after statement";
        return stmt;
    }

    stmt.type = StatementType::SELECT;
    return stmt;
}

// DELETE FROM <name> WHERE <col> = <val>
// unlike SELECT, the WHERE clause is required here - we don't want an
// unqualified DELETE wiping a whole table by accident.
Statement parseDelete(const std::vector<Token>& tokens) {
    Statement stmt;

    size_t pos = 1; // skip DELETE
    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER ||
        toUpper(tokens[pos].text) != "FROM") {
        stmt.errorMessage = "malformed command: expected FROM after DELETE";
        return stmt;
    }
    pos++;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER) {
        stmt.errorMessage = "malformed command: expected a table name after FROM";
        return stmt;
    }
    stmt.tableName = tokens[pos].text;
    pos++;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENTIFIER ||
        toUpper(tokens[pos].text) != "WHERE") {
        stmt.errorMessage = "malformed command: DELETE requires a WHERE clause "
                             "(deleting a whole table isn't supported)";
        return stmt;
    }
    if (!parseWhereClause(tokens, pos, stmt.where, stmt.errorMessage)) {
        return stmt;
    }

    if (pos < tokens.size() && tokens[pos].type != TokenType::END) {
        stmt.errorMessage = "malformed command: unexpected input after statement";
        return stmt;
    }

    stmt.type = StatementType::DELETE_STMT;
    return stmt;
}

} // namespace

// figures out which statement kind we're looking at from the first
// keyword, then hands the token list off to that statement's own
// left-to-right parse function.
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

    // tolerate one trailing semicolon, like people habitually type in real
    // sql clients - it carries no meaning here since a statement is always
    // exactly one line, so it's just dropped before the real parsing.
    if (tokens.size() >= 2 && tokens[tokens.size() - 2].type == TokenType::SYMBOL &&
        tokens[tokens.size() - 2].text == ";") {
        tokens.erase(tokens.end() - 2);
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

    // stoll (used for NUMBER tokens) can throw on something like a number
    // literal that's too big for a long long - catch that here instead of
    // letting it crash the whole repl.
    try {
        if (keyword == "CREATE") {
            return parseCreateTable(tokens);
        }

        if (keyword == "INSERT") {
            return parseInsert(tokens);
        }

        if (keyword == "SELECT") {
            return parseSelect(tokens);
        }

        if (keyword == "DELETE") {
            return parseDelete(tokens);
        }
    } catch (const std::exception& e) {
        stmt.type = StatementType::PARSE_ERROR;
        stmt.errorMessage = std::string("malformed command: ") + e.what();
        return stmt;
    }

    stmt.type = StatementType::PARSE_ERROR;
    stmt.errorMessage = "malformed command: unrecognized keyword '" + tokens[0].text + "'";
    return stmt;
}
