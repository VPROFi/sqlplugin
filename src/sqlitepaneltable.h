#ifndef __SQLITEPANELTABLE_H__
#define __SQLITEPANELTABLE_H__

#include "plugin.h"
#include "sqlite/sqlitedb.h"
#include <memory>

class SqlitePanelTable : public FarPanel
{
private:
	std::unique_ptr<SQLiteDB> & db;

	std::wstring title;
	std::wstring object;
	std::vector<wchar_t *> columnTitles;
	std::wstring types;
	std::wstring widths;
	SQLiteDB::sq_columns columns;

	// copy and assignment not allowed
	SqlitePanelTable(const SqlitePanelTable&) = delete;
	void operator=(const SqlitePanelTable&) = delete;

public:
	int ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change) override;
	int GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber) override;
	void GetOpenPluginInfo(struct OpenPluginInfo * info) override;
	int DeleteFiles(struct PluginPanelItem *panelItem, int itemsNumber, int opMode) override;
 	explicit SqlitePanelTable(PanelIndex index_, std::unique_ptr<SQLiteDB> & db, const wchar_t * dir);
	virtual ~SqlitePanelTable();

	bool Valid(void) override;
};


#endif // __SQLITEPANELTABLE__
