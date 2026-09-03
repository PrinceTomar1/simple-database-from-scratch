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

    int findColumnIndex(const std::string& colName) const;

private:
    std::string tableName;
    std::vector<ColumnDef> columnDefs;
};
