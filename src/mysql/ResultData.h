#ifndef RESULTDATA_
#define RESULTDATA_
#include <vector>
#include <string>
#include <memory>
#include "MySQLHeader.h"

class ResultDataRow {
public:
	ResultDataRow(unsigned long *lengths, MYSQL_ROW row, unsigned int columnCount);
	ResultDataRow(MYSQL_STMT* statement, MYSQL_BIND* bind, unsigned int columnCount);
	std::vector<std::string> & getValues() {
		return values;
	}
	bool isFieldNull(unsigned int index) {
		return nullFields[index];
	}
private:
	explicit ResultDataRow(unsigned int columns);
	unsigned long long columnCount = 0;
	std::vector<bool> nullFields;
	std::vector<std::string> values;
};

class ResultData {
public:
	explicit ResultData(MYSQL_RES* result);
	ResultData(MYSQL_STMT* result, MYSQL_RES* metaData);
	ResultData();
	~ResultData();
	std::vector<std::string> & getColumns() { return columns; }
	std::vector<ResultDataRow> & getRows() { return rows; }
	std::vector<int> & getColumnTypes() { return columnTypes; }
private:
	ResultData(unsigned int columns, unsigned int rows);
	unsigned int columnCount = 0;
	std::vector<std::string> columns;
	std::vector<int> columnTypes;
	std::vector<ResultDataRow> rows;
};

#endif