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
