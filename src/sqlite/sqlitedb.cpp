#include "sqlitedb.h"
#include <utils.h>
#include <cstring>
#include <cassert>

#include <common/log.h>
#include <common/utf8util.h>

//#include <signal.h>
//raise(SIGTRAP);

extern const char * LOG_FILE;
#define LOG_SOURCE_FILE "sqlitedb.cpp"

std::wstring SQLiteDB::LastError(void) const
{
	std::wstring rc;

	if (!db)
		rc = L"Error: Database not initialized";
	else {
		std::string e(sqlite3_errmsg(db));
		std::wstring ew(e.begin(), e.end());
		rc = L"Error: [";
		rc += std::to_wstring(sqlite3_errcode(db));
		rc += L"]";
		rc += L' ';
		rc += ew;
	}
	return rc;
}

bool SQLiteDB::ValidFormat(const unsigned char* file_hdr, const size_t file_hdr_len)
{
	//! SQLite db file header ("SQLite format 3")
	static const unsigned char sqlite_db_hdr[] =
		{ 0x53, 0x51, 0x4c, 0x69, 0x74, 0x65, 0x20, 0x66, 0x6f, 0x72, 0x6d, 0x61, 0x74, 0x20, 0x33, 0x00 };

	return (file_hdr &&
		sizeof(sqlite_db_hdr) <= file_hdr_len &&
		memcmp(file_hdr, sqlite_db_hdr, sizeof(sqlite_db_hdr)) == 0);

}

SQLiteDB::SQLiteDB(const wchar_t * _db_filename):
	db_filename(_db_filename),
	db(nullptr)
{

	if( sqlite3_open(
			Wide2MB(_db_filename).c_str(),	/* Database filename (UTF-8) */
			&db)          			/* OUT: SQLite db handle */ != SQLITE_OK ) {
		LOG_ERROR("sqlite3_open(%S) ... %S\n", _db_filename, LastError().c_str());
		db = nullptr;
		return;
	}

	if( !InitTokenizers() || !InitCollations() ) {
		sqlite3_close(db);
		db = nullptr;
		return;
	}

	db_name = db_name = ExtractFileName(db_filename);
}

SQLiteDB::~SQLiteDB(void)
{
	if( db != nullptr ) {
		if( sqlite3_close(db) != SQLITE_OK ) {
			LOG_ERROR("sqlite3_close(%S) ... %S\n", db_filename.c_str(), LastError().c_str());
		}
		db = nullptr;
	}
}

SQLiteDB::obj_type SQLiteDB::ObjectTypeByName(const char* type_name) const
{
	assert(type_name && type_name[0]);
	if( !type_name || type_name[0] == 0 )
		return ot_unknown;
	if( StrCiCmp(type_name, "table") == 0 )
		return ot_table;
	if( StrCiCmp(type_name, "view") == 0 )
		return ot_view;
	if( StrCiCmp(type_name, "index") == 0 )
		return ot_index;
	return ot_unknown;
}

const wchar_t * SQLiteDB::ObjectNameByType(SQLiteDB::obj_type type) const
{
	const wchar_t * type_name[] {
		L"unknown",	///< Unknown type
		L"master",	///< Master table (sqlite_master)
		L"table",	///< Table
		L"view",	///< View
		L"index"	///< Index
	};

	if( type < (sizeof(type_name)/sizeof(type_name[0])))
		return type_name[type];

	return L"UNKNOWN";
}


bool SQLiteDB::GetObjectsList(sq_objects& objects) const
{
	assert(db);

	//Add master table
	sq_object master_table;
	master_table.name = SQLITE_MASTER;
	master_table.row_count = 0;
	master_table.type = ot_master;
	objects.push_back(master_table);

	//Add tables/views
	sqlite_statement stmt(db);
	if( stmt.prepare("select name,type from " SQLITE_MASTER) != SQLITE_OK )
		return false;

	while( stmt.step_execute() == SQLITE_ROW ) {
		sq_object obj;
		LOG_INFO("stmt.get_text(0) %s\n", stmt.get_text(0));
		obj.name = stmt.get_text(0);
		obj.row_count = 0;
		obj.type = ObjectTypeByName(stmt.get_text(1));
		LOG_INFO("stmt.get_text(1) %s\n", stmt.get_text(1));
		objects.push_back(obj);
	}

	//Get tables row count
	for( std::vector<sq_object>::iterator it = objects.begin(); it != objects.end(); ++it ) {
		if( it->type == ot_master || it->type == ot_table || it->type == ot_view ) {
			std::string query = "select count(*) from '";
			query += it->name;
			query += '\'';
			if( stmt.prepare(query.c_str() ) == SQLITE_OK && stmt.step_execute() == SQLITE_ROW )
				it->row_count = stmt.get_int64(0);
		}
	}
	return true;
}

SQLiteDB::obj_type SQLiteDB::GetDbObjectType(const char* object_name) const
{
	assert(db);
	assert(object_name && object_name[0]);

	if( StrCiCmp(object_name, SQLITE_MASTER) == 0 )
		return ot_master;

	sqlite_statement stmt(db);
	if( stmt.prepare("select type from " SQLITE_MASTER " where name=?") != SQLITE_OK ) {
		LOG_ERROR("prepare: select type from " SQLITE_MASTER " where name=%s ... %S\n", object_name, LastError().c_str());
		return ot_unknown;
	}

	if (stmt.bind(1, object_name) != SQLITE_OK) {
		LOG_ERROR("bind: select type from " SQLITE_MASTER " where name=%s ... %S\n", object_name, LastError().c_str());
		return ot_unknown;
	}

	if( stmt.step_execute() != SQLITE_ROW ) {
		LOG_ERROR("step_execute: select type from " SQLITE_MASTER " where name=%s ... %S\n", object_name, LastError().c_str());
		return ot_unknown;
	}
	return ObjectTypeByName(stmt.get_text(0));
}

SQLiteDB::col_type SQLiteDB::CoumnTypeByName(const char* ct) const
{
	if( !ct || ct[0] == 0 )
		return ct_unknown;

	if( StrnCiCmp(ct, "INT", 3) == 0 ||
		StrnCiCmp(ct, "TINYINT", 7) == 0 ||
		StrnCiCmp(ct, "SMALLINT", 8) == 0 ||
		StrnCiCmp(ct, "MEDIUMINT", 9) == 0 ||
		StrnCiCmp(ct, "BIGINT", 6) == 0 ||
		StrnCiCmp(ct, "NUMERIC", 7) == 0 ||
		StrnCiCmp(ct, "DECIMAL", 7) == 0 ||
		StrnCiCmp(ct, "BOOLEAN", 7) == 0)
		return ct_integer;
	else if (StrnCiCmp(ct, "BLOB", 4) == 0)
		return ct_blob;
	else if (StrnCiCmp(ct, "REAL", 4) == 0 ||
		StrnCiCmp(ct, "DOUBLE", 6) == 0 ||
		StrnCiCmp(ct, "FLOAT", 5) == 0)
		return ct_float;
	return ct_text;
}

bool SQLiteDB::ReadColumnDescription(const char* object_name, sq_columns & columns) const
{
	assert(db);
	assert(object_name && object_name[0]);

	std::string query = "pragma table_info('";
	query += object_name;
	query += "')";
	sqlite_statement stmt(db);
	if( stmt.prepare(query.c_str()) != SQLITE_OK ) {
		LOG_ERROR("pragma table_info('%s') ... %S\n", object_name, LastError().c_str());
		return false;
	}

	while( stmt.step_execute() == SQLITE_ROW ) {
		sq_column col;
		col.name = stmt.get_text(1);
		LOG_INFO("stmt.get_text(1) %s\n", stmt.get_text(1));
		col.type = CoumnTypeByName(stmt.get_text(2));
		LOG_INFO("stmt.get_text(2) %s\n", stmt.get_text(2));
		columns.push_back(col);
	}
	return true;
}

bool SQLiteDB::GetRowCount(const char* object_name, uint64_t& count) const
{
	assert(db);
	assert(object_name && object_name[0]);

	std::string query = "select count(*) from '";
	query += object_name;
	query += '\'';
	sqlite_statement stmt(db);

	if( stmt.prepare(query.c_str()) != SQLITE_OK ) {
		LOG_ERROR("prepare: select count(*) from '%s' ... %S\n", object_name, LastError().c_str());
		return false;
	}

	if( stmt.step_execute() != SQLITE_ROW ) {
		LOG_ERROR("step_execute: select count(*) from '%s' ... %S\n", object_name, LastError().c_str());
		return false;
	}
	count = stmt.get_int64(0);
	return true;
}

bool SQLiteDB::GetCreationSql(const char* object_name, std::string& query) const
{
	assert(db);
	assert(object_name && object_name[0]);

	if( StrCiCmp(object_name, SQLITE_MASTER) == 0 )
		return false;
	sqlite_statement stmt(db);
	if( stmt.prepare("select sql from " SQLITE_MASTER " where name=?") != SQLITE_OK ) {
		LOG_ERROR("prepare: select sql from " SQLITE_MASTER " where name=%s ... %S\n", object_name, LastError().c_str());
		return false;
	}

	if (stmt.bind(1, object_name) != SQLITE_OK) {
		LOG_ERROR("bind: select sql from " SQLITE_MASTER " where name=%s ... %S\n", object_name, LastError().c_str());
		return false;
	}

	if (stmt.step_execute() != SQLITE_ROW) {
		LOG_ERROR("step_execute: select sql from " SQLITE_MASTER " where name=%s ... %S\n", object_name, LastError().c_str());
		return false;
	}

	const char* txt = stmt.get_text(0);
	if( !txt )
		return false;
	query = txt;
	return true;
}

bool SQLiteDB::ExecuteQuery(const char* query) const
{
	sqlite_statement stmt(db);
	if( stmt.prepare(query) != SQLITE_OK ) {
		LOG_ERROR("prepare: %s ... %S\n", query, LastError().c_str());
		return false;
	}
	const int state = stmt.step_execute();
	return state == SQLITE_DONE || state == SQLITE_OK || state == SQLITE_ROW;
}


//Custom tokenizer support
static sqlite3_tokenizer	_tokinizer = { nullptr };
static sqlite3_tokenizer_module	_tokinizer_mod;
int fts3_create(int, const char *const*, sqlite3_tokenizer** tokenizer) { *tokenizer = &_tokinizer; return 0; }
int fts3_destroy(sqlite3_tokenizer*) { return 0; }
int fts3_open(sqlite3_tokenizer*, const char*, int, sqlite3_tokenizer_cursor**) { return 0; }
int fts3_close(sqlite3_tokenizer_cursor*) { return 0; }
int fts3_next(sqlite3_tokenizer_cursor*, const char**, int*, int*, int*, int*) { return 0; }

bool SQLiteDB::InitTokenizers(void) const
{

	//Initialize dummy tokenizer
	if (!_tokinizer.pModule) {
		_tokinizer_mod.iVersion = 0;
		_tokinizer_mod.xCreate = &fts3_create;
		_tokinizer_mod.xDestroy = &fts3_destroy;
		_tokinizer_mod.xOpen = &fts3_open;
		_tokinizer_mod.xClose = &fts3_close;
		_tokinizer_mod.xNext = &fts3_next;
		_tokinizer.pModule = &_tokinizer_mod;
	}

	//Read tokenizers names and register it as dummy stub
	sqlite_statement stmt(db);
	if( stmt.prepare("select sql from " SQLITE_MASTER " where type='table'") != SQLITE_OK ) {
		LOG_ERROR("stmt.prepare(select sql from " SQLITE_MASTER " where type='table') ... %S\n", LastError().c_str());
		return false;
	}

	while (stmt.step_execute() == SQLITE_ROW) {
		const char* cs = stmt.get_text(0);
		if (!cs)
			continue;
		std::string crt_sql = cs;
		crt_sql = StrToUprExt(const_cast<char *>(crt_sql.c_str()));
		LOG_INFO("crt_sql: %s\n", crt_sql.c_str());
		if (crt_sql.find("fts3") == std::string::npos)
			continue;
		const char* tok = "tokenize ";
		const size_t tok_pos = crt_sql.find(tok);
		if (tok_pos == std::string::npos)
			continue;
		const size_t tok_name_b = tok_pos + strlen(tok);
		const size_t tok_name_e = crt_sql.find_first_of(", )", tok_name_b);
		if (tok_name_e == std::string::npos)
			continue;
		const std::string tokenizer_name = std::string(cs).substr(tok_name_b, tok_name_e - tok_name_b);
		//Register tokenizer
		static const sqlite3_tokenizer_module* ptr = &_tokinizer_mod;
		sqlite_statement stmt_rt(db);
		if (stmt_rt.prepare("select fts3_tokenizer(?, ?)") != SQLITE_OK ||
			stmt_rt.bind(1, tokenizer_name.c_str()) != SQLITE_OK ||
			stmt_rt.bind(2, &ptr, sizeof(ptr)) != SQLITE_OK ||
			stmt_rt.step_execute() != SQLITE_OK) {
			LOG_ERROR("prepare(select fts3_tokenizer(?, ?)), bind(), step_execute() ... %S\n", LastError().c_str());
			return false;
		}
	}
	return true;
}

//Collation support
int db_collation(void*, int, const void*, int, const void*) { return 0; }
void db_collation_reg(void*, sqlite3* db, int text_rep, const char* name) { sqlite3_create_collation(db, name, text_rep, nullptr, &db_collation); }

bool SQLiteDB::InitCollations(void) const
{
	if( sqlite3_collation_needed(db, nullptr, &db_collation_reg) != SQLITE_OK ) {
		LOG_ERROR("sqlite3_collation_needed() ... %S\n", LastError().c_str());
		return false;
	}
	return true;
}
