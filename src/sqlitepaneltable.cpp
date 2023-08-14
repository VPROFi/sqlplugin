#include "sqlitepaneltable.h"
#include "fardialog.h"
#include "progress.h"
#include "exporter.h"
#include "editor.h"

#include <common/log.h>
#include <sqlite/sqlite.h>
#include <utils.h>

extern const char * LOG_FILE;
#define LOG_SOURCE_FILE "sqlitepaneltable.cpp"

bool SqlitePanelTable::Valid(void)
{
	return columns.size() != 0;
}

SqlitePanelTable::SqlitePanelTable(PanelIndex index_, std::unique_ptr<SQLiteDB> & _db, const wchar_t * dir):
	FarPanel(index_),
	db(_db)
{
	object = dir;
	columns.clear();

	if( !db->ReadColumnDescription(Wide2MB(dir).c_str(), columns) ) {
		const std::wstring err_descr = db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_read), db->GetDbName().c_str(), err_descr.c_str() };
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		return;
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

SqlitePanelTable::~SqlitePanelTable()
{
	LOG_INFO("\n");
	for( auto item : columnTitles )
		free((void *)item);
}

void SqlitePanelTable::GetOpenPluginInfo(struct OpenPluginInfo * info)
{
	LOG_INFO("\n");
	FarPanel::GetOpenPluginInfo(info);
	title = info->PanelTitle;
	title += db->GetDbName();
	title += L" [" + object + L"]";
	info->PanelTitle = title.c_str();
}

int SqlitePanelTable::ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change)
{
	LOG_INFO("\n");

	//F4 (edit row)
	if( controlState == 0 && (key == VK_F4 || key == VK_RETURN) ) {
		editor re(db, Wide2MB(object.c_str()).c_str());
		re.update();
		return int(true);
	}

	//Shift+F4 (insert row)
	if( controlState == PKF_SHIFT && key == VK_F4 ) {
		editor re(db, Wide2MB(object.c_str()).c_str());
		re.insert();
		return int(true);
	}

	return IsPanelProcessKey(key, controlState);
}

int SqlitePanelTable::GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber)
{
	LOG_INFO("\n");

	const static wchar_t * dots = L"..";

	uint64_t row_count = 0;
	if( !db->GetRowCount(Wide2MB(object.c_str()).c_str(), row_count) ) {
		const std::wstring err_descr = db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_read), db->GetDbName().c_str(), err_descr.c_str() };
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		return int(false);
	}

	if( row_count >= INT32_MAX )
		row_count = INT32_MAX;

	progress prg_wnd(ps_reading, row_count);

	*pItemsNumber = static_cast<int>(row_count)+1;
	*pPanelItem = (struct PluginPanelItem *)malloc((*pItemsNumber) * sizeof(PluginPanelItem));
	memset(*pPanelItem, 0, (*pItemsNumber) * sizeof(PluginPanelItem));
	PluginPanelItem * pi = *pPanelItem;
	const size_t col_num = columns.size();

	pi->FindData.lpwszFileName = dots;
	pi->FindData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;

	const wchar_t ** customColumnData = (const wchar_t **)malloc(col_num*sizeof(const wchar_t *));
	if( customColumnData ) {
		memset(customColumnData, 0, col_num*sizeof(const wchar_t *));
		for( size_t j = 0; j < col_num; ++j )
			customColumnData[j] = wcsdup(dots);
		pi->FindData.nPhysicalSize = 0;
		pi->CustomColumnNumber = col_num;
		pi->CustomColumnData = customColumnData;
	}
	pi++;

	std::string query = "select rowid,* from '";
	query += Wide2MB(object.c_str());
	query += '\'';
	sqlite_statement stmt(db->GetDb());
	if (stmt.prepare(query.c_str()) != SQLITE_OK) {
		prg_wnd.hide();
		const std::wstring err_descr = db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_read), db->GetDbName().c_str(), err_descr.c_str() };
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		FreeFindData(*pPanelItem, *pItemsNumber);
		*pPanelItem = 0;
		*pItemsNumber = 0;
		return int(false);
	}

	uint64_t i = 0;

	while( row_count-- ) {

		if( i % 100 == 0 )
			prg_wnd.update(i);

		i++;

		if( stmt.step_execute() != SQLITE_ROW ) {
			prg_wnd.hide();
			const std::wstring err_descr = db->LastError();
			const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_read), db->GetDbName().c_str(), err_descr.c_str() };
			Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
			FreeFindData(*pPanelItem, *pItemsNumber);
			*pPanelItem = 0;
			*pItemsNumber = 0;
			return int(false);
		}

		if( progress::aborted() ) {
			*pItemsNumber = static_cast<int>(i);
			return true;	//Show incomplete data
		}

		const wchar_t ** customColumnData = (const wchar_t **)malloc(col_num*sizeof(const wchar_t *));
		if( customColumnData ) {
			memset(customColumnData, 0, col_num*sizeof(const wchar_t *));
			std::string data;
			for( size_t j = 0; j < col_num; ++j ) {
				exporter::get_text(stmt, static_cast<int>(j) + 1 /* rowid */, data);
				customColumnData[j] = wcsdup(MB2Wide(data.c_str()).c_str());
			}
			pi->FindData.nPhysicalSize = stmt.get_int64(0);
			pi->CustomColumnNumber = col_num;
			pi->CustomColumnData = customColumnData;
		}
		pi++;
	}

	prg_wnd.update(i+row_count);

	return int(true);
}

int SqlitePanelTable::DeleteFiles(struct PluginPanelItem *panelItem, int itemsNumber, int opMode)
{
	LOG_INFO("\n");
	editor ed(db, Wide2MB(object.c_str()).c_str());
	return ed.remove(panelItem, itemsNumber);
}
