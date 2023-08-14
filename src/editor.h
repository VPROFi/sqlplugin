/**************************************************************************
 *  SQLiteDB plug-in for FAR 3.0 modifed by VPROFi 2023 for far2l         *
 *  Copyright (C) 2010-2014 by Artem Senichev <artemsen@gmail.com>        *
 *  https://sourceforge.net/projects/farplugs/                            *
 *                                                                        *
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

#ifndef __EDITOR_H__
#define __EDITOR_H__

#include "plugin.h"
#include "farpanel.h"
#include <sqlite/sqlitedb.h>

class editor : FarPanel
{
public:
	/**
	 * Constructor.
	 * \param db DB instance
	 * \param table_name edited table name
	 */
	editor(std::unique_ptr<SQLiteDB> & db, const char* table_name);

	int ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change) override {return 0;};
	int GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber) override {return 0;};

	/**
	 * Create table row (insert).
	 */
	void insert() const;

	/**
	 * Edit table row (update).
	 */
	void update() const;

	/**
	 * Remove (drop) tables/views/rows etc.
	 * \param items far panel items list
	 * \param items_count number of items
	 * \return operation result state (false on error)
	 */
	bool remove(PluginPanelItem* items, const size_t items_count) const;

private:
	//! Edit field description
	struct field {
		SQLiteDB::sq_column column;
		std::string value;
	};

	/**
	 * Edit (GUI).
	 * \param db_data DB data map (column-type-value)
	 * \param create_mode mode (true for create new row)
	 * \return false if user canceled operation
	 */
	bool edit(std::vector<field>& db_data, const bool create_mode) const;

	struct row_control {
		FarDialogItem label;
		FarDialogItem semi;
		FarDialogItem field;
	};
	
	/**
	 * Create row control.
	 * \param name label
	 * \param value current value
	 * \param poz_y vertical position
	 * \param width_name name width
	 * \param width_val value width
	 * \param ro read-only flag
	 * \return created control
	 */
	row_control create_row_control(const wchar_t* name, const wchar_t* value, const size_t poz_y, const size_t width_name, const size_t width_val, const bool ro) const;

	/**
	 * Free row control.
	 */
	void free_row_control(row_control & rc) const;

	/**
	 * Execute update/insert query.
	 * \param row_id row id (for update), nullptr for insert
	 * \param db_data data description
	 * \return operation result state (false on error)
	 */
	bool exec_update(const char* row_id, const std::vector<field>& db_data) const;

private:
	std::unique_ptr<SQLiteDB> & 	_db;	///< DB instance
	std::string			_table_name;	///< Edited table name
};

#endif // __EDITOR_H__
