#include "database.h"

#include <iostream>
#include <sstream>

#include "storage.h"

Database::Database(std::string dataDir) : dataDir(std::move(dataDir)) {}

void Database::loadFromDisk() {
    for (const std::string& tableName : listStoredTableNames(dataDir)) {
        Table table;
        std::string err = loadTable(dataDir, tableName, table);
        if (!err.empty()) {
            std::cerr << "warning: skipping table '" << tableName << "' - " << err << std::endl;
            continue;
        }
        tables.emplace(tableName, std::move(table));
    }
}

namespace {

// renders rows as a plain header + one line per row, space separated.
// nothing fancy - no column alignment, just something readable.
std::string formatResult(const Table& table, const std::vector<const Row*>& rows) {
    std::ostringstream out;

    const auto& cols = table.schema();
    for (size_t i = 0; i < cols.size(); i++) {
        if (i > 0) out << " | ";
        out << cols[i].name;
    }
    out << "\n";

    if (rows.empty()) {
        out << "(0 rows)";
        return out.str();
    }

    for (const Row* row : rows) {
        for (size_t i = 0; i < row->values.size(); i++) {
            if (i > 0) out << " | ";
            out << row->values[i].toDisplayString();
        }
        out << "\n";
    }
    out << "(" << rows.size() << (rows.size() == 1 ? " row)" : " rows)");
    return out.str();
}

} // namespace

std::string Database::doCreateTable(const Statement& stmt) {
    if (tables.find(stmt.tableName) != tables.end()) {
        return "error: table '" + stmt.tableName + "' already exists";
    }
    auto result = tables.emplace(stmt.tableName, Table(stmt.tableName, stmt.columns));

    std::string saveErr = saveTable(dataDir, result.first->second);
    if (!saveErr.empty()) {
        tables.erase(result.first);
        return saveErr;
    }

    return "table '" + stmt.tableName + "' created";
}

std::string Database::doInsert(const Statement& stmt) {
    auto it = tables.find(stmt.tableName);
    if (it == tables.end()) {
        return "error: unknown table '" + stmt.tableName + "'";
    }

    std::string err = it->second.insertRow(stmt.values);
    if (!err.empty()) {
        return err;
    }

    std::string saveErr = saveTable(dataDir, it->second);
    if (!saveErr.empty()) {
        return saveErr;
    }

    return "1 row inserted";
}

std::string Database::doSelect(const Statement& stmt) {
    auto it = tables.find(stmt.tableName);
    if (it == tables.end()) {
        return "error: unknown table '" + stmt.tableName + "'";
    }

    std::vector<const Row*> rows;

    if (stmt.where.present) {
        std::string err = it->second.selectWhere(stmt.where, rows);
        if (!err.empty()) {
            return err;
        }
    } else {
        for (const Row& row : it->second.allRows()) {
            rows.push_back(&row);
        }
    }

    return formatResult(it->second, rows);
}

std::string Database::execute(const Statement& stmt) {
    switch (stmt.type) {
        case StatementType::CREATE_TABLE:
            return doCreateTable(stmt);
        case StatementType::INSERT:
            return doInsert(stmt);
        case StatementType::SELECT:
            return doSelect(stmt);
        default:
            return "error: statement not supported yet";
    }
}
