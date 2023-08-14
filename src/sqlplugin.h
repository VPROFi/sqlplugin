#ifndef __SQLPLUGIN_H__
#define __SQLPLUGIN_H__

#include "plugin.h"

struct PluginUserData {
	DWORD size;
	union {
		void * data;
	} data;
};

class SqlPlugin : public Plugin {
	private:
		// Panels
		enum {
			PanelSqlite,
			PanelTypeMax
		};

		// copy and assignment not allowed
		SqlPlugin(const SqlPlugin&) = delete;
		void operator=(const SqlPlugin&) = delete;

	public:

		explicit SqlPlugin(const PluginStartupInfo * info);
		virtual ~SqlPlugin() override;

		// far2l api
		HANDLE OpenFilePlugin(const wchar_t * name,const unsigned char * data, int dataSize, int opMode) override;
		HANDLE OpenPlugin(int openFrom, INT_PTR item) override;
		void ClosePlugin(HANDLE hPlugin) override;
};

#endif /* __SQLPLUGIN_H__ */
