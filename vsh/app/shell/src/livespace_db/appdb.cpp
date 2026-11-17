/*
 * The application database's queries: its pages (tbl_appinfo_page), their icons
 * (tbl_appinfo_icon), the applications' parameters (tbl_appinfo), the settings (tbl_config),
 * the URI schemes (tbl_uri) and the themes (tbl_theme). Each runs on the connection of the
 * caller's transaction.
 */
#include <string.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/list>
#include <std/algorithm>
#include <common/string_util.h>
#include <dbutil/statement.h>
#include "livespace_db_internal.h"
#include "lsdb_statement.h"

using dbutil::RefPtr;
using dbutil::Statement;

/* the values no bits update may set */
static unsigned int s_fixedKeys[] = {
	LsdbKeyHash("SUPPORT_URI"),
	LsdbKeyHash("_fg_title_id"),
	LsdbKeyHash("_bg_title_id"),
};

/* the number of pages, the folders' pages left out */
int AppDbGetPageCount(ActivityDbConnection *connection, int *count)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(
		paf::string("SELECT count(*) FROM tbl_appinfo_page WHERE 0 <= pageNo"), &statement);

	if (result != 0)
		return result;
	result = statement->Step(1);
	if (result != 0)
		return result;
	dbutil::Value value;

	memset(&value, 0, sizeof(value));
	statement->GetValue(0, &value);
	*count = value.type == SQLITE_INTEGER ? (int)value.data.integer : 0;
	return result;
}

/* binds a page: its number to ?1, its columns to ?2 ... ?8 */
static __attribute__((noinline)) int AppDbBindPage(RefPtr<Statement> *statement, int pageNo, const LsdbPage *page)
{
	int result = (*statement)->ClearBindings();

	if (result != 0)
		return result;
	result = (*statement)->Reset();
	if (result != 0)
		return result;
	result = AppDbBindInt(statement, 1, pageNo);
	if (result != 0)
		return result;
	result = AppDbBindString(statement, 2, &page->themeFile);
	if (result != 0)
		return result;
	result = AppDbBindInt(statement, 3, (unsigned int)page->bgColor);
	if (result != 0)
		return result;
	result = AppDbBindInt(statement, 4, (unsigned int)page->texWidth);
	if (result != 0)
		return result;
	result = AppDbBindInt(statement, 5, (unsigned int)page->texHeight);
	if (result != 0)
		return result;
	result = AppDbBindInt(statement, 6, (unsigned int)page->imageWidth);
	if (result != 0)
		return result;
	result = AppDbBindInt(statement, 7, (unsigned int)page->imageHeight);
	if (result != 0)
		return result;
	return AppDbBindInt(statement, 8, (unsigned int)page->reserved01);
}

/*
 * Binds an icon: the fields columns (LSDB_ICON_FIELD_*) to ?1 ... ?8, with insert also its type,
 * title ID and folder to ?20 ... ?22; then where it is: the title ID's icons (?30), the rowid
 * (?30) or the position on pageNo (?30, ?31; pos, or the icon's own with a negative pos).
 */
static __attribute__((noinline)) int AppDbBindIcon(RefPtr<Statement> *statement, int pageNo, const paf::string *titleId,
	const LsdbIcon *icon, bool insert, unsigned int fields, int pos, const void *image,
	int imageSize, int type)
{
	int result = (*statement)->ClearBindings();

	if (result != 0)
		return result;
	result = (*statement)->Reset();
	if (result != 0)
		return result;
	if (fields & LSDB_ICON_FIELD_TITLE) {
		result = AppDbBindString(statement, 1, &icon->title);
		if (result != 0)
			return result;
	}
	if (fields & LSDB_ICON_FIELD_ICON_PATH) {
		result = AppDbBindString(statement, 2, &icon->iconPath);
		if (result != 0)
			return result;
	}
	if (fields & LSDB_ICON_FIELD_ICON0_TYPE) {
		result = AppDbBindInt(statement, 3, icon->icon0Type);
		if (result != 0)
			return result;
	}
	if (fields & LSDB_ICON_FIELD_COMMAND) {
		result = AppDbBindString(statement, 4, &icon->command);
		if (result != 0)
			return result;
	}
	if (fields & LSDB_ICON_FIELD_PARENTAL_LOCK_LV) {
		result = AppDbBindInt(statement, 5, icon->parentalLockLv);
		if (result != 0)
			return result;
	}
	if (fields & LSDB_ICON_FIELD_STATUS) {
		result = AppDbBindInt(statement, 6, (unsigned int)icon->status);
		if (result != 0)
			return result;
	}
	if (fields & LSDB_ICON_FIELD_IMAGE) {
		dbutil::Value value;

		value.type = SQLITE_BLOB;
		value.name = NULL;
		value.size = imageSize;
		value.data.integer = 0;
		value.data.blob = image;
		result = (*statement)->Bind(7, &value, true);
		if (result != 0)
			return result;
	}
	if (fields & LSDB_ICON_FIELD_THEME_ICON_PATH) {
		result = AppDbBindString(statement, 8, &icon->iconPath);
		if (result != 0)
			return result;
	}
	if (insert) {
		result = AppDbBindInt(statement, 20, icon->type);
		if (result != 0)
			return result;
		result = AppDbBindString(statement, 21, &icon->titleId);
		if (result != 0)
			return result;
		LsdbFolder *folder = icon->folder.get();

		if (folder != NULL && LSDB_IS_FOLDER_ID(folder->id)) {
			result = AppDbBindInt(statement, 22, folder->id);
			if (result != 0)
				return result;
		}
	}
	if (titleId->size() != 0)
		return AppDbBindString(statement, 30, titleId);
	if (type != -1)
		return AppDbBindInt(statement, 30, type);
	if (pos < 0)
		pos = icon->pos;
	result = AppDbBindInt(statement, 30, pos);
	if (result != 0)
		return result;
	return AppDbBindInt(statement, 31, pageNo);
}

/*
 * Inserts a page at pageNo with its icons (create: a page of its own, a folder's, not counted
 * among the pages).
 */
int AppDbAddPage(ActivityDbConnection *connection, int pageNo, LsdbIconList *icons, LsdbPage *page,
	int create)
{
	if (!create) {
		if (pageNo < 0)
			return LSDB_ERROR_INVALID_ARGUMENT;
		int count = 0;
		int result = AppDbGetPageCount(connection, &count);

		if (result != 0)
			return result;
		if (pageNo > count)
			return LSDB_ERROR_PAGE_RANGE;
	}
	RefPtr<Statement> pageStatement;
	int result = connection->Prepare(paf::string("INSERT INTO tbl_appinfo_page(pageNo, themeFile, "
		"bgColor, texWidth, texHeight, imageWidth, imageHeight, reserved01) "
		"VALUES(?, ?, ?, ?, ?, ?, ?, ?)"), &pageStatement);

	if (result != 0)
		return result;
	RefPtr<Statement> iconStatement;

	result = connection->Prepare(paf::string("INSERT INTO tbl_appinfo_icon(title, iconPath, "
		"icon0Type, command, parentalLockLv, status, reserved05, type, titleId, reserved01, "
		"pos, pageId) VALUES(?1,?2,?3,?4,?5,?6,?7,?20,?21,?22,?30,(SELECT pageId FROM "
		"tbl_appinfo_page WHERE pageNo=?31))"), &iconStatement);
	if (result != 0)
		return result;
	result = AppDbBindPage(&pageStatement, pageNo, page);
	if (result != 0)
		return result;
	result = pageStatement->Step(1);
	if (result == 1)
		result = 0;
	else if (result != 0)
		return result;
	for (LsdbIconList::iterator icon = icons->begin(); icon != icons->end(); ++icon) {
		result = AppDbBindIcon(&iconStatement, pageNo, &paf::string((const char *)NULL),
			icon->get(), true, LSDB_ICON_FIELD_ALL, -1, NULL, 0, -1);
		if (result != 0)
			return result;
		result = iconStatement->Step(1);
		if (result != 1) {
			if (result == LSDB_DBUTIL_ERROR_CONSTRAINT)
				result = LSDB_ERROR_POSITION_USED;
			return result;
		}
		result = 0;
	}
	return result;
}

/* Creates or upgrades the tables of a database of the schema version given. */
int AppDbCreateTables(ActivityDbConnection *connection, int version)
{
	int result = 0;

	if (version <= 0) {
		const char *sql[] = {
			"CREATE TABLE IF NOT EXISTS tbl_appinfo_page(pageId INTEGER PRIMARY KEY NOT NULL ,"
			"pageNo INT NOT NULL,themeFile TEXT,bgColor INT,texWidth INT,texHeight INT,"
			"imageWidth INT,imageHeight INT,reserved01,reserved02,reserved03,reserved04,"
			"reserved05)",
			"CREATE TABLE IF NOT EXISTS tbl_appinfo_icon(pageId REFERENCES "
			"tbl_appinfo_page(pageId) ON DELETE RESTRICT NOT NULL,pos INT NOT NULL,iconPath "
			"TEXT,title TEXT COLLATE NOCASE,type NOT NULL,command TEXT, titleId TEXT,icon0Type "
			"NOT NULL,parentalLockLv INT,status INT,reserved01,reserved02,reserved03,"
			"reserved04,reserved05,PRIMARY KEY(pageId, pos));",
			"CREATE TABLE IF NOT EXISTS tbl_appinfo(titleId TEXT NOT NULL,key INTEGER NOT NULL,"
			"val,PRIMARY KEY(titleId, key));",
			"CREATE TABLE IF NOT EXISTS tbl_config (ikey INTEGER PRIMARY KEY NOT NULL ,val);",
			"CREATE INDEX IF NOT EXISTS idx_page_no ON tbl_appinfo_page ( pageNo )",
			"CREATE INDEX IF NOT EXISTS idx_icon_pos ON tbl_appinfo_icon ( pos, pageId )",
			"CREATE INDEX IF NOT EXISTS idx_icon_title ON tbl_appinfo_icon (title, titleId, type)",
		};

		result = SceActivityDb_D3277649(connection, sql, 7);
		if (result != 0)
			return result;
		LsdbIconList icons;
		LsdbPage page;

		result = AppDbAddPage(connection, LSDB_PAGE_NO_FOLDERS, &icons, &page, 1);
		if (result != 0)
			return result;
	}
	if (version < 2) {
		const char *sql[] = {
			"CREATE TABLE IF NOT EXISTS tbl_uri(titleId TEXT NOT NULL,flag INT,scheme TEXT NOT "
			"NULL,hostpath TEXT)",
			"CREATE INDEX id_scheme ON tbl_uri (scheme)",
		};

		result = SceActivityDb_D3277649(connection, sql, 2);
		if (result != 0)
			return result;
	}
	if (version < 3) {
		const char *sql[] = {
			"DROP TRIGGER IF EXISTS tgr_deletePage",
			"DROP TRIGGER IF EXISTS tgr_insertPage",
			"CREATE TRIGGER IF NOT EXISTS tgr_deletePage2 AFTER DELETE ON tbl_appinfo_page WHEN "
			"OLD.pageNo >= 0 BEGIN UPDATE tbl_appinfo_page SET pageNo=pageNo-1 WHERE "
			"tbl_appinfo_page.pageNo>OLD.pageNo; END",
			"CREATE TRIGGER IF NOT EXISTS tgr_insertPage2 BEFORE INSERT ON tbl_appinfo_page WHEN "
			"NEW.pageNo >= 0 BEGIN UPDATE tbl_appinfo_page SET pageNo=pageNo+1 WHERE "
			"tbl_appinfo_page.pageNo>=NEW.pageNo; END",
		};

		result = SceActivityDb_D3277649(connection, sql, 4);
		if (result != 0)
			return result;
	}
	if (version < 6) {
		const char *sql[] = {
			"CREATE TABLE IF NOT EXISTS tbl_theme(id TEXT PRIMARY KEY NOT NULL,"
			"packageImageFilePath TEXT,homePreviewFilePath TEXT,startPreviewFilePath TEXT,"
			"contentVer INTEGER,lastModifiedTime TEXT,size INTEGER,type INTEGER,titleDefault "
			"BLOB,providerDefault BLOB,title0 BLOB,provider0 BLOB,title1 BLOB,provider1 BLOB,"
			"title2 BLOB,provider2 BLOB,title3 BLOB,provider3 BLOB,title4 BLOB,provider4 BLOB,"
			"title5 BLOB,provider5 BLOB,title6 BLOB,provider6 BLOB,title7 BLOB,provider7 BLOB,"
			"title8 BLOB,provider8 BLOB,title9 BLOB,provider9 BLOB,title10 BLOB,provider10 BLOB,"
			"title11 BLOB,provider11 BLOB,title12 BLOB,provider12 BLOB,title13 BLOB,provider13 "
			"BLOB,title14 BLOB,provider14 BLOB,title15 BLOB,provider15 BLOB,title16 BLOB,"
			"provider16 BLOB,title17 BLOB,provider17 BLOB,title18 BLOB,provider18 BLOB,title19 "
			"BLOB,provider19 BLOB,title20 BLOB,provider20 BLOB,title21 BLOB,provider21 BLOB,"
			"title22 BLOB,provider22 BLOB,title23 BLOB,provider23 BLOB,title24 BLOB,provider24 "
			"BLOB,title25 BLOB,provider25 BLOB,reserved01,reserved02,reserved03,reserved04,"
			"reserved05)",
			"CREATE INDEX idx_theme_time ON tbl_theme(lastModifiedTime)",
		};

		result = SceActivityDb_D3277649(connection, sql, 2);
		if (result != 0)
			return result;
	}
	if (version < 7) {
		dbutil::Value value;

		memset(&value, 0, sizeof(value));
		value.type = SQLITE_INTEGER;
		value.data.integer = (unsigned int)LSDB_PAGE_DEFAULT_RESERVED01;
		result = connection->Execute(paf::string("UPDATE tbl_appinfo_page SET reserved01=?"), 1,
			&value, NULL, NULL, NULL);
	}
	return result;
}

/* no position: an icon not on a page */
int AppDbIsNoPosition(int pageNo, int pos)
{
	return pageNo == -1 && pos == -1;
}

/* binds a setting's value: its integer's bits in mask, or its text */
static __attribute__((noinline)) int AppDbBindValue(RefPtr<Statement> *statement, int index,
	const LsdbValue *value, SceInt64 mask = -1)
{
	if (value->type == LSDB_VALUE_INTEGER)
		return AppDbBindInt(statement, index, value->integer & mask);
	if (value->type == LSDB_VALUE_TEXT)
		return AppDbBindText(statement, index, LsdbValueText(*value), -1);
	return LSDB_ERROR_INVALID_ARGUMENT;
}

/* sets a setting (tbl_config) */
static __attribute__((noinline)) int AppDbSetConfigValue(ActivityDbConnection *connection, int key, const LsdbValue *value)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("REPLACE INTO tbl_config VALUES(?, ?)"),
		&statement);

	if (result != 0)
		return result;
	result = AppDbBindInt(&statement, 1, key);
	if (result != 0)
		return result;
	result = AppDbBindValue(&statement, 2, value);
	if (result != 0)
		return result;
	result = statement->Step(1);
	if (result == 1)
		result = 0;
	return result;
}

int AppDbSetConfigString(ActivityDbConnection *connection, int key, const char *value)
{
	LsdbValue text(value);

	return AppDbSetConfigValue(connection, key, &text);
}

/* Sets the bits of a setting in mask to bits, or makes the setting bits. */
int AppDbUpdateConfigBits(ActivityDbConnection *connection, int key, SceInt64 bits, SceInt64 mask)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(
		paf::string("UPDATE tbl_config SET val=((val & ?2) | ?3) WHERE ikey=?1"), &statement);

	if (result == 0) {
		result = AppDbBindInt(&statement, 1, key);
		if (result == 0) {
			result = AppDbBindInt(&statement, 2, ~mask);
			if (result == 0) {
				result = AppDbBindInt(&statement, 3, bits & mask);
				if (result == 0) {
					result = statement->Step(1);
					if (result == 1)
						result = 0;
					if (result == 0 && connection->GetChanges() == 0)
						result = LSDB_ERROR_NOT_FOUND;
				}
			}
		}
	}
	statement = RefPtr<Statement>();
	if (result != 0) {
		LsdbValue value;

		value.integer = bits;
		result = AppDbSetConfigValue(connection, key, &value);
	}
	return result;
}

/* takes a column's value: an integer or a copy of its text */
static __attribute__((noinline)) void LsdbValueSetColumn(const dbutil::Value *column, LsdbValue *value)
{
	if (column->type == SQLITE_INTEGER) {
		SceInt64 integer = column->data.integer;

		if (value->type == LSDB_VALUE_TEXT) {
			delete value->text;
			value->text = NULL;
		}
		value->integer = integer;
		value->type = LSDB_VALUE_INTEGER;
	} else if (column->type == SQLITE_TEXT) {
		const char *text = column->data.text;

		if (value->type == LSDB_VALUE_TEXT && value->text != NULL)
			*value->text = text;
		else
			value->text = new paf::string(text);
		value->type = LSDB_VALUE_TEXT;
	}
}

/* reads a setting; LSDB_ERROR_NOT_FOUND without it */
static __attribute__((noinline)) int AppDbGetConfigValue(ActivityDbConnection *connection, int key, LsdbValue *value)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("SELECT val FROM tbl_config WHERE ikey=?"),
		&statement);

	if (result != 0)
		return result;
	result = AppDbBindInt(&statement, 1, key);
	if (result != 0)
		return result;
	result = statement->Step(1);
	if (result == 1)
		return LSDB_ERROR_NOT_FOUND;
	if (result == 0) {
		dbutil::Value column;

		result = statement->GetValue(0, &column);
		if (result == 0)
			LsdbValueSetColumn(&column, value);
	}
	return result;
}

/* a setting's text: "" for a setting that is not text, defaultValue without the setting */
paf::string AppDbGetConfigString(ActivityDbConnection *connection, int key,
	const char *defaultValue)
{
	LsdbValue value;

	if (AppDbGetConfigValue(connection, key, &value) == 0)
		defaultValue = LsdbValueText(value);
	return paf::string(defaultValue);
}

/* a setting's integer: 0 for a setting that is not one, defaultValue without the setting */
SceInt64 AppDbGetConfigInt64(ActivityDbConnection *connection, int key, SceInt64 defaultValue)
{
	LsdbValue value;

	if (AppDbGetConfigValue(connection, key, &value) == 0)
		defaultValue = value.type == LSDB_VALUE_INTEGER ? value.integer : 0;
	return defaultValue;
}

/*
 * Reads an icon from its row (rowid, pos, iconPath, title, type, command, titleId, icon0Type,
 * parentalLockLv, status, reserved01, reserved02 from column 1): a folder's icon gets its
 * folder, and with theme the icon's theme image (reserved02) is its image.
 */
static __attribute__((noinline)) void AppDbReadIcon(RefPtr<Statement> *statement, LsdbIcon *icon,
	bool theme)
{
	paf::string themeIconPath;

	icon->rowid = AppDbColumnInt(statement, 1);
	icon->pos = AppDbColumnInt(statement, 2);
	icon->iconPath = AppDbColumnString(statement, 3);
	icon->title = AppDbColumnString(statement, 4);
	icon->type = AppDbColumnInt(statement, 5);
	icon->command = AppDbColumnString(statement, 6);
	icon->titleId = AppDbColumnString(statement, 7);
	icon->icon0Type = AppDbColumnInt(statement, 8);
	icon->parentalLockLv = AppDbColumnInt(statement, 9);
	icon->status = AppDbColumnInt(statement, 10);

	int folderId = AppDbColumnInt(statement, 11);

	if (LSDB_IS_FOLDER_ID(folderId)) {
		if (icon->folder.get() == NULL)
			icon->folder = paf::common::SharedPtr<LsdbFolder>(new LsdbFolder);
		icon->folder->id = folderId;
	}
	themeIconPath = AppDbColumnString(statement, 12);
	if (theme && themeIconPath.size() != 0)
		icon->iconPath = themeIconPath;
}

/*
 * Adds the icon of the row a statement stepped to (its page number in column 0) to its page's
 * list, while the page shows fewer than LSDB_PAGE_ICONS; a folder's icon gets the title IDs of
 * the folder's applications.
 */
static __attribute__((noinline)) int AppDbAddIconRow(ActivityDbConnection *connection,
	RefPtr<Statement> *statement, int firstPageNo, LsdbPageIconLists *pages)
{
	dbutil::Value column;
	int result = (*statement)->GetValue(0, &column);

	if (result != 0)
		return result;
	int index = (int)column.data.integer - firstPageNo;

	if (index < 0 || (unsigned int)index >= pages->size()) {
		result = LSDB_ERROR_DATABASE;
	} else if ((*pages)[index]->size() < LSDB_PAGE_ICONS) {
		LsdbIconPtr icon(new LsdbIcon);
		LsdbIcon *item = icon.get();

		(*pages)[index]->push_back(icon);
		AppDbReadIcon(statement, item, true);

		LsdbFolder *folder = item->folder.get();

		if (folder != NULL && LSDB_IS_FOLDER_ID(folder->id)) {
			RefPtr<Statement> titles;

			if (connection->Prepare(paf::string("SELECT titleId FROM tbl_appinfo_icon WHERE "
				"pageId=(SELECT pageId FROM tbl_appinfo_page WHERE pageNo=?)"), &titles) == 0) {
				int folderId = folder->id;

				if (titles->ClearBindings() == 0 && titles->Reset() == 0 &&
				    AppDbBindInt(&titles, 1, folderId) == 0) {
					while (titles->Step(1) == 0) {
						paf::string titleId = AppDbColumnString(&titles, 0);

						if (titleId.size() != 0)
							folder->titleIds.push_back(titleId);
						folder->iconCount++;
					}
				}
			}
		}
	}
	return result;
}

/*
 * Reads the icons of count pages from pageNo, a list per page (pages has a list for each);
 * LSDB_ERROR_NOT_FOUND when a page without icons does not exist.
 */
int AppDbGetIcons(ActivityDbConnection *connection, int pageNo, unsigned int count,
	LsdbPageIconLists *pages)
{
	if (count == 0 || pages->size() < count)
		return LSDB_ERROR_INVALID_ARGUMENT;
	int result;
	{
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("SELECT (SELECT pageNo FROM tbl_appinfo_page "
			"WHERE tbl_appinfo_icon.pageId = pageId) AS pageNo,rowid, pos, iconPath, title, "
			"type, command, titleId, icon0Type, parentalLockLv, status,reserved01,reserved02 "
			"FROM tbl_appinfo_icon WHERE  EXISTS (SELECT pageNo FROM tbl_appinfo_page WHERE "
			"tbl_appinfo_icon.pageId = pageId AND ? <= tbl_appinfo_page.pageNo AND "
			"tbl_appinfo_page.pageNo < ?)ORDER BY pos"), &statement);
		if (result != 0)
			return result;
		int endPageNo = pageNo + count;

		result = statement->ClearBindings();
		if (result == 0) {
			result = statement->Reset();
			if (result == 0) {
				result = AppDbBindInt(&statement, 1, pageNo);
				if (result == 0)
					result = AppDbBindInt(&statement, 2, endPageNo);
			}
		}
		if (result != 0)
			return result;
		for (LsdbPageIconLists::iterator page = pages->begin(); page != pages->end(); ++page)
			(*page)->clear();
		while ((result = statement->Step(1)) == 0) {
			result = AppDbAddIconRow(connection, &statement, pageNo, pages);
			if (result != 0)
				return result;
		}
		if (result != 1)
			return result;
	}
	RefPtr<Statement> page;
	LsdbPageIconLists::iterator list = pages->begin();

	result = 0;
	for (unsigned int i = 0; i < count; i++, pageNo++, ++list) {
		if ((*list)->size() != 0)
			continue;
		if (page.Get() == NULL) {
			result = connection->Prepare(paf::string("SELECT pageId, pageNo, themeFile, bgColor, "
				"texWidth, texHeight, imageWidth, imageHeight, reserved01 FROM "
				"tbl_appinfo_page WHERE pageNo=?"), &page);
			if (result != 0)
				return result;
		}
		result = page->ClearBindings();
		if (result == 0) {
			result = page->Reset();
			if (result == 0)
				result = AppDbBindInt(&page, 1, pageNo);
		}
		if (result != 0)
			return result;
		result = page->Step(1);
		if (result != 0) {
			if (result == 1)
				result = LSDB_ERROR_NOT_FOUND;
			return result;
		}
	}
	return result;
}

/* a page's columns, each by its name, from the row a statement stepped to */
static inline int AppDbReadPage(RefPtr<Statement> *statement, paf::common::SharedPtr<LsdbPage> page)
{
	int columns = (*statement)->m_columnCount;
	int result = 0;

	for (int column = 0; column < columns; column++) {
		dbutil::Value value;

		result = (*statement)->GetValue(column, &value);
		if (result != 0)
			break;
		if (sce_paf_strcmp(value.name, "themeFile") == 0) {
			paf::string themeFile;

			if (value.type == SQLITE_TEXT)
				themeFile.assign(value.data.text, value.size);
			page->themeFile = themeFile;
		} else if (sce_paf_strcmp(value.name, "bgColor") == 0) {
			page->bgColor = value.type == SQLITE_INTEGER ? value.data.integer : 0;
		} else if (sce_paf_strcmp(value.name, "texWidth") == 0) {
			page->texWidth = value.type == SQLITE_INTEGER ? value.data.integer : 0;
		} else if (sce_paf_strcmp(value.name, "texHeight") == 0) {
			page->texHeight = value.type == SQLITE_INTEGER ? value.data.integer : 0;
		} else if (sce_paf_strcmp(value.name, "imageWidth") == 0) {
			page->imageWidth = value.type == SQLITE_INTEGER ? value.data.integer : 0;
		} else if (sce_paf_strcmp(value.name, "imageHeight") == 0) {
			page->imageHeight = value.type == SQLITE_INTEGER ? value.data.integer : 0;
		} else if (sce_paf_strcmp(value.name, "reserved01") == 0) {
			page->reserved01 = value.type == SQLITE_INTEGER ? value.data.integer : 0;
		}
	}
	return result;
}

/*
 * Reads count pages from pageNo into pages (a page for each), each column by its name;
 * LSDB_ERROR_NOT_FOUND when a page does not exist.
 */
int AppDbGetPage(ActivityDbConnection *connection, int pageNo, unsigned int count,
	LsdbPageList *pages)
{
	if (count == 0 || pages->size() < count)
		return LSDB_ERROR_INVALID_ARGUMENT;
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("SELECT pageId, pageNo, themeFile, bgColor, "
		"texWidth, texHeight, imageWidth, imageHeight, reserved01 FROM tbl_appinfo_page "
		"WHERE pageNo=?"), &statement);

	if (result == 0) {
		LsdbPageList::iterator it = pages->begin();

		for (unsigned int i = 0; i < count; i++, pageNo++, ++it) {
			result = statement->ClearBindings();
			if (result == 0) {
				result = statement->Reset();
				if (result == 0)
					result = AppDbBindInt(&statement, 1, pageNo);
			}
			if (result != 0)
				break;
			result = statement->Step(1);
			if (result != 0) {
				if (result == 1)
					result = LSDB_ERROR_NOT_FOUND;
				break;
			}
			result = AppDbReadPage(&statement, *it);
			if (result != 0)
				break;
		}
	}
	return result;
}

/*
 * Replaces the icons of a page with list's, through the prepared DELETE and INSERT; a page left
 * without icons must exist (LSDB_ERROR_POSITION_USED otherwise).
 */
static __attribute__((noinline)) int AppDbReplaceIcons(ActivityDbConnection *connection, int pageNo,
	LsdbIconList *list, RefPtr<Statement> *deleteStatement, RefPtr<Statement> *insertStatement)
{
	int result = (*deleteStatement)->ClearBindings();

	if (result == 0) {
		result = (*deleteStatement)->Reset();
		if (result == 0)
			result = AppDbBindInt(deleteStatement, 1, pageNo);
	}
	if (result != 0)
		return result;
	do
		result = (*deleteStatement)->Step(1);
	while (result == 0);
	if (result != 1)
		return result;
	result = 0;
	if (list->size() == 0) {
		RefPtr<Statement> page;

		result = connection->Prepare(paf::string("SELECT pageId, pageNo, themeFile, bgColor, "
			"texWidth, texHeight, imageWidth, imageHeight, reserved01 FROM tbl_appinfo_page "
			"WHERE pageNo=?"), &page);
		if (result == 0) {
			result = page->ClearBindings();
			if (result == 0) {
				result = page->Reset();
				if (result == 0)
					result = AppDbBindInt(&page, 1, pageNo);
			}
			if (result == 0) {
				result = page->Step(1);
				if (result == 1)
					result = LSDB_ERROR_POSITION_USED;
			}
		}
		return result;
	}
	for (LsdbIconList::iterator icon = list->begin(); icon != list->end(); ++icon) {
		result = AppDbBindIcon(insertStatement, pageNo, &paf::string((const char *)NULL),
			icon->get(), true, LSDB_ICON_FIELD_ALL, -1, NULL, 0, -1);
		if (result != 0)
			break;
		do
			result = (*insertStatement)->Step(1);
		while (result == 0);
		if (result != 1) {
			if (result == LSDB_DBUTIL_ERROR_CONSTRAINT)
				result = LSDB_ERROR_POSITION_USED;
			break;
		}
		result = 0;
	}
	return result;
}

/*
 * Replaces the icons of each page of pageNos with its list of lists; a page number must be a
 * page's, LSDB_PAGE_NO_FOLDERS or a folder's.
 */
int AppDbReplacePageIcons(ActivityDbConnection *connection, paf::vector<int> *pageNos,
	LsdbPageIconLists *lists)
{
	if (pageNos->size() < lists->size() || pageNos->size() == 0)
		return LSDB_ERROR_INVALID_ARGUMENT;
	RefPtr<Statement> deleteStatement;
	int result = connection->Prepare(paf::string("DELETE FROM tbl_appinfo_icon WHERE pageId="
		"(SELECT pageId FROM tbl_appinfo_page WHERE pageNo=?)"), &deleteStatement);

	if (result != 0)
		return result;
	RefPtr<Statement> insertStatement;

	result = connection->Prepare(paf::string("INSERT INTO tbl_appinfo_icon(title, iconPath, "
		"icon0Type, command, parentalLockLv, status, reserved05, type, titleId, reserved01, "
		"pos, pageId) VALUES(?1,?2,?3,?4,?5,?6,?7,?20,?21,?22,?30,(SELECT pageId FROM "
		"tbl_appinfo_page WHERE pageNo=?31))"), &insertStatement);
	if (result != 0)
		return result;
	LsdbPageIconLists::iterator list = lists->begin();

	for (paf::vector<int>::iterator pageNo = pageNos->begin(); pageNo != pageNos->end();
	     ++pageNo, ++list) {
		if ((*pageNo < 0 && *pageNo != LSDB_PAGE_NO_FOLDERS && !LSDB_IS_FOLDER_ID(*pageNo)) ||
		    list == lists->end())
			return LSDB_ERROR_INVALID_ARGUMENT;
		result = AppDbReplaceIcons(connection, *pageNo, list->get(), &deleteStatement,
			&insertStatement);
		if (result != 0)
			break;
	}
	return result;
}

/*
 * The UPDATE of the icons a title's (titleId), a type's or a position's icons, of the fields
 * given: the icon's columns ?1..?8, the title ID, type or position ?30 and page number ?31.
 */
static __attribute__((noinline)) paf::string AppDbIconUpdateSql(const paf::string *titleId,
	unsigned int fields, int type)
{
	paf::string sql("UPDATE tbl_appinfo_icon SET ");

	if (fields & LSDB_ICON_FIELD_TITLE)
		sql += "title=?1,";
	if (fields & LSDB_ICON_FIELD_ICON_PATH)
		sql += "iconPath=?2,";
	if (fields & LSDB_ICON_FIELD_ICON0_TYPE)
		sql += "icon0Type=?3,";
	if (fields & LSDB_ICON_FIELD_COMMAND)
		sql += "command=?4,";
	if (fields & LSDB_ICON_FIELD_PARENTAL_LOCK_LV)
		sql += "parentalLockLv=?5,";
	if (fields & LSDB_ICON_FIELD_STATUS)
		sql += "status=?6,";
	if (fields & LSDB_ICON_FIELD_IMAGE)
		sql += "reserved05=?7,";
	if (fields & LSDB_ICON_FIELD_THEME_ICON_PATH)
		sql += "reserved02=?8,";
	sql[sql.size() - 1] = ' ';
	if (titleId->size() != 0)
		sql += "WHERE titleId=?30";
	else if (type == -1)
		sql += "WHERE pos=?30 AND pageId=(SELECT pageId FROM tbl_appinfo_page WHERE pageNo=?31)";
	else
		sql += "WHERE type=?30";
	return sql;
}

/*
 * Updates the fields given of a title's icons (titleId not empty), a type's (type not -1) or
 * the icon at the icon's position on pageNo; LSDB_ERROR_NOT_FOUND when no icon changed.
 */
static __attribute__((noinline)) int AppDbUpdateIcons(ActivityDbConnection *connection, int pageNo,
	const paf::string *titleId, const LsdbIcon *icon, unsigned int fields, const void *image,
	int imageSize, int type)
{
	if (icon == NULL)
		return LSDB_ERROR_INVALID_ARGUMENT;
	RefPtr<Statement> statement;
	int result = connection->Prepare(AppDbIconUpdateSql(titleId, fields, type), &statement);

	if (result == 0) {
		result = AppDbBindIcon(&statement, pageNo, titleId, icon, false, fields, -1, image,
			imageSize, type);
		if (result == 0) {
			do
				result = statement->Step(1);
			while (result == 0);
			if (result == 1)
				result = connection->GetChanges() == 0 ? LSDB_ERROR_NOT_FOUND : 0;
		}
	}
	return result;
}

/* updates the fields given of the icon at the icon's position on pageNo, its image with them */
int AppDbUpdateIcon(ActivityDbConnection *connection, int pageNo, const LsdbIconPtr *icon,
	unsigned int fields, const void *image, int imageSize)
{
	return AppDbUpdateIcons(connection, pageNo, &paf::string((const char *)NULL), icon->get(),
		fields, image, imageSize, -1);
}

/* updates the fields given of a title's icons */
int AppDbUpdateTitleIcons(ActivityDbConnection *connection, const paf::string *titleId,
	const LsdbIconPtr *icon, unsigned int fields)
{
	return AppDbUpdateIcons(connection, -1, titleId, icon->get(), fields, NULL, 0, -1);
}

/* updates the fields given of the icons of a type */
int AppDbUpdateTypeIcons(ActivityDbConnection *connection, int type, const LsdbIconPtr *icon,
	unsigned int fields)
{
	const LsdbIcon *item = icon->get();

	return AppDbUpdateIcons(connection, -1, &paf::string((const char *)NULL), item, fields, NULL,
		0, type);
}

/* deletes the icon at pos on pageNo; LSDB_ERROR_NOT_FOUND when there is none */
static __attribute__((noinline)) int AppDbDeleteIconAt(ActivityDbConnection *connection, int pageNo,
	int pos)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("DELETE FROM tbl_appinfo_icon WHERE pageId="
		"(SELECT pageId FROM tbl_appinfo_page WHERE pageNo=?) AND pos=?"), &statement);

	if (result == 0) {
		result = AppDbBindInt(&statement, 1, pageNo);
		if (result == 0) {
			result = AppDbBindInt(&statement, 2, pos);
			if (result == 0) {
				result = statement->Step(1);
				if (result == 1)
					result = 0;
				if (result == 0 && connection->GetChanges() == 0)
					result = LSDB_ERROR_NOT_FOUND;
			}
		}
	}
	return result;
}

/* binds the move of the icon at fromPos on fromPage to toPos on toPage */
static __attribute__((noinline)) int AppDbBindMove(RefPtr<Statement> *statement, int fromPage,
	int fromPos, int toPage, int toPos)
{
	int result;

	(*statement)->ClearBindings();
	(*statement)->Reset();
	result = AppDbBindInt(statement, 1, toPage);
	if (result == 0) {
		result = AppDbBindInt(statement, 2, toPos);
		if (result == 0) {
			result = AppDbBindInt(statement, 3, fromPage);
			if (result == 0)
				result = AppDbBindInt(statement, 4, fromPos);
		}
	}
	return result;
}

/*
 * Moves each icon at fromPositions[i] on fromPages[i] to toPositions[i] on toPages[i], first to
 * a free position of its new page (-toPos - 1), then in place; an icon moved to no page (-1,
 * -1) is deleted. LSDB_ERROR_POSITION_USED when an icon is missing or its place is taken.
 */
int AppDbMoveIcons(ActivityDbConnection *connection, paf::vector<int> *fromPages,
	paf::vector<int> *fromPositions, paf::vector<int> *toPages, paf::vector<int> *toPositions)
{
	unsigned int count = fromPages->size();

	if (count == 0 || count != fromPositions->size() || count != toPages->size() ||
	    count != toPositions->size())
		return LSDB_ERROR_INVALID_ARGUMENT;
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("UPDATE tbl_appinfo_icon SET pageId=(SELECT "
		"PageId FROM tbl_appinfo_page WHERE pageNo=?), pos=? WHERE pageId=(SELECT pageId FROM "
		"tbl_appinfo_page WHERE pageNo=?) AND pos=?"), &statement);

	if (result != 0)
		return result;
	paf::vector<int>::iterator fromPage = fromPages->begin();
	paf::vector<int>::iterator fromPos = fromPositions->begin();
	paf::vector<int>::iterator toPage = toPages->begin();
	paf::vector<int>::iterator toPos;

	for (toPos = toPositions->begin(); toPos != toPositions->end();
	     ++fromPage, ++fromPos, ++toPage, ++toPos) {
		int pageNo = *toPage;

		if (pageNo >= 0 || pageNo == LSDB_PAGE_NO_FOLDERS || LSDB_IS_FOLDER_ID(pageNo)) {
			result = AppDbBindMove(&statement, *fromPage, *fromPos, pageNo, -*toPos - 1);
			if (result != 0)
				return result;
			result = statement->Step(1);
			if (result != 1) {
				if (result == LSDB_DBUTIL_ERROR_CONSTRAINT)
					result = LSDB_ERROR_POSITION_USED;
				return result;
			}
			result = 0;
			if (connection->GetChanges() == 0)
				return LSDB_ERROR_POSITION_USED;
		} else {
			if ((pageNo & *toPos) != -1)	/* not both -1 */
				return LSDB_ERROR_INVALID_ARGUMENT;
			result = AppDbDeleteIconAt(connection, *fromPage, *fromPos);
			if (result != 0)
				return result;
		}
	}
	toPage = toPages->begin();
	for (toPos = toPositions->begin(); toPos != toPositions->end(); ++toPage, ++toPos) {
		if ((*toPage & *toPos) == -1)	/* both -1 */
			continue;
		result = AppDbBindMove(&statement, *toPage, -*toPos - 1, *toPage, *toPos);
		if (result != 0)
			return result;
		result = statement->Step(1);
		if (result != 1) {
			if (result == LSDB_DBUTIL_ERROR_CONSTRAINT)
				result = LSDB_ERROR_POSITION_USED;
			return result;
		}
		result = 0;
		if (connection->GetChanges() == 0)
			return LSDB_ERROR_POSITION_USED;
	}
	return result;
}

/* updates each page of pageNos from its page of pages */
int AppDbUpdatePage(ActivityDbConnection *connection, paf::vector<int> *pageNos, LsdbPageList *pages)
{
	if (pageNos->size() < pages->size() || pageNos->size() == 0)
		return LSDB_ERROR_INVALID_ARGUMENT;
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("UPDATE tbl_appinfo_page SET themeFile=?2, "
		"bgColor=?3, texWidth=?4, texHeight=?5, imageWidth=?6, imageHeight=?7, reserved01=?8 "
		"WHERE pageNo=?1"), &statement);

	if (result != 0)
		return result;
	LsdbPageList::iterator page = pages->begin();

	for (paf::vector<int>::iterator pageNo = pageNos->begin(); pageNo != pageNos->end();
	     ++pageNo, ++page) {
		if ((*pageNo < 0 && *pageNo != LSDB_PAGE_NO_FOLDERS && !LSDB_IS_FOLDER_ID(*pageNo)) ||
		    page == pages->end())
			return LSDB_ERROR_INVALID_ARGUMENT;
		result = AppDbBindPage(&statement, *pageNo, page->get());
		if (result != 0)
			return result;
		do
			result = statement->Step(1);
		while (result == 0);
		if (result != 1)
			return result;
	}
	return 0;
}

/* deletes a page, and a folder's page its icons' folder references */
int AppDbDeletePage(ActivityDbConnection *connection, int pageNo)
{
	bool folder = LSDB_IS_FOLDER_ID(pageNo);

	if (pageNo < 0 && !folder)
		return LSDB_ERROR_INVALID_ARGUMENT;
	int result;
	{
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("DELETE FROM tbl_appinfo_page WHERE pageNo=?"),
			&statement);
		if (result != 0)
			return result;
		result = statement->ClearBindings();
		if (result == 0) {
			result = statement->Reset();
			if (result == 0)
				result = AppDbBindInt(&statement, 1, pageNo);
		}
		if (result != 0)
			return result;
		result = statement->Step(1);
		if (result != 1) {
			if (result == LSDB_DBUTIL_ERROR_CONSTRAINT)
				result = LSDB_ERROR_EXISTS;
			return result;
		}
		result = 0;
		if (connection->GetChanges() == 0)
			return LSDB_ERROR_NOT_FOUND;
	}
	if (!folder)
		return result;
	RefPtr<Statement> statement;

	result = connection->Prepare(paf::string("DELETE FROM tbl_appinfo_icon WHERE reserved01=?"),
		&statement);
	if (result != 0)
		return result;
	result = statement->ClearBindings();
	if (result == 0) {
		result = statement->Reset();
		if (result == 0)
			result = AppDbBindInt(&statement, 1, pageNo);
	}
	if (result != 0)
		return result;
	result = statement->Step(1);
	if (result == 1)
		result = 0;
	return result;
}

/*
 * Counts the icons of pageCount pages from pageNo, or of every page when pageNo is no page's,
 * LSDB_PAGE_NO_FOLDERS' or a folder's.
 */
int AppDbCountIcons(ActivityDbConnection *connection, int pageNo, unsigned int pageCount,
	int *count)
{
	bool all = pageNo < 0 && pageNo != LSDB_PAGE_NO_FOLDERS && !LSDB_IS_FOLDER_ID(pageNo);
	RefPtr<Statement> statement;
	paf::string sql("SELECT count(*) FROM tbl_appinfo_icon ");

	if (!all)
		sql += "WHERE pageId IN (SELECT pageId FROM tbl_appinfo_page WHERE pageNo BETWEEN ? AND ?)";
	int result = connection->Prepare(sql, &statement);

	if (result != 0)
		return result;
	if (!all) {
		result = AppDbBindInt(&statement, 1, pageNo);
		if (result != 0)
			return result;
		result = AppDbBindInt(&statement, 2, pageNo + (SceUInt64)pageCount - 1);
		if (result != 0)
			return result;
	}
	result = statement->Step(1);
	if (result != 0)
		return result;
	dbutil::Value value;

	result = statement->GetValue(0, &value);
	if (result != 0)
		return result;
	*count = value.type == SQLITE_INTEGER ? value.data.integer : 0;
	return result;
}

/* counts the icons on pages (no folder's) that are no folder's icons */
int AppDbCountUserIcons(ActivityDbConnection *connection, int *count)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("SELECT count(*) FROM tbl_appinfo_icon WHERE "
		"type!=? AND pageId != (SELECT pageId FROM tbl_appinfo_page WHERE pageNo=?)"), &statement);

	if (result != 0)
		return result;
	result = AppDbBindInt(&statement, 1, LSDB_ICON_TYPE_FOLDER);
	if (result != 0)
		return result;
	result = AppDbBindInt(&statement, 2, LSDB_PAGE_NO_FOLDERS);
	if (result != 0)
		return result;
	result = statement->Step(1);
	if (result != 0)
		return result;
	dbutil::Value value;

	result = statement->GetValue(0, &value);
	if (result != 0)
		return result;
	*count = AppDbColumnInt(&statement, 0);
	return result;
}

/* reads the icon at pos on pageNo; LSDB_ERROR_NOT_FOUND when there is none */
int AppDbGetIconAt(ActivityDbConnection *connection, int pageNo, int pos, LsdbIcon *icon)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("SELECT pageId, rowid, pos, iconPath, title, "
		"type, command, titleId, icon0Type, parentalLockLv, status,reserved01,reserved02 FROM "
		"tbl_appinfo_icon WHERE pageId=(SELECT pageId FROM tbl_appinfo_page WHERE pageNo=?) AND "
		"pos=?"), &statement);

	if (result != 0)
		return result;
	result = AppDbBindInt(&statement, 1, pageNo);
	if (result != 0)
		return result;
	result = AppDbBindInt(&statement, 2, pos);
	if (result != 0)
		return result;
	result = statement->Step(1);
	if (result == 0)
		AppDbReadIcon(&statement, icon, true);
	else if (result == 1)
		result = LSDB_ERROR_NOT_FOUND;
	return result;
}

/* reads a folder's icon and the page it is on; LSDB_ERROR_NOT_FOUND when there is none */
int AppDbGetFolderIcon(ActivityDbConnection *connection, int folderId, int *pageNo, LsdbIcon *icon)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("SELECT (SELECT pageNo FROM tbl_appinfo_page "
		"WHERE tbl_appinfo_icon.pageId = pageId) AS pageNo,rowid, pos, iconPath, title, type, "
		"command, titleId, icon0Type, parentalLockLv, status,reserved01,reserved02 FROM "
		"tbl_appinfo_icon WHERE reserved01=?"), &statement);

	if (result != 0)
		return result;
	result = AppDbBindInt(&statement, 1, folderId);
	if (result != 0)
		return result;
	result = statement->Step(1);
	if (result != 0) {
		if (result == 1)
			result = LSDB_ERROR_NOT_FOUND;
		return result;
	}
	*pageNo = AppDbColumnInt(&statement, 0);
	AppDbReadIcon(&statement, icon, true);
	return result;
}

/*
 * The IDs of the folders of a title, in the order of their pages and positions;
 * LSDB_ERROR_NOT_FOUND when there are none.
 */
int AppDbFindFolders(ActivityDbConnection *connection, const paf::string *title,
	paf::vector<int> *folderIds)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("SELECT tbl_appinfo_icon.reserved01 FROM "
		"tbl_appinfo_icon INNER JOIN tbl_appinfo_page ON tbl_appinfo_icon.pageId="
		"tbl_appinfo_page.pageId WHERE title=? AND type=? ORDER BY tbl_appinfo_page.pageNo "
		"ASC, tbl_appinfo_icon.pos ASC"), &statement);

	if (result != 0)
		return result;
	result = AppDbBindString(&statement, 1, title);
	if (result != 0)
		return result;
	result = AppDbBindInt(&statement, 2, LSDB_ICON_TYPE_FOLDER);
	if (result != 0)
		return result;
	while ((result = statement->Step(1)) == 0)
		folderIds->push_back(AppDbColumnInt(&statement, 0));
	if (result == 1)
		result = 0;
	if (folderIds->size() == 0)
		result = LSDB_ERROR_NOT_FOUND;
	return result;
}

/* the position after the last icon of a page, 0 when it has none or on any error */
static inline int AppDbGetNextPos(ActivityDbConnection *connection, int pageNo)
{
	RefPtr<Statement> statement;
	int pos = 0;
	int result = connection->Prepare(paf::string("SELECT max(pos)+1 FROM tbl_appinfo_icon WHERE "
		"pageId=(SELECT pageId FROM tbl_appinfo_page WHERE pageNo=?)"), &statement);

	if (result == 0) {
		result = statement->ClearBindings();
		if (result == 0) {
			result = statement->Reset();
			if (result == 0)
				result = AppDbBindInt(&statement, 1, pageNo);
		}
		if (result == 0) {
			result = statement->Step(1);
			if (result == 0) {
				dbutil::Value value;

				result = statement->GetValue(0, &value);
				if (result == 0 && value.type == SQLITE_INTEGER)
					pos = value.data.integer & 0x7FFFFFFF;
			}
		}
	}
	if (result != 0)
		pos = 0;
	return pos;
}

/*
 * Adds an icon at its position on pageNo, or after the last on LSDB_PAGE_NO_FOLDERS' page, with
 * its image when given; the icon gets its rowid. Nothing is added for a page number that is no
 * page's.
 */
int AppDbAddIcon(ActivityDbConnection *connection, int pageNo, const LsdbIconPtr *icon,
	const void *image, int imageSize)
{
	LsdbIcon *item = icon->get();
	int pos = -1;
	unsigned int fields = LSDB_ICON_FIELD_ALL;

	if (image != NULL && imageSize != 0)
		fields = LSDB_ICON_FIELD_ALL | LSDB_ICON_FIELD_IMAGE;
	if (pageNo < 0 && pageNo != LSDB_PAGE_NO_FOLDERS && !LSDB_IS_FOLDER_ID(pageNo))
		return 0;
	if (item->type == 0 && item->titleId.size() == 0)
		return LSDB_ERROR_NO_TITLE_ID;
	if (pageNo == LSDB_PAGE_NO_FOLDERS)
		pos = AppDbGetNextPos(connection, pageNo);
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("INSERT INTO tbl_appinfo_icon(title, iconPath, "
		"icon0Type, command, parentalLockLv, status, reserved05, type, titleId, reserved01, "
		"pos, pageId) VALUES(?1,?2,?3,?4,?5,?6,?7,?20,?21,?22,?30,(SELECT pageId FROM "
		"tbl_appinfo_page WHERE pageNo=?31))"), &statement);

	if (result == 0) {
		result = AppDbBindIcon(&statement, pageNo, &paf::string((const char *)NULL), item, true,
			fields, pos, image, imageSize, -1);
		if (result == 0) {
			result = statement->Step(1);
			if (result == 1) {
				item->rowid = connection->GetLastInsertRowid();
				result = 0;
			} else if (result == LSDB_DBUTIL_ERROR_CONSTRAINT) {
				result = LSDB_ERROR_POSITION_USED;
			}
		}
	}
	return result;
}

/* deletes a title's icons; LSDB_ERROR_NOT_FOUND when it has none */
int AppDbDeleteTitleIcons(ActivityDbConnection *connection, const paf::string *titleId)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("DELETE FROM tbl_appinfo_icon WHERE titleId=?"),
		&statement);

	if (result == 0) {
		result = statement->ClearBindings();
		if (result == 0) {
			result = statement->Reset();
			if (result == 0)
				result = AppDbBindString(&statement, 1, titleId);
		}
		if (result == 0) {
			result = statement->Step(1);
			if (result == 1)
				result = 0;
			if (result == 0 && connection->GetChanges() == 0)
				result = LSDB_ERROR_NOT_FOUND;
		}
	}
	return result;
}

/* binds a title ID as a statement's first parameter */
static __attribute__((noinline)) int AppDbBindTitleId(RefPtr<Statement> *statement,
	const paf::string *titleId)
{
	int result = (*statement)->ClearBindings();

	if (result == 0) {
		result = (*statement)->Reset();
		if (result == 0)
			result = AppDbBindString(statement, 1, titleId);
	}
	return result;
}

/* adds the parameter of the row a statement stepped to (its columns key and val) to info */
static __attribute__((noinline)) int AppDbReadAppInfoRow(RefPtr<Statement> *statement,
	LsdbAppInfoPtr *info)
{
	unsigned int key = 0;
	LsdbValue value;
	int columns = (*statement)->m_columnCount;

	for (int column = 0; column < columns; column++) {
		dbutil::Value field;
		int result = (*statement)->GetValue(column, &field);

		if (result != 0)
			return result;
		if (sce_paf_strcmp(field.name, "key") == 0)
			key = field.type == SQLITE_INTEGER ? field.data.integer : 0;
		else if (sce_paf_strcmp(field.name, "val") == 0)
			LsdbValueSetColumn(&field, &value);
	}
	(*info)->insert(paf::pair<unsigned int, LsdbValue>(key, value));
	return 0;
}

/* reads a title's parameters, its title ID with them; LSDB_ERROR_NOT_FOUND when it has none */
int AppDbGetAppInfo(ActivityDbConnection *connection, const paf::string *titleId,
	LsdbAppInfoPtr *info)
{
	if (titleId->size() == 0)
		return LSDB_ERROR_NO_TITLE_ID;
	(*info)->clear();

	int result;
	{
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("SELECT key, val FROM tbl_appinfo WHERE "
			"titleId=?"), &statement);
		if (result == 0) {
			result = AppDbBindTitleId(&statement, titleId);
			if (result == 0) {
				while ((result = statement->Step(1)) == 0) {
					result = AppDbReadAppInfoRow(&statement, info);
					if (result != 0)
						break;
				}
				if (result == 1)
					result = (*info)->size() == 0 ? LSDB_ERROR_NOT_FOUND : 0;
			}
		}
	}
	if (result == 0) {
		paf::string *id = new paf::string(*titleId);

		(*info)->insert(paf::pair<unsigned int, LsdbValue>(LSDB_KEY_TITLE_ID, LsdbValue(id)));
		delete id;
	}
	return result;
}

/* reads one parameter of a title; LSDB_ERROR_NOT_FOUND when it has none */
int AppDbGetAppInfoValue(ActivityDbConnection *connection, const paf::string *titleId,
	unsigned int key, LsdbValue *value)
{
	if (titleId->size() == 0)
		return LSDB_ERROR_NO_TITLE_ID;
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("SELECT val FROM tbl_appinfo WHERE titleId=? "
		"AND key=?"), &statement);

	if (result != 0)
		return result;
	result = AppDbBindString(&statement, 1, titleId);
	if (result != 0)
		return result;
	result = AppDbBindInt(&statement, 2, key);
	if (result != 0)
		return result;
	result = statement->Step(1);
	if (result != 0) {
		if (result == 1)
			result = LSDB_ERROR_NOT_FOUND;
		return result;
	}
	dbutil::Value column;

	result = statement->GetValue(0, &column);
	if (result == 0)
		LsdbValueSetColumn(&column, value);
	return result;
}

/* whether a title has parameters */
int AppDbHasAppInfo(ActivityDbConnection *connection, const paf::string *titleId)
{
	int found = 0;

	if (titleId->size() == 0)
		return found;
	RefPtr<Statement> statement;

	if (connection->Prepare(paf::string("SELECT 1 FROM tbl_appinfo WHERE titleId=?"),
		&statement) != 0)
		return found;
	if (AppDbBindTitleId(&statement, titleId) == 0 && statement->Step(1) == 0)
		found = 1;
	return found;
}

/* an operator's SQL, NULL for none */
static inline const char *AppDbQueryOpSql(int op)
{
	switch (op) {
	case LSDB_QUERY_OP_EQUAL:
		return "=?";
	case LSDB_QUERY_OP_LIKE:
		return " LIKE ? ESCAPE '$'";
	case LSDB_QUERY_OP_NOT_EQUAL:
		return "!=?";
	case LSDB_QUERY_OP_BITS:
		return "&?=?";
	default:
		return NULL;
	}
}

/* a condition's table alias: a without one */
static inline paf::string AppDbQueryAlias(const paf::string *alias)
{
	if (alias->size() == 0)
		return paf::string("a");
	return *alias;
}

/*
 * Adds a condition's test of the rows of alias (a without one) to where: its key, and any of
 * its values by its operator.
 */
static __attribute__((noinline)) int AppDbAddQueryCondition(paf::string *where,
	LsdbQueryCondition *condition, const paf::string *alias)
{
	int result = 0;

	*where += "(";
	where->append(AppDbQueryAlias(alias));
	*where += ".key=?";
	if (condition->values.size() != 0) {
		*where += " AND (";
		for (paf::vector<LsdbValue>::iterator value = condition->values.begin();
		     value != condition->values.end(); ++value) {
			if ((value->type == LSDB_VALUE_INTEGER && condition->op == LSDB_QUERY_OP_LIKE) ||
			    (value->type == LSDB_VALUE_TEXT && condition->op == LSDB_QUERY_OP_BITS)) {
				result = LSDB_ERROR_QUERY_TYPE;
				goto out;
			}
			if (value != condition->values.begin())
				*where += " OR ";
			where->append(AppDbQueryAlias(alias));

			paf::string column(".val");

			where->append(column + AppDbQueryOpSql(condition->op));
		}
		*where += ")";
	}
	*where += ")";
out:
	return result;
}

/*
 * The SELECT of the title IDs that meet every condition: one alias of tbl_appinfo per
 * condition, a0 the first, the others joined to it by title ID.
 */
static __attribute__((noinline)) int AppDbBuildTitleQuery(LsdbQueryCondition *condition,
	paf::string *sql)
{
	int result = 0;
	paf::string join;
	paf::string where;
	paf::string alias;

	*sql = "SELECT DISTINCT titleId FROM tbl_appinfo a ";
	for (int index = 0; condition != NULL; condition = condition->next, index++) {
		if (condition->key == 0)
			continue;
		if (condition->key == LSDB_KEY_TITLE_ID) {
			result = LSDB_ERROR_INVALID_ARGUMENT;
			goto out;
		}
		char number[64];

		sce_paf_snprintf(number, sizeof(number), "%i", index);
		alias = "a" + paf::string(number);
		if (index == 0) {
			*sql = "SELECT a0.titleId FROM tbl_appinfo a0 ";
			where += "WHERE ";
		} else {
			if (index == 1) {
				sql->append(" INNER JOIN tbl_appinfo " + alias);
				join += " ON ";
			} else {
				sql->append(",tbl_appinfo " + alias);
				join += " AND ";
			}
			join.append("a0.titleId = " + alias + ".titleId ");
			where += " AND ";
		}
		result = AppDbAddQueryCondition(&where, condition, &alias);
		if (result != 0)
			goto out;
	}
	sql->append(join + where);
out:
	return result;
}

/*
 * The SELECT of the title IDs of the icons whose titles meet every condition: the icons of
 * type ?, outside page ? unless flags has 2, ordered by title unless flags has 4.
 */
static __attribute__((noinline)) int AppDbBuildIconQuery(LsdbQueryCondition *condition,
	paf::string *sql, unsigned int flags)
{
	int result;
	paf::string join;
	paf::string where;
	paf::string tail;
	paf::string alias;

	*sql = "SELECT i.titleId FROM tbl_appinfo_icon i WHERE ";
	if ((flags & 2) == 0)
		*sql += "(SELECT pageId FROM tbl_appinfo_page WHERE pageNo=?)!=i.pageId AND ";
	for (int index = 0; condition != NULL; condition = condition->next, index++) {
		if (condition->key == 0)
			continue;
		if (condition->key == LSDB_KEY_TITLE_ID) {
			result = LSDB_ERROR_INVALID_ARGUMENT;
			goto out;
		}
		char number[64];

		sce_paf_snprintf(number, sizeof(number), "%i", index);
		alias = "a" + paf::string(number);
		if (index == 0) {
			*sql += "(SELECT 1 FROM tbl_appinfo a0 ";
			where += "WHERE a0.titleId=i.titleId AND ";
			tail = ") AND ";
		} else {
			if (index == 1) {
				sql->append("INNER JOIN tbl_appinfo " + alias);
				join += " ON ";
			} else {
				sql->append(", tbl_appinfo " + alias);
				join += " AND ";
			}
			join.append("a0.titleId = " + alias + ".titleId ");
			where += " AND ";
		}
		result = AppDbAddQueryCondition(&where, condition, &alias);
		if (result != 0)
			goto out;
	}
	result = 0;
	sql->append(join + where + tail + "i.type=? ");
	if ((flags & 4) == 0)
		*sql += "ORDER BY i.title";
out:
	return result;
}

/*
 * Binds a condition's parameters from *index on: its key, then each value (a LIKE text with its
 * '_' and '%' escaped by '$' and a '%' after it; an integer's mask before it for
 * LSDB_QUERY_OP_BITS).
 */
static __attribute__((noinline)) int AppDbBindQueryCondition(RefPtr<Statement> *statement,
	int *index, const LsdbQueryCondition *condition)
{
	int result = 0;

	if (condition->key == 0)
		goto out;
	result = AppDbBindInt(statement, (*index)++, condition->key);
	if (result != 0)
		goto out;
	for (unsigned int i = 0; i < condition->values.size(); i++) {
		if (condition->values.at(i).type == LSDB_VALUE_TEXT) {
			if (condition->op == LSDB_QUERY_OP_LIKE) {
				paf::string text(LsdbValueText(condition->values.at(i)));
				paf::string::size_type found;

				for (paf::string::size_type pos = text.size() - 1;; pos = found - 1) {
					found = text.find_last_of("_%", pos, 2);
					if (found == paf::string::npos)
						break;
					text.insert(found, 1, '$');
					if (found == 0)
						break;
				}

				int parameter = (*index)++;
				paf::string pattern = text + '%';

				result = AppDbBindString(statement, parameter, &pattern);
			} else {
				int parameter = (*index)++;

				result = AppDbBindText(statement, parameter,
					LsdbValueText(condition->values.at(i)), -1);
			}
		} else {
			if (condition->op == LSDB_QUERY_OP_BITS)
				result = AppDbBindInt(statement, (*index)++, condition->mask);
			if (result != 0)
				break;

			int parameter = (*index)++;
			const LsdbValue &value = condition->values.at(i);

			result = AppDbBindInt(statement, parameter,
				value.type == LSDB_VALUE_INTEGER ? value.integer : 0);
		}
		if (result != 0)
			break;
	}
out:
	return result;
}

/*
 * The title IDs that meet every condition into titleIds: of the applications with flags 1, else
 * of the icons (see AppDbBuildIconQuery).
 */
int AppDbFindTitles(ActivityDbConnection *connection, LsdbQueryCondition *conditions,
	paf::vector<paf::string> *titleIds, unsigned int flags)
{
	RefPtr<Statement> statement;
	int result;

	titleIds->clear();
	{
		paf::string sql;

		if (conditions->key == LSDB_KEY_TITLE_ID) {
			result = LSDB_ERROR_INVALID_ARGUMENT;
		} else {
			if ((flags & 1) != 0)
				result = AppDbBuildTitleQuery(conditions, &sql);
			else
				result = AppDbBuildIconQuery(conditions, &sql, flags);
			if (result == 0)
				result = connection->Prepare(sql, &statement);
			if (result == 0) {
				if ((flags & 1) != 0) {
					int index = 1;

					result = statement->ClearBindings();
					if (result == 0)
						result = statement->Reset();
					if (result == 0) {
						for (const LsdbQueryCondition *condition = conditions;
						     condition != NULL; condition = condition->next) {
							result = AppDbBindQueryCondition(&statement, &index,
								condition);
							if (result != 0)
								break;
						}
					}
				} else {
					int index = 1;

					result = statement->ClearBindings();
					if (result == 0)
						result = statement->Reset();
					if (result == 0 && (flags & 2) == 0)
						result = AppDbBindInt(&statement, index++,
							LSDB_PAGE_NO_FOLDERS);
					if (result == 0) {
						const LsdbQueryCondition *condition;

						for (condition = conditions; condition != NULL;
						     condition = condition->next) {
							result = AppDbBindQueryCondition(&statement, &index,
								condition);
							if (result != 0)
								break;
						}
						if (condition == NULL)
							result = AppDbBindInt(&statement, index++, 0);
					}
				}
			}
		}
	}
	if (result == 0) {
		for (;;) {
			result = statement->Step(1);
			if (result != 0)
				break;

			dbutil::Value value;

			result = statement->GetValue(0, &value);
			if (result != 0)
				goto out;
			if (value.type == SQLITE_TEXT) {
				paf::string titleId(value.data.text);

				titleIds->push_back(titleId);
			}
		}
		if (result == 1)
			result = 0;
	}
out:
	return result;
}

/*
 * A title's icon into icon (with theme, its theme image) and, without pageNo NULL, the number
 * of its page; LSDB_ERROR_NOT_FOUND without one.
 */
static __attribute__((noinline)) int AppDbGetTitleIconOnly(ActivityDbConnection *connection,
	const paf::string *titleId, LsdbIconPtr *icon, int *pageNo, bool theme)
{
	if (titleId->size() == 0)
		return LSDB_ERROR_NO_TITLE_ID;

	SceInt64 pageId;
	int result;

	{
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("SELECT pageId, rowid, pos, iconPath, title, "
			"type, command, titleId, icon0Type, parentalLockLv, status,reserved01,reserved02 "
			"FROM tbl_appinfo_icon WHERE titleId=?"), &statement);
		if (result != 0)
			return result;
		result = statement->ClearBindings();
		if (result == 0)
			result = statement->Reset();
		if (result == 0)
			result = AppDbBindString(&statement, 1, titleId);
		if (result != 0)
			return result;
		result = statement->Step(1);
		if (result != 0) {
			if (result == 1)
				result = LSDB_ERROR_NOT_FOUND;
			return result;
		}

		dbutil::Value column;

		result = statement->GetValue(0, &column);
		if (result != 0)
			return result;
		pageId = column.type == SQLITE_INTEGER ? column.data.integer : 0;
		AppDbReadIcon(&statement, icon->get(), theme);
	}
	if (pageNo == NULL)
		return result;

	RefPtr<Statement> statement;

	result = connection->Prepare(paf::string("SELECT pageNo FROM tbl_appinfo_page WHERE pageId=?"),
		&statement);
	if (result != 0)
		return result;
	result = AppDbBindInt(&statement, 1, pageId);
	if (result != 0)
		return result;
	result = statement->Step(1);
	if (result != 0) {
		if (result == 1)
			result = LSDB_ERROR_NOT_FOUND;
		return result;
	}

	dbutil::Value column;

	result = statement->GetValue(0, &column);
	if (result == 0)
		*pageNo = column.type == SQLITE_INTEGER ? column.data.integer : 0;
	return result;
}

/*
 * A title's icon (see AppDbGetTitleIconOnly); with theme, a title without one takes the icon of
 * the title its BOOT_INSTALL_DIR names.
 */
int AppDbFindTitleIcon(ActivityDbConnection *connection, const paf::string *titleId,
	LsdbIconPtr *icon, int *pageNo, bool theme)
{
	int result = AppDbGetTitleIconOnly(connection, titleId, icon, pageNo, theme);

	if (result == LSDB_ERROR_NOT_FOUND && theme) {
		LsdbValue value;

		result = AppDbGetAppInfoValue(connection, titleId, LSDB_KEY_BOOT_INSTALL_DIR, &value);
		if (result == 0) {
			paf::string installTitleId(LsdbValueText(value));

			result = AppDbGetTitleIconOnly(connection, &installTitleId, icon, pageNo,
				theme);
		}
	}
	return result;
}

/*
 * The SELECT of the titles last launched: those of any of count categories (parameters 4 on),
 * by their last launch, latest first, at most parameter 3.
 */
static __attribute__((noinline)) paf::string AppDbLatestTitlesSql(const char **values,
	unsigned int count)
{
	paf::string sql("SELECT a1.titleId FROM tbl_appinfo a1 INNER JOIN tbl_appinfo a2 ON "
		"a1.titleId = a2.titleId AND a2.key=?1 WHERE a1.key=?2 ");

	if (count != 0) {
		sql += "AND(";
		for (unsigned int i = 0; i < count; i++) {
			if (i != 0)
				sql.append(paf::string(" OR "));
			sql += "a1.val=?";

			char number[64];

			sce_paf_snprintf(number, sizeof(number), "%lld", (SceInt64)(i + 4));
			sql.append(paf::string(number));
		}
		sql += ") ";
	}
	sql += "ORDER BY a2.val DESC LIMIT ?3";
	return sql;
}

/* binds the latest titles' parameters: limit when positive, else no limit */
static __attribute__((noinline)) int AppDbBindLatestTitles(RefPtr<Statement> *statement,
	const char **values, unsigned int count, int limit)
{
	int result = (*statement)->ClearBindings();

	if (result == 0)
		result = (*statement)->Reset();
	if (result == 0) {
		int max = -1;

		if (limit > 0)
			max = limit;
		result = AppDbBindInt(statement, 1, LSDB_KEY_LLTIME);
		if (result == 0)
			result = AppDbBindInt(statement, 2, LSDB_KEY_CATEGORY);
		if (result == 0)
			result = AppDbBindInt(statement, 3, max);
		if (result == 0) {
			int index = 4;

			for (unsigned int i = 0; i < count; i++) {
				result = AppDbBindText(statement, index++, values[i], -1);
				if (result != 0)
					break;
			}
		}
	}
	return result;
}

/* the title IDs of the titles last launched (see AppDbLatestTitlesSql) into titleIds */
int AppDbGetTitlesByCategory(ActivityDbConnection *connection, const char **values,
	unsigned int count, int limit, paf::vector<paf::string> *titleIds)
{
	titleIds->clear();

	RefPtr<Statement> statement;
	int result = connection->Prepare(AppDbLatestTitlesSql(values, count), &statement);

	if (result != 0)
		return result;
	result = AppDbBindLatestTitles(&statement, values, count, limit);
	if (result != 0)
		return result;
	while ((result = statement->Step(1)) == 0)
		titleIds->push_back(AppDbColumnString(&statement, 0));
	if (result == 1)
		result = 0;
	return result;
}

/* binds an icon's title update: its rowid, its title ID, and the title's key (else STITLE) */
static __attribute__((noinline)) int AppDbBindIconTitle(RefPtr<Statement> *statement,
	SceInt64 rowid, const char *titleId, unsigned int key)
{
	int result;

	(*statement)->ClearBindings();
	(*statement)->Reset();
	result = AppDbBindText(statement, 1, titleId, -1);
	if (result == 0)
		result = AppDbBindText(statement, 2, titleId, -1);
	if (result == 0)
		result = AppDbBindInt(statement, 3, key);
	if (result == 0)
		result = AppDbBindInt(statement, 4, key);
	if (result == 0)
		result = AppDbBindInt(statement, 5, LSDB_KEY_STITLE);
	if (result == 0)
		result = AppDbBindInt(statement, 6, rowid);
	if (result == 0)
		result = AppDbBindText(statement, 7, titleId, -1);
	if (result == 0)
		result = AppDbBindText(statement, 8, titleId, -1);
	if (result == 0)
		result = AppDbBindInt(statement, 9, key);
	if (result == 0)
		result = AppDbBindInt(statement, 10, key);
	if (result == 0)
		result = AppDbBindInt(statement, 11, LSDB_KEY_STITLE);
	return result;
}

/* sets the title of every application's icon to the title of key, else its STITLE */
int AppDbUpdateIconTitles(ActivityDbConnection *connection, unsigned int key)
{
	RefPtr<Statement> select;
	int result = connection->Prepare(paf::string("SELECT rowid, titleId FROM tbl_appinfo_icon "
		"WHERE EXISTS(SELECT 1 FROM tbl_appinfo WHERE tbl_appinfo_icon.titleId = "
		"tbl_appinfo.titleId)"), &select);

	if (result != 0)
		return result;

	RefPtr<Statement> update;

	result = connection->Prepare(paf::string("UPDATE tbl_appinfo_icon SET title=(SELECT val "
		"FROM tbl_appinfo WHERE titleId=? AND key=(CASE WHEN EXISTS(SELECT 1 FROM tbl_appinfo "
		"WHERE titleId=? AND key=?) THEN ? ELSE ? END)) WHERE rowid=? AND title!= (SELECT val "
		"FROM tbl_appinfo WHERE titleId=? AND key=(CASE WHEN EXISTS(SELECT 1 FROM tbl_appinfo "
		"WHERE titleId=? AND key=?) THEN ? ELSE ? END)) "), &update);
	if (result != 0)
		return result;
	while ((result = select->Step(1)) == 0) {
		SceInt64 rowid = AppDbColumnInt(&select, 0);
		paf::string titleId = AppDbColumnString(&select, 1);

		result = AppDbBindIconTitle(&update, rowid, titleId.c_str(), key);
		if (result != 0)
			break;
		result = update->Step(1);
		if (result != 1)
			break;
	}
	if (result == 1)
		result = 0;
	return result;
}

/* LSDB_ERROR_NO_KEY when any of an application's values has a key of 0 */
static __attribute__((noinline)) int AppDbCheckAppInfoKeys(const LsdbAppInfoPtr *info)
{
	for (LsdbAppInfo::iterator value = (*info)->begin(); value != (*info)->end(); ++value)
		if (value->first == 0)
			return LSDB_ERROR_NO_KEY;
	return 0;
}

/* a text value's string, "" for another value */
static inline paf::string LsdbValueString(const LsdbValue &value)
{
	if (value.type == LSDB_VALUE_TEXT && value.text != NULL)
		return *value.text;
	return paf::string("");
}

/*
 * Adds an application's values (tbl_appinfo), its title ID's left out, an integer's bits in
 * mask; with replace, over the values it has. LSDB_ERROR_EXISTS when one exists without replace.
 */
int AppDbAddAppInfo(ActivityDbConnection *connection, const LsdbAppInfoPtr *info, bool replace,
	SceInt64 mask)
{
	LsdbAppInfo::iterator id = (*info)->find(LSDB_KEY_TITLE_ID);

	if (id == (*info)->end() || LsdbValueText(id->second)[0] == '\0')
		return LSDB_ERROR_NO_TITLE_ID;

	int result = AppDbCheckAppInfoKeys(info);

	if (result != 0)
		return result;

	const char *sql = "INSERT INTO tbl_appinfo(titleId, key, val) VALUES(?,?,?)";

	if (replace)
		sql = "REPLACE INTO tbl_appinfo(titleId, key, val) VALUES(?,?,?)";

	RefPtr<Statement> statement;

	result = connection->Prepare(paf::string(sql), &statement);
	if (result != 0)
		return result;
	for (LsdbAppInfo::iterator value = (*info)->begin(); value != (*info)->end(); ++value) {
		if (value == id)
			continue;
		{
			paf::string titleId = LsdbValueString(id->second);
			unsigned int key = value->first;

			result = statement->ClearBindings();
			if (result == 0)
				result = statement->Reset();
			if (result == 0)
				result = AppDbBindString(&statement, 1, &titleId);
			if (result == 0)
				result = AppDbBindInt(&statement, 2, key);
			if (result == 0)
				result = AppDbBindValue(&statement, 3, &value->second, mask);
		}
		if (result != 0)
			return result;
		do
			result = statement->Step(1);
		while (result == 0);
		if (result != 1) {
			if (result == LSDB_DBUTIL_ERROR_CONSTRAINT)
				result = LSDB_ERROR_EXISTS;
			return result;
		}
		result = 0;
	}
	return result;
}

/* deletes a title's values (tbl_appinfo); LSDB_ERROR_NOT_FOUND when it has none */
int AppDbDeleteAppInfo(ActivityDbConnection *connection, const paf::string *titleId)
{
	if (titleId->size() == 0)
		return LSDB_ERROR_NO_TITLE_ID;

	int result;

	{
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("DELETE FROM tbl_appinfo WHERE titleId=?"),
			&statement);
		if (result != 0)
			return result;
		result = AppDbBindTitleId(&statement, titleId);
		if (result != 0)
			return result;
		do
			result = statement->Step(1);
		while (result == 0);
		if (result != 1)
			return result;
	}
	result = 0;
	if (connection->GetChanges() == 0)
		result = LSDB_ERROR_NOT_FOUND;
	return result;
}

/* replaces all of an application's values with info's */
int AppDbReplaceAppInfo(ActivityDbConnection *connection, const LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator id = (*info)->find(LSDB_KEY_TITLE_ID);

	if (id == (*info)->end() || LsdbValueText(id->second)[0] == '\0')
		return LSDB_ERROR_NO_TITLE_ID;

	int result = AppDbCheckAppInfoKeys(info);

	if (result == 0) {
		paf::string titleId = LsdbValueString(id->second);

		result = AppDbDeleteAppInfo(connection, &titleId);
		if (result == 0)
			result = AppDbAddAppInfo(connection, info, false, -1);
	}
	return result;
}

/*
 * Sets an application's values (an integer's bits in mask); with update, only those of an
 * application that has values (LSDB_ERROR_NOT_FOUND otherwise).
 */
int AppDbSetAppInfo(ActivityDbConnection *connection, const LsdbAppInfoPtr *info, bool update,
	SceInt64 mask)
{
	LsdbAppInfo::iterator id = (*info)->find(LSDB_KEY_TITLE_ID);

	if (id == (*info)->end() || LsdbValueText(id->second)[0] == '\0')
		return LSDB_ERROR_NO_TITLE_ID;

	int result = AppDbCheckAppInfoKeys(info);

	if (result != 0)
		return result;
	if (update) {
		paf::string titleId = LsdbValueString(id->second);

		{
			RefPtr<Statement> statement;

			result = connection->Prepare(paf::string("SELECT count(*) FROM tbl_appinfo "
				"WHERE titleId=?"), &statement);
			if (result == 0)
				result = AppDbBindString(&statement, 1, &titleId);
			if (result == 0)
				result = statement->Step(1);
			if (result == 0) {
				dbutil::Value column;

				result = statement->GetValue(0, &column);
				if (result == 0) {
					if (column.type != SQLITE_INTEGER)
						result = LSDB_ERROR_DATABASE;
					else if ((int)column.data.integer == 0)
						result = LSDB_ERROR_NOT_FOUND;
				}
			}
		}
		if (result != 0)
			return result;
	}
	return AppDbAddAppInfo(connection, info, true, mask);
}

/* deletes the values of a title's background application, if it has one */
int AppDbDeleteBgAppInfo(ActivityDbConnection *connection, const paf::string *titleId)
{
	LsdbValue value;
	int result = AppDbGetAppInfoValue(connection, titleId, LSDB_KEY_BG_TITLE_ID, &value);

	if (result == 0) {
		paf::string bgTitleId(LsdbValueText(value));

		result = AppDbDeleteAppInfo(connection, &bgTitleId);
	}
	if (result == LSDB_ERROR_NOT_FOUND)
		result = 0;
	return result;
}

/* calls visitor with each application's values, title by title */
int AppDbForEachAppInfo(ActivityDbConnection *connection, LsdbAppInfoVisitor visitor, void *arg)
{
	RefPtr<Statement> statement;
	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return LSDB_ERROR_NO_MEMORY;

	int result = connection->Prepare(paf::string("SELECT titleId, key, val FROM tbl_appinfo "
		"ORDER BY titleId"), &statement);

	if (result != 0)
		return result;
	for (;;) {
		result = statement->Step(1);
		if (result != 0) {
			if (result == 1) {
				if (info->size() != 0)
					visitor(&info, arg);
				result = 0;
			}
			break;
		}

		dbutil::Value column;

		result = statement->GetValue(0, &column);
		if (result != 0)
			break;

		bool named = info->find(LSDB_KEY_TITLE_ID) != info->end();
		paf::string titleId;

		if (column.type == SQLITE_TEXT)
			titleId.assign(column.data.text, column.size);

		bool same = false;

		if (named)
			same = titleId == paf::string(LsdbValueText((*info)[LSDB_KEY_TITLE_ID]));
		if (!same) {
			if (info->size() != 0)
				visitor(&info, arg);
			info->clear();

			paf::string *id = new paf::string(titleId);

			info->insert(paf::pair<unsigned int, LsdbValue>(LSDB_KEY_TITLE_ID, LsdbValue(id)));
			delete id;
		}
		result = AppDbReadAppInfoRow(&statement, &info);
		if (result != 0)
			break;
	}
	return result;
}

/*
 * Sets the bits in mask of a title's value of key to bits' (an integer; the whole value for
 * text), adding the value where the title has none.
 */
int AppDbUpdateAppInfoBits(ActivityDbConnection *connection, const paf::string *titleId,
	unsigned int key, const LsdbValue *bits, SceInt64 mask)
{
	for (int i = 0; i < 3; i++)
		if (s_fixedKeys[i] == key)
			return LSDB_ERROR_NO_KEY;
	if (titleId->size() == 0)
		return LSDB_ERROR_NO_TITLE_ID;

	int result;

	if (bits->type == LSDB_VALUE_INTEGER) {
		{
			RefPtr<Statement> statement;

			result = connection->Prepare(paf::string("UPDATE tbl_appinfo SET val=((val & ?3) "
				"| ?4) WHERE titleId=?1 AND key=?2"), &statement);
			if (result == 0)
				result = AppDbBindString(&statement, 1, titleId);
			if (result == 0)
				result = AppDbBindInt(&statement, 2, key);
			if (result == 0)
				result = AppDbBindInt(&statement, 3, ~mask);
			if (result == 0)
				result = AppDbBindInt(&statement, 4,
					bits->type == LSDB_VALUE_INTEGER ? bits->integer & mask : 0);
			if (result == 0) {
				result = statement->Step(1);
				if (result == 1)
					result = 0;
				if (result == 0 && connection->GetChanges() == 0)
					result = LSDB_ERROR_NOT_FOUND;
			}
		}
		if (result != LSDB_ERROR_NOT_FOUND)
			return result;
	}

	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return LSDB_ERROR_NO_MEMORY;

	paf::string *id = new paf::string(*titleId);

	info->insert(paf::pair<unsigned int, LsdbValue>(LSDB_KEY_TITLE_ID, LsdbValue(id)));
	delete id;
	info->insert(paf::pair<unsigned int, LsdbValue>(key, *bits));
	return AppDbSetAppInfo(connection, &info, true, mask);
}

/*
 * Reads a scheme an application gives in its SUPPORT_URI ("scheme:hostpath", or "<...>" to
 * claim the scheme for itself) into uri. A title not trusted may not give a character of
 * "\"<>{|}" past the first six, an http(s) URI without a host, a "ps" or "sce" scheme, or a
 * scheme another title claimed. Returns 1 when the scheme is taken, else 0.
 */
static __attribute__((noinline)) int AppDbParseUri(ActivityDbConnection *connection,
	const paf::string *text, bool trusted, LsdbUri *uri)
{
	size_t colon = text->find(':');

	if (colon == paf::string::npos || colon == 0)
		return 0;

	size_t end = text->size();

	if (!trusted && text->find_first_of("\"<>{|}", 6) != paf::string::npos)
		return 0;

	bool claimed = false;

	if ((*text)[0] == '<')
		claimed = (*text)[text->size() - 1] == '>';

	size_t start = 0;

	if (claimed) {
		start = 1;
		end--;
	}
	uri->scheme = paf::string(*text, start, colon - start);
	uri->hostpath = paf::string(*text, colon + 1, end - colon - 1);
	if (!trusted) {
		if (uri->hostpath.size() != 0 &&
			(uri->scheme == "http" || uri->scheme == "https")) {
			if (uri->hostpath.size() < 4 || uri->hostpath[0] != '/' ||
				uri->hostpath[1] != '/' ||
				uri->hostpath.find('/', 3) == paf::string::npos)
				return 0;
		}
		if (sce_paf_strncmp(uri->scheme.c_str(), "ps", 2) == 0 ||
			sce_paf_strncmp(uri->scheme.c_str(), "sce", 3) == 0)
			return 0;

		RefPtr<Statement> statement;
		int result = connection->Prepare(paf::string("SELECT 1 FROM tbl_uri "
			"WHERE scheme=? AND (flag&?)=0"), &statement);

		if (result == 0) {
			result = AppDbBindString(&statement, 1, &uri->scheme);
			if (result == 0) {
				result = AppDbBindInt(&statement, 2, LSDB_URI_FLAG_SHARED);
				if (result == 0)
					result = statement->Step(1);
			}
		}
		if (result != 1)
			return 0;
	}
	if (!claimed)
		uri->flag |= LSDB_URI_FLAG_SHARED;
	return 1;
}

/* the titles trusted with any URI scheme */
static const char *s_uriTitles[] = {
	"PCSI00013",
};

static inline bool AppDbIsUriTitle(const char *titleId)
{
	for (int i = 0; i < sizeof(s_uriTitles) / sizeof(s_uriTitles[0]); i++)
		if (sce_paf_strcmp(titleId, s_uriTitles[i]) == 0)
			return true;
	return false;
}

/* adds the URI schemes of an application's SUPPORT_URI (a line each) to tbl_uri */
int AppDbAddUris(ActivityDbConnection *connection, LsdbAppInfoPtr *app)
{
	paf::vector<LsdbUri> uris;
	const char *titleId = sceLsdbGetName(app);
	const char *supportUri = SceLsdb_AE60C3A5(app);

	if (supportUri != NULL) {
		bool trusted = SceLsdb_41019D89(app) != 0 || AppDbIsUriTitle(titleId);
		paf::list<paf::string> lines;

		paf::common::string_util::tokenize(lines, paf::string(supportUri), 0, '\n');
		for (paf::list<paf::string>::iterator it = lines.begin(); it != lines.end(); ++it) {
			LsdbUri uri;

			if (AppDbParseUri(connection, &*it, trusted, &uri) != 0) {
				paf::vector<LsdbUri>::iterator end = uris.end();

				if (paf::find(uris.begin(), end, uri) == end)
					uris.push_back(uri);
			}
		}
	}
	if (uris.size() == 0)
		return 0;

	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("INSERT INTO tbl_uri VALUES(?,?,?,?)"),
		&statement);

	if (result != 0)
		return result;
	for (paf::vector<LsdbUri>::iterator it = uris.begin(); it != uris.end(); ++it) {
		result = statement->ClearBindings();
		if (result == 0)
			result = statement->Reset();
		if (result == 0)
			result = AppDbBindText(&statement, 1, titleId, -1);
		if (result == 0)
			result = AppDbBindInt(&statement, 2, it->flag);
		if (result == 0)
			result = AppDbBindString(&statement, 3, &it->scheme);
		if (result == 0)
			result = AppDbBindString(&statement, 4, &it->hostpath);
		if (result != 0)
			break;
		result = statement->Step(1);
		if (result == 1)
			result = 0;
		if (result != 0)
			break;
	}
	return result;
}

/* removes a title's URI schemes */
int AppDbDeleteUris(ActivityDbConnection *connection, const paf::string *titleId)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("DELETE FROM tbl_uri WHERE titleId=?"),
		&statement);

	if (result == 0) {
		result = AppDbBindString(&statement, 1, titleId);
		if (result == 0) {
			result = statement->Step(1);
			if (result == 1)
				result = 0;
		}
	}
	return result;
}

/* replaces an application's URI schemes with those of its SUPPORT_URI */
int AppDbUpdateUris(ActivityDbConnection *connection, LsdbAppInfoPtr *app)
{
	int result;

	{
		paf::string titleId(sceLsdbGetName(app));

		result = AppDbDeleteUris(connection, &titleId);
	}
	if (result == 0)
		result = AppDbAddUris(connection, app);
	return result;
}

/* replaces an application's URI schemes with those of the SUPPORT_URI the database holds */
int AppDbRefreshUris(ActivityDbConnection *connection, LsdbAppInfoPtr *app)
{
	if (SceLsdb_AE60C3A5(app) == NULL)
		return 0;

	LsdbAppInfoPtr stored(new LsdbAppInfo);
	int result;

	{
		paf::string titleId(sceLsdbGetName(app));

		result = AppDbGetAppInfo(connection, &titleId, &stored);
	}
	if (result != 0)
		return result;
	{
		paf::string titleId(sceLsdbGetName(&stored));

		result = AppDbDeleteUris(connection, &titleId);
	}
	if (result == 0)
		result = AppDbAddUris(connection, &stored);
	if (result != 0)
		return result;
	return 0;
}

/* a title that handles a URI: how much of the URI's hostpath its own matched, and its times */
struct LsdbUriTitle {
	LsdbUriTitle() : matchLength(0), ltime(0), ctime(0)
	{
	}

	__attribute__((always_inline)) ~LsdbUriTitle()
	{
	}

	paf::string titleId;
	int matchLength;		/* -1 for a hostpath that does not match */
	SceUInt64 ltime;		/* the last launch */
	SceUInt64 ctime;		/* the install */
};

static __attribute__((noinline)) int AppDbBindUriTitles(RefPtr<Statement> *statement,
	const paf::string *scheme, SceInt64 flags)
{
	int result = (*statement)->ClearBindings();

	if (result == 0)
		result = (*statement)->Reset();
	if (result == 0)
		result = AppDbBindInt(statement, 1, LSDB_KEY_LLTIME);
	if (result == 0)
		result = AppDbBindInt(statement, 2, LSDB_KEY_CTIME);
	if (result == 0)
		result = AppDbBindString(statement, 3, scheme);
	if (result == 0 && flags != 0)
		result = AppDbBindInt(statement, 4, flags);
	return result;
}

/* a time column, kept as text */
static inline SceUInt64 AppDbColumnTime(RefPtr<Statement> *statement, int column)
{
	dbutil::Value value;

	memset(&value, 0, sizeof(value));
	(*statement)->GetValue(column, &value);
	return value.type == SQLITE_TEXT ? SceActivityDb_ADFC2C4C(value.data.text) : 0;
}

/*
 * Keeps the row's title in best where it handles hostpath better: a longer hostpath prefix,
 * then a later launch, then a later install.
 */
static __attribute__((noinline)) void AppDbReadUriTitle(RefPtr<Statement> *statement,
	const paf::string *hostpath, LsdbUriTitle *best)
{
	LsdbUriTitle title;

	title.titleId = AppDbColumnString(statement, 0);
	title.ltime = AppDbColumnTime(statement, 1);
	title.ctime = AppDbColumnTime(statement, 2);
	{
		paf::string prefix = AppDbColumnString(statement, 3);

		if (prefix.size() == 0)
			title.matchLength = 0;
		else if (hostpath->size() >= prefix.size() &&
			sce_paf_memcmp(prefix.c_str(), hostpath->c_str(), prefix.size()) == 0)
			title.matchLength = prefix.size();
		else
			title.matchLength = -1;
	}
	if (title.matchLength < 0)
		return;
	if (best->matchLength < title.matchLength) {
		*best = title;
	} else if (best->matchLength == title.matchLength) {
		if (best->ltime < title.ltime) {
			*best = title;
		} else if (best->ltime == title.ltime) {
			if (best->ctime < title.ctime)
				*best = title;
			else if (best->ctime == 0)
				*best = title;
		}
	}
}

/* the title that handles a URI ("scheme:hostpath") best, of those whose flag has flags */
int AppDbGetUriTitle(ActivityDbConnection *connection, const char *uri, unsigned int flags,
	paf::string *titleId)
{
	paf::string scheme;
	paf::string hostpath;
	LsdbUriTitle best;
	bool parsed = false;

	{
		paf::string text(uri);
		size_t colon = text.find(':');

		if (colon != paf::string::npos && colon != 0) {
			scheme = paf::string(text, 0, colon);
			hostpath = paf::string(text, colon + 1, text.size() - colon - 1);
			parsed = true;
		}
	}
	if (!parsed)
		return LSDB_ERROR_INVALID_ARGUMENT;

	RefPtr<Statement> statement;
	const char *sql = "SELECT tbl_uri.titleId AS titleId, a1.val AS ltime, a2.val AS ctime, "
		"hostpath FROM tbl_uri LEFT OUTER JOIN tbl_appinfo a1 ON "
		"tbl_uri.titleId=a1.titleId AND a1.key=? LEFT OUTER JOIN tbl_appinfo a2 ON "
		"tbl_uri.titleId=a2.titleId AND a2.key=? WHERE scheme=?";

	if (flags != 0)
		sql = "SELECT tbl_uri.titleId AS titleId, a1.val AS ltime, a2.val AS ctime, "
			"hostpath FROM tbl_uri LEFT OUTER JOIN tbl_appinfo a1 ON "
			"tbl_uri.titleId=a1.titleId AND a1.key=? LEFT OUTER JOIN tbl_appinfo a2 "
			"ON tbl_uri.titleId=a2.titleId AND a2.key=? WHERE scheme=? AND (flag&?)";

	int result = connection->Prepare(paf::string(sql), &statement);

	if (result == 0) {
		result = AppDbBindUriTitles(&statement, &scheme, flags);
		if (result == 0) {
			while ((result = statement->Step(1)) == 0)
				AppDbReadUriTitle(&statement, &hostpath, &best);
			if (result == 1) {
				if (best.titleId.size() != 0) {
					*titleId = best.titleId;
					result = 0;
				} else {
					result = LSDB_ERROR_NOT_FOUND;
				}
			}
		}
	}
	return result;
}

/* the URIs ("scheme:hostpath") the applications give for schemes, each once */
int AppDbGetSchemeUris(ActivityDbConnection *connection, const char **schemes, unsigned int count,
	paf::vector<paf::string> *uris)
{
	RefPtr<Statement> statement;

	if (schemes == NULL)
		return LSDB_ERROR_INVALID_ARGUMENT;

	int result = connection->Prepare(paf::string("SELECT hostpath FROM tbl_uri WHERE scheme=?"),
		&statement);

	if (result != 0)
		goto out;
	uris->clear();
	for (unsigned int i = 0; i < count; i++) {
		const char *scheme = schemes[i];

		if (scheme == NULL) {
			result = LSDB_ERROR_INVALID_ARGUMENT;
			goto out;
		}
		result = statement->ClearBindings();
		if (result == 0)
			result = statement->Reset();
		if (result == 0)
			result = AppDbBindText(&statement, 1, scheme, -1);
		if (result != 0)
			goto out;
		while ((result = statement->Step(1)) == 0) {
			dbutil::Value column;

			result = statement->GetValue(0, &column);
			if (result != 0)
				goto out;
			if (column.size != 0 && column.type == SQLITE_TEXT) {
				paf::string uri = paf::string(scheme) + ':' + column.data.text;

				if (paf::find(uris->begin(), uris->end(), uri) == uris->end())
					uris->push_back(uri);
			}
		}
		if (result != 1)
			goto out;
		result = 0;
	}
out:
	return result;
}

/* a text of theme.xml: its UTF-16 characters as a blob, NULL for none */
static inline int AppDbBindWString(RefPtr<Statement> *statement, int index,
	const paf::wstring *text)
{
	if (text->size() != 0) {
		dbutil::Value value;

		value.type = SQLITE_BLOB;
		value.name = NULL;
		value.size = text->size() * sizeof(wchar_t);
		value.data.integer = 0;
		value.data.blob = text->c_str();
		return (*statement)->Bind(index, &value, false);
	}
	return AppDbBindNull(statement, index);
}

/* binds a theme's record: its texts in each language from ?30 (titles) and ?60 (providers) */
static __attribute__((noinline)) int AppDbBindTheme(RefPtr<Statement> *statement,
	const paf::string *id, int type, const paf::string *packageImageFilePath,
	const paf::string *homePreviewFilePath, const paf::string *startPreviewFilePath,
	const paf::wstring *titleDefault, const paf::wstring *providerDefault, int contentVer,
	const SceUInt64 *lastModifiedTime, unsigned int size, const paf::wstring *titles,
	int titleCount, const paf::wstring *providers, int providerCount)
{
	int result = (*statement)->ClearBindings();

	if (result == 0)
		result = (*statement)->Reset();
	if (result == 0)
		result = AppDbBindString(statement, 1, id);
	if (result == 0)
		result = AppDbBindString(statement, 2, packageImageFilePath);
	if (result == 0)
		result = AppDbBindString(statement, 3, homePreviewFilePath);
	if (result == 0)
		result = AppDbBindString(statement, 4, startPreviewFilePath);
	if (result == 0)
		result = AppDbBindInt(statement, 5, contentVer);
	if (result == 0) {
		char time[30] = "";

		SceActivityDb_9A8944FC(lastModifiedTime, time, sizeof(time));
		result = AppDbBindText(statement, 6, time, -1);
	}
	if (result == 0)
		result = AppDbBindInt(statement, 7, size);
	if (result == 0)
		result = AppDbBindInt(statement, 8, type);
	if (result == 0)
		result = AppDbBindWString(statement, 9, titleDefault);
	if (result == 0)
		result = AppDbBindWString(statement, 10, providerDefault);
	if (result == 0) {
		for (int i = 0; i < titleCount; i++) {
			result = AppDbBindWString(statement, 30 + i, &titles[i]);
			if (result != 0)
				return result;
		}
		for (int i = 0; i < providerCount; i++) {
			result = AppDbBindWString(statement, 60 + i, &providers[i]);
			if (result != 0)
				return result;
		}
	}
	return result;
}

/* adds a custom theme (its texts in each language), or replaces the one of its id */
int AppDbSetTheme(ActivityDbConnection *connection, const paf::string *id, int type,
	const paf::string *packageImageFilePath, const paf::string *homePreviewFilePath,
	const paf::string *startPreviewFilePath, const paf::wstring *titleDefault,
	const paf::wstring *providerDefault, int contentVer, const SceUInt64 *lastModifiedTime,
	unsigned int size, const paf::wstring *titles, int titleCount,
	const paf::wstring *providers, int providerCount)
{
	RefPtr<Statement> statement;

	if (titles == NULL || providers == NULL || type != LSDB_THEME_TYPE_CUSTOM)
		return LSDB_ERROR_INVALID_ARGUMENT;

	int result = connection->Prepare(paf::string("REPLACE INTO tbl_theme(id, "
		"packageImageFilePath, homePreviewFilePath, startPreviewFilePath, contentVer, "
		"lastModifiedTime, size, type, titleDefault, providerDefault, title0, title1, "
		"title2, title3, title4, title5, title6, title7, title8, title9, title10, "
		"title11, title12, title13, title14, title15, title16, title17, title18, "
		"title19, title20, title21, title22, title23, title24, title25, provider0, "
		"provider1, provider2, provider3, provider4, provider5, provider6, provider7, "
		"provider8, provider9, provider10, provider11, provider12, provider13, "
		"provider14, provider15, provider16, provider17, provider18, provider19, "
		"provider20, provider21, provider22, provider23, provider24, provider25) "
		"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10,?30, ?31, ?32, ?33, ?34, ?35, "
		"?36, ?37, ?38, ?39, ?40, ?41, ?42, ?43, ?44, ?45, ?46, ?47, ?48, ?49, ?50, ?51, "
		"?52, ?53, ?54, ?55, ?60, ?61, ?62, ?63, ?64, ?65, ?66, ?67, ?68, ?69, ?70, ?71, "
		"?72, ?73, ?74, ?75, ?76, ?77, ?78, ?79, ?80, ?81, ?82, ?83, ?84, ?85)"),
		&statement);

	if (result == 0) {
		result = AppDbBindTheme(&statement, id, LSDB_THEME_TYPE_CUSTOM,
			packageImageFilePath, homePreviewFilePath, startPreviewFilePath,
			titleDefault, providerDefault, contentVer, lastModifiedTime, size, titles,
			titleCount, providers, providerCount);
		if (result == 0) {
			result = statement->Step(1);
			if (result == 1)
				result = 0;
		}
	}
	return result;
}

int AppDbDeleteTheme(ActivityDbConnection *connection, const paf::string *id)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("DELETE FROM tbl_theme WHERE id=?"),
		&statement);

	if (result == 0) {
		result = AppDbBindString(&statement, 1, id);
		if (result == 0) {
			result = statement->Step(1);
			if (result == 1)
				result = 0;
		}
	}
	return result;
}

int AppDbDeleteAllThemes(ActivityDbConnection *connection)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("DELETE FROM tbl_theme"), &statement);

	if (result == 0) {
		result = statement->Step(1);
		if (result == 1)
			result = 0;
	}
	return result;
}

int AppDbGetThemeCount(ActivityDbConnection *connection, int *count)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("SELECT count(*) FROM tbl_theme"),
		&statement);

	if (result == 0) {
		result = statement->Step(1);
		if (result == 0)
			*count = AppDbColumnInt(&statement, 0);
	}
	return result;
}

/* the themes' ids, the latest modified first */
int AppDbGetThemeIds(ActivityDbConnection *connection, paf::vector<paf::string> *ids)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("SELECT id FROM tbl_theme ORDER BY "
		"lastModifiedTime DESC"), &statement);

	if (result == 0) {
		while ((result = statement->Step(1)) == 0)
			ids->push_back(AppDbColumnString(&statement, 0));
		if (result == 1)
			result = 0;
	}
	return result;
}

/* a text of theme.xml from a blob column, empty for none */
static inline paf::wstring AppDbColumnWString(RefPtr<Statement> *statement, int column)
{
	dbutil::Value value;

	memset(&value, 0, sizeof(value));
	(*statement)->GetValue(column, &value);
	paf::wstring text;

	if (value.type == SQLITE_BLOB)
		text.assign((const wchar_t *)value.data.blob, value.size / sizeof(wchar_t));
	return text;
}

/* reads a theme's row: its title and provider in the language read, else the default ones */
static __attribute__((noinline)) void AppDbReadTheme(RefPtr<Statement> *statement,
	LsdbTheme *theme)
{
	theme->id = AppDbColumnString(statement, 0);
	theme->packageImageFilePath = AppDbColumnString(statement, 1);
	theme->homePreviewFilePath = AppDbColumnString(statement, 2);
	theme->startPreviewFilePath = AppDbColumnString(statement, 3);
	theme->contentVer = AppDbColumnInt(statement, 4);
	theme->lastModifiedTime = AppDbColumnTime(statement, 5);
	theme->size = AppDbColumnInt(statement, 6);
	theme->type = AppDbColumnInt(statement, 7);
	theme->titleDefault = AppDbColumnWString(statement, 8);
	theme->providerDefault = AppDbColumnWString(statement, 9);

	paf::wstring title = AppDbColumnWString(statement, 10);

	if (title.size() != 0)
		theme->titleDefault = title;

	paf::wstring provider = AppDbColumnWString(statement, 11);

	if (provider.size() != 0)
		theme->providerDefault = provider;
}

/* a theme's record, its title and provider in language (titleN, providerN) */
int AppDbGetTheme(ActivityDbConnection *connection, const paf::string *id, int language,
	LsdbTheme *theme)
{
	RefPtr<Statement> statement;
	char title[16];
	char provider[16];

	sce_paf_snprintf(title, sizeof(title), ", title%d ", language);
	sce_paf_snprintf(provider, sizeof(provider), ", provider%d ", language);

	paf::string sql("SELECT id, packageImageFilePath, homePreviewFilePath, "
		"startPreviewFilePath, contentVer, lastModifiedTime, size, type, titleDefault, "
		"providerDefault");

	sql += title;
	sql += provider;
	sql += "FROM tbl_theme WHERE id=?";

	int result = connection->Prepare(sql, &statement);

	if (result == 0) {
		result = AppDbBindString(&statement, 1, id);
		if (result == 0) {
			result = statement->Step(1);
			if (result == 0)
				AppDbReadTheme(&statement, theme);
			else if (result == 1)
				result = LSDB_ERROR_NOT_FOUND;
		}
	}
	return result;
}

/* the themes' bytes in all, or the error */
SceInt64 AppDbGetThemesSize(ActivityDbConnection *connection)
{
	RefPtr<Statement> statement;
	SceInt64 size = 0;
	int result = connection->Prepare(paf::string("SELECT sum(size) FROM tbl_theme"),
		&statement);

	if (result == 0) {
		result = statement->Step(1);
		if (result == 0)
			size = AppDbColumnInt(&statement, 0);
	}
	return result != 0 ? result : size;
}
