#include "database.h"

Database::Database(std::string dataDir) : dataDir(std::move(dataDir)) {}

std::string Database::doCreateTable(const Statement& stmt) {
    if (tables.find(stmt.tableName) != tables.end()) {
        return "error: table '" + stmt.tableName + "' already exists";
    }
    tables.emplace(stmt.tableName, Table(stmt.tableName, stmt.columns));
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
    return "1 row inserted";
}

std::string Database::execute(const Statement& stmt) {
    switch (stmt.type) {
        case StatementType::CREATE_TABLE:
            return doCreateTable(stmt);
        case StatementType::INSERT:
            return doInsert(stmt);
        default:
            return "error: statement not supported yet";
    }
}
