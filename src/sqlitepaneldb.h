#ifndef __SQLITEPANELDB_H__
#define __SQLITEPANELDB_H__

#include "plugin.h"
#include "sqlite/sqlitedb.h"
#include <memory>

enum {
	SqliteColumnTypeIndex,
	SqliteColumnMaxIndex
};

class SqlitePanelDb : public FarPanel
{
private:
	std::unique_ptr<SQLiteDB> & db;

	void ViewDbObject(PluginPanelItem * ppi);
	void ViewDbCreateSql(PluginPanelItem * ppi);
	void ViewPragmaStatements(void);

	// copy and assignment not allowed
	SqlitePanelDb(const SqlitePanelDb&) = delete;
	void operator=(const SqlitePanelDb&) = delete;

public:
	int ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change) override;
	int GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber) override;
	int DeleteFiles(struct PluginPanelItem *panelItem, int itemsNumber, int opMode) override;
	void FreeFindData(struct PluginPanelItem * panelItem, int itemsNumber) override;
 	explicit SqlitePanelDb(PanelIndex index_, std::unique_ptr<SQLiteDB> & db);
	virtual ~SqlitePanelDb();
};


#endif // __SQLITEPANELDB__
