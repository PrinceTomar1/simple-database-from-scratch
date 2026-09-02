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

    if (keyword == "CREATE" || keyword == "INSERT" || keyword == "SELECT" || keyword == "DELETE") {
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
