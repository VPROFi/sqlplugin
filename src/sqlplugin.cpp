#include "sqlplugin.h"
#include "sqlitepanel.h"

#include <cassert>

#include <common/log.h>

#include <memory>

extern const char * LOG_FILE;
#define LOG_SOURCE_FILE "sqlplugin.cpp"

SqlPlugin::SqlPlugin(const PluginStartupInfo * info):
	Plugin(info)
{
	LOG_INFO("\n");
}

SqlPlugin::~SqlPlugin()
{
	LOG_INFO("\n");
}

HANDLE SqlPlugin::OpenFilePlugin(const wchar_t * name,const unsigned char * data, int dataSize, int opMode)
{
	LOG_INFO("name=%S, *data=%p, dataSize=%d, opMode=%d\n", name, data, dataSize, opMode);
	auto sqlite = std::make_unique<SqlitePanel>(name, data, dataSize, opMode);
	if( sqlite->Valid() ) {
		panel.push_back(std::move(sqlite));
		return panel.back().get();
	}
	return INVALID_HANDLE_VALUE;
}

HANDLE SqlPlugin::OpenPlugin(int openFrom, INT_PTR item)
{
	//Determine file name for open
	std::wstring file_name;
	if( openFrom == OPEN_COMMANDLINE && item ) {
		file_name = (const wchar_t *)item;
	} else if ( openFrom == OPEN_PLUGINSMENU || openFrom == OPEN_DISKMENU ) {
		FarApi api;
		if( auto ppi = api.GetCurrentPanelItem() ) {
			if( Plugin::FSF.LStricmp(ppi->FindData.lpwszFileName, L"..") != 0 )
				file_name = ppi->FindData.lpwszFileName;
			api.FreePanelItem(ppi);
		}
	} else {
		LOG_INFO("openFrom %u, item %u\n", openFrom, item);
	}

	if( !file_name.empty() ) {

		// sqlite db
		auto sqlite = std::make_unique<SqlitePanel>(file_name.c_str());
		if( sqlite->Valid() ) {
			panel.push_back(std::move(sqlite));
			return panel.back().get();
		}

		// other db ...
	}

	return INVALID_HANDLE_VALUE;
}

void SqlPlugin::ClosePlugin(HANDLE hPlugin)
{
	LOG_INFO("hPlugin %p\n", hPlugin);
	for( auto it = panel.begin(); it != panel.end(); it++ )
		if( (*it).get() == hPlugin ) {
			panel.erase(it);
			break;
		}
}
