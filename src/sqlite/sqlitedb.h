#ifndef __SQLITEDB_H__
#define __SQLITEDB_H__

#include "sqlite.h"

//#define SQLITE_MASTER "sqlite_master"
#define SQLITE_MASTER "sqlite_schema"

class SQLiteDB {
private:
	std::wstring db_name;
	std::wstring db_filename;
	sqlite3 * db;

	// copy and assignment not allowed
	SQLiteDB(const SQLiteDB&) = delete;
	void operator=(const SQLiteDB&) = delete;

	bool InitTokenizers(void) const;
	bool InitCollations(void) const;

public:

	sqlite3 * GetDb() { return db; };	

	//! Database object types.
	enum obj_type {
		ot_unknown,	///< Unknown type
		ot_master,	///< Master table (sqlite_master)
		ot_table,	///< Table
		ot_view,	///< View
		ot_index	///< Index
	};

	obj_type ObjectTypeByName(const char* type_name) const;
	const wchar_t * ObjectNameByType(obj_type type) const;
	obj_type GetDbObjectType(const char* object_name) const;

	//! Database object description.
	struct sq_object {
		std::string name;	///< Object name
		obj_type type;		///< Object type
		uint64_t row_count;	///< Number of records (count), only for tables and views
	};
	typedef std::vector<sq_object> sq_objects;

	bool GetObjectsList(sq_objects& objects) const;

	//! Column types.
	enum col_type {
		ct_integer,
		ct_float,
		ct_blob,
		ct_text,
		ct_unknown
	};

	col_type CoumnTypeByName(const char* ct) const;

	//! Column description.
	struct sq_column {
		std::string name;	///< Name
		col_type type;		///< Type
	};
	typedef std::vector<sq_column> sq_columns;

	bool ReadColumnDescription(const char* object_name, sq_columns & columns) const;

	bool GetRowCount(const char* object_name, uint64_t& count) const;

	bool GetCreationSql(const char* object_name, std::string& query) const;

	bool ExecuteQuery(const char* query) const;

	// check db without create object
	static bool ValidFormat(const unsigned char* data, const size_t size);

	// check valid db
	bool Valid(void) { return db != nullptr; };

	std::wstring LastError(void) const;

	const std::wstring & GetDbName(void) const {return db_name;};

	SQLiteDB(const wchar_t * db_filename);
	~SQLiteDB();

};

#endif /* __SQLITEDB_H__ */
