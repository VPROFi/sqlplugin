#ifndef __SQLITE_H__
#define __SQLITE_H__

#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <engine/sqlite3.h>
#include <engine/fts3_tokenizer.h>

#ifdef __cplusplus
}
#endif

/**
 * SQLite statement wrapper.
 */
class sqlite_statement
{
public:
	sqlite_statement(sqlite3* db) : _stmt(NULL), _db(db)				{ assert(_db); }
	~sqlite_statement()													{ close(); }

	//Prepare query
	inline int prepare(const char* query)							{ close(); return sqlite3_prepare_v2(_db, query, -1, &_stmt, NULL); }

	//Bind query parameter
	inline int bind(const int index, const void* val, const int size)	{ return sqlite3_bind_blob(_stmt, index, val, size, SQLITE_TRANSIENT); }
	inline int bind(const int index, const double val)					{ return sqlite3_bind_double(_stmt, index, val); }
	inline int bind(const int index, const int val)						{ return sqlite3_bind_int(_stmt, index, val); }
	inline int bind(const int index, const sqlite3_int64 val)			{ return sqlite3_bind_int64(_stmt, index, val); }
	inline int bind(const int index, const char* val)					{ return sqlite3_bind_text(_stmt, index, val, static_cast<int>(strlen(val)), SQLITE_TRANSIENT); }
	inline int bind_null(const int index)								{ return sqlite3_bind_null(_stmt, index); }

	//Execute query step
	inline int step_execute()											{ return sqlite3_step(_stmt); }

	//Query result - get column properties
	inline int column_count() const										{ return sqlite3_column_count(_stmt); };
	inline int column_type(const int index)	const						{ return sqlite3_column_type(_stmt, index); };
	inline const char* column_name(const int index) const			{ return sqlite3_column_name(_stmt, index); };

	//Query result - get row data
	inline int get_length(const int index) const						{ return sqlite3_column_bytes(_stmt, index); }
	inline const void* get_blob(const int index) const					{ return sqlite3_column_blob(_stmt, index); }
	inline int get_int(const int index) const							{ return sqlite3_column_int(_stmt, index); }
	inline sqlite3_int64 get_int64(const int index) const				{ return sqlite3_column_int64(_stmt, index); }
	inline const char* get_text(const int index) const				{ return reinterpret_cast<const char *>(sqlite3_column_text(_stmt, index)); }

	//Close statement
	inline void close()													{ if (_stmt) { sqlite3_finalize(_stmt); _stmt = NULL; }	}

private:
	sqlite3_stmt*	_stmt;	///< SQLite statement
	sqlite3*		_db;	///< SQLite database
};

#endif /* __SQLITE_H__ */
