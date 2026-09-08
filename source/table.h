#pragma once

#include <string>
#include <vector>

#include "parser.h"
#include "value.h"

// a single row is just a vector of values, one per column, in schema order.
struct Row {
    std::vector<Value> values;
};

class Table {
public:
    Table() = default;
    Table(std::string name, std::vector<ColumnDef> columns);

    const std::string& name() const { return tableName; }
    const std::vector<ColumnDef>& schema() const { return columnDefs; }
    const std::vector<Row>& allRows() const { return rows; }

    int findColumnIndex(const std::string& colName) const;

    // returns an error message on failure, empty string on success.
    std::string insertRow(const std::vector<Value>& values);

    // finds rows matching a WHERE clause (equality on one column).
    // returns an error message on failure (e.g. unknown column),
    // empty string on success with matches filled in.
    std::string selectWhere(const WhereClause& where, std::vector<const Row*>& matches) const;

private:
    std::string tableName;
    std::vector<ColumnDef> columnDefs;
    std::vector<Row> rows;
};
