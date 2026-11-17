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
#include <thread/thread.h>
#include "livespace_db.h"
#include "lsdb_keys.h"
#include <activity_db/activity_db.h>

/* SceLsdb's errors (no SDK header names them) */
#define LSDB_ERROR_INVALID_ARGUMENT	0x80101001
#define LSDB_ERROR_NOT_READY		0x80101003	/* no database handle, or none opened */
#define LSDB_ERROR_NO_MEMORY		0x80101002
#define LSDB_ERROR_NO_TITLE_ID		0x80101009	/* an application without its title ID */
#define LSDB_ERROR_NOT_FOUND		0x80101005
#define LSDB_ERROR_REBUILD		0x801010F3	/* the database was rebuilt */
#define LSDB_ERROR_PAGE_RANGE		0x8010100B	/* a page number past the last page */
#define LSDB_ERROR_POSITION_USED	0x8010100C	/* an icon where the page has one */
#define LSDB_ERROR_EXISTS		0x80101004	/* a page deleted with its icons, a row inserted twice */
#define LSDB_ERROR_NO_KEY		0x80101011	/* a key of 0, or one a bits update may not set */

/* dbutil's error for SQLite's SQLITE_CONSTRAINT */
#define LSDB_DBUTIL_ERROR_CONSTRAINT	0x80103213

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

	/* a text value of its own */
	LsdbValue(const char *text) : type(LSDB_VALUE_TEXT), text(new paf::string(text))
	{
	}

	LsdbValue(const LsdbValue &other) : type(other.type)
	{
		if (type == LSDB_VALUE_INTEGER)
			integer = other.integer;
		else if (type == LSDB_VALUE_TEXT)
			text = other.text != NULL ? new paf::string(*other.text) : new paf::string("");
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
	int reserved;
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
struct LsdbThemeInfo {
	paf::string path;
	int reserved;
	HomeProperty m_homeProperty;
};

#define LSDB_SET_PAGE_THEME_ICONS	0x1
#define LSDB_SET_PAGE_THEME_PAGES	0x2

struct LsdbDbBlob;

typedef void (*LsdbAppInfoListener)(int event, void *arg, unsigned int userData);
typedef void (*LsdbAppInfoVisitor)(LsdbAppInfoPtr *info, void *arg);

/* the application database's queries (appdb.cpp), each on the connection of a transaction */
int AppDbCreateTables(ActivityDbConnection *connection, int version);
int AppDbSetConfigString(ActivityDbConnection *connection, int key, const char *value);
int AppDbUpdateConfigBits(ActivityDbConnection *connection, int key, SceInt64 bits, SceInt64 mask);
paf::string AppDbGetConfigString(ActivityDbConnection *connection, int key, const char *defaultValue);
SceInt64 AppDbGetConfigInt64(ActivityDbConnection *connection, int key, SceInt64 defaultValue);
int AppDbGetIcons(ActivityDbConnection *connection, int pageNo, unsigned int count,
	LsdbPageIconLists *pages);
int AppDbGetPage(ActivityDbConnection *connection, int pageNo, unsigned int count, LsdbPageList *pages);
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
int AppDbUpdatePage(ActivityDbConnection *connection, paf::vector<int> *pageNos, LsdbPageList *pages);
int AppDbAddPage(ActivityDbConnection *connection, int pageNo, LsdbIconList *icons, LsdbPage *page,
	int create);
int AppDbDeletePage(ActivityDbConnection *connection, int pageNo);
int AppDbIsNoPosition(int pageNo, int pos);
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

int AppDbDeleteThemeOf(ActivityDbConnection *connection, const paf::string *titleId);
int AppDbDeleteAppInfo(ActivityDbConnection *connection, const paf::string *titleId);
int AppDbDeleteUris(ActivityDbConnection *connection, const paf::string *titleId);
int AppDbGetPageCount(ActivityDbConnection *connection, int *count);
int AppDbCountIcons(ActivityDbConnection *connection, int pageNo, unsigned int pageCount,
	int *count);
int AppDbCountUserIcons(ActivityDbConnection *connection, int *count);
int AppDbFindFolders(ActivityDbConnection *connection, const paf::string *title,
	paf::vector<int> *folderIds);
int AppDbGetAppInfo(ActivityDbConnection *connection, const paf::string *titleId, LsdbAppInfoPtr *info);
int AppDbHasAppInfo(ActivityDbConnection *connection, const paf::string *titleId);
int AppDbSetAppInfoValue(ActivityDbConnection *connection, const paf::string *titleId,
	unsigned int key, LsdbValue *value, bool replace);
int AppDbReplaceAppInfo(ActivityDbConnection *connection, const LsdbAppInfoPtr *info);
int AppDbMergeAppInfo(ActivityDbConnection *connection, LsdbAppInfoPtr *app, bool main);
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
int SceLsdb_2AB5A403(const char *titleId, const LsdbIconLocation *location);
int SceLsdb_2DF6EA05(const char *titleId);
int SceLsdb_30900530(void);
int SceLsdb_E05F71CB(void);
int SceLsdb_A92420DA(int added, int pageNo);
int SceLsdb_6E4B6EBF(int folderId, int *pageNo, int *pos, const char *title);
unsigned int SceLsdb_63C29E70(int which);

/* an application's values: its title ID, its SUPPORT_URI text, whether it is a "gda" title */
const char *sceLsdbGetName(const LsdbAppInfoPtr *app);
const char *SceLsdb_AE60C3A5(const LsdbAppInfoPtr *app);
int SceLsdb_41019D89(const LsdbAppInfoPtr *app);
}

/* a blob of a database row, read through its own transaction (dbblob.cpp) */
typedef int (*LsdbDbBlobTransaction)(int op, void *arg, int reserved,
	ActivityDbConnection **connection);
paf::common::SharedPtr<LsdbDbBlob> LsdbDbBlobOpen(const char *database, const char *table,
	const char *column, SceInt64 rowid, int flags, LsdbDbBlobTransaction transaction, void *arg,
	int *result);

/* the shell's registry settings (shell_config.cpp) */
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
	int GetIconImage(const SceInt64 *rowid, paf::common::SharedPtr<LsdbDbBlob> *image);
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
	int SetAppInfoValue(const paf::string *titleId, unsigned int key, LsdbValue *value,
		bool replace);
	int GetFolderIcon(const int *folderId, int *pageNo, const LsdbIconPtr *icon);
	void NotifyListeners(int event, LsdbAppInfoPtr *info);
	int RegisterApp(LsdbAppInfoPtr *app, int pageNo, const LsdbIconPtr *icon,
		paf::vector<LsdbAppInfoPtr> *related);
	int UnregisterApp(const paf::string *titleId, bool removeFolder, int *pageNo);
	int UpdateApp(LsdbAppInfoPtr *app, const LsdbIconPtr *icon, paf::vector<LsdbAppInfoPtr> *related);
	int RefreshApp(LsdbAppInfoPtr *app, const LsdbIconPtr *icon, unsigned int fields,
		paf::vector<LsdbAppInfoPtr> *related, bool notify);
	int UpdateAppInfoBits(const paf::string *titleId, unsigned int key, const LsdbValue *bits, SceInt64 mask, bool notify);
	int ForEachAppInfo(LsdbAppInfoVisitor visitor, void *arg);
	int UpdateIconTitles(int language);
	int FindTitles(LsdbQueryCondition *conditions, paf::vector<paf::string> *titleIds,
		unsigned int flags);
	int GetTitlesByCategory(const char **categories, unsigned int count, int limit,
		paf::vector<paf::string> *titleIds);
	int GetUriTitle(const char *uri, paf::string *titleId, unsigned int flags);
	int GetSchemeUris(const char **schemes, unsigned int count, paf::vector<paf::string> *uris);
	int SetPageTheme(const paf::common::SharedPtr<LsdbThemeInfo> *theme, unsigned int flags);
	int SetTheme(const LsdbTheme *theme, const LsdbThemeText *title, const LsdbThemeText *provider);
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
	int GetIconImage(const SceInt64 *rowid, paf::common::SharedPtr<LsdbDbBlob> *image);
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
	int SetAppInfoValue(const paf::string *titleId, unsigned int key, LsdbValue *value,
		bool replace);
	int GetFolderIcon(const int *folderId, int *pageNo, const LsdbIconPtr *icon);
	int RegisterApp(LsdbAppInfoPtr *app, int pageNo, const LsdbIconPtr *icon,
		paf::vector<LsdbAppInfoPtr> *related);
	int UnregisterApp(const paf::string *titleId, bool removeFolder, int *pageNo);
	int UpdateApp(LsdbAppInfoPtr *app, const LsdbIconPtr *icon, paf::vector<LsdbAppInfoPtr> *related);
	int RefreshApp(LsdbAppInfoPtr *app, const LsdbIconPtr *icon, unsigned int fields,
		paf::vector<LsdbAppInfoPtr> *related);
	int UpdateAppInfoBits(const paf::string *titleId, unsigned int key, const LsdbValue *bits, SceInt64 mask);
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
	int SetTheme(const LsdbTheme *theme, const LsdbThemeText *title, const LsdbThemeText *provider);
	int DeleteTheme(const paf::string *id);
	int DeleteAllThemes();
	int GetThemeCount();
};

/* the client the exports are methods of */
struct AppInfoSClient {
	AppInfoSClientImpl *m_impl;
};

#endif
