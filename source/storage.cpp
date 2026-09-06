#include "storage.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

std::string tablePath(const std::string& dataDir, const std::string& tableName) {
    return (fs::path(dataDir) / (tableName + ".tbl")).string();
}

// splits a stored row line into raw field strings, respecting double
// quotes so commas inside a quoted text value don't split the row.
std::vector<std::string> splitRowLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (inQuotes) {
            if (c == '\\' && i + 1 < line.size() && (line[i + 1] == '"' || line[i + 1] == '\\')) {
                current.push_back(line[i + 1]);
                i++;
                continue;
            }
            if (c == '"') {
                inQuotes = false;
                continue;
            }
            current.push_back(c);
        } else {
            if (c == '"') {
                inQuotes = true;
                continue;
            }
            if (c == ',') {
                fields.push_back(current);
                current.clear();
                continue;
            }
            current.push_back(c);
        }
    }
    fields.push_back(current);
    return fields;
}

std::string joinRow(const Row& row) {
    std::ostringstream out;
    for (size_t i = 0; i < row.values.size(); i++) {
        if (i > 0) out << ",";
        out << row.values[i].toStorageString();
    }
    return out.str();
}

} // namespace

std::string saveTable(const std::string& dataDir, const Table& table) {
    std::error_code ec;
    fs::create_directories(dataDir, ec);
    if (ec) {
        return "error: could not create data directory '" + dataDir + "': " + ec.message();
    }

    std::string path = tablePath(dataDir, table.name());
    std::string tmpPath = path + ".tmp";

    std::ofstream out(tmpPath, std::ios::trunc);
    if (!out) {
        return "error: could not open '" + tmpPath + "' for writing";
    }

    out << "TABLE " << table.name() << "\n";
    out << "COLUMNS ";
    const auto& cols = table.schema();
    for (size_t i = 0; i < cols.size(); i++) {
        if (i > 0) out << ",";
        out << cols[i].name << ":" << columnTypeToString(cols[i].type);
    }
    out << "\n";

    for (const Row& row : table.allRows()) {
        out << joinRow(row) << "\n";
    }
    out.close();

    // rename into place so a crash mid-write can't leave a half-written
    // table file behind.
    std::error_code renameEc;
    fs::rename(tmpPath, path, renameEc);
    if (renameEc) {
        return "error: could not save table '" + table.name() + "': " + renameEc.message();
    }

    return "";
}

std::string loadTable(const std::string& dataDir, const std::string& tableName, Table& outTable) {
    std::string path = tablePath(dataDir, tableName);
    if (!fs::exists(path)) {
        return "error: no stored data for table '" + tableName + "'";
    }

    std::ifstream in(path);
    if (!in) {
        return "error: could not open '" + path + "' for reading";
    }

    std::string headerLine;
    if (!std::getline(in, headerLine) || headerLine.rfind("TABLE ", 0) != 0) {
        return "error: corrupt table file '" + path + "' (missing TABLE header)";
    }
    std::string storedName = headerLine.substr(6);

    std::string columnsLine;
    if (!std::getline(in, columnsLine) || columnsLine.rfind("COLUMNS ", 0) != 0) {
        return "error: corrupt table file '" + path + "' (missing COLUMNS header)";
    }
    std::string columnsSpec = columnsLine.substr(8);

    std::vector<ColumnDef> columns;
    std::stringstream colsStream(columnsSpec);
    std::string colEntry;
    while (std::getline(colsStream, colEntry, ',')) {
        size_t sep = colEntry.find(':');
        if (sep == std::string::npos) {
            return "error: corrupt table file '" + path + "' (bad column entry '" + colEntry + "')";
        }
        std::string colName = colEntry.substr(0, sep);
        std::string typeName = colEntry.substr(sep + 1);
        bool ok = false;
        ColumnType type = columnTypeFromString(typeName, ok);
        if (!ok) {
            return "error: corrupt table file '" + path + "' (bad type '" + typeName + "')";
        }
        columns.push_back({colName, type});
    }

    Table table(storedName, columns);

    std::string rowLine;
    while (std::getline(in, rowLine)) {
        if (rowLine.empty()) continue;
        std::vector<std::string> fields = splitRowLine(rowLine);
        if (fields.size() != columns.size()) {
            return "error: corrupt table file '" + path + "' (row has wrong number of fields)";
        }
        std::vector<Value> values;
        for (size_t i = 0; i < fields.size(); i++) {
            if (columns[i].type == ColumnType::INTEGER) {
                values.push_back(Value::makeInt(std::stoll(fields[i])));
            } else {
                values.push_back(Value::makeText(fields[i]));
            }
        }
        std::string err = table.insertRow(values);
        if (!err.empty()) {
            return "error: corrupt table file '" + path + "': " + err;
        }
    }

    outTable = std::move(table);
    return "";
}

std::vector<std::string> listStoredTableNames(const std::string& dataDir) {
    std::vector<std::string> names;
    if (!fs::exists(dataDir)) {
        return names;
    }
    for (const auto& entry : fs::directory_iterator(dataDir)) {
        if (!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        const std::string suffix = ".tbl";
        if (filename.size() > suffix.size() &&
            filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
            names.push_back(filename.substr(0, filename.size() - suffix.size()));
        }
    }
    return names;
}

void removeTableFile(const std::string& dataDir, const std::string& tableName) {
    std::error_code ec;
    fs::remove(tablePath(dataDir, tableName), ec);
}
