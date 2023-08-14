#include "sqlitepanelquery.h"
#include "fardialog.h"
#include "progress.h"
#include "exporter.h"

#include <common/log.h>
#include <sqlite/sqlite.h>
#include <utils.h>

extern const char * LOG_FILE;
#define LOG_SOURCE_FILE "sqlitepanelquery.cpp"

bool SqlitePanelQuery::Valid(void)
{
	return columns.size() != 0;
}

SqlitePanelQuery::SqlitePanelQuery(PanelIndex index_, std::unique_ptr<SQLiteDB> & _db, const char * _query):
	FarPanel(index_),
	db(_db)
{
	columns.clear();
	query = _query;

	LOG_INFO("query %s\n", query.c_str());

	//Get column description
	sqlite_statement stmt(db->GetDb());
	if( stmt.prepare(query.c_str()) != SQLITE_OK || (stmt.step_execute() != SQLITE_ROW && stmt.step_execute() != SQLITE_DONE) ) {
		const std::wstring query_descr = MB2Wide(query.c_str());
		const std::wstring err_descr = db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_sql), db->GetDbName().c_str(), query_descr.c_str(), err_descr.c_str()};
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		return;
	}

	const int col_count = stmt.column_count();
	for (int i = 0; i < col_count; ++i) {
		SQLiteDB::sq_column col;
		col.name = stmt.column_name(i);
		col.type = SQLiteDB::ct_text;
		columns.push_back(col);
	}

	size_t col_num = columns.size();
	size_t index = 0;
	for( auto & item : columns ) {
		columnTitles.push_back(wcsdup(MB2Wide(item.name.c_str()).c_str()));
		widths += L"0";
		types += L"C" + std::to_wstring(index);
		if( col_num-- ) {
			widths += L",";
			types += L",";
		}
		index++;
	}

	auto nmodes = GetPanelModesArray();
	for( size_t i =0; i < PanelModeMax; i++ ) {
		nmodes[i].StatusColumnTypes = types.c_str();
		nmodes[i].StatusColumnWidths = widths.c_str();
	}
	nmodes[4].ColumnTypes = types.c_str();
	nmodes[4].ColumnWidths = widths.c_str();
	nmodes[4].ColumnTitles = columnTitles.data();
	nmodes[5].ColumnTypes =  types.c_str();
	nmodes[5].ColumnWidths = widths.c_str();
	nmodes[5].ColumnTitles = columnTitles.data();

	LOG_INFO("\n");
}

SqlitePanelQuery::~SqlitePanelQuery()
{
	LOG_INFO("\n");
	for( auto item : columnTitles )
		free((void *)item);
}

void SqlitePanelQuery::GetOpenPluginInfo(struct OpenPluginInfo * info)
{
	LOG_INFO("\n");
	FarPanel::GetOpenPluginInfo(info);
	title = info->PanelTitle;
	title += db->GetDbName();
	title += L" [" + MB2Wide(query.c_str()) + L"]";
	info->PanelTitle = title.c_str();
}

int SqlitePanelQuery::ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change)
{
	LOG_INFO("\n");
	return IsPanelProcessKey(key, controlState);
}

int SqlitePanelQuery::GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber)
{
	LOG_INFO("select %s\n", query.c_str());

	progress prg_wnd(ps_reading);

	//Read all data to buffer - we don't know rowset size
	std::vector<PluginPanelItem> buff;

	const static wchar_t * dots = L"..";
	//All dots (..)
	PluginPanelItem dot_item;
	memset(&dot_item, 0, sizeof(dot_item));
	dot_item.FindData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	dot_item.FindData.lpwszFileName = dots;
	const size_t col_num = columns.size();
	const wchar_t ** customColumnData = (const wchar_t **)malloc(col_num*sizeof(const wchar_t *));
	if( customColumnData ) {
		memset(customColumnData, 0, col_num*sizeof(const wchar_t *));
		for( size_t j = 0; j < col_num; ++j )
			customColumnData[j] = wcsdup(dots);
		dot_item.FindData.nPhysicalSize = 0;
		dot_item.CustomColumnNumber = col_num;
		dot_item.CustomColumnData = customColumnData;
	}
	buff.push_back(dot_item);

	sqlite_statement stmt(db->GetDb());
	if( stmt.prepare(query.c_str()) != SQLITE_OK ) {
		prg_wnd.hide();

		const std::wstring query_descr = MB2Wide(query.c_str());
		const std::wstring err_descr = db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_sql), db->GetDbName().c_str(), query_descr.c_str(), err_descr.c_str()};
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);

		for( size_t j = 0; j < col_num; ++j )
			free((void *)customColumnData[j]);
		free((void *)customColumnData);

		return int(false);
	}

	int state = SQLITE_OK;
	while ((state = stmt.step_execute()) == SQLITE_ROW) {
		if (progress::aborted()) {
			state = SQLITE_DONE;
			break;	//Show incomplete data
		}

		PluginPanelItem item;
		memset(&item, 0, sizeof(item));
		const int col_count = stmt.column_count();
		const wchar_t** custom_column_data = (const wchar_t **)malloc(col_count*sizeof(const wchar_t *));
		if( custom_column_data ) {
			memset(custom_column_data, 0, col_count*sizeof(const wchar_t *));
			std::string data;
			for( size_t j = 0; j < static_cast<size_t>(col_count); ++j ) {
				exporter::get_text(stmt, static_cast<int>(j), data);
				custom_column_data[j] = wcsdup(MB2Wide(data.c_str()).c_str());
			}
		}
		item.CustomColumnData = custom_column_data;
		item.CustomColumnNumber = col_count;
		buff.push_back(item);
	}

	if( state != SQLITE_DONE ) {
		prg_wnd.hide();

		const std::wstring query_descr = MB2Wide(query.c_str());
		const std::wstring err_descr = db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_read), db->GetDbName().c_str(), query_descr.c_str(), err_descr.c_str()};
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);

		auto itemsNumber = buff.size();
		PluginPanelItem * panelItem = &buff.front();
		while( itemsNumber-- ) {
			while( (panelItem+itemsNumber)->CustomColumnNumber-- )
				free((void *)(panelItem+itemsNumber)->CustomColumnData[(panelItem+itemsNumber)->CustomColumnNumber]);
			free((void *)(panelItem+itemsNumber)->CustomColumnData);
		}
		return int(false);
	}

	*pItemsNumber = buff.size();

	if( buff.size() >= INT32_MAX )
		*pItemsNumber = INT32_MAX;
	else
		*pItemsNumber = buff.size();

	*pPanelItem = (struct PluginPanelItem *)malloc((*pItemsNumber) * sizeof(PluginPanelItem));

	if( *pPanelItem )
		memmove(*pPanelItem, &buff.front(), sizeof(PluginPanelItem) * (*pItemsNumber));
	else {
		*pItemsNumber = 0;

		auto itemsNumber = buff.size();
		PluginPanelItem * panelItem = &buff.front();
		while( itemsNumber-- ) {
			while( (panelItem+itemsNumber)->CustomColumnNumber-- )
				free((void *)(panelItem+itemsNumber)->CustomColumnData[(panelItem+itemsNumber)->CustomColumnNumber]);
			free((void *)(panelItem+itemsNumber)->CustomColumnData);
		}
	}

	return int(true);
}
