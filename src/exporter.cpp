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

#include "exporter.h"
#include "progress.h"
#include <cassert>
#include <utils.h>

#include <locale>
#include <codecvt>

#include <common/log.h>

extern const char * LOG_FILE;
#define LOG_SOURCE_FILE "exporter.cpp"

#define MAX_BLOB_LENGTH 100
#define MAX_TEXT_LENGTH 1024


exporter::exporter(std::unique_ptr<SQLiteDB> & db)
: FarPanel(), _db(db)
{
	assert(_db);
}

int exporter::ProcessKey(HANDLE hPlugin, int key, unsigned int controlState, bool & change)
{
	return 0;
}

int exporter::GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber)
{
	return 0;
}


bool exporter::export_data() const
{
	LOG_INFO("\n");

	PluginPanelItem * ppi = GetCurrentPanelItem(0);

	if (!ppi || Plugin::FSF.LStricmp(ppi->FindData.lpwszFileName, L"..") == 0 || (ppi->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		return false;
	const wchar_t* db_object_name = ppi->FindData.lpwszFileName;

	LOG_INFO("export: %S\n", db_object_name);

	//Get destination path
	std::wstring dst_file_name;
	wchar_t dir[512];
	Plugin::psi.Control(PANEL_PASSIVE,FCTL_GETPANELDIR,sizeof(dir)/sizeof(dir[0]),(LONG_PTR)dir);
	dst_file_name = dir;

	if (!dst_file_name.empty() && *dst_file_name.rbegin() != L'/')
		dst_file_name += L'/';
	dst_file_name += db_object_name;
	dst_file_name += L".txt";

	FarDialogItem dlg_items[10];
	memset(dlg_items, 0, sizeof(dlg_items));

	dlg_items[0].Type = DI_DOUBLEBOX;
	dlg_items[0].X1 = 3;
	dlg_items[0].X2 = 56;
	dlg_items[0].Y1 = 1;
	dlg_items[0].Y2 = 8;
	dlg_items[0].PtrData = GetMsg(ps_exp_title);

	dlg_items[1].Type = DI_TEXT;
	dlg_items[1].X1 = 5;
	dlg_items[1].X2 = 54;
	dlg_items[1].Y1 = 2;
	std::wstring src_label(16 + wcslen(db_object_name), 0);
	swprintf(&src_label.front(), src_label.size(), GetMsg(ps_exp_main), db_object_name);
	dlg_items[1].PtrData = src_label.c_str();

	dlg_items[2].Type = DI_EDIT;
	dlg_items[2].X1 = 5;
	dlg_items[2].X2 = 54;
	dlg_items[2].Y1 = 3;
	dlg_items[2].PtrData = dst_file_name.c_str();

	dlg_items[3].Type = DI_TEXT;
	dlg_items[3].Y1 = 4;
	dlg_items[3].Flags = DIF_SEPARATOR;

	dlg_items[4].Type = DI_TEXT;
	dlg_items[4].X1 = 5;
	dlg_items[4].X2 = 20;
	dlg_items[4].Y1 = 5;
	dlg_items[4].PtrData = GetMsg(ps_exp_fmt);

	dlg_items[5].Type = DI_RADIOBUTTON;
	dlg_items[5].X1 = 21;
	dlg_items[5].X2 = 29;
	dlg_items[5].Y1 = 5;
	dlg_items[5].PtrData = L"CSV";
	dlg_items[5].Selected = 1;

	dlg_items[6].Type = DI_RADIOBUTTON;
	dlg_items[6].X1 = 30;
	dlg_items[6].X2 = 44;
	dlg_items[6].Y1 = 5;
	dlg_items[6].PtrData = GetMsg(ps_exp_fmt_text);

	dlg_items[7].Type = DI_TEXT;
	dlg_items[7].Y1 = 6;
	dlg_items[7].Flags = DIF_SEPARATOR;

	dlg_items[8].Type = DI_BUTTON;
	dlg_items[8].PtrData = GetMsg(ps_exp_exp);
	dlg_items[8].Y1 = 7;
	dlg_items[8].Flags = DIF_CENTERGROUP;
	dlg_items[8].Focus = 1;
	dlg_items[8].DefaultButton = 1;


	dlg_items[9].Type = DI_BUTTON;
	dlg_items[9].PtrData = GetMsg(ps_cancel);
	dlg_items[9].Y1 = 7;
	dlg_items[9].Flags = DIF_CENTERGROUP;

	const HANDLE dlg = Plugin::psi.DialogInit(Plugin::psi.ModuleNumber, -1, -1, 60, 10, nullptr, dlg_items, sizeof(dlg_items) / sizeof(dlg_items[0]), 0, 0, nullptr, (LONG_PTR)0);
	const intptr_t rc = Plugin::psi.DialogRun(dlg);
	if (rc < 0 || rc == 9 /* cancel */) {
		Plugin::psi.DialogFree(dlg);
		FreePanelItem(ppi);
		return false;
	}
	dst_file_name = reinterpret_cast<const wchar_t*>(Plugin::psi.SendDlgMessage(dlg, DM_GETCONSTTEXTPTR, 2, (LONG_PTR)0));
	const format fmt = Plugin::psi.SendDlgMessage(dlg, DM_GETCHECK, 5, (LONG_PTR)0) == BSTATE_CHECKED ? fmt_csv : fmt_text;
	Plugin::psi.DialogFree(dlg);
	auto ret = export_data(db_object_name, fmt, dst_file_name.c_str());
	FreePanelItem(ppi);
	return ret;
}


bool exporter::export_data(const wchar_t* db_object, const format fmt, std::wstring& file_name) const
{
	assert(db_object && db_object[0]);
	file_name = get_temp_file_name(fmt == fmt_csv ? L"csv" : L"txt");
	return export_data(db_object, fmt, file_name.c_str());
}


bool exporter::export_data(const wchar_t* _db_object, const format fmt, const wchar_t* file_name) const
{
	assert(_db_object && _db_object[0]);
	assert(file_name && file_name[0]);

	LOG_INFO("db_object: %S file: %S\n", _db_object, file_name);

	//Get row count and  columns description
	uint64_t row_count = 0;
	SQLiteDB::sq_columns columns_descr;
	std::string db_object = Wide2MB(_db_object);

	if( !_db->GetRowCount(db_object.c_str(), row_count) || !_db->ReadColumnDescription(db_object.c_str(), columns_descr) ) {
		const std::wstring err_descr = _db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_read), _db->GetDbName().c_str(), err_descr.c_str() };
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		return false;
	}

	const size_t colimns_count = columns_descr.size();

	progress prg_wnd(ps_reading, row_count);

	//Get maximum with for each column
	std::vector<size_t> columns_width(colimns_count);
	if (fmt == fmt_text) {
		std::string query = "select ";
		for (size_t i = 0; i < columns_width.size(); ++i) {
			if (i)
				query += ", ";
			query += "max(length([";
			query += columns_descr[i].name;
			query += "]))";
		}
		query += " from '";
		query += db_object;
		query += '\'';


		sqlite_statement stmt(_db->GetDb());
		if (stmt.prepare(query.c_str()) == SQLITE_OK && stmt.step_execute() == SQLITE_ROW ) {
			for (size_t i = 0; i < columns_width.size(); ++i)
				columns_width[i] = stmt.get_int(static_cast<int>(i));
		} else {
			for (size_t i = 0; i < columns_width.size(); ++i)
				columns_width[i] = 20;
		}
		for (size_t i = 0; i < columns_width.size(); ++i) {
			if (columns_width[i] < columns_descr[i].name.length())
				columns_width[i] = columns_descr[i].name.length();
			if (columns_width[i] > MAX_TEXT_LENGTH)
				columns_width[i] = MAX_TEXT_LENGTH;
		}
	}

	//Create output file
	HANDLE file = CreateFile(file_name, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		prg_wnd.hide();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_writef), file_name };
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_ERRORTYPE | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		return false;
	}
	DWORD bytes_written;

	//Write BOM for text file
	//if (fmt == fmt_text) {
	//	const unsigned char utf16_bom[] = { 0xff, 0xfe };
	//	WriteFile(file, utf16_bom, sizeof(utf16_bom), &bytes_written, nullptr);
	//}

	//Write header (columns names)
	std::string out_text;
	for (size_t i = 0; i < colimns_count; ++i) {
		if (fmt == fmt_csv) {
			out_text += columns_descr[i].name;
			if (i != colimns_count - 1)
				out_text += ';';
		}
		else {
			std::string col_name;
			if (i)
				col_name = ' ';
			col_name += columns_descr[i].name;
			col_name.resize(columns_width[i] + (i && i != colimns_count - 1 ? 2 : 1), ' ');
			out_text += col_name;
			if (i != colimns_count - 1)
				out_text += "│";//0x2502;
		}
	}
	out_text += "\n";

	//Header separator
	if (fmt == fmt_text) {
		for (size_t i = 0; i < columns_descr.size(); ++i) {
			std::u32string s(columns_width[i] + (i && i != colimns_count - 1 ? 2 : 1), u'─' /*0x2500*/);
			std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
			std::string _col_sep = conv.to_bytes(s);
			out_text += _col_sep;
			if (i != colimns_count - 1)
				out_text += "┼";//0x253C;
		}
		out_text += "\n";
	}
	WriteFile(file, out_text.c_str(), static_cast<DWORD>(out_text.length() * sizeof(char)), &bytes_written, nullptr);

	//Read data
	std::string query = "select * from '";
	query += db_object;
	query += '\'';
	sqlite_statement stmt(_db->GetDb());
	if (stmt.prepare(query.c_str()) != SQLITE_OK) {
		prg_wnd.hide();
		const std::wstring err_descr = _db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_read), _db->GetDbName().c_str(), err_descr.c_str() };
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		CloseHandle(file);
		return false;
	}

	int count = 0;
	int state = SQLITE_OK;
	while ((state = stmt.step_execute()) == SQLITE_ROW) {
		if (++count % 100 == 0)
			prg_wnd.update(count);
		if (progress::aborted()) {
			CloseHandle(file);
			return false;
		}

		out_text.clear();
		for (int i = 0; i < static_cast<int>(colimns_count); ++i) {
			std::string col_data;
			get_text(stmt, i, col_data);
			if (fmt == fmt_csv) {
				const bool use_quote = columns_descr[i].type == SQLiteDB::ct_text && col_data.find(';') != std::string::npos;
				if (use_quote) {
					out_text += '"';
					//Replace quote by double quote
					size_t qpos = 0;
					while ((qpos = col_data.find('"', qpos)) != std::string::npos) {
						col_data.insert(qpos, 1, '"');
						qpos += 2;
					}
				}
				out_text += col_data;
				if (use_quote)
					out_text += '"';
				if (i != static_cast<int>(colimns_count) - 1)
					out_text += ';';
			}
			else {
				if (i)
					col_data = " " + col_data;
				col_data.resize(columns_width[i] + (i && i != static_cast<int>(colimns_count) - 1 ? 2 : 1), ' ');
				if (col_data.size() > MAX_TEXT_LENGTH) {
					col_data.erase(MAX_TEXT_LENGTH - 3);
					col_data += "...";
				}
				out_text += col_data;
				if (i != static_cast<int>(colimns_count) - 1)
					out_text += "│";//0x2502;
			}
		}
		out_text += "\n";

		if (!WriteFile(file, out_text.c_str(), static_cast<DWORD>(out_text.length() * sizeof(char)), &bytes_written, nullptr)) {
			const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_writef), file_name };
			Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_ERRORTYPE | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
			CloseHandle(file);
			return false;
		}
	}

	CloseHandle(file);

	if (state != SQLITE_DONE) {
		prg_wnd.hide();
		const std::wstring err_descr = _db->LastError();
		const wchar_t* err_msg[] = {GetMsg(ps_title_short), GetMsg(ps_err_read), _db->GetDbName().c_str(), err_descr.c_str()};
		Plugin::psi.Message(Plugin::psi.ModuleNumber, FMSG_WARNING | FMSG_MB_OK, nullptr, err_msg, sizeof(err_msg) / sizeof(err_msg[0]), 0);
		return false;
	}

	return true;
}


std::wstring exporter::get_temp_file_name(const wchar_t* ext)
{
	std::wstring tmp_file_name = L"sqlite.tmp";

	wchar_t tmp_name[MAX_PATH];

	if (Plugin::FSF.MkTemp(tmp_name, MAX_PATH, L"sql"))
 		tmp_file_name = tmp_name;

	if (ext && ext[0]) {
		tmp_file_name += L'.';
		tmp_file_name += ext;
	}

	LOG_INFO("tmp_file_name: %S\n", tmp_file_name.c_str());

	return tmp_file_name;
}


void exporter::get_text(const sqlite_statement& stmt, const int idx, std::string& data)
{
	if (stmt.column_type(idx) == SQLITE_BLOB) {
		const int blob_len = stmt.get_length(idx);
		data = "[";
		data += std::to_string(blob_len);
		data += "]:0x";
		const unsigned char* blob_data = static_cast<const unsigned char*>(stmt.get_blob(idx));
		for (int j = 0; j < blob_len && j < MAX_BLOB_LENGTH; ++j) {
			char h[3];
			snprintf(h, sizeof(h)/sizeof(h[0]), "%02x", blob_data[j]);
			data += h;
		}
		if (blob_len >= MAX_BLOB_LENGTH)
			data += "...";
	}
	else {
		const char* txt = stmt.get_text(idx);
		data = txt ? txt : std::string();
		//Replace unreadable symbols
		const size_t len = data.length();
		for (size_t i = 0; i < len; ++i) {
			char& sym = data[i];
			if (sym < ' ')
				sym = ' ';
		}
	}
}
