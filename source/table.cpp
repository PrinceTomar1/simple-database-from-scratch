#include "table.h"

#include <algorithm>

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

std::string Table::selectWhere(const WhereClause& where, std::vector<const Row*>& matches) const {
    int colIndex = findColumnIndex(where.column);
    if (colIndex < 0) {
        return "error: unknown column '" + where.column + "' on table '" + tableName + "'";
    }
    if (columnDefs[colIndex].type != where.value.type) {
        return "error: type mismatch for column '" + where.column + "' (expected " +
               columnTypeToString(columnDefs[colIndex].type) + ", got " +
               columnTypeToString(where.value.type) + ")";
    }

    for (const Row& row : rows) {
        const Value& cell = row.values[colIndex];
        bool matched = (cell.type == ColumnType::INTEGER)
                           ? (cell.intValue == where.value.intValue)
                           : (cell.textValue == where.value.textValue);
        if (matched) {
            matches.push_back(&row);
        }
    }
    return "";
}

std::string Table::deleteWhere(const WhereClause& where, size_t& outDeletedCount) {
    int colIndex = findColumnIndex(where.column);
    if (colIndex < 0) {
        return "error: unknown column '" + where.column + "' on table '" + tableName + "'";
    }
    if (columnDefs[colIndex].type != where.value.type) {
        return "error: type mismatch for column '" + where.column + "' (expected " +
               columnTypeToString(columnDefs[colIndex].type) + ", got " +
               columnTypeToString(where.value.type) + ")";
    }

    size_t before = rows.size();
    rows.erase(std::remove_if(rows.begin(), rows.end(),
                               [&](const Row& row) {
                                   const Value& cell = row.values[colIndex];
                                   return (cell.type == ColumnType::INTEGER)
                                              ? (cell.intValue == where.value.intValue)
                                              : (cell.textValue == where.value.textValue);
                               }),
               rows.end());

    outDeletedCount = before - rows.size();
    return "";
}
