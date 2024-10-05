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

#include "editor.h"
#include "exporter.h"
#include "progress.h"
#include <cassert>
#include <utils.h>

#include <cstdlib>

#include <map>

#include <common/log.h>

extern const char * LOG_FILE;
#define LOG_SOURCE_FILE "editor.cpp"

editor::editor(std::unique_ptr<SQLiteDB> & db, const char* table_name)
: _db(db), _table_name(table_name ? table_name : std::string())
{
	assert(_db->GetDb());
}

void editor::update() const
{
	assert(!_table_name.empty());

	//Get edited row id
	auto ppi = GetCurrentPanelItem();
	if( ppi ) {
		if( Plugin::FSF.LStricmp(ppi->FindData.lpwszFileName, L"..") == 0 ) {
			FreePanelItem(ppi);
			return;
		}
		FreePanelItem(ppi);
	} else
		return;

	const uint64_t row_id = ppi->FindData.nPhysicalSize;

	//Read current row data
	std::vector<field> db_data;

	std::string query = "select * from ";
	query += _table_name;
	query += " where rowid=";
	query += std::to_string(row_id);

	sqlite_statement stmt(_db->GetDb());
	if( stmt.prepare(query.c_str()) != SQLITE_OK || stmt.step_execute() != SQLITE_ROW ) {
		const std::wstring query_descr = MB2Wide(query.c_str());
		const std::wstring err_descr = _db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_read), _db->GetDbName().c_str(), query_descr.c_str(), err_descr.c_str()};
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		return;
	}
	const int col_num = stmt.column_count();
	for (int i = 0; i < col_num; ++i) {
		field f;
		f.column.name = stmt.column_name(i);
		switch (stmt.column_type(i)) {
			case SQLITE_INTEGER: f.column.type = SQLiteDB::ct_integer; break;
			case SQLITE_FLOAT: f.column.type = SQLiteDB::ct_float; break;
			case SQLITE3_TEXT: f.column.type = SQLiteDB::ct_text; break;
			case SQLITE_BLOB:
			default:
				f.column.type = SQLiteDB::ct_blob; break;
		}
		exporter::get_text(stmt, i, f.value);
		db_data.push_back(f);
	}
	stmt.close();

	if( edit(db_data, false) && exec_update(std::to_string(row_id).c_str(), db_data) ) {
		Plugin::psi.Control(PANEL_ACTIVE, FCTL_UPDATEPANEL, 0, 0);
		PanelRedrawInfo pri;
		memset(&pri, 0, sizeof(pri));
		Plugin::psi.Control(PANEL_ACTIVE, FCTL_REDRAWPANEL, 0, (LONG_PTR)&pri);
	}
}

void editor::insert() const
{
	assert(!_table_name.empty());

	std::vector<field> db_data;

	//Get columns description
	SQLiteDB::sq_columns columns;
	if( !_db->ReadColumnDescription(_table_name.c_str(), columns) ) {
		const std::wstring err_descr = _db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_read), _db->GetDbName().c_str(), err_descr.c_str() };
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		return;
	}

	db_data.reserve(columns.size());
	for( auto & item : columns ) {
		field f;
		f.column = item;
		if (f.column.type == SQLiteDB::ct_blob)
			f.column.type = SQLiteDB::ct_text;	//Allow edit
		db_data.push_back(f);
	}

	if( edit(db_data, true) && exec_update(nullptr, db_data) ) {
		Plugin::psi.Control(PANEL_ACTIVE, FCTL_UPDATEPANEL, 0, 0);
		PanelRedrawInfo pri;
		memset(&pri, 0, sizeof(pri));
		Plugin::psi.Control(PANEL_ACTIVE, FCTL_REDRAWPANEL, 0, (LONG_PTR)&pri);
	}
}


bool editor::edit(std::vector<field>& db_data, const bool create_mode) const
{
	//Calculate dialog's size
	size_t max_wnd_width = 80;
	SMALL_RECT rc_far_wnd;
	if( Plugin::psi.AdvControl(Plugin::psi.ModuleNumber, ACTL_GETFARRECT, &rc_far_wnd, 0) )
		max_wnd_width = rc_far_wnd.Right + 1;
	size_t max_label_length = 0;
	size_t max_value_length = 0;

	for( auto & item : db_data ) {
		const size_t label_len = item.column.name.length();
		const size_t value_len = item.value.length();
		if (max_label_length < label_len)
			max_label_length = label_len;
		if (max_value_length < value_len)
			max_value_length = value_len;
	}

	if( max_value_length < 40 )
		max_value_length = 40;
	if( max_value_length + max_label_length + 12 > max_wnd_width )
		max_value_length = max_wnd_width - max_label_length - 12;

	const size_t dlg_height = 6 + /* border, buttons etc */ db_data.size();
	size_t dlg_width = 12 + /* border, buttons etc */ max_label_length + max_value_length;
	if( dlg_width < 30 )
		dlg_width = 30;

	//Build dialog
	std::vector<FarDialogItem> dlg_items;

	FarDialogItem dlg_item;

	memset(&dlg_item, 0, sizeof(dlg_item));
	dlg_item.Type = DI_DOUBLEBOX;
	dlg_item.X1 = 3;
	dlg_item.X2 = dlg_width - 4;
	dlg_item.Y1 = 1;
	dlg_item.Y2 = dlg_height - 2;
	dlg_item.PtrData = GetMsg(create_mode ? ps_insert_row_title : ps_edit_row_title);
	dlg_items.push_back(dlg_item);

	std::map<std::string, int> editor_fields;
	size_t y_pos = 2;

	for( auto & item : db_data ) {
		const wchar_t* val = wcsdup(MB2Wide(item.value.c_str()).c_str());
		const wchar_t* name = wcsdup(MB2Wide(item.column.name.c_str()).c_str());

		row_control row_ctl = create_row_control(name, val, y_pos++, max_label_length, max_value_length, item.column.type == SQLiteDB::ct_blob);

		if( y_pos == 3 )
			row_ctl.field.Focus = 1;

		dlg_items.push_back(row_ctl.label);
		dlg_items.push_back(row_ctl.semi);
		dlg_items.push_back(row_ctl.field);
		editor_fields.insert(make_pair(item.column.name, static_cast<int>(dlg_items.size()) - 1));
	}

	size_t last_pos = y_pos;

	memset(&dlg_item, 0, sizeof(dlg_item));
	dlg_item.Type = DI_TEXT;
	dlg_item.Y1 = dlg_height - 4;
	dlg_item.Flags = DIF_SEPARATOR;
	dlg_items.push_back(dlg_item);
	dlg_item.Type = DI_BUTTON;
	dlg_item.Y1 = dlg_height - 3;
	dlg_item.PtrData = GetMsg(ps_save);
	dlg_item.Flags = DIF_CENTERGROUP;
	dlg_item.DefaultButton = 1;
	dlg_items.push_back(dlg_item);
	dlg_item.Type = DI_BUTTON;
	dlg_item.Y1 = dlg_height - 3;
	dlg_item.PtrData = GetMsg(ps_cancel);
	dlg_item.Flags = DIF_CENTERGROUP;
	dlg_items.push_back(dlg_item);

	const HANDLE dlg = Plugin::psi.DialogInit(Plugin::psi.ModuleNumber, -1, -1, dlg_width, dlg_height, nullptr, &dlg_items.front(), dlg_items.size(), 0, 0, nullptr, 0);
	const auto rc = Plugin::psi.DialogRun(dlg);
	if (rc < 0 || rc == static_cast<int>(dlg_items.size()) - 1 /* cancel */) {
		Plugin::psi.DialogFree(dlg);
		for( auto & item : dlg_items ) {
			if( item.Y1 >= static_cast<int>(y_pos) &&  item.Y1 < static_cast<int>(last_pos) )
				free((void *)item.PtrData);
		}
		return false;
	}

	//Get changed data
	db_data.clear();

	for( std::map<std::string, int>::const_iterator it = editor_fields.begin(); it != editor_fields.end(); ++it) {
		if( Plugin::psi.SendDlgMessage(dlg, DM_EDITUNCHANGEDFLAG, it->second, static_cast<LONG_PTR>(-1)) == 0 ) {
			field f;
			f.column.name = it->first;
			f.value = Wide2MB(reinterpret_cast<const wchar_t*>(Plugin::psi.SendDlgMessage(dlg, DM_GETCONSTTEXTPTR, it->second, 0)));
			db_data.push_back(f);
		}
	}

	Plugin::psi.DialogFree(dlg);

	for( auto & item : dlg_items ) {
		if( item.Y1 >= static_cast<int>(y_pos) &&  item.Y1 < static_cast<int>(last_pos) )
			free((void *)item.PtrData);
	}

	return !db_data.empty();
}

bool editor::remove(PluginPanelItem* items, const size_t items_count) const
{
	if( items_count == 1 && items->FindData.lpwszFileName && Plugin::FSF.LStricmp(items->FindData.lpwszFileName, L"..") == 0 )
		return false;

	const wchar_t* quest_msg[] = { GetMsg(ps_title_short), GetMsg(ps_drop_question) };
	if( Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_MB_YESNO | FMSG_WARNING, nullptr, quest_msg, sizeof(quest_msg) / sizeof(quest_msg[0]), 0) != 0)
		return false;

	if (_table_name.empty()) {
		for (size_t i = 0; i < items_count; ++i) {
			std::string query = "drop ";
			if (items[i].FindData.nPhysicalSize == SQLiteDB::ot_table)
				query += "table";
			else if (items[i].FindData.nPhysicalSize == SQLiteDB::ot_view)
				query += "view";
			else if (items[i].FindData.nPhysicalSize == SQLiteDB::ot_index)
				query += "index";
			else
				continue;
			query += ' ';
			query += Wide2MB(items[i].FindData.lpwszFileName);

			LOG_INFO("execute drop: %s\n", query.c_str());

			if (!_db->ExecuteQuery(query.c_str())) {
				const std::wstring query_descr = MB2Wide(query.c_str());
				const std::wstring err_descr = _db->LastError();
				const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_sql), _db->GetDbName().c_str(), query_descr.c_str(), err_descr.c_str()};
				Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
				break;
			}
		}
	}
	else {
		std::string query = "delete from ";
		query += _table_name;
		query += " where rowid in (";
		for (size_t i = 0; i < items_count; ++i) {
			if( i )
				query += ',';
			query += std::to_string(items[i].FindData.nPhysicalSize);
		}
		query += ')';

		LOG_INFO("execute delete: %s\n", query.c_str());

		if (!_db->ExecuteQuery(query.c_str())) {
			const std::wstring query_descr = MB2Wide(query.c_str());
			const std::wstring err_descr = _db->LastError();
			const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_sql), _db->GetDbName().c_str(), query_descr.c_str(), err_descr.c_str()};
			Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		}

	}

	return true;
}

bool editor::exec_update(const char* row_id, const std::vector<field>& db_data) const
{
	std::string query;

	if (row_id && row_id[0]) {
		//Update query
		query = "update '";
		query += _table_name;
		query += "' set ";

		for( std::vector<field>::const_iterator it = db_data.begin(); it != db_data.end(); ++it ) {
			if (it != db_data.begin())
				query += ',';
			query += it->column.name;
			query += "=?";
		}
		query += " where rowid=";
		query += row_id;
	}
	else {
		//Insert query
		query = "insert into '";
		query += _table_name;
		query += "' (";
		for( std::vector<field>::const_iterator it = db_data.begin(); it != db_data.end(); ++it ) {
			if (it != db_data.begin())
				query += ',';
			query += it->column.name;
		}
		query += ") values (";
		for (size_t i = 0; i < db_data.size(); ++i) {
			if (i != 0)
				query += ',';
			query += '?';
		}
		query += ')';
	}

	sqlite_statement stmt(_db->GetDb());
	if( stmt.prepare(query.c_str()) != SQLITE_OK ) {
		const std::wstring query_descr = MB2Wide(query.c_str());
		const std::wstring err_descr = _db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_sql), _db->GetDbName().c_str(), query_descr.c_str(), err_descr.c_str()};
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		return false;
	}
	int idx = 0;
	for( std::vector<field>::const_iterator it = db_data.begin(); it != db_data.end(); ++it ) {
		++idx;
		int bind_rc = SQLITE_OK;
		if (it->column.type == SQLiteDB::ct_float)
			bind_rc = stmt.bind(idx, std::atof(it->value.c_str()));
		else if (it->column.type == SQLiteDB::ct_integer)
			bind_rc = stmt.bind(idx, static_cast<const sqlite3_int64>(std::atoll(it->value.c_str())));
		else
			bind_rc = stmt.bind(idx, it->value.c_str());
		if (bind_rc != SQLITE_OK) {
			const std::wstring query_descr = MB2Wide(query.c_str());
			const std::wstring err_descr = _db->LastError();
			const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_sql), _db->GetDbName().c_str(), query_descr.c_str(), err_descr.c_str()};
			Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
			return false;
		}
	}
	if( stmt.step_execute() != SQLITE_DONE ) {
		const std::wstring query_descr = MB2Wide(query.c_str());
		const std::wstring err_descr = _db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_sql), _db->GetDbName().c_str(), query_descr.c_str(), err_descr.c_str()};
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		return false;
	}
	return true;
}

editor::row_control editor::create_row_control(const wchar_t* name, const wchar_t* value, const size_t poz_y, const size_t width_name, const size_t width_val, const bool ro) const
{
	row_control rc;
	memset(&rc, 0, sizeof(rc));

	rc.label.Type = DI_TEXT;
	rc.label.X1 = 5;
	rc.label.X2 = 5 + width_name - 1;
	rc.label.Y1 = poz_y;
	rc.label.PtrData = name;

	rc.semi.Type = DI_TEXT;
	rc.semi.X1 = rc.semi.X2 = rc.label.X2 + 1;
	rc.semi.Y1 = poz_y;
	rc.semi.PtrData = wcsdup(L":");

	rc.field.Type = DI_EDIT;
	rc.field.X1 = rc.semi.X1 + 2;
	rc.field.X2 = rc.field.X1 + width_val - 1;
	rc.field.Y1 = poz_y;
	rc.field.PtrData = value;
	if (ro)
		rc.field.Flags = DIF_READONLY;

	return rc;
}
