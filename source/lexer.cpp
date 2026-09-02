#include "lexer.h"

#include <cctype>
#include <stdexcept>

namespace {

bool isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

} // namespace

std::vector<Token> tokenize(const std::string& line) {
    std::vector<Token> tokens;
    size_t i = 0;
    size_t n = line.size();

    while (i < n) {
        char c = line[i];

        if (std::isspace(static_cast<unsigned char>(c))) {
            i++;
            continue;
        }

        // string literal, e.g. "Prince"
        if (c == '"') {
            std::string value;
            i++; // skip opening quote
            bool closed = false;
            while (i < n) {
                char ch = line[i];
                if (ch == '\\' && i + 1 < n) {
                    char next = line[i + 1];
                    if (next == '"' || next == '\\') {
                        value.push_back(next);
                        i += 2;
                        continue;
                    }
                }
                if (ch == '"') {
                    closed = true;
                    i++;
                    break;
                }
                value.push_back(ch);
                i++;
            }
            if (!closed) {
                throw std::runtime_error("unterminated string literal");
            }
            tokens.push_back({TokenType::STRING, value});
            continue;
        }

        // number literal (integers, optionally negative)
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '-' && i + 1 < n && std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            std::string value;
            value.push_back(c);
            i++;
            while (i < n && std::isdigit(static_cast<unsigned char>(line[i]))) {
                value.push_back(line[i]);
                i++;
            }
            tokens.push_back({TokenType::NUMBER, value});
            continue;
        }

        // identifier / keyword
        if (isIdentStart(c)) {
            std::string value;
            while (i < n && isIdentChar(line[i])) {
                value.push_back(line[i]);
                i++;
            }
            tokens.push_back({TokenType::IDENTIFIER, value});
            continue;
        }

        // single-char symbols we care about
        if (c == '(' || c == ')' || c == ',' || c == '=' || c == '*' || c == ';') {
            tokens.push_back({TokenType::SYMBOL, std::string(1, c)});
            i++;
            continue;
        }

        throw std::runtime_error(std::string("unexpected character '") + c + "'");
    }

    tokens.push_back({TokenType::END, ""});
    return tokens;
}
