#ifndef __SQLITEPANELQUERY_H__
#define __SQLITEPANELQUERY_H__

#include "plugin.h"
#include "sqlite/sqlitedb.h"
#include <memory>

class SqlitePanelQuery : public FarPanel
{
private:
	std::unique_ptr<SQLiteDB> & db;

	std::string query;

	std::wstring title;
	std::vector<wchar_t *> columnTitles;
	std::wstring types;
	std::wstring widths;
	SQLiteDB::sq_columns columns;

	// copy and assignment not allowed
	SqlitePanelQuery(const SqlitePanelQuery&) = delete;
	void operator=(const SqlitePanelQuery&) = delete;

public:
	int ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change) override;
	int GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber) override;
	void GetOpenPluginInfo(struct OpenPluginInfo * info) override;
 	explicit SqlitePanelQuery(PanelIndex index_, std::unique_ptr<SQLiteDB> & db, const char * query);
	virtual ~SqlitePanelQuery();

	bool Valid(void) override;
};


#endif // __SQLITEPANELQUERY__
