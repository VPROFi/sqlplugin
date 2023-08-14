#include "sqlitepanel.h"
#include "fardialog.h"
#include "progress.h"
#include "exporter.h"
#include <common/log.h>
#include <common/utf8util.h>
#include <sqlite/sqlite.h>
#include <utils.h>

extern const char * LOG_FILE;
#define LOG_SOURCE_FILE "sqlitepanel.cpp"

SqlitePanel::SqlitePanel(const wchar_t * name, const unsigned char * data, int dataSize, int opMode):
	FarPanel()
{
	LOG_INFO("name=%S, *data=%p, dataSize=%d, opMode=0x%08X\n", name, data, dataSize, opMode);

//OPM_SILENT	Плагин должен минимизировать запросы пользователя по возможности, так как вызываемая функция - это часть более комплексной операции.
//OPM_FIND	Функция плагина была вызвана из Диалога поиска файлов, или другой команды, сканирующей каталоги. Экранный вывод должен быть минимизирован.
//OPM_VIEW	Функция плагина вызвана как часть операции просмотра файла. Если файл просматривается на панели быстрого просмотра, то OPM_VIEW используется совместно с OPM_QUICKVIEW.
//OPM_QUICKVIEW	Функция плагина вызвана как часть операции просмотра файла на панели быстрого просмотра (пользователь нажал Ctrl+Q в панелях).
//OPM_EDIT	Функция плагина вызвана как часть операции редактирования файла.
//OPM_DESCR	Функция была вызвана для запроса или для изменения файла и его описания.
//OPM_TOPLEVEL	Все файлы в плагине будут обработаны. Этот флаг устанавливается во время обработки команд Shift+F2 и Shift+F3 вне базового файла плагина. Переданный в функцию плагина список также содержит всю необходимую информацию, поэтому плагин может игнорировать этот флаг, или же повысить скорость операции.
//OPM_PGDN	Функция плагина вызвана после нажатия Ctrl+PgDn в панелях.
//OPM_COMMANDS	Функция плагина вызвана из меню архивных команд Shift+F3.
//OPM_NONE	Нулевая константа.

	active = 0;
	topIndex = 0;
	dirIndex = 0;

	if( opMode & OPM_FIND )
		return;
	if( !SQLiteDB::ValidFormat(data, dataSize) ) {
		LOG_INFO("unsupported format\n");
		return;
	}

	db = std::make_unique<SQLiteDB>(name);
	if( Valid() )
		panels.push_back(std::make_unique<SqlitePanelDb>(SqliteDbPanelIndex, db));
}

SqlitePanel::SqlitePanel(const wchar_t * name):
	FarPanel()
{
	LOG_INFO("name=%S\n", name);

	active = 0;
	topIndex = 0;
	dirIndex = 0;

	db = std::make_unique<SQLiteDB>(name);
	if( Valid() )
		panels.push_back(std::make_unique<SqlitePanelDb>(SqliteDbPanelIndex, db));
}

SqlitePanel::~SqlitePanel()
{
	LOG_INFO("\n");
}

bool SqlitePanel::OpenQuery(const char* query)
{
	LOG_INFO("%s\n", query);

	assert(query && query[0]);

	if (!query || query[0] == 0)
		return false;

	_last_sql_query = query;

	//Check query for select
	const char* select_word = query;
	while( select_word[0] && !isalpha(select_word[0]) )
		++select_word;
	if( StrnCiCmp(select_word, "select", 6) != 0 ) {

		LOG_INFO("NOT SELECT: %s\n", query);
		//Update query - just execute without read result
		progress prg_wnd(ps_execsql);
		for (auto ps = query; ps; ) {
			auto pe = strstr(ps, ";\n");
			const char* next = pe ? pe + 2 : nullptr;
			while (isspace(*ps)) ++ps;
			std::string one_query(ps, next ? size_t(next - ps - 1) : strlen(ps));
			if (*ps && !(*ps=='-' && ps[1]=='-') && !db->ExecuteQuery(one_query.c_str())) {
				prg_wnd.hide();
				const std::wstring query_descr = MB2Wide(one_query.c_str());
				const std::wstring err_descr = db->LastError();
				const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_sql), db->GetDbName().c_str(), query_descr.c_str(), err_descr.c_str() };
				Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
				return false;
			}
			ps = next;
		}
	}
	else {

		LOG_INFO("SELECT: %s\n", query);
		panels.push_back(std::make_unique<SqlitePanelQuery>(SqliteTablePanelIndex, db, query));
		if( !panels[++active]->Valid() ) {
			panels.pop_back();
			active--;
			return false;
		} else
			StorePosition();
	}

	Plugin::psi.Control(PANEL_ACTIVE, FCTL_UPDATEPANEL, 0, 0);
	PanelRedrawInfo pri;
	memset(&pri, 0, sizeof(pri));
	Plugin::psi.Control(PANEL_ACTIVE, FCTL_REDRAWPANEL, 0, (LONG_PTR)&pri);
	return true;
}


void SqlitePanel::StorePosition(void)
{
	PanelInfo pi = {0};
	if( auto ppi = GetCurrentPanelItem(&pi) ) {
		topIndex = pi.TopPanelItem;
		dirIndex = pi.CurrentItem;
		FreePanelItem(ppi);
	}
}

void SqlitePanel::EditSqlQuery(void)
{

	LOG_INFO("\n");

	const std::wstring tmp_file_name = exporter::get_temp_file_name(L"sql");

	HANDLE file = CreateFile(tmp_file_name.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if( file == INVALID_HANDLE_VALUE ) {
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_writef), tmp_file_name.c_str() };
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_ERRORTYPE | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		return;
	}
	//Save last used query
	if( !_last_sql_query.empty() ) {
		DWORD bytes_written;
		if( !WriteFile(file, _last_sql_query.c_str(), static_cast<DWORD>(_last_sql_query.length()), &bytes_written, nullptr)) {
			const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_writef), tmp_file_name.c_str() };
			Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_ERRORTYPE | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
			CloseHandle(file);
			return;
		}
	}
	CloseHandle(file);

	//Open query editor
	if( Plugin::psi.Editor(tmp_file_name.c_str(), L"SQLite query", 0, 0, -1, -1, EF_DISABLEHISTORY, 1, 1, CP_UTF8) == EEC_MODIFIED) {
		//Read query
		LARGE_INTEGER file_size;
		HANDLE file = CreateFile(tmp_file_name.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &file_size)) {
			const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_readf), tmp_file_name.c_str() };
			Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_ERRORTYPE | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
			return;
		}
		DWORD bytes_read = static_cast<DWORD>(file_size.QuadPart);
		if (bytes_read > 1024 * 1024)	//I think SQL query is not so big...
			return;
		std::vector<char> file_buff(bytes_read);
		if( !ReadFile(file, &file_buff.front(), bytes_read, &bytes_read, nullptr) ) {
			const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_readf), tmp_file_name.c_str() };
			Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_ERRORTYPE | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
			CloseHandle(file);
			return;
		}
		CloseHandle(file);
		DeleteFile(tmp_file_name.c_str());

		//Remove BOM 
		if (file_buff.size() > 2 && file_buff[0] == (char)0xef && file_buff[1] == (char)0xbb && file_buff[2] == (char)0xbf)	//UTF-8
			file_buff.erase(file_buff.begin(), file_buff.begin() + 3);
		else if (file_buff.size() > 1 && file_buff[0] == (char)0xfe && file_buff[1] == (char)0xff)	//UTF-16 (BE)
			file_buff.erase(file_buff.begin(), file_buff.begin() + 2);
		else if (file_buff.size() > 1 && file_buff[0] == (char)0xff && file_buff[1] == (char)0xfe)	//UTF-16 (LE)
			file_buff.erase(file_buff.begin(), file_buff.begin() + 2);
		else if (file_buff.size() > 3 && file_buff[0] == (char)0x00 && file_buff[1] == (char)0x00 && file_buff[1] == (char)0xfe && file_buff[2] == (char)0xff)	//UTF-32 (BE)
			file_buff.erase(file_buff.begin(), file_buff.begin() + 4);
		else if (file_buff.size() > 3 && file_buff[0] == (char)0x00 && file_buff[1] == (char)0x00 && file_buff[1] == (char)0xff && file_buff[2] == (char)0xfe)	//UTF-32 (LE)
			file_buff.erase(file_buff.begin(), file_buff.begin() + 4);
		else if (file_buff.size() > 3 && file_buff[0] == (char)0x2b && file_buff[1] == (char)0x2f)	//UTF-7
			file_buff.erase(file_buff.begin(), file_buff.begin() + 4);
		if (file_buff.empty())
			return;
		//Add terminated null
		file_buff.push_back(0);
		file_buff.push_back(0);

		_last_sql_query = reinterpret_cast<const char*>(&file_buff.front());

		//Replace '\r\n' to '\n'
		size_t r_pos = 0;
		while ((r_pos = _last_sql_query.find('\r', r_pos)) != std::string::npos)
			_last_sql_query.erase(r_pos, 1);

		OpenQuery(_last_sql_query.c_str());
	}
}

int SqlitePanel::ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change)
{

	LOG_INFO("\n");

	if( active > 0 && controlState == 0 && key == VK_RETURN ) {
		if( auto ppi = GetCurrentPanelItem() ) {
			if( Plugin::FSF.LStricmp(ppi->FindData.lpwszFileName, L"..") == 0 ) {
				if( active > 0 ) {
					panels.pop_back();
					active--;
					PanelRedrawInfo pri;
					pri.TopPanelItem = topIndex;
					pri.CurrentItem = dirIndex;
					Plugin::psi.Control(hPlugin, FCTL_UPDATEPANEL, TRUE, 0);
					Plugin::psi.Control(hPlugin, FCTL_REDRAWPANEL, 0, (LONG_PTR)&pri);
				}
				FreePanelItem(ppi);
				return TRUE;
			}
			FreePanelItem(ppi);
		}
	}

	if( controlState == 0 && key == VK_F6 ) {
		EditSqlQuery();
		return TRUE;
	}

	return active < panels.size() ? panels[active]->ProcessKey(hPlugin, key, controlState, change):int(false);
}

int SqlitePanel::GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber)
{
	LOG_INFO("\n");
	return active < panels.size() ? panels[active]->GetFindData(pPanelItem, pItemsNumber):int(false);
}

void SqlitePanel::FreeFindData(struct PluginPanelItem * panelItem, int itemsNumber)
{
	LOG_INFO("\n");
	if( active < panels.size() )
		panels[active]->FreeFindData(panelItem, itemsNumber);
}

void SqlitePanel::GetOpenPluginInfo(struct OpenPluginInfo * info)
{
	LOG_INFO("\n");
	if( active < panels.size() )
		panels[active]->GetOpenPluginInfo(info);
}

bool SqlitePanel::Valid(void)
{
	return db != 0 && db->Valid();
}

int SqlitePanel::SetDirectory(const wchar_t *dir, int opMode)
{
	LOG_INFO("dir %S opMode %u\n", dir, opMode);
	if( (opMode & (OPM_FIND | OPM_SILENT) ) != 0  )
		return 0;

	if( Plugin::FSF.LStricmp(dir, L"..") == 0 || \
		Plugin::FSF.LStricmp(dir, L"/") == 0 || \
		Plugin::FSF.LStricmp(dir, L"\\" ) == 0 ) {
		LOG_INFO("dir == '..'\n");
		if( active > 0 ) {
			panels.pop_back();
			active--;
		}
	} else {

		switch( db->GetDbObjectType(Wide2MB(dir).c_str()) ) {
		case SQLiteDB::ot_unknown:
			break;
		case SQLiteDB::ot_master:
		case SQLiteDB::ot_table:
			panels.push_back(std::make_unique<SqlitePanelTable>(SqliteTablePanelIndex, db, dir));
			if( !panels[++active]->Valid() ) {
				panels.pop_back();
				active--;
			} else
				StorePosition();
			return int(true);
		case SQLiteDB::ot_view:
		case SQLiteDB::ot_index:
			break;
		};


	}
	return int(false);
}

int SqlitePanel::DeleteFiles(struct PluginPanelItem *panelItem, int itemsNumber, int opMode)
{
	LOG_INFO("\n");
	if( active < panels.size() )
		return panels[active]->DeleteFiles(panelItem, itemsNumber, opMode);
	return int(false);
}
