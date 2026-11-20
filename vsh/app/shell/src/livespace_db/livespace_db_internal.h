/*
 * SceLsdb (vs0:vsh/shell/livespace_db.suprx): the shell's LiveSpace database. It keeps the
 * application information database (ur0:shell/db/app.db: tbl_appinfo, tbl_appinfo_page,
 * tbl_appinfo_icon), the LiveArea and new-event tables, and the LiveArea templates.
 *
 * Built by Sony with SNC 3.57 in C++ on paf. No debug record or string names its files or
 * classes; Sony's object names (sony-mine) name the daemons: SceLsdbAppInfoDaemon,
 * SceLsdbLiDaemon, SceLsdbNeDaemon, SceLsdbDbBlob, SceShellLsdbAppInfoSClientImpl,
 * SceShellLsdbNewEvtAccImpl. Class, member and file names are ours, after those.
 */
#ifndef LIVESPACE_DB_INTERNAL_H
#define LIVESPACE_DB_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/map>
#include <std/vector>
#include <std/algorithm>
#include <common/shared_ptr.h>
#include <file/file.h>
#include <rtc/rtc.h>
#include <thread/thread.h>
#include <plugin.h>
#include "livespace_db.h"
#include "lsdb_keys.h"
#include "livearea.h"
#include <activity_db/activity_db.h>
#include <activity_db/activity_db_internal.h>

/* SceLsdb's errors (no SDK header names them) */
#define LSDB_ERROR_INVALID_ARGUMENT	0x80101001
#define LSDB_ERROR_NOT_READY		0x80101003	/* no database handle, or none opened */
#define LSDB_ERROR_NO_MEMORY		0x80101002
#define LSDB_ERROR_NO_TITLE_ID		0x80101009	/* an application without its title ID */
#define LSDB_ERROR_NOT_FOUND		0x80101005
#define LSDB_ERROR_REBUILD		0x801010F3	/* the database was rebuilt */
#define LSDB_ERROR_PAGE_RANGE		0x8010100B	/* a page number past the last page */
#define LSDB_ERROR_POSITION_USED	0x8010100C	/* an icon where the page has one */
/* a page deleted with its icons, a row inserted twice */
#define LSDB_ERROR_EXISTS		0x80101004
/* a key of 0, or one a bits update may not set */
#define LSDB_ERROR_NO_KEY		0x80101011

/* dbutil's error for SQLite's SQLITE_CONSTRAINT */
#define LSDB_DBUTIL_ERROR_CONSTRAINT	0x80103213
/* the error SceLsdb gives for an UPDATE whose step failed (dbutil's range) */
#define LSDB_ERROR_UPDATE_FAILED	0x80103202

/* the page numbers: the pages from 0, the folders' pages in a range of their own, and the page
   of the icons no page shows */
#define LSDB_FOLDER_ID_MIN		(-20000000)
#define LSDB_FOLDER_ID_END		(-10000000)
#define LSDB_IS_FOLDER_ID(id)		((id) >= LSDB_FOLDER_ID_MIN && (id) < LSDB_FOLDER_ID_END)
#define LSDB_PAGE_NO_FOLDERS		(-100000000)

/* a page's reserved01 without a theme: the font color 0xFFFFFF with its shadow */
#define LSDB_PAGE_DEFAULT_RESERVED01	0x1FFFFFF

/* the columns of an icon a query writes */
#define LSDB_ICON_FIELD_TITLE			0x00000001
#define LSDB_ICON_FIELD_ICON_PATH		0x00000002
#define LSDB_ICON_FIELD_ICON0_TYPE		0x00000004
#define LSDB_ICON_FIELD_COMMAND			0x00000008
#define LSDB_ICON_FIELD_PARENTAL_LOCK_LV	0x00000010
#define LSDB_ICON_FIELD_STATUS			0x00000020
#define LSDB_ICON_FIELD_ALL			0x00FFFFFF
#define LSDB_ICON_FIELD_IMAGE			0x40000000	/* reserved05 */
#define LSDB_ICON_FIELD_THEME_ICON_PATH		0x80000000

struct ActivityDbAccess;
namespace dbutil {
class Connection;
}
typedef struct sqlite3_blob sqlite3_blob;
typedef struct sqlite3_context sqlite3_context;
typedef struct Mem sqlite3_value;

/* the records the application database hands out; laid out with their readers (appdb.cpp) */
/* a folder icon's folder */
struct LsdbFolder {
	LsdbFolder() : iconCount(0), id(0)
	{
	}

	paf::vector<paf::string> titleIds;	/* its applications on the memory card */
	int iconCount;
	int id;			/* the folder's page (its icon's reserved01) */
};

/* a page (tbl_appinfo_page) */
struct LsdbPage {
	LsdbPage() : bgColor(0), texWidth(0), texHeight(0), imageWidth(0), imageHeight(0),
		reserved01(LSDB_PAGE_DEFAULT_RESERVED01)
	{
	}

	paf::string themeFile;
	int bgColor;
	int texWidth;
	int texHeight;
	int imageWidth;
	int imageHeight;
	int reserved01;
};

typedef paf::vector<paf::common::SharedPtr<LsdbPage> > LsdbPageList;

/* an icon of a page (tbl_appinfo_icon) */
struct LsdbIcon {
	LsdbIcon() : rowid(0), pos(0), type(0), icon0Type(0), parentalLockLv(0), status(0)
	{
	}

	SceInt64 rowid;
	int pos;
	paf::string title;
	paf::string iconPath;
	int type;			/* LSDB_ICON_TYPE_* */
	paf::string titleId;
	int icon0Type;
	paf::string command;
	int parentalLockLv;
	int status;
	paf::common::SharedPtr<LsdbFolder> folder;	/* a folder icon's folder */
};

typedef paf::common::SharedPtr<LsdbIcon> LsdbIconPtr;
typedef paf::vector<LsdbIconPtr> LsdbIconList;
PAF_VECTOR_NOINLINE_PUSH_BACK(LsdbIconPtr, paf::allocator<LsdbIconPtr>)
PAF_VECTOR_NOINLINE_CLEAR(LsdbIconPtr, paf::allocator<LsdbIconPtr>)
typedef paf::common::SharedPtr<LsdbIconList> LsdbIconListPtr;

/* the icons of consecutive pages, a list per page */
typedef paf::vector<LsdbIconListPtr> LsdbPageIconLists;

#define LSDB_PAGE_ICONS		10	/* the icons a page shows */
#define LSDB_ERROR_DATABASE	0x80101000	/* a row the database could not give */

#define LSDB_ICON_TYPE_FOLDER	5

#define LSDB_VALUE_INTEGER	0
#define LSDB_VALUE_TEXT		1

/* an application parameter's value: a text (owned) or an integer */
struct LsdbValue {
	LsdbValue() : type(LSDB_VALUE_INTEGER), integer(0)
	{
	}

	/* a text value, a copy of text ("" without one) */
	LsdbValue(const paf::string *text) : type(LSDB_VALUE_TEXT),
		text(text != NULL ? new paf::string(*text) : new paf::string(""))
	{
	}

	/* a text value, a copy of text */
	LsdbValue(const paf::string &text) : type(LSDB_VALUE_TEXT), text(new paf::string(text))
	{
	}

	/* a text value of its own */
	LsdbValue(const char *text) : type(LSDB_VALUE_TEXT), text(new paf::string(text))
	{
	}

	LsdbValue(const LsdbValue &other) : type(other.type)
	{
		if (type == LSDB_VALUE_INTEGER)
			integer = other.integer;
		else if (type == LSDB_VALUE_TEXT)
			text = other.text != NULL ? new paf::string(
				*other.text) : new paf::string("");
	}

	/* sets a copy of text, or an integer */
	void SetText(const paf::string &value)
	{
		if (type == LSDB_VALUE_TEXT && text != NULL)
			*text = value;
		else
			text = new paf::string(value);
		type = LSDB_VALUE_TEXT;
	}

	void SetText(const char *value)
	{
		if (type == LSDB_VALUE_TEXT && text != NULL)
			*text = value;
		else
			text = new paf::string(value);
		type = LSDB_VALUE_TEXT;
	}

	void SetInteger(SceInt64 value)
	{
		if (type == LSDB_VALUE_TEXT) {
			delete text;
			text = NULL;
		}
		integer = value;
		type = LSDB_VALUE_INTEGER;
	}

	~LsdbValue()
	{
		if (type == LSDB_VALUE_TEXT) {
			delete text;
			text = NULL;
		}
	}

	int type;
	union {
		SceInt64 integer;
		paf::string *text;
	};
};


/* an application's parameters (tbl_appinfo), by key */
typedef paf::map<unsigned int, LsdbValue> LsdbAppInfo;

/* Sony's build inlines an application's find everywhere */
namespace paf {
template<>
inline _Tree<_Tmap_traits<unsigned int, LsdbValue, less<unsigned int> > >::iterator
_Tree<_Tmap_traits<unsigned int, LsdbValue, less<unsigned int> > >::find(const unsigned int &key)
{
	return iterator(_Find(key));
}
}
typedef paf::common::SharedPtr<LsdbAppInfo> LsdbAppInfoPtr;

/* the keys SceLsdb itself reads: FNV-1a hashes of "_org_path" and "_title_id" */
#define LSDB_KEY_ORG_PATH	0x9A9B3542u
#define LSDB_KEY_TITLE_ID	0xA1541070u
#define LSDB_KEY_BOOT_INSTALL_DIR	0xFACE5965u	/* "BOOT_INSTALL_DIR" */
#define LSDB_KEY_LLTIME		0x4C5BFEC6u	/* "#_lltime": the title's last launch */
#define LSDB_KEY_CATEGORY	0x21CA76B1u	/* "CATEGORY" */
#define LSDB_KEY_STITLE		0xEED70EC4u	/* "STITLE" */
#define LSDB_KEY_BG_TITLE_ID	0xC1C050C4u	/* "_bg_title_id" */
#define LSDB_KEY_CTIME		0xA246C97Du	/* "#_ctime": the title's install */
#define LSDB_KEY_PARENTAL_LEVEL	0x879D390Du	/* "PARENTAL_LEVEL" */
#define LSDB_KEY_TITLE		0x222641E9u	/* "TITLE" */
#define LSDB_KEY_LIVEAREA_PATH	0x9CCBEDE2u	/* "#_livearea_path" */
#define LSDB_KEY_ATTRIBUTE	0x19FAFFCFu	/* "ATTRIBUTE" */
#define LSDB_KEY_LAREA_TYPE	0x2A9A9293u	/* "LAREA_TYPE" */
#define LSDB_KEY_APP_MEMSIZE	0xDCE149FAu	/* "_app_memsize" */
#define LSDB_KEY_PROMOTE_ATTR	0xCF78B9EDu	/* "#_promote_attr" */
#define LSDB_KEY_ATTRIBUTE_MINOR	0x8FC97EB1u	/* "ATTRIBUTE_MINOR" */
#define LSDB_KEY_FG_TITLE_ID	0x308A7798u	/* "_fg_title_id" */
#define LSDB_KEY_DISC_ID	0x194D012Cu	/* "DISC_ID" */
#define LSDB_KEY_TITLE_ID_SFO	0xF960D54Fu	/* "TITLE_ID" */
#define LSDB_KEY_APPTYPE	0xFAEA72ADu	/* "_apptype" */
#define LSDB_KEY_SELF_PATH	0xB4232566u	/* "_self_path" */
#define LSDB_KEY_PHY_MEMSIZE	0xAA162102u	/* "_phy_memsize" */
#define LSDB_KEY_EBOOT_ATTR	0xEA522F51u	/* "_eboot_attr" */
#define LSDB_KEY_APP_VER	0xBCD7161Eu	/* "APP_VER" */
#define LSDB_KEY_CONTENT_ID	0x4D6FEFCAu	/* "CONTENT_ID" */
#define LSDB_KEY_NP_COMMUNICATION_ID	0x10C1A2E0u	/* "NP_COMMUNICATION_ID" */
#define LSDB_KEY_SYSTEM_VER	0xE2327E05u	/* "_system_ver" */
#define LSDB_KEY_GAMEDATA_ID	0x924BC88Du	/* "GAMEDATA_ID" */
#define LSDB_KEY_BOOTABLE	0x78CF5165u	/* "BOOTABLE" */
#define LSDB_KEY_CONTENT_LOCATION	0x39A02870u	/* "CONTENT_LOCATION" */
#define LSDB_KEY_SPLASH_PATH	0x6711F0ADu	/* "_splash_path" */
#define LSDB_KEY_UP_STATUS	0xF1A6E2F1u	/* "#_up_status" */
#define LSDB_KEY_SUPPORT_URI	0x8FE4356Du	/* "SUPPORT_URI" */
#define LSDB_KEY_INSTALL_DIR_SAVEDATA_ADD_1	0xC79BFEECu	/* "INSTALL_DIR_SAVEDATA_ADD_1" */

/*
 * A title query's condition: a parameter (key) compared with any of values by op; conditions
 * are chained by next. A key of 0 is skipped.
 */
struct LsdbQueryCondition {
	unsigned int key;
	paf::vector<LsdbValue> values;
	int op;				/* LSDB_QUERY_OP_* */
	SceInt64 mask;			/* LSDB_QUERY_OP_BITS: the bits compared */
	LsdbQueryCondition *next;
};

#define LSDB_QUERY_OP_EQUAL	0	/* "=?" */
#define LSDB_QUERY_OP_LIKE	1	/* " LIKE ? ESCAPE '$'": text only */
#define LSDB_QUERY_OP_NOT_EQUAL	2	/* "!=?" */
#define LSDB_QUERY_OP_BITS	3	/* "&?=?": the bits of an integer */

#define LSDB_ERROR_QUERY_TYPE	0x80101007	/* a value of the wrong type for its operator */

/* a value's text, "" when it has none */
static inline const char *LsdbValueText(const LsdbValue &value)
{
	return value.type == LSDB_VALUE_TEXT && value.text != NULL ? value.text->c_str() : "";
}

/* where an icon is: its page and position, and the folder that holds it */
struct LsdbIconLocation {
	int pageNo;
	int pos;
	int folderPageNo;
	int folderPos;
	paf::string folderTitle;
};
/* a theme's record (tbl_theme) */
struct LsdbTheme {
	LsdbTheme() : type(0), contentVer(0), lastModifiedTime(0), size(0)
	{
	}

	virtual ~LsdbTheme()
	{
	}

	paf::string id;
	int type;			/* LSDB_THEME_TYPE_CUSTOM */
	paf::string packageImageFilePath;
	paf::string homePreviewFilePath;
	paf::string startPreviewFilePath;
	paf::wstring titleDefault;
	paf::wstring providerDefault;
	int contentVer;
	SceUInt64 lastModifiedTime;	/* an rtc tick */
	int size;
};

#define LSDB_THEME_TYPE_CUSTOM	100
#define LSDB_THEME_LANGUAGES	20

/* a text of theme.xml in each of its languages (m_default, m_ja ... m_tr) */
struct LsdbThemeText {
	paf::wstring m_default;
	paf::wstring m_language[LSDB_THEME_LANGUAGES];
};

/* theme.xml's classes, by the element names Sony's serializer gives them */
struct BackgroundParam {
	unsigned int GetFontColor() const;

	paf::string m_imageFilePath;
	paf::string m_thumbnailFilePath;
	int m_waveType;
	unsigned int m_fontColor;
	bool m_fontShadow;
};

struct IconParam {
	paf::string m_iconFilePath;
	int m_pagePos;
	int m_iconPos;
};

#define LSDB_THEME_PAGES	10

struct HomeProperty {
	/* the icon of a system application (or of the power button, type 6) */
	const IconParam *GetIconParam(const char *titleId, int type) const;

	BackgroundParam m_bgParam[LSDB_THEME_PAGES];
	paf::string m_bgmFilePath;
	paf::string m_basePageFilePath;
	paf::string m_curPageFilePath;
	IconParam m_browser;
	IconParam m_video;
	IconParam m_music;
	IconParam m_ps3Link;
	IconParam m_party;
	IconParam m_trophy;
	IconParam m_map;
	IconParam m_near;
	IconParam m_hostCollabo;
	IconParam m_welcomePark;
	IconParam m_ps4Link;
	IconParam m_friend;
	IconParam m_email;
	IconParam m_calendar;
	IconParam m_store;
	IconParam m_message;
	IconParam m_parental;
	IconParam m_camera;
	IconParam m_settings;
	IconParam m_power;
};

/* the system applications a theme gives icons to; their count goes to *count */
const char *const *LsdbThemeGetSystemTitleIds(unsigned int *count);

/* an installed theme: its folder and its theme.xml */
struct StartScreenProperty {
	paf::string m_filePath;
	int m_dateColor;
	int m_dateLayout;
	int m_notifyBgColor;
	int m_notifyBorderColor;
	int m_notifyFontColor;
};

struct InfomationBarProperty {
	int m_barColor;
	int m_indicatorColor;
	int m_noticeFontColor;
	int m_noticeGlowColor;
	paf::string m_newNoticeFilePath;
	paf::string m_noNoticeFilePath;
};

struct InfomationProperty {
	InfomationProperty();

	int m_contentVer;
	LsdbThemeText m_title;
	LsdbThemeText m_provider;
	paf::string m_packageImageFilePath;
	paf::string m_startPreviewFilePath;
	paf::string m_homePreviewFilePath;
};

/* an installed theme: its folder and theme.xml (the shell's ctor 0x813BC572, 1456 bytes) */
struct LsdbThemeInfo {
	LsdbThemeInfo();

	paf::string path;
	int m_slot;				/* -1 at construction; the shell does not read it */
	HomeProperty m_homeProperty;
	StartScreenProperty m_startScreenProperty;
	InfomationBarProperty m_infomationBarProperty;
	InfomationProperty m_infomationProperty;
	paf::string m_contentPath;		/* not read by the shell or SceLsdb */
	int m_status;				/* 0 at construction; not read */
	SceUInt64 m_lastModifiedTime;		/* an rtc tick */
};

/* the layout the shell's code gives */
typedef char LsdbThemeSizeCheck[sizeof(LsdbTheme) == 104 ? 1 : -1];
typedef char LsdbThemeInfoSizeCheck[sizeof(LsdbThemeInfo) == 1456 &&
	offsetof(LsdbThemeInfo, m_startScreenProperty) == 812 &&
	offsetof(LsdbThemeInfo, m_infomationBarProperty) == 844 &&
	offsetof(LsdbThemeInfo, m_infomationProperty) == 884 &&
	offsetof(LsdbThemeInfo, m_contentPath) == 1428 &&
	offsetof(LsdbThemeInfo, m_lastModifiedTime) == 1448 ? 1 : -1];

#define LSDB_SET_PAGE_THEME_ICONS	0x1
#define LSDB_SET_PAGE_THEME_PAGES	0x2

class DbBlob;

typedef void (*LsdbAppInfoListener)(int event, void *arg, unsigned int userData);
typedef void (*LsdbAppInfoVisitor)(LsdbAppInfoPtr *info, void *arg);

/* the application database's queries (appdb.cpp), each on the connection of a transaction */
int AppDbCreateTables(ActivityDbConnection *connection, int version);
int AppDbSetConfigString(ActivityDbConnection *connection, int key, const char *value);
int AppDbUpdateConfigBits(ActivityDbConnection *connection, int key, SceInt64 bits, SceInt64 mask);
paf::string AppDbGetConfigString(ActivityDbConnection *connection, int key,
	const char *defaultValue);
SceInt64 AppDbGetConfigInt64(ActivityDbConnection *connection, int key, SceInt64 defaultValue);
int AppDbGetIcons(ActivityDbConnection *connection, int pageNo, unsigned int count,
	LsdbPageIconLists *pages);
int AppDbGetPage(ActivityDbConnection *connection, int pageNo, unsigned int count,
	LsdbPageList *pages);
int AppDbReplacePageIcons(ActivityDbConnection *connection, paf::vector<int> *pageNos,
	LsdbPageIconLists *lists);
int AppDbUpdateIcon(ActivityDbConnection *connection, int pageNo, const LsdbIconPtr *icon,
	unsigned int fields, const void *image, int imageSize);
int AppDbGetIconAt(ActivityDbConnection *connection, int pageNo, int pos, LsdbIcon *icon);
int AppDbGetFolderIcon(ActivityDbConnection *connection, int folderId, int *pageNo, LsdbIcon *icon);
int AppDbAddIcon(ActivityDbConnection *connection, int pageNo, const LsdbIconPtr *icon,
	const void *image, int imageSize);
int AppDbDeleteTitleIcons(ActivityDbConnection *connection, const paf::string *titleId);
int AppDbMoveIcons(ActivityDbConnection *connection, paf::vector<int> *fromPages,
	paf::vector<int> *fromPositions, paf::vector<int> *toPages, paf::vector<int> *toPositions);
int AppDbUpdatePage(ActivityDbConnection *connection, paf::vector<int> *pageNos,
	LsdbPageList *pages);
int AppDbAddPage(ActivityDbConnection *connection, int pageNo, LsdbIconList *icons, LsdbPage *page,
	int create);
int AppDbDeletePage(ActivityDbConnection *connection, int pageNo);
int AppDbIsNoPosition(int pageNo, int pos);
int AppDbGetTitleIconOnly(ActivityDbConnection *connection, const paf::string *titleId,
	LsdbIconPtr *icon, int *pageNo, bool theme);
int AppDbFindTitleIcon(ActivityDbConnection *connection, const paf::string *titleId,
	LsdbIconPtr *icon, int *pageNo, bool theme);
int AppDbSetAppInfo(ActivityDbConnection *connection, const LsdbAppInfoPtr *info, bool update,
	SceInt64 mask = -1);
int AppDbAddAppInfo(ActivityDbConnection *connection, const LsdbAppInfoPtr *info, bool replace,
	SceInt64 mask);
int AppDbAddUris(ActivityDbConnection *connection, LsdbAppInfoPtr *app);

/* a URI scheme an application handles (a row of tbl_uri, its title ID left out) */
struct LsdbUri {
	LsdbUri() : flag(0)
	{
	}

	SceInt64 flag;			/* LSDB_URI_FLAG_* */
	paf::string scheme;
	paf::string hostpath;
};

#define LSDB_URI_FLAG_SHARED	1	/* not in <...>: other titles may claim the scheme */

inline bool operator==(const LsdbUri &left, const LsdbUri &right)
{
	return left.scheme == right.scheme && left.hostpath == right.hostpath;
}

PAF_VECTOR_NOINLINE_PUSH_BACK(LsdbUri, paf::allocator<LsdbUri>)

/* Sony's build calls the records' find out of line */
namespace paf {
template<>
inline __attribute__((noinline)) vector<LsdbUri>::iterator
find(vector<LsdbUri>::iterator _First, vector<LsdbUri>::iterator _Last, const LsdbUri &_Val)
{
	for (; _First != _Last; ++_First)
		if (*_First == _Val)
			break;
	return _First;
}
}

int AppDbDeleteBgAppInfo(ActivityDbConnection *connection, const paf::string *titleId);
int AppDbDeleteAppInfo(ActivityDbConnection *connection, const paf::string *titleId);
int AppDbDeleteUris(ActivityDbConnection *connection, const paf::string *titleId);
int AppDbGetPageCount(ActivityDbConnection *connection, int *count);
int AppDbCountIcons(ActivityDbConnection *connection, int pageNo, unsigned int pageCount,
	int *count);
int AppDbCountUserIcons(ActivityDbConnection *connection, int *count);
int AppDbFindFolders(ActivityDbConnection *connection, const paf::string *title,
	paf::vector<int> *folderIds);
int AppDbGetAppInfo(ActivityDbConnection *connection, const paf::string *titleId,
	LsdbAppInfoPtr *info);
int AppDbHasAppInfo(ActivityDbConnection *connection, const paf::string *titleId);
int AppDbReplaceAppInfo(ActivityDbConnection *connection, const LsdbAppInfoPtr *info);
int AppDbUpdateUris(ActivityDbConnection *connection, LsdbAppInfoPtr *app);
int AppDbRefreshUris(ActivityDbConnection *connection, LsdbAppInfoPtr *app);
int AppDbUpdateTitleIcons(ActivityDbConnection *connection, const paf::string *titleId,
	const LsdbIconPtr *icon, unsigned int fields);
int AppDbUpdateAppInfoBits(ActivityDbConnection *connection, const paf::string *titleId,
	unsigned int key, const LsdbValue *bits, SceInt64 mask);
int AppDbGetAppInfoValue(ActivityDbConnection *connection, const paf::string *titleId,
	unsigned int key, LsdbValue *value);
int AppDbUpdateTypeIcons(ActivityDbConnection *connection, int type, const LsdbIconPtr *icon,
	unsigned int fields);
int AppDbGetPageCount(ActivityDbConnection *connection, int *count);
int AppDbForEachAppInfo(ActivityDbConnection *connection, LsdbAppInfoVisitor visitor, void *arg);
int AppDbUpdateIconTitles(ActivityDbConnection *connection, unsigned int key);
int AppDbFindTitles(ActivityDbConnection *connection, LsdbQueryCondition *conditions,
	paf::vector<paf::string> *titleIds, unsigned int flags);
int AppDbGetTitlesByCategory(ActivityDbConnection *connection, const char **categories,
	unsigned int count, int limit, paf::vector<paf::string> *titleIds);
int AppDbGetUriTitle(ActivityDbConnection *connection, const char *uri, unsigned int flags,
	paf::string *titleId);
int AppDbGetSchemeUris(ActivityDbConnection *connection, const char **schemes, unsigned int count,
	paf::vector<paf::string> *uris);
int AppDbSetTheme(ActivityDbConnection *connection, const paf::string *id, int type,
	const paf::string *packageImageFilePath, const paf::string *homePreviewFilePath,
	const paf::string *startPreviewFilePath, const paf::wstring *titleDefault,
	const paf::wstring *providerDefault, int contentVer, const SceUInt64 *lastModifiedTime,
	unsigned int size, const paf::wstring *titles, int titleCount,
	const paf::wstring *providers, int providerCount);
int AppDbDeleteTheme(ActivityDbConnection *connection, const paf::string *id);
int AppDbDeleteAllThemes(ActivityDbConnection *connection);
int AppDbGetThemeCount(ActivityDbConnection *connection, int *count);
int AppDbGetTheme(ActivityDbConnection *connection, const paf::string *id, int language,
	LsdbTheme *theme);
SceInt64 AppDbGetThemesSize(ActivityDbConnection *connection);
int AppDbGetThemeIds(ActivityDbConnection *connection, paf::vector<paf::string> *ids);

/* the exports of the other files the daemon calls */
extern "C" {
int SceLsdb_06CD70C5(void);
int SceLsdb_68059E87(void);
int SceLsdb_60B535FD(int source, int language);
paf::string SceLsdb_88FDC55C(int source);
int SceLsdb_2AB5A403(const char *titleId, const LsdbIconLocation *location);
int SceLsdb_2DF6EA05(const char *titleId);
int SceLsdb_30900530(void);
void SceLsdb_E05F71CB(void);
int SceLsdb_A92420DA(bool added, int pageNo);
bool SceLsdb_0746F1E9(const char *titleId, LsdbIconLocation *location);
bool SceLsdb_74DDAE58(int pageNo, int pos);
int SceLsdb_81759902(void);
bool SceLsdb_8DEDCA55(void);
void SceLsdb_DEC47CBD(bool readOnly);
int SceLsdb_6E4B6EBF(int folderId, int *pageNo, int *pos, const char *title);
unsigned int SceLsdb_63C29E70(int language);

/* an application's values: its title ID, its SUPPORT_URI text, whether it is a "gda" title */
const char *sceLsdbGetName(const LsdbAppInfoPtr *app);
/* an application's _bg_title_id text, and its subtype through the game plugin */
const char *SceLsdb_457F369E(const LsdbAppInfoPtr *app);
int SceLsdb_298134FE(const LsdbAppInfoPtr *app);
const char *SceLsdb_AE60C3A5(const LsdbAppInfoPtr *app);
bool SceLsdb_41019D89(const LsdbAppInfoPtr *app);
int sceLsdbGetSystemVersion(const LsdbAppInfoPtr *app);
int sceLsdbGetType(const LsdbAppInfoPtr *app);
const char *SceLsdb_C070E42E(const LsdbAppInfoPtr *app);
int SceLsdb_BE72A404(const char *titleId);
bool SceLsdb_69B32F6C(int value);
int SceLsdb_1673AD48(void);
}

/* 1 for a title the PSP compatibility layer runs (SceCompatForVsh) */
extern "C" int SceCompatForVsh_70997F92(const char *titleId);

/* the game plugin's interface (Plugin::GetInterface(1) of "game_plugin"), as SceLsdb uses it */
struct LsdbGamePluginInterface {
	int (*getType)(const LsdbAppInfoPtr *app, int *type);
	int (*getSubType)(const LsdbAppInfoPtr *app, int *type);
	void *reserved[47];
	int (*getMode)(void);
};

/* the game plugin's interface, NULL without the plugin */
static inline LsdbGamePluginInterface *LsdbGamePlugin(void)
{
	paf::Plugin *plugin = paf::Plugin::Find("game_plugin");

	if (plugin == NULL)
		return NULL;
	return (LsdbGamePluginInterface *)plugin->GetInterface(1);
}

/* SceLsdb_C36501F7 without telling the listeners (appinfo_daemon_api.cpp) */
int LsdbUpdateAppInfoBits(const paf::string *titleId, unsigned int key, const LsdbValue *bits,
	SceInt64 mask);

/*
 * A blob of a database row, read and written as a paf::File (dbblob.cpp). With a transaction
 * function, each access begins a transaction (op 0) for the blob and ends it (op 1);
 * readOnly says the transaction has nothing to commit. Without one, the blob stays open.
 */
#define DB_BLOB_TRANSACTION_BEGIN	0
#define DB_BLOB_TRANSACTION_END		1
#define DB_BLOB_FILE_TYPE		0x102	/* its paf::File type */

typedef int (*DbBlobTransaction)(int op, bool readOnly, void *arg,
	ActivityDbConnection **connection);

class DbBlob : public paf::File {
public:
	/* what Open takes: the blob's database, table, column and row */
	class OpenArg : public paf::File::OpenArg {
	public:
		OpenArg(const char *database, const char *table, const char *column,
			SceInt64 rowid, int flags, DbBlobTransaction transaction, void *arg) :
			database(database), table(table), column(column), rowid(rowid),
			flags(flags), transaction(transaction), arg(arg)
		{
			file_type = DB_BLOB_FILE_TYPE;
		}

		paf::string database;
		paf::string table;
		paf::string column;
		SceInt64 rowid;
		int flags;			/* 1: writable */
		DbBlobTransaction transaction;
		void *arg;			/* the transaction's */
	};

	DbBlob() : m_mutex("SceLsdbDbBlob", 0), m_arg(NULL), m_connection(NULL), m_blob(NULL),
		m_offset(0)
	{
	}

	virtual ~DbBlob();
	virtual int32_t GetFileType() const;
	virtual int32_t GetCapability() const;
	virtual int64_t GetFileSize() const;
	virtual int32_t Open(const paf::File::OpenArg *param);
	virtual int32_t OpenAsync(const paf::File::OpenArg *param);
	virtual int32_t Close();
	virtual int32_t CloseAsync();
	virtual bool IsOpened() const;
	virtual bool IsEof();
	virtual int32_t Abort();
	virtual int32_t Read(void *buf, size_t nbyte);
	virtual int32_t ReadAsync(void *buf, size_t nbyte);
	virtual int32_t Write(const void *buf, size_t nbyte);
	virtual int32_t WriteAsync(const void *buf, size_t nbyte);
	virtual int64_t Seek(int64_t offset, int32_t whence);
	virtual int32_t SeekAsync(int64_t offset, int32_t whence);
	virtual int32_t Flush();
	virtual int32_t FlushAsync();
	virtual int32_t WaitAsync(int64_t *result);
	virtual int32_t PollAsync(int64_t *result);

private:
	int BeginTransaction(bool readOnly);
	void EndTransaction(bool readOnly);

	paf::thread::RMutex m_mutex;
	OpenArg *m_arg;			/* NULL while closed */
	ActivityDbConnection *m_connection;
	sqlite3_blob *m_blob;		/* NULL outside a transaction */
	int m_offset;
};

paf::common::SharedPtr<DbBlob> DbBlobOpen(const char *database, const char *table,
	const char *column, SceInt64 rowid, int flags, DbBlobTransaction transaction, void *arg,
	int *result);

/* the registry settings the database reads (dbblob.cpp) */
int LsdbGetAppDbRebuildFlag(void);
int LsdbSetAppDbRebuildFlag(int flag);
int LsdbGetRegion(void);

/* the application database's path */
extern const char *g_lsdbAppDbPath;

/*
 * The application information daemon (Sony's SceLsdbAppInfoDaemon): one per process, made on
 * first use. Every request takes the activity database's connection for one transaction, runs
 * the query (appdb.cpp) and ends the transaction with the query's result.
 */
class AppInfoDaemon {
public:
	AppInfoDaemon();
	static AppInfoDaemon *GetInstance();
	static int Upgrade(ActivityDbConnection *connection, int version);

	int Open(ActivityDbAccess *access, int result);
	int Close();
	int AddListener(LsdbAppInfoListener listener, void *userData);
	void RemoveListener(void *userData);
	int GetOpenResult();
	paf::string GetConfigString(int key, const char *defaultValue);
	int SetConfigString(int key, const char *value);
	SceInt64 GetConfigInt64(int key, SceInt64 defaultValue);
	int UpdateConfigBits(int key, SceInt64 bits, SceInt64 mask);
	ActivityDbConnection *BeginTransaction();
	int EndTransaction(int state);
	int GetIcons(int pageNo, unsigned int count, LsdbPageIconLists *pages);
	int GetPage(int pageNo, unsigned int flags, LsdbPageList *pages);
	int ReplacePageIcons(paf::vector<int> *pageNos, LsdbPageIconLists *lists);
	int UpdateIcon(int pageNo, const LsdbIconPtr *icon, unsigned int fields, const void *image,
		int imageSize);
	int AddIcon(int pageNo, const LsdbIconPtr *icon);
	int DeleteTitleIcons(const paf::string *titleId);
	int MoveIcons(paf::vector<int> *fromPages, paf::vector<int> *fromPositions,
		paf::vector<int> *toPages, paf::vector<int> *toPositions);
	int GetIconImage(const SceInt64 *rowid, paf::common::SharedPtr<DbBlob> *image);
	int UpdatePage(paf::vector<int> *pageNos, LsdbPageList *pages);
	int AddPage(int pageNo, LsdbIconList *icons, LsdbPage *page);
	int DeletePage(int pageNo);
	int CreateFolder(int pageNo, const LsdbIconPtr *icon, const LsdbIconPtr *folderIcon,
		const void *image, int imageSize);
	int GetPageCount();
	int CountIcons(int pageNo, unsigned int pageCount);
	int CountUserIcons();
	int FindFolders(const paf::string *title, paf::vector<int> *folderIds);
	int GetAppInfo(const paf::string *titleId, LsdbAppInfoPtr *info);
	int GetAppInfoValue(const paf::string *titleId, unsigned int key, LsdbValue *value);
	int HasAppInfo(const paf::string *titleId);
	int FindTitleIcon(const paf::string *titleId, LsdbIconPtr *icon, int *pageNo, bool theme);
	int GetFolderIcon(const int *folderId, int *pageNo, const LsdbIconPtr *icon);
	void NotifyListeners(int event, LsdbAppInfoPtr *info);
	int RegisterApp(LsdbAppInfoPtr *app, int pageNo, const LsdbIconPtr *icon,
		paf::vector<LsdbAppInfoPtr> *related);
	int UnregisterApp(const paf::string *titleId, bool removeFolder, int *pageNo);
	int UpdateApp(LsdbAppInfoPtr *app, const LsdbIconPtr *icon,
		paf::vector<LsdbAppInfoPtr> *related);
	int RefreshApp(LsdbAppInfoPtr *app, const LsdbIconPtr *icon, unsigned int fields,
		paf::vector<LsdbAppInfoPtr> *related, bool notify);
	int UpdateAppInfoBits(const paf::string *titleId, unsigned int key, const LsdbValue *bits,
		SceInt64 mask, bool notify);
	int ForEachAppInfo(LsdbAppInfoVisitor visitor, void *arg);
	int UpdateIconTitles(int language);
	int FindTitles(LsdbQueryCondition *conditions, paf::vector<paf::string> *titleIds,
		unsigned int flags);
	int GetTitlesByCategory(const char **categories, unsigned int count, int limit,
		paf::vector<paf::string> *titleIds);
	int GetUriTitle(const char *uri, paf::string *titleId, unsigned int flags);
	int GetSchemeUris(const char **schemes, unsigned int count, paf::vector<paf::string> *uris);
	int SetPageTheme(const paf::common::SharedPtr<LsdbThemeInfo> *theme, unsigned int flags);
	int SetTheme(const LsdbTheme *theme, const LsdbThemeText *title,
		const LsdbThemeText *provider);
	int DeleteTheme(const paf::string *id);
	int DeleteAllThemes();
	int GetThemeCount();

	ActivityDbAccess *m_access;
	int m_reserved;
	paf::thread::RMutex m_mutex;
	paf::map<unsigned int, LsdbAppInfoListener> m_listeners;
};

/* the shell's client's implementation (Sony's SceShellLsdbAppInfoSClientImpl): each request
   goes to the daemon */
class AppInfoSClientImpl {
public:
	~AppInfoSClientImpl() {}

	int GetIcons(int pageNo, unsigned int count, LsdbPageIconLists *pages);
	int GetPage(int pageNo, unsigned int flags, LsdbPageList *pages);
	int ReplacePageIcons(paf::vector<int> *pageNos, LsdbPageIconLists *lists);
	int UpdateIcon(int pageNo, const LsdbIconPtr *icon, unsigned int fields, const void *image,
		int imageSize);
	int AddIcon(int pageNo, const LsdbIconPtr *icon);
	int DeleteTitleIcons(const paf::string *titleId);
	int MoveIcons(paf::vector<int> *fromPages, paf::vector<int> *fromPositions,
		paf::vector<int> *toPages, paf::vector<int> *toPositions);
	int GetIconImage(const SceInt64 *rowid, paf::common::SharedPtr<DbBlob> *image);
	int UpdatePage(paf::vector<int> *pageNos, LsdbPageList *pages);
	int AddPage(int pageNo, LsdbIconList *icons, LsdbPage *page);
	int DeletePage(int pageNo);
	int CreateFolder(int pageNo, const LsdbIconPtr *icon, const LsdbIconPtr *folderIcon,
		const void *image, int imageSize);
	int GetPageCount();
	int CountIcons(int pageNo, unsigned int pageCount);
	int CountUserIcons();
	int FindFolders(const paf::string *title, paf::vector<int> *folderIds);
	int GetAppInfo(const paf::string *titleId, LsdbAppInfoPtr *info);
	int GetAppInfoValue(const paf::string *titleId, unsigned int key, LsdbValue *value);
	int HasAppInfo(const paf::string *titleId);
	int FindTitleIcon(const paf::string *titleId, LsdbIconPtr *icon, int *pageNo, bool theme);
	int GetFolderIcon(const int *folderId, int *pageNo, const LsdbIconPtr *icon);
	int RegisterApp(LsdbAppInfoPtr *app, int pageNo, const LsdbIconPtr *icon,
		paf::vector<LsdbAppInfoPtr> *related);
	int UnregisterApp(const paf::string *titleId, bool removeFolder, int *pageNo);
	int UpdateApp(LsdbAppInfoPtr *app, const LsdbIconPtr *icon,
		paf::vector<LsdbAppInfoPtr> *related);
	int RefreshApp(LsdbAppInfoPtr *app, const LsdbIconPtr *icon, unsigned int fields,
		paf::vector<LsdbAppInfoPtr> *related);
	int UpdateAppInfoBits(const paf::string *titleId, unsigned int key, const LsdbValue *bits,
		SceInt64 mask);
	int ForEachAppInfo(LsdbAppInfoVisitor visitor, void *arg);
	int BeginTransaction();
	int EndTransaction(int state);
	int FindTitles(LsdbQueryCondition *conditions, paf::vector<paf::string> *titleIds,
		unsigned int flags);
	int GetTitlesByCategory(const char **categories, unsigned int count, int limit,
		paf::vector<paf::string> *titleIds);
	int GetUriTitle(const char *uri, paf::string *titleId, unsigned int flags);
	int GetSchemeUris(const char **schemes, unsigned int count, paf::vector<paf::string> *uris);
	paf::string GetConfigString(int key, const char *defaultValue);
	SceInt64 GetConfigInt64(int key, SceInt64 defaultValue);
	int SetConfigString(int key, const char *value);
	int UpdateConfigBits(int key, SceInt64 bits, SceInt64 mask);
	int SetPageTheme(const paf::common::SharedPtr<LsdbThemeInfo> *theme, unsigned int flags);
	int SetTheme(const LsdbTheme *theme, const LsdbThemeText *title,
		const LsdbThemeText *provider);
	int DeleteTheme(const paf::string *id);
	int DeleteAllThemes();
	int GetThemeCount();
};

/* the client the exports are methods of */
struct AppInfoSClient {
	AppInfoSClientImpl *m_impl;
};

/*
 * A listener of the daemon's application events: the callback and its argument (the shell's
 * handle on SceLsdb_BF481B29 and SceLsdb_5FCAF61C).
 */
typedef void (*LsdbEventCallback)(int event, void *info, void *arg);

struct LsdbEventListener {
	LsdbEventListener() : callback(NULL), arg(NULL)
	{
	}

	LsdbEventCallback callback;
	void *arg;
};

extern "C" {
int SceLsdb_5FCAF61C(LsdbEventListener *listener);
int SceLsdb_BF481B29(LsdbEventListener *listener, LsdbEventCallback callback, void *arg);
}

/*
 * A reader of the application database for another thread: each query runs in a transaction
 * of its own that is rolled back. It notes any change the daemon reports.
 */
class AppInfoQClientImpl {
public:
	AppInfoQClientImpl();
	~AppInfoQClientImpl();
	int Open();
	int GetAppInfo(const paf::string *titleId, LsdbAppInfoPtr *info);
	int GetAppInfoValue(const paf::string *titleId, unsigned int key, LsdbValue *value);
	int HasAppInfo(const paf::string *titleId);
	int FindTitleIcon(const paf::string *titleId, LsdbIconPtr *icon, bool theme);
	int FindTitles(LsdbQueryCondition *conditions, paf::vector<paf::string> *titleIds,
		unsigned int flags);
	int GetTitlesByCategory(const char **categories, unsigned int count, int limit,
		paf::vector<paf::string> *titleIds);
	int GetUriTitle(const char *uri, paf::string *titleId, unsigned int flags);
	int GetSchemeUris(const char **schemes, unsigned int count, paf::vector<paf::string> *uris);
	paf::string GetConfigString(int key, const char *defaultValue);
	SceInt64 GetConfigInt64(int key, SceInt64 defaultValue);
	bool IsChanged(bool clear);
	int GetThemeIds(paf::vector<paf::string> *ids);
	int GetTheme(const paf::string *id, int language, LsdbTheme *theme);
	SceInt64 GetThemesSize();

	volatile int32_t m_changed;
};

/* the reader the exports are methods of */
struct AppInfoQClient {
	AppInfoQClientImpl *m_impl;
};

/*
 * The new-event daemon (Sony's SceLsdbNeDaemon: tbl_newevent) and the LiveArea daemon
 * (SceLsdbLiDaemon: tbl_livearea and its frames), each one per process like AppInfoDaemon.
 */
/* the transaction's end that keeps nothing */
#define LSDB_TRANSACTION_ROLLBACK	-1

#define LSDB_ERROR_NO_EVENT_KEY		0x80101008	/* an event without its IDs or rowid */

/* a new event's row: by its rowid, else by its title's and item's IDs */
struct LsdbNewEventKey {
	LsdbNewEventKey() : rowid(0)
	{
	}

	paf::string titleId;		/* id_title_id */
	paf::string itemId;		/* id_item_id */
	SceInt64 rowid;
};

PAF_VECTOR_NOINLINE_PUSH_BACK(LsdbNewEventKey, paf::allocator<LsdbNewEventKey>)

/* a row of tbl_newevent */
struct LsdbNewEvent : LsdbNewEventKey {
	LsdbNewEvent()
		: msgType(256), actType(0), newFlag(false), popupNo(true), iconData(NULL),
		  iconDataSize(0), execMode(0), seId(0), seVolume(0), hash(0), notifyFlags(0)
	{
	}

	SceInt64 msgType;
	int actType;			/* act_type's low word */
	bool newFlag;
	bool popupNo;			/* to pop up */
	paf::string iconPath;		/* empty for an application's own icon0.png in vs0 */
	const void *iconData;		/* the icon_data to write; NULL when read */
	int iconDataSize;		/* length(icon_data) */
	paf::string msgArg[9];
	paf::string title;
	paf::string desc;
	int execMode;
	paf::string execTitleId;
	paf::string execArg;
	paf::string timeUpdated;	/* "YYYY-MM-DD HH:MM:SS" */
	int seId;
	float seVolume;
	int hash;
	unsigned int notifyFlags;	/* act_type's high word */
};

typedef paf::common::SharedPtr<LsdbNewEvent> LsdbNewEventPtr;
PAF_VECTOR_NOINLINE_PUSH_BACK(LsdbNewEventPtr, paf::allocator<LsdbNewEventPtr>)
PAF_VECTOR_NOINLINE_CLEAR(LsdbNewEventPtr, paf::allocator<LsdbNewEventPtr>)

/* an event's notifications (notifyFlags): all three, by default and without the accessor */
#define LSDB_NOTIFY_ALL		0x0E

/* a listener of the new-event daemon: called with its userData on each change */
typedef void (*NeDaemonListener)(void *userData);

class NeDaemon {
public:
	NeDaemon();
	static NeDaemon *Instance();
	static NeDaemon *GetInstance();
	static int Upgrade(ActivityDbConnection *connection, int version);

	int Open(ActivityDbAccess *access);
	void Close();
	void NotifyListeners();
	int ResetPopupCount();
	int ClearPopups();
	ActivityDbConnection *BeginTransaction();
	int EndTransaction(int result);
	int AddListener(NeDaemonListener listener, void *userData);
	void RemoveListener(void *userData);
	int DeleteEvents(bool all);
	unsigned int GetNotifyFlags(const LsdbNewEvent *event, SceInt64 msgType);
	int Insert(LsdbNewEvent *event, bool popup);
	int Update(LsdbNewEventPtr *events, int count, unsigned int fields, bool deleted,
		bool keepNew);
	int DeleteKeys(const LsdbNewEventKey *keys, int count);
	int Delete(const LsdbNewEventKey *keys, int count);
	int ClearNewFlags();
	int GetPopupCount();
	int DeleteTitleEvents(const char *titleId);
	int UpdateRow(ActivityDbConnection *connection, SceUInt64 changes,
		const LsdbNewEvent *event, SceInt64 *rowid, int *hash);

	/* the SQL functions tbl_newevent's triggers and inserts call */
	static void IncrementPopupNum(sqlite3_context *context, int argc,
		sqlite3_value **argv);
	static void DecrementPopupNum(sqlite3_context *context, int argc,
		sqlite3_value **argv);
	static void GetNextPopupNo(sqlite3_context *context, int argc,
		sqlite3_value **argv);

	ActivityDbAccess *m_access;
	int m_reserved;
	paf::thread::RMutex m_mutex;
	paf::map<unsigned int, NeDaemonListener> m_listeners;
	int m_eventCount;		/* -1 until counted */
	volatile int32_t m_popupCount;
};

/*
 * The new-event accessor's implementation (Sony's SceShellLsdbNewEvtAccImpl; the accessor is
 * Sony's newevent::NewEventAccessor): each request goes to the daemon.
 */
class NewEventAccessorImpl {
public:
	NewEventAccessorImpl();
	~NewEventAccessorImpl();

	int Open();
	void Close();
	static int Insert(LsdbNewEvent *event, bool popup);
	static int Update(LsdbNewEventPtr *events, int count, unsigned int fields, bool keepNew);
	static int Update(LsdbNewEventPtr *events, int count, unsigned int fields);
	static int Delete(const LsdbNewEventKey *keys, int count);
	static int DeleteAll(bool all);
	int GetEvent(LsdbNewEvent *event);
	void ClearChanged();
	int GetEvents(paf::vector<LsdbNewEventPtr> *events, int max, int *count, int *newCount,
		bool clear);
	int GetIcon(const SceInt64 *rowid, paf::common::SharedPtr<DbBlob> *icon);
	bool IsChanged();
	int GetPopup(LsdbNewEvent *event);
	int ClearPopup(const SceInt64 *rowid);
	int ClearPopups();
	int GetPopupCount();
	int UpdateNewFlags(LsdbNewEventPtr *events, int count);
	int ClearNewFlags();
	unsigned int GetNotifyFlags(const LsdbNewEvent *event);

	volatile int32_t m_changed;
};

/* the accessor the exports are methods of */
struct NewEventAccessor {
	NewEventAccessorImpl *m_impl;
};

/* SceAppMgrUser's: the path app0 of process pid is mounted from (292 bytes) */
extern "C" int sceAppMgrGetRawPathOfApp0ByPidForShell(SceUID pid, char *path);
extern "C" int sceAppMgrGetRawPath(const char *path, char *rawPath, int size);

/* a title's parental control state the LiveArea keeps (tbl_livearea's pcc columns) */
struct LiPcc {
	SceUInt64 updDate;		/* an rtc tick */
	int resStatus;			/* kept less one */
	int signOff;
};

#define LI_PCC_UPD_DATE		1
#define LI_PCC_RES_STATUS	2
#define LI_PCC_SIGN_OFF		4

/* the LiveArea's errors: no template.xml, one that cannot be read, one too large */
#define LI_ERROR_NO_TEMPLATE		0x80104003
#define LI_ERROR_INVALID_TEMPLATE	0x80104004
#define LI_ERROR_TEMPLATE_TOO_LARGE	0x80104009
#define LI_ERROR_NO_FILE		0x80104005	/* an image file not found */
#define LI_ERROR_FILES_TOO_LARGE	0x80104007	/* a template's files over LI_FILES_SIZE_MAX */
#define LI_ERROR_IMAGE_FORMAT		0x80104008	/* an image neither PNG nor GIM */
#define LI_ERROR_NO_FRAME		0x8010400A	/* no frame of that id */

/* the LiveAreas ur0: holds, a frame's userdata read back, the copy buffer of a file */
#define LI_UR0_TITLE_MAX		88
#define LI_USERDATA_MAX			1023
#define LI_COPY_BUFFER_SIZE		0x2000
#define LI_FILES_SIZE_MAX		0x100000
#define LI_RAW_PATH_SIZE		1024

/* the LiveArea database (tbl_livearea, its frames and its files): one per process */
class LiDaemon {
public:
	LiDaemon() : m_access(NULL), m_mutex("SceLsdbLiDaemon", 0)
	{
	}

	static LiDaemon *Instance();
	static LiDaemon *GetInstance();
	static int Upgrade(ActivityDbConnection *connection, int version);

	int Open(ActivityDbAccess *access);
	int Close();
	int UpdateTitles(const paf::string &titleId, int language, paf::string *path);
	int Register(const paf::string *titleId, const char *dir, int language);
	int DeleteTitle(ActivityDbConnection *connection, const paf::string *titleId);
	int Delete(const paf::string *titleId);
	int GetTemplate(paf::common::SharedPtr<LiTemplate> *tmpl, const paf::string *titleId,
			bool keepDefault);
	int GetUserdata(const paf::string *titleId, const paf::string *frameId,
			paf::string *userdata, SceUID pid);
	int StoreContents(const paf::string *titleId, paf::string *path, bool system,
			  bool network);
	int Update(const paf::string *titleId, paf::common::SharedPtr<LiTemplate> *tmpl,
		   const char *dir, bool keepStyle, paf::string *path, bool system, bool network,
		   unsigned int *mask, paf::string *storedPath);
	int UpdateFromDir(const paf::string *titleId, const char *dir, unsigned int *mask,
			  const SceUInt64 *modifiedDate, paf::string *storedPath, SceUID pid);
	int UpdateTemplate(const paf::string *titleId,
			   paf::common::SharedPtr<LiTemplate> *tmpl, const char *dir, bool keepStyle,
			   unsigned int *mask, paf::string *storedPath, SceUID pid);
	void ResetContentsPath(const paf::string *titleId, const char *path);
	int HasFunction(const paf::string *titleId, int func);
	int GetPcc(const paf::string *titleId, LiPcc *pcc);
	int SetPcc(const paf::string *titleId, bool function0, const LiPcc *pcc, int pccFlags);
	int SetFunction(const paf::string *titleId, const LiFunction *function);
	int RemoveFunction(const paf::string *titleId, int func);
	int GetModifiedDate(const paf::string *titleId, SceUInt64 *date);
	int SetModifiedDate(const paf::string *titleId, const SceUInt64 *date);
	int GetRevision(const paf::string *titleId, const char *frameId, SceInt64 *rev,
			paf::string *orgPath, SceUID pid);
	int GetUr0TitleToRemove(paf::vector<paf::string> *excluded, paf::string *titleId);

	/* add_rm_list(path): a file the database dropped, to remove once it is committed */
	static void AddRmList(sqlite3_context *context, int argc, sqlite3_value **argv);

	/* the files to remove when a transaction ends: those dropped, or added if it was rolled
	   back */
	static void RemoveFiles(int rolledBack);

	ActivityDbAccess *m_access;
	int m_reserved;
	paf::thread::RMutex m_mutex;
	paf::vector<paf::string> m_rmList;	/* the files the database dropped */
	paf::vector<paf::string> m_addList;	/* the files written for it */
};

/* the listener of application events the LiveArea keeps its titles with */
int LsdbTitleListenerInit(void);
int LsdbTitleListenerTerm(void);

/* the application database: the activity database's access object, one per process */
class LsdbDatabase {
public:
	LsdbDatabase() : m_access(NULL)
	{
	}

	static LsdbDatabase *GetInstance();
	static int Upgrade(ActivityDbConnection *connection, int version);

	int Open(int state, bool inMemory);
	int Close();
	int SetLanguage(int language);
	int GetLanguage();
	int ResetEvents();
	int GetOpenResult();
	int GetConfigText(paf::string *text);
	int SetConfigText(const paf::string *text);

	ActivityDbAccess *m_access;
};

/* the client exports the shell's theme code calls (appinfo_sclient.cpp, appinfo_qclient.cpp) */
extern "C" {
AppInfoSClient *SceLsdb_D322A870(AppInfoSClient *client);
AppInfoSClient *SceLsdb_56D8DCF0(AppInfoSClient *client);
/* the pages counted, and the icons of the pages (or of a folder, by its id) from the page
   given (PLACEHOLDER descriptions) */
int SceLsdb_94128795(AppInfoSClient *client);
int SceLsdb_D74AC354(AppInfoSClient *client, int pageNo, unsigned int count,
	LsdbPageIconLists *pages);
int SceLsdb_DB73BD7E(AppInfoSClient *client, const paf::string *titleId, unsigned int key,
	LsdbValue *value);
/* the bits of mask in an application's integer parameter set to value's */
int SceLsdb_7D7F6809(AppInfoSClient *client, const paf::string *titleId, unsigned int key,
	const LsdbValue *value, SceInt64 mask);
int SceLsdb_6F73E332(AppInfoSClient *client, const paf::common::SharedPtr<LsdbThemeInfo> *theme,
	unsigned int flags);
int SceLsdb_01790EDE(AppInfoSClient *client, const LsdbTheme *theme, const LsdbThemeText *title,
	const LsdbThemeText *provider);
int SceLsdb_54F188A5(AppInfoSClient *client, const paf::string *id);
int SceLsdb_637814FE(AppInfoSClient *client);
int SceLsdb_CF4B3684(AppInfoSClient *client, int pageNo, unsigned int pageCount);
int SceLsdb_4DB9BCB4(AppInfoSClient *client);
AppInfoQClient *SceLsdb_A6D8772C(AppInfoQClient *client);
int SceLsdb_DEF5C3F9(AppInfoQClient *client);
AppInfoQClient *SceLsdb_E2101B03(AppInfoQClient *client);
int SceLsdb_E90B81CD(AppInfoQClient *client);
int SceLsdb_76B75713(AppInfoQClient *client, LsdbQueryCondition *conditions,
	paf::vector<paf::string> *titleIds, unsigned int flags);
int SceLsdb_D239031A(AppInfoQClient *client, const paf::string *titleId, LsdbAppInfoPtr *info);
int SceLsdb_65ECEF39(AppInfoQClient *client, const paf::string *titleId, unsigned int key,
	LsdbValue *value);
SceInt64 SceLsdb_61F17FDA(AppInfoQClient *client, int key, SceInt64 defaultValue);
int SceLsdb_64DD44C2(AppInfoQClient *client, const paf::string *id, int language,
	LsdbTheme *theme);
}

#endif
