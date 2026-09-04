#include "table.h"

Table::Table(std::string name, std::vector<ColumnDef> columns)
    : tableName(std::move(name)), columnDefs(std::move(columns)) {}

int Table::findColumnIndex(const std::string& colName) const {
    for (size_t i = 0; i < columnDefs.size(); i++) {
        if (columnDefs[i].name == colName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string Table::insertRow(const std::vector<Value>& values) {
    if (values.size() != columnDefs.size()) {
        return "error: table '" + tableName + "' has " + std::to_string(columnDefs.size()) +
               " column(s) but " + std::to_string(values.size()) + " value(s) were given";
    }

    for (size_t i = 0; i < values.size(); i++) {
        if (values[i].type != columnDefs[i].type) {
            return "error: type mismatch for column '" + columnDefs[i].name + "' (expected " +
                   columnTypeToString(columnDefs[i].type) + ", got " +
                   columnTypeToString(values[i].type) + ")";
        }
    }

    Row row;
    row.values = values;
    rows.push_back(std::move(row));
    return "";
}
