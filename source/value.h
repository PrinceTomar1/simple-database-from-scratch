#pragma once

#include <string>

// we only support two column types - keeps everything else simple.
enum class ColumnType {
    INTEGER,
    TEXT
};

// converts a type name from the parser (e.g. "INTEGER") into a ColumnType.
// sets ok to false if the name isn't recognized.
ColumnType columnTypeFromString(const std::string& name, bool& ok);
std::string columnTypeToString(ColumnType type);

// a single cell value. tagged union-ish struct rather than std::variant
// just because it reads a little more plainly for this project.
struct Value {
    ColumnType type;
    long long intValue = 0;
    std::string textValue;

    static Value makeInt(long long v);
    static Value makeText(const std::string& v);

    // how this value gets printed in query results
    std::string toDisplayString() const;

    // how this value gets written into a table's data file. text values
    // are quoted and escaped, integers are just the plain number.
    std::string toStorageString() const;

    // used as a key when we build the per-column index
    std::string toIndexKey() const;
};
