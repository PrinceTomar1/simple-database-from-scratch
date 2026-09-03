#include "value.h"

#include <algorithm>
#include <cctype>

namespace {
std::string toUpper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return std::toupper(c); });
    return out;
}
} // namespace

ColumnType columnTypeFromString(const std::string& name, bool& ok) {
    ok = true;
    std::string upper = toUpper(name);
    if (upper == "INTEGER") {
        return ColumnType::INTEGER;
    }
    if (upper == "TEXT") {
        return ColumnType::TEXT;
    }
    ok = false;
    return ColumnType::INTEGER;
}

std::string columnTypeToString(ColumnType type) {
    switch (type) {
        case ColumnType::INTEGER:
            return "INTEGER";
        case ColumnType::TEXT:
            return "TEXT";
    }
    return "UNKNOWN";
}

Value Value::makeInt(long long v) {
    Value val;
    val.type = ColumnType::INTEGER;
    val.intValue = v;
    return val;
}

Value Value::makeText(const std::string& v) {
    Value val;
    val.type = ColumnType::TEXT;
    val.textValue = v;
    return val;
}

std::string Value::toDisplayString() const {
    if (type == ColumnType::INTEGER) {
        return std::to_string(intValue);
    }
    return textValue;
}

std::string Value::toStorageString() const {
    if (type == ColumnType::INTEGER) {
        return std::to_string(intValue);
    }
    std::string escaped;
    escaped.push_back('"');
    for (char c : textValue) {
        if (c == '"' || c == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    escaped.push_back('"');
    return escaped;
}

std::string Value::toIndexKey() const {
    if (type == ColumnType::INTEGER) {
        return "i:" + std::to_string(intValue);
    }
    return "t:" + textValue;
}
