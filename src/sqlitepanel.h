#ifndef __SQLITEPANEL_H__
#define __SQLITEPANEL_H__

#include "sqlitepaneldb.h"
#include "sqlitepaneltable.h"
#include "sqlitepanelquery.h"

class SqlitePanel : public FarPanel
{
private:
	std::unique_ptr<SQLiteDB> db;

	uint32_t active;
	uint32_t dirIndex;
	uint32_t topIndex;

	std::vector<std::unique_ptr<FarPanel>> panels;

	std::string _last_sql_query;
	void EditSqlQuery(void);
	bool OpenQuery(const char* query);

	void StorePosition(void);
	
	// copy and assignment not allowed
	SqlitePanel(const SqlitePanel&) = delete;
	void operator=(const SqlitePanel&) = delete;

public:
        // validate file
	bool Valid(void) override;

	int ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change) override;
	int GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber) override;
	void FreeFindData(struct PluginPanelItem * panelItem, int itemsNumber) override;
	void GetOpenPluginInfo(struct OpenPluginInfo * info) override;
	int SetDirectory(const wchar_t *dir, int opMode) override;
	int DeleteFiles(struct PluginPanelItem *panelItem, int itemsNumber, int opMode) override;
	explicit SqlitePanel(const wchar_t * name, const unsigned char * data, int dataSize, int opMode);
	explicit SqlitePanel(const wchar_t * name);
	virtual ~SqlitePanel();
};


#endif // __SQLITEPANEL__
