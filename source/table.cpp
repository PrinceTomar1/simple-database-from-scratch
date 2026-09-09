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

void Table::indexRow(size_t rowPos) {
    const Row& row = rows[rowPos];
    for (size_t col = 0; col < columnDefs.size(); col++) {
        columnIndexes[static_cast<int>(col)][row.values[col].toIndexKey()].push_back(rowPos);
    }
}

void Table::rebuildIndexes() {
    columnIndexes.clear();
    for (size_t i = 0; i < rows.size(); i++) {
        indexRow(i);
    }
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
    indexRow(rows.size() - 1);
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

    // hash lookup instead of scanning every row - this is the whole
    // point of keeping the per-column index around.
    auto colIt = columnIndexes.find(colIndex);
    if (colIt == columnIndexes.end()) {
        return "";
    }
    auto valueIt = colIt->second.find(where.value.toIndexKey());
    if (valueIt == colIt->second.end()) {
        return "";
    }
    for (size_t rowPos : valueIt->second) {
        matches.push_back(&rows[rowPos]);
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

    // row positions shifted, so the cheapest correct thing to do is just
    // rebuild the index from scratch. tables are small enough that this
    // is fine.
    if (outDeletedCount > 0) {
        rebuildIndexes();
    }
    return "";
}
