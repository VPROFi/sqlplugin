#include "sqlitepaneldb.h"
#include "fardialog.h"
#include "exporter.h"
#include "editor.h"
#include <common/log.h>
#include <sqlite/sqlite.h>
#include <utils.h>

extern const char * LOG_FILE;
#define LOG_SOURCE_FILE "sqlitepaneldb.cpp"

SqlitePanelDb::SqlitePanelDb(PanelIndex index_, std::unique_ptr<SQLiteDB> & _db):
	FarPanel(index_),
	db(_db)
{
	LOG_INFO("\n");
}

SqlitePanelDb::~SqlitePanelDb()
{
	LOG_INFO("\n");
}

void SqlitePanelDb::ViewPragmaStatements(void)
{
	const char* pst[] = {
		"auto_vacuum", "automatic_index", "busy_timeout", "cache_size",
		"checkpoint_fullfsync", "encoding", "foreign_keys",
		"freelist_count", "fullfsync", "ignore_check_constraints",
		"integrity_check", "journal_mode", "journal_size_limit",
		"legacy_file_format", "locking_mode", "max_page_count",
		"page_count", "page_size", "quick_check", "read_uncommitted",
		"recursive_triggers", "reverse_unordered_selects",
		"schema_version", "secure_delete", "synchronous", "temp_store",
		"user_version", "wal_autocheckpoint", "wal_checkpoint"
	};

	std::vector<std::wstring> pragma_values;
	for( size_t i = 0; i < sizeof(pst) / sizeof(pst[0]); ++i ) {
		std::string query = "pragma ";
		query += pst[i];
		sqlite_statement stmt( db->GetDb() );
		if( stmt.prepare(query.c_str()) == SQLITE_OK && stmt.step_execute() == SQLITE_ROW ) {
			std::wstring pv = towstr(pst[i]);
			pv += L": ";
			if( pv.length() < 28 )
				pv.resize(28, ' ');
			if( stmt.get_text(0) )
				pv += MB2Wide(stmt.get_text(0));
			pragma_values.push_back(pv);
		}
	}
	if( pragma_values.empty() )
		return;
	std::vector<FarListItem> far_items;
	far_items.resize(pragma_values.size());
	memset(&far_items.front(), 0, sizeof(FarListItem) * pragma_values.size());
	for (size_t i = 0; i < pragma_values.size(); ++i)
		far_items[i].Text = pragma_values[i].c_str();
	far_items[0].Flags |= LIF_SELECTED;
	FarList far_list;
	memset(&far_list, 0, sizeof(far_list));
	far_list.ItemsNumber = far_items.size();
	far_list.Items = &far_items.front();

	FarDialogItem dlg_items[4];
	memset(dlg_items, 0, sizeof(dlg_items));

	dlg_items[0].Type = DI_DOUBLEBOX;
	dlg_items[0].X1 = 3;
	dlg_items[0].X2 = 56;
	dlg_items[0].Y1 = 1;
	dlg_items[0].Y2 = 18;
	dlg_items[0].PtrData = GetMsg(ps_title_pragma);

	dlg_items[1].Type = DI_LISTBOX;
	dlg_items[1].X1 = 4;
	dlg_items[1].X2 = 55;
	dlg_items[1].Y1 = 2;
	dlg_items[1].Y2 = 15;
	dlg_items[1].ListItems = &far_list;
	dlg_items[1].Flags = DIF_LISTNOBOX | DIF_LISTNOAMPERSAND;
	dlg_items[1].Focus = 1;


	dlg_items[2].Type = DI_TEXT;
	dlg_items[2].Y1 = 16;
	dlg_items[2].Flags = DIF_SEPARATOR;

	dlg_items[3].Type = DI_BUTTON;
	dlg_items[3].PtrData = GetMsg(ps_cancel);
	dlg_items[3].Y1 = 17;
	dlg_items[3].Flags = DIF_CENTERGROUP;
	dlg_items[3].DefaultButton = 1;

	const HANDLE dlg = Plugin::psi.DialogInit(Plugin::psi.ModuleNumber, -1, -1, 60, 20, nullptr, dlg_items, sizeof(dlg_items) / sizeof(dlg_items[0]), 0, 0, nullptr, 0);
	if( dlg != INVALID_HANDLE_VALUE ) {
		Plugin::psi.DialogRun(dlg);
		Plugin::psi.DialogFree(dlg);
	}

}

void SqlitePanelDb::ViewDbCreateSql(PluginPanelItem * ppi)
{
	std::string cr_sql;
	if( db->GetCreationSql( Wide2MB(ppi->FindData.lpwszFileName).c_str(), cr_sql) ) {
		std::wstring tmp_file_name;
		tmp_file_name = exporter::get_temp_file_name(L"sql");
		HANDLE file = CreateFile(tmp_file_name.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if( file == INVALID_HANDLE_VALUE ) {
			const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_writef), tmp_file_name.c_str() };
			Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_ERRORTYPE | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
			return;
		}
		DWORD bytes_written;
		if( !WriteFile(file, cr_sql.c_str(), static_cast<DWORD>(cr_sql.length()), &bytes_written, nullptr)) {
			const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_writef), tmp_file_name.c_str() };
			Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_ERRORTYPE | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
			CloseHandle(file);
			return;
		}
		CloseHandle(file);
		Plugin::psi.Viewer(tmp_file_name.c_str(), ppi->FindData.lpwszFileName, 0, 0, -1, -1, VF_ENABLE_F6 | VF_DISABLEHISTORY | VF_DELETEONLYFILEONCLOSE | VF_NONMODAL, CP_UTF8);
	}
}

void SqlitePanelDb::ViewDbObject(PluginPanelItem * ppi)
{

	LOG_INFO("\n");

	std::wstring tmp_file_name;

	//For unknown types show create sql only
	if ((ppi->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
		std::string cr_sql;
		if( !db->GetCreationSql( Wide2MB(ppi->FindData.lpwszFileName).c_str(), cr_sql) )
			return;
		tmp_file_name = exporter::get_temp_file_name(L"sql");
		LOG_INFO("tmp_file_name: %S\n", tmp_file_name.c_str());

		HANDLE file = CreateFile(tmp_file_name.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if( file == INVALID_HANDLE_VALUE ) {
			const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_writef), tmp_file_name.c_str() };
			Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_ERRORTYPE | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
			return;
		}
		DWORD bytes_written;
		if( !WriteFile(file, cr_sql.c_str(), static_cast<DWORD>(cr_sql.length()), &bytes_written, nullptr)) {
			const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_writef), tmp_file_name.c_str() };
			Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_ERRORTYPE | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
			CloseHandle(file);
			return;
		}
		CloseHandle(file);
	}
	else {
		//Export data
		exporter ex(db);
		if( !ex.export_data(ppi->FindData.lpwszFileName, exporter::fmt_text, tmp_file_name) )
			return;
	}

	std::wstring title = GetMsg(ps_title_short);
	title += L": ";
	title += ppi->FindData.lpwszFileName;
	Plugin::psi.Viewer(tmp_file_name.c_str(), title.c_str(), 0, 0, -1, -1, VF_ENABLE_F6 | VF_DISABLEHISTORY | VF_DELETEONLYFILEONCLOSE | VF_IMMEDIATERETURN | VF_NONMODAL, CP_UTF8);
	ViewerSetMode vm;
	memset(&vm, 0, sizeof(vm));
	vm.Type = VSMT_WRAP;
	vm.Param.iParam = 0;
	Plugin::psi.ViewerControl(VCTL_SETMODE, &vm);
	vm.Type = VSMT_WRAP;//VSMT_HEX; //VSMT_VIEWMODE;
	vm.Flags = VSMFL_REDRAW;
	vm.Param.iParam = 0;//VMT_TEXT;
	Plugin::psi.ViewerControl(VCTL_SETMODE, &vm);
}

int SqlitePanelDb::ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change)
{
	LOG_INFO("\n");

	if( controlState == 0 ) {
		switch( key ) {
		case VK_F5:
			{
			exporter ex(db);
			ex.export_data();
			return int(true);
			}
		case VK_F3:
		case VK_F4:
			if( auto ppi = GetCurrentPanelItem() ) {
				if( Plugin::FSF.LStricmp(ppi->FindData.lpwszFileName, L"..") != 0 )
					key == VK_F3 ? ViewDbObject(ppi):ViewDbCreateSql(ppi);
				FreePanelItem(ppi);
			}
			return TRUE;
		}

	}

	if( controlState == PKF_SHIFT && key == VK_F4 ) {
		ViewPragmaStatements();
		return TRUE;
	}

	return IsPanelProcessKey(key, controlState);
}

int SqlitePanelDb::GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber)
{
	LOG_INFO("\n");

	SQLiteDB::sq_objects db_objects;
	if( !db->GetObjectsList(db_objects) ) {
		const std::wstring err_descr = db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_read), db->GetDbName().c_str(), err_descr.c_str() };
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		return int(false);
	}

	*pItemsNumber = db_objects.size();
	*pPanelItem = (struct PluginPanelItem *)malloc((*pItemsNumber) * sizeof(PluginPanelItem));
	memset(*pPanelItem, 0, (*pItemsNumber) * sizeof(PluginPanelItem));
	PluginPanelItem * pi = *pPanelItem;

	for( const auto & item : db_objects ) {
		pi->FindData.lpwszFileName = wcsdup(MB2Wide(item.name.c_str()).c_str());
		pi->FindData.dwFileAttributes |= FILE_FLAG_DELETE_ON_CLOSE;
		pi->FindData.nFileSize = item.row_count;
		pi->FindData.nPhysicalSize = item.type;

		if( item.type == SQLiteDB::ot_master || item.type == SQLiteDB::ot_table || item.type == SQLiteDB::ot_view )
			pi->FindData.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;

		const wchar_t ** customColumnData = (const wchar_t **)malloc(SqliteColumnMaxIndex*sizeof(const wchar_t *));
		if( customColumnData ) {
			memset(customColumnData, 0, SqliteColumnMaxIndex*sizeof(const wchar_t *));
			customColumnData[SqliteColumnTypeIndex] = db->ObjectNameByType(item.type);
			pi->CustomColumnNumber = SqliteColumnMaxIndex;
			pi->CustomColumnData = customColumnData;
		}
		pi++;
	}
	return int(true);
}

void SqlitePanelDb::FreeFindData(struct PluginPanelItem * panelItem, int itemsNumber)
{
	LOG_INFO("\n");
	while( itemsNumber-- ) {
		assert( (panelItem+itemsNumber)->FindData.dwFileAttributes & FILE_FLAG_DELETE_ON_CLOSE );
		free((void *)(panelItem+itemsNumber)->FindData.lpwszFileName);
		free((void *)(panelItem+itemsNumber)->CustomColumnData);
	}
	free((void *)panelItem);
}

int SqlitePanelDb::DeleteFiles(struct PluginPanelItem *panelItem, int itemsNumber, int opMode)
{
	LOG_INFO("\n");
	editor ed(db, nullptr);
	return ed.remove(panelItem, itemsNumber);
}
