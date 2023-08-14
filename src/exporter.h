#ifndef __EXPORTER_H__
#define __EXPORTER_H__

/**************************************************************************
 *  SQLiteDB plug-in for FAR 3.0 modifed by VPROFi 2023 for far2l         *
 *  Copyright (C) 2010-2014 by Artem Senichev <artemsen@gmail.com>        *
 *  https://sourceforge.net/projects/farplugs/                            *
 *                                                                        *
 *  This program is free software: you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 3 of the License, or     *
 *  (at your option) any later version.                                   *
 *                                                                        *
 *  This program is distributed in the hope that it will be useful,       *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

#include "plugin.h"
#include "farpanel.h"
#include <sqlite/sqlitedb.h>

class exporter : FarPanel
{
public:
	/**
	 * Constructor.
	 * \param db DB instance
	 */
	exporter(std::unique_ptr<SQLiteDB> & db);

	int ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change) override;
	int GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber) override;

	//Export formats.
	enum format {
		fmt_csv,
		fmt_text
	};

	/**
	 * Export data to file (GUI mode).
	 * \return operation result status (false on error)
	 */
	bool export_data() const;

	/**
	 * Export data to file (GUI mode).
	 * \param db_object DB object name (table or view)
	 * \param fmt export format
	 * \param file_name output file name
	 * \return operation result status (false on error)
	 */
	bool export_data(const wchar_t* db_object, const format fmt, std::wstring& file_name) const;

	/**
	 * Get text from SQL statement.
	 * \param stmt SQL statement
	 * \param idx column index
	 * \param data string data
	 */
	static void get_text(const sqlite_statement& stmt, const int idx, std::string& data);

	/**
	 * Get temporary file name.
	 * \param ext file extension
	 * \return temporary file name
	 */
	static std::wstring get_temp_file_name(const wchar_t* ext);

private:
	/**
	 * Export data to file (GUI mode).
	 * \param db_object DB object name (table or view)
	 * \param fmt export format
	 * \param file_name output file name
	 * \return operation result status (false on error)
	 */
	bool export_data(const wchar_t* db_object, const format fmt, const wchar_t* file_name) const;

private:
	std::unique_ptr<SQLiteDB> & _db;	///< DB instance
};

#endif //__EXPORTER__