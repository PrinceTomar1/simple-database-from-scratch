#pragma once

#include <string>
#include <vector>

// token types the lexer can produce. keeping this small on purpose -
// the grammar we support doesn't need much more than this.
enum class TokenType {
    IDENTIFIER,
    NUMBER,
    STRING,
    SYMBOL,
    END
};

struct Token {
    TokenType type;
    std::string text;
};

// turns a single line of input into a list of tokens.
// throws std::runtime_error on things like unterminated strings.
std::vector<Token> tokenize(const std::string& line);
