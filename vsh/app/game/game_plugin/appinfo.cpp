/*
 * The game plugin's application information: an application's livespace_db parameters, read
 * from its param.sfo and copied from its old record (the times, the live area path and the
 * shell's own keys, which a reinstalled or updated application keeps). The file's and every
 * name here are ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include <std/string>
#include <libsha256.h>
#include <rtc/rtc.h>
#include <dir/dir.h>
#include <file/file.h>
#include <file/localfile.h>
#include <vshbridge/vshbridge.h>
#include <plugin.h>
#include <common/shared_ptr.h>

#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "../../shell/plugins.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the errors (values only) */
#define GAME_ERROR_NO_PLUGIN	-2146430719	/* 0x80101101: a plugin the call needs is not loaded */
#define GAME_ERROR_BAD_DATA	-2146430716	/* 0x80101104: a file whose contents do not check */
#define GAME_ERROR_NO_VALUE	-2146430713	/* 0x80101107: a key the record does not have */
#define GAME_ERROR_CATEGORY	-2146430705	/* 0x8010110F: a category the plugin does not know */
#define GAME_ERROR_NO_INFO_MEMORY	-2146430718	/* 0x80101102: no memory for an information */
#define GAME_ERROR_NO_STORAGE	-2146430663	/* 0x80101139: a path on no storage the plugin knows */
#define GAME_ERROR_NO_DRIVE	-2146430682	/* 0x80101126: a path without a drive */
#define GAME_ERROR_NO_SFO_PARAMS	-2146430672	/* 0x80101130: a category with no param.sfo table */

/*
 * topmenu_plugin's functions an application's registration calls around applauncher_plugin's
 * (PLACEHOLDER: what they do in topmenu_plugin is not read yet)
 */
enum {
	TOPMENU_PLUGIN_FUNC_REGISTER_BEGIN	= 76,
	TOPMENU_PLUGIN_FUNC_REGISTER_END	= 77,
};

/* applauncher_plugin's functions */
enum {
	APPLAUNCHER_PLUGIN_FUNC_REGISTER	= 0,
	APPLAUNCHER_PLUGIN_FUNC_UNREGISTER	= 1,	/* PLACEHOLDER: meaning not read yet */
	APPLAUNCHER_PLUGIN_FUNC_UPDATE		= 2,	/* PLACEHOLDER: meaning not read yet */
};

/* the mount IDs of gro0: and grw0: (values only) */
#define GAME_MOUNT_ID_GRO0	0x900
#define GAME_MOUNT_ID_GRW0	0xA00

/* where the shell keeps an application's LiveArea copy */
#define GAME_APPMETA_DIR	"ur0:appmeta/"

/* the length of str, at most max, 0 without one (0x8100DEA4) */
size_t gameStrnlen(const char *str, size_t max)
{
	size_t len;

	if (str == NULL)
		return 0;

	len = 0;
	if (max != 0) {
		do {
			if (*str++ == '\0')
				break;
			len++;
		} while (len < max);
	}

	return len;
}

/* the value of key: the current time (0x8100DEC0) */
int gameAppInfoSetTimeNow(unsigned int key, LsdbAppInfoPtr *info)
{
	rtc::Tick tick;
	char time[128];
	int ret;

	ret = rtc::GetCurrentTick(&tick);
	if (ret < 0)
		return ret;

	ret = rtc::FormatSQLiteDateTime(time, sizeof(time), &tick, ' ');
	if (ret < 0)
		return ret;

	gameLsdbValueSetText((**info)[key], time);

	return 0;
}

/* the value of key: src's text, or the current time when src has none (0x8100DF5E) */
int gameAppInfoCopyTime(unsigned int key, const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst)
{
	LsdbAppInfo::iterator it = (*src)->find(key);
	int ret;

	if (it == (*src)->end()) {
		ret = gameAppInfoSetTimeNow(key, dst);
		if (ret < 0)
			return ret;
		return 0;
	}

	gameLsdbValueSetText((**dst)[key], LsdbValueText(it->second));

	return 0;
}

/* the value of key: src's (0x8100E046) */
static __attribute__((noinline)) int gameAppInfoCopyValue(unsigned int key, const LsdbAppInfoPtr *src,
							 LsdbAppInfoPtr *dst)
{
	LsdbAppInfo::iterator it = (*src)->find(key);

	if (it == (*src)->end())
		return GAME_ERROR_NO_VALUE;

	gameLsdbValueAssign((**dst)[key], it->second);

	return 0;
}

/*
 * The values of count keys: src's, for each key dst does not have yet (every key when
 * overwrite is set). A key src does not have is left alone (0x8100E178).
 */
static int gameAppInfoCopyValues(const unsigned int *keys, unsigned int count,
				 const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst,
				 bool overwrite)
{
	LsdbAppInfo::iterator end = (*dst)->end();
	unsigned int i;
	int ret;

	for (i = 0; i < count; i++, keys++) {
		if (!overwrite && (*dst)->find(*keys) != end)
			continue;

		ret = gameAppInfoCopyValue(*keys, src, dst);
		if (ret < 0 && ret != GAME_ERROR_NO_VALUE)
			return ret;
	}

	return 0;
}

/* the shell's keys (lsdb_keys.h) dst does not have yet: src's (0x8100E21C) */
int gameAppInfoCopyShellKeys(const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst)
{
	int ret;

	ret = gameAppInfoCopyValues((const unsigned int *)&s_lsdbKeys,
				    sizeof(s_lsdbKeys) / sizeof(unsigned int), src, dst, false);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * What an application keeps from its old record: its live area path, times, library location
 * and configuration, and the promotion and update states it does not have yet (0x8100E242).
 */
int gameAppInfoCopyKept(const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst)
{
	unsigned int keys[] = {
		LSDB_KEY_LIVEAREA_PATH,
		LSDB_KEY_CTIME,
		LSDB_KEY_LLTIME,
		LSDB_KEY_LIBLOCATION,
		LSDB_KEY_APPCONF,
	};
	unsigned int states[] = {
		LSDB_KEY_PROMOTE_ATTR,
		LSDB_KEY_UP_STATUS,
	};
	int ret;

	ret = gameAppInfoCopyValues(keys, sizeof(keys) / sizeof(keys[0]), src, dst, true);
	if (ret < 0)
		return ret;

	ret = gameAppInfoCopyValues(states, sizeof(states) / sizeof(states[0]), src, dst, false);
	if (ret < 0)
		return ret;

	return 0;
}

/* the promotion state (0x8100E2DA) */
int gameAppInfoSetPromoteAttr(SceUInt32 attr, LsdbAppInfoPtr *info)
{
	gameLsdbValueSetInteger((**info)[LSDB_KEY_PROMOTE_ATTR], attr);

	return 0;
}

/*
 * An application registered with applauncher_plugin. On an install (mode 1) its old LiveArea
 * path goes unless it is kept; an application whose boot attribute or LiveArea type says it has
 * no LiveArea registers without one. When the registration fails, the LiveArea copy in
 * ur0:appmeta goes (0x8100E354).
 */
int gameAppRegister(GameAppRegistration *app, int mode, bool keepLiveArea, bool notifyTopmenu,
		    const void *param)
{
	LsdbAppInfo::iterator end = app->info->end();

	if (mode == 1 && !keepLiveArea) {
		LsdbAppInfo::iterator it = app->info->find(LSDB_KEY_LIVEAREA_PATH);

		if (it != end)
			app->info->erase(it);
	}

	LsdbAppInfo::iterator it = app->info->find(LSDB_KEY_EBOOT_ATTR);

	if (it == end)
		return GAME_ERROR_NO_VALUE;
	if ((int)gameLsdbValueInteger(it->second) & 1)
		app->liveAreaState = 0;

	it = app->info->find(LSDB_KEY_LAREA_TYPE);
	if (it != end && (int)gameLsdbValueInteger(it->second) == 3)
		app->liveAreaState = 0;

	it = app->info->find(LSDB_KEY_TITLE_ID);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));

	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	if (applauncher == NULL)
		return GAME_ERROR_NO_PLUGIN;

	ShellPluginInterface *applauncherIface =
		(ShellPluginInterface *)applauncher->GetInterface(1);

	if (applauncherIface == NULL)
		return GAME_ERROR_NO_PLUGIN;

	Plugin *topmenu = Plugin::Find("topmenu_plugin");

	if (topmenu == NULL)
		return GAME_ERROR_NO_PLUGIN;

	ShellPluginInterface *topmenuIface = (ShellPluginInterface *)topmenu->GetInterface(1);

	if (topmenuIface == NULL)
		return GAME_ERROR_NO_PLUGIN;

	if (notifyTopmenu)
		((void (*)(const char *))
		 topmenuIface->func[TOPMENU_PLUGIN_FUNC_REGISTER_BEGIN])(titleId.c_str());

	app->mode = mode;

	int ret = ((int (*)(GameAppRegistration *, const void *))
		   applauncherIface->func[APPLAUNCHER_PLUGIN_FUNC_REGISTER])(app, param);

	if (ret >= 0)
		return 0;

	it = app->info->find(LSDB_KEY_TITLE_ID);
	if (it != app->info->end()) {
		string id(LsdbValueText(it->second));

		if (gameCheckTitleId(id.c_str()) == 0) {
			string dir = string(GAME_APPMETA_DIR) + id;

			gameRemoveDirectory(dir.c_str(), 0, 0, 0, true);
		}
	}

	if (notifyTopmenu)
		((void (*)(const char *, int))
		 topmenuIface->func[TOPMENU_PLUGIN_FUNC_REGISTER_END])(titleId.c_str(), 0);

	return ret;
}

/* an application taken out of applauncher_plugin (0x8100E7F2) */
int gameAppUnregister(const char *const *titleId)
{
	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	if (applauncher == NULL)
		return GAME_ERROR_NO_PLUGIN;

	ShellPluginInterface *iface = (ShellPluginInterface *)applauncher->GetInterface(1);

	if (iface == NULL)
		return GAME_ERROR_NO_PLUGIN;

	string id(*titleId);
	int ret = ((int (*)(string *))iface->func[APPLAUNCHER_PLUGIN_FUNC_UNREGISTER])(&id);

	if (ret < 0)
		return ret;

	return 0;
}

/* an application's short title (0x8100E88C) */
void gameAppInfoGetStitle(const LsdbAppInfoPtr *info, string *stitle)
{
	*stitle = sceLsdbGetStitle(info);
}

/* an application's title (0x8100E8AE) */
void gameAppInfoGetTitle(const LsdbAppInfoPtr *info, string *title)
{
	*title = sceLsdbGetTitle(info);
}

/* a path's drive, up to and with its ':' (0x8100E8D0) */
int gamePathGetDrive(const string &path, string *drive)
{
	string::size_type pos = path.find(':');

	if (pos == string::npos)
		return GAME_ERROR_NO_DRIVE;

	*drive = path.substr(0, pos + 1);

	return 0;
}

/* whether a drive is not one of the data drives, gda and wda (0x8100E988) */
bool gameDriveIsNotData(const string &drive)
{
	return drive != "gda" && drive != "wda";
}

/*
 * An application's LiveArea path, under dir: sce_sys/livearea for game data (gda, wda), from
 * the directory of the application it belongs to when it has no LiveArea of its own in appDir,
 * else livearea; with contents unless its LiveArea type says otherwise (0x8100E9C8).
 */
int gameAppInfoSetLiveAreaPath(LsdbAppInfoPtr *info, const string *appDir, const string *dir)
{
	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_ORG_PATH);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string orgPath(LsdbValueText(it->second));

	it = (*info)->find(LSDB_KEY_TITLE_ID);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));

	it = (*info)->find(LSDB_KEY_CATEGORY);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string category(LsdbValueText(it->second));
	int liveAreaType = 0;

	it = (*info)->find(LSDB_KEY_LAREA_TYPE);
	if (it != end)
		liveAreaType = (int)gameLsdbValueInteger(it->second);

	string path;

	if (category == "gda" || category == "wda") {
		string installDir;

		it = (*info)->find(LSDB_KEY_BOOT_INSTALL_DIR);
		if (it == end)
			installDir = titleId;
		else
			installDir = LsdbValueText(it->second);

		string liveArea = *appDir + string("/sce_sys/livearea");

		if (titleId == installDir || Dir::IsExist(liveArea.c_str())) {
			path = *dir + string("/sce_sys/livearea");
		} else {
			string baseDir(*dir);

			baseDir.replace(baseDir.rfind("/", string::npos, sce_paf_strlen("/")) + 1,
					titleId.size(), installDir);
			path = baseDir + string("/sce_sys/livearea");
			path = path;
		}
	} else {
		path = *dir + string("/livearea");
	}

	if (liveAreaType == 0)
		path.append(string("/contents"));

	gameLsdbValueSetText((**info)[LSDB_KEY_LIVEAREA_PATH], path);

	return 0;
}

/*
 * What bit 21 of an application's attribute and its bits 24-27 say (0x8100F2A8; PLACEHOLDER:
 * the meaning of these attribute bits is not read yet): 1, 2 or 3 for the values 0, 1 and 2,
 * 0 otherwise.
 */
int gameAppInfoGetAttributeKind(const LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_ATTRIBUTE);
	SceInt64 attr = 0;

	if (it != (*info)->end())
		attr = (int)gameLsdbValueInteger(it->second);

	if (attr & 0x200000) {
		switch ((unsigned int)((attr & 0xF000000) >> 24)) {
		case 0:
			return 1;
		case 1:
			return 2;
		case 2:
			return 3;
		}
	}

	return 0;
}

/* the categories of a game, of an EG and of an ME application (by their first four bytes) */
static const char *s_gameCategories[] = {
	"gc", "gda", "gd", "gp", "gdb", "wda", "gdc", "gpc", "gdd", "gpd",
};
static const char *s_egCategories[] = { "EG" };
static const char *s_meCategories[] = { "ME" };

/*
 * An application's kind, from its category: 1 a game, 2 EG, 3 ME, 4 mb, 5 mba (0x8100F350;
 * PLACEHOLDER: what EG, ME, mb and mba stand for is not read yet).
 */
int gameAppInfoGetCategoryKind(const LsdbAppInfoPtr *info, int *kind)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);
	unsigned int i;

	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	const char *category = LsdbValueText(it->second);

	for (i = 0; i < sizeof(s_gameCategories) / sizeof(s_gameCategories[0]); i++) {
		if (sce_paf_strncmp(category, s_gameCategories[i], 4) == 0) {
			*kind = 1;
			return 0;
		}
	}
	for (i = 0; i < sizeof(s_egCategories) / sizeof(s_egCategories[0]); i++) {
		if (sce_paf_strncmp(category, s_egCategories[i], 4) == 0) {
			*kind = 2;
			return 0;
		}
	}
	for (i = 0; i < sizeof(s_meCategories) / sizeof(s_meCategories[0]); i++) {
		if (sce_paf_strncmp(category, s_meCategories[i], 4) == 0) {
			*kind = 3;
			return 0;
		}
	}
	if (sce_paf_strncmp(category, "mb", 3) == 0) {
		*kind = 4;
		return 0;
	}
	if (sce_paf_strncmp(category, "mba", 4) == 0) {
		*kind = 5;
		return 0;
	}

	return GAME_ERROR_CATEGORY;
}

/*
 * What applauncher_plugin shows of an application: its icon (from the application it belongs to
 * for game data without one of its own), title ID, storage, content location, kind and parental
 * level (0x8100F474).
 */
int gameAppEntryFill(GameAppRegistration *app, const string *appDir, const string *dir)
{
	LsdbAppInfo::iterator end = app->info->end();
	LsdbAppInfo::iterator it = app->info->find(LSDB_KEY_ORG_PATH);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string orgPath(LsdbValueText(it->second));

	it = app->info->find(LSDB_KEY_TITLE_ID);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));

	it = app->info->find(LSDB_KEY_CATEGORY);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string category(LsdbValueText(it->second));
	string ext(".png");
	int kind;
	int ret = gameAppInfoGetCategoryKind(&app->info, &kind);

	if (ret < 0)
		return ret;
	if (kind == 2 || kind == 3)
		ext = ".dds";

	LocalFileStat stat;

	if (category == "gda" || category == "wda") {
		string installDir;

		it = app->info->find(LSDB_KEY_BOOT_INSTALL_DIR);
		if (it == end)
			installDir = titleId;
		else
			installDir = LsdbValueText(it->second);

		string icon = *appDir + string("/sce_sys/icon0") + ext;

		if (LocalFile::Getstat(icon.c_str(), &stat) == GAME_PAF_ERROR_NOT_FOUND &&
		    titleId != installDir) {
			icon = *dir;
			icon.replace(icon.rfind("/", string::npos, sce_paf_strlen("/")) + 1,
				     titleId.size(), installDir);
			app->entry->iconPath = icon + string("/sce_sys/icon0") + ext;
		} else {
			app->entry->iconPath = *dir + string("/sce_sys/icon0") + ext;
		}
	} else {
		app->entry->iconPath = *dir + string("/icon0") + ext;
	}

	app->entry->storage = category == "wda" ? 2 : 0;

	it = app->info->find(LSDB_KEY_TITLE_ID);
	if (it == end)
		return GAME_ERROR_NO_VALUE;
	app->entry->titleId = LsdbValueText(it->second);

	if (category == "wda") {
		it = app->info->find(LSDB_KEY_CONTENT_LOCATION);
		if (it != end)
			app->entry->contentLocation = LsdbValueText(it->second);
	}

	if (orgPath.find("pspemu/PSP/GAME") == string::npos) {
		app->entry->kind = 5;
	} else {
		switch (gameAppInfoGetAttributeKind(&app->info)) {
		case 1:
			app->entry->kind = 3;
			break;
		case 2:
			app->entry->kind = 2;
			break;
		case 3:
			app->entry->kind = 4;
			break;
		default:
			app->entry->kind = category == "ME";
			break;
		}
	}

	app->entry->parentalLevel = 0;
	it = app->info->find(LSDB_KEY_PARENTAL_LEVEL);
	if (it != end)
		app->entry->parentalLevel = (int)gameLsdbValueInteger(it->second);

	return 0;
}

/* whether a category is a game's: gd, gp, gdc, gpc or gdb (0x81010294) */
bool gameCategoryIsGame(const string &category)
{
	return category == "gd" || category == "gp" || category == "gdc" || category == "gpc" ||
	       category == "gdb";
}

/* the categories of a game patch */
static const char *s_patchCategories[] = { "gp", "gpc", "gpd" };

/* whether a category is a game patch's, by its first four bytes (0x81010338) */
bool gameCategoryIsPatch(const string &category)
{
	unsigned int i;

	for (i = 0; i < sizeof(s_patchCategories) / sizeof(s_patchCategories[0]); i++) {
		if (sce_paf_strncmp(category.c_str(), s_patchCategories[i], 4) == 0)
			return true;
	}

	return false;
}

/* PLACEHOLDER: whether type is one of 6, 7, 14, 15 and 16; what the values are is not read
   yet (0x8101036A) */
bool gameTypeIsSpecial(int type)
{
	return type == 6 || type == 7 || type == 14 || type == 15 || type == 16;
}

/* whether a category is ME or EG (0x81010386) */
bool gameCategoryIsEgMe(const string &category)
{
	return category == "ME" || category == "EG";
}

/* grw0: and gro0: unmounted (0x810103C6) */
void gameUnmountGameDrives(void)
{
	vshIoUmount(GAME_MOUNT_ID_GRW0, 0, 0, 0);
	vshIoUmount(GAME_MOUNT_ID_GRO0, 0, 0, 0);
}

/* the sum of values[0..last], as a percentage (0x810103E6) */
float gameSumPercent(int last, const float *values)
{
	float sum = 0.0f;
	int i;

	for (i = last + 1; i != 0; i--)
		sum += *values++;

	return sum * 100.0f;
}

/* PLACEHOLDER: what is read with the versions; the system versions under their own keys */
static GameSfoParam s_sfoVersionParams[] = {
	{ "CATEGORY", 0, GAME_SFO_FORMAT_TEXT, 4, 4, 0 },
	{ "PARENTAL_LEVEL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "TITLE_ID", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "PSP2_SYSTEM_VER", LsdbKeyHash("_system_ver"), GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "PSP2_DISP_VER", LsdbKeyHash("_disp_ver"), GAME_SFO_FORMAT_TEXT, 7, 8, 0 },
	{ "PSP2_SYSTEM_ROOT_VER", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "VERSION", 0, GAME_SFO_FORMAT_TEXT, 6, 8, 0 },
	{ "REGION_DENY", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
};

/* PLACEHOLDER: the category, attribute and application version */
static const GameSfoParam s_sfoAppVerParams[] = {
	{ "CATEGORY", 0, GAME_SFO_FORMAT_TEXT, 4, 4, 0 },
	{ "ATTRIBUTE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "APP_VER", 0, GAME_SFO_FORMAT_TEXT, 6, 8, 0 },
};

/* PLACEHOLDER: what an installation checks */
static GameSfoParam s_sfoCheckParams[] = {
	{ "CATEGORY", 0, GAME_SFO_FORMAT_TEXT, 4, 4, 0 },
	{ "ATTRIBUTE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "APP_VER", 0, GAME_SFO_FORMAT_TEXT, 6, 8, 0 },
	{ "PSP2_SYSTEM_VER", LsdbKeyHash("_system_ver"), GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "PARENTAL_LEVEL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "REGION_DENY", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
};

/* an application's parameters, all that its information keeps */
static GameSfoParam s_sfoAppParams[] = {
	{ "CATEGORY", 0, GAME_SFO_FORMAT_TEXT, 4, 4, 0 },
	{ "STITLE", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_00", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_01", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_02", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_03", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_04", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_05", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_06", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_07", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_08", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_09", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_10", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_11", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_12", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_13", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_14", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_15", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_16", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_17", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_18", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_19", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "TITLE", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_00", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_01", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_02", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_03", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_04", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_05", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_06", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_07", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_08", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_09", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_10", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_11", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_12", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_13", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_14", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_15", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_16", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_17", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_18", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_19", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "PARENTAL_LEVEL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "ATTRIBUTE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "ATTRIBUTE2", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "TITLE_ID", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_SAVEDATA", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_ADDCONT", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_ADDCONT_ADD_1", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_ADDCONT_ADD_2", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_ADDCONT_ADD_3", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_ADDCONT_ADD_4", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_ADDCONT_ADD_5", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_ADDCONT_ADD_6", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_ADDCONT_ADD_7", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_SAVEDATA_ADD_1", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_SAVEDATA_ADD_2", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_SAVEDATA_ADD_3", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_SAVEDATA_ADD_4", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_SAVEDATA_ADD_5", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_SAVEDATA_ADD_6", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_SAVEDATA_ADD_7", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "PSP2_SYSTEM_VER", LsdbKeyHash("_system_ver"), GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "PSP2_DISP_VER", LsdbKeyHash("_disp_ver"), GAME_SFO_FORMAT_TEXT, 7, 8, 0 },
	{ "APP_VER", 0, GAME_SFO_FORMAT_TEXT, 6, 8, 0 },
	{ "VERSION", 0, GAME_SFO_FORMAT_TEXT, 6, 8, 0 },
	{ "SAVEDATA_MAX_SIZE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "LAREA_TYPE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "SUPPORT_URI", 0, GAME_SFO_FORMAT_TEXT, 512, 512, 0 },
	{ "REFERER_TITLE_ID", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "INSTALL_DIR_CACHE", LsdbKeyHash("INSTALL_DIR_CACHE0"), GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "IMPORT_SAVEDATA_INFO", 0, GAME_SFO_FORMAT_TEXT, 1024, 1024, 0 },
	{ "PARENT_INSTALL_DIR", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "BOOT_INSTALL_DIR", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "CONTENT_ID", 0, GAME_SFO_FORMAT_TEXT, 48, 48, 0 },
	{ "NP_COMMUNICATION_ID", 0, GAME_SFO_FORMAT_TEXT, 13, 16, 0 },
	{ "REGION_DENY", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "CONTENT_LOCATION", 0, GAME_SFO_FORMAT_TEXT, 256, 256, 0 },
	{ "ATTRIBUTE_MINOR", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
};

/* PLACEHOLDER: the attributes and the save data import */
static const GameSfoParam s_sfoAttributeParams[] = {
	{ "CATEGORY", 0, GAME_SFO_FORMAT_TEXT, 4, 4, 0 },
	{ "ATTRIBUTE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "ATTRIBUTE2", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "TITLE_ID", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "ATTRIBUTE_MINOR", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "IMPORT_SAVEDATA_INFO", 0, GAME_SFO_FORMAT_TEXT, 1024, 1024, 0 },
};

/* the titles and the parental level */
static const GameSfoParam s_sfoTitleParams[] = {
	{ "STITLE", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_00", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_01", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_02", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_03", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_04", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_05", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_06", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_07", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_08", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_09", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_10", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_11", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_12", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_13", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_14", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_15", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_16", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_17", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_18", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "STITLE_19", 0, GAME_SFO_FORMAT_TEXT, 52, 52, 0 },
	{ "TITLE", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_00", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_01", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_02", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_03", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_04", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_05", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_06", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_07", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_08", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_09", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_10", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_11", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_12", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_13", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_14", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_15", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_16", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_17", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_18", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "TITLE_19", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "PARENTAL_LEVEL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
};

/* PLACEHOLDER: what save data checks */
static GameSfoParam s_sfoSaveDataParams[] = {
	{ "SAVEDATA_MAX_SIZE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "PSP2_DISP_VER", LsdbKeyHash("_disp_ver"), GAME_SFO_FORMAT_TEXT, 7, 8, 0 },
	{ "PSP2_SYSTEM_VER", LsdbKeyHash("_system_ver"), GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "INSTALL_DIR_SAVEDATA", 0, GAME_SFO_FORMAT_TEXT, 10, 12, 0 },
	{ "APP_VER", 0, GAME_SFO_FORMAT_TEXT, 6, 8, 0 },
};

/* the game categories again, for the test by a category's own length */
static const char *s_gameCategoryNames[] = {
	"gc", "gda", "gd", "gp", "gdb", "wda", "gdc", "gpc", "gdd", "gpd",
};

/* the keys of the localized short titles and titles, STITLE_00..19 and TITLE_00..19 */
static unsigned int s_stitleKeys[] = {
	LsdbKeyHash("STITLE_00"), LsdbKeyHash("STITLE_01"), LsdbKeyHash("STITLE_02"),
	LsdbKeyHash("STITLE_03"), LsdbKeyHash("STITLE_04"), LsdbKeyHash("STITLE_05"),
	LsdbKeyHash("STITLE_06"), LsdbKeyHash("STITLE_07"), LsdbKeyHash("STITLE_08"),
	LsdbKeyHash("STITLE_09"), LsdbKeyHash("STITLE_10"), LsdbKeyHash("STITLE_11"),
	LsdbKeyHash("STITLE_12"), LsdbKeyHash("STITLE_13"), LsdbKeyHash("STITLE_14"),
	LsdbKeyHash("STITLE_15"), LsdbKeyHash("STITLE_16"), LsdbKeyHash("STITLE_17"),
	LsdbKeyHash("STITLE_18"), LsdbKeyHash("STITLE_19"),
};
static unsigned int s_titleKeys[] = {
	LsdbKeyHash("TITLE_00"), LsdbKeyHash("TITLE_01"), LsdbKeyHash("TITLE_02"),
	LsdbKeyHash("TITLE_03"), LsdbKeyHash("TITLE_04"), LsdbKeyHash("TITLE_05"),
	LsdbKeyHash("TITLE_06"), LsdbKeyHash("TITLE_07"), LsdbKeyHash("TITLE_08"),
	LsdbKeyHash("TITLE_09"), LsdbKeyHash("TITLE_10"), LsdbKeyHash("TITLE_11"),
	LsdbKeyHash("TITLE_12"), LsdbKeyHash("TITLE_13"), LsdbKeyHash("TITLE_14"),
	LsdbKeyHash("TITLE_15"), LsdbKeyHash("TITLE_16"), LsdbKeyHash("TITLE_17"),
	LsdbKeyHash("TITLE_18"), LsdbKeyHash("TITLE_19"),
};

/* the keys of the additional contents' directories, INSTALL_DIR_ADDCONT_ADD_1..7 */
static unsigned int s_addcontDirKeys[] = {
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_1"), LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_2"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_3"), LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_4"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_5"), LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_6"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_7"),
};

/*
 * The key of language's short title: STITLE_<language>. Sony's code returns the plain key (STITLE) for
 * language -1 without storing it, which its unsigned range check never lets through (0x81010412).
 */
int gameStitleKey(unsigned int language, unsigned int *key)
{
	if (language >= 20)
		return GAME_ERROR_INVALID_ARGUMENT;
	if (language == (unsigned int)-1)
		return (int)LSDB_KEY_STITLE;

	*key = s_stitleKeys[language];

	return 0;
}

/*
 * The key of language's title: TITLE_<language>. Sony's code returns the plain key (TITLE) for
 * language -1 without storing it, which its unsigned range check never lets through (0x81010442).
 */
int gameTitleKey(unsigned int language, unsigned int *key)
{
	if (language >= 20)
		return GAME_ERROR_INVALID_ARGUMENT;
	if (language == (unsigned int)-1)
		return (int)LSDB_KEY_TITLE;

	*key = s_titleKeys[language];

	return 0;
}

/* a param.sfo's parameters loaded into an application's information (0x81010472) */
int gameSfoLoad(const string &path, const GameSfoParam *params, unsigned int count,
		LsdbAppInfoPtr *info)
{
	void *sfo = NULL;
	int size;
	int ret;

	size = LocalFile::GetFileSize(path.c_str());
	if (size < 0) {
		ret = size;
		goto out;
	}

	ret = gameFileReadAlloc(path.c_str(), 0, &sfo, size);
	if (ret < 0)
		goto out;

	ret = gameSfoLoadParams(sfo, size, params, count, info, true);
	if (ret < 0)
		goto out;

	ret = 0;
out:
	if (sfo != NULL) {
		sce_paf_free(sfo);
		sfo = NULL;
	}

	return ret;
}

/*
 * A param.sfo's parameters loaded into an application's information once the file's SHA-256
 * digest begins with digest (0x810104F6).
 */
int gameSfoLoadVerified(const string &path, const GameSfoParam *params, unsigned int count,
			LsdbAppInfoPtr *info, const char *digest, SceSize digestLength)
{
	unsigned char sum[32];
	void *sfo = NULL;
	int size;
	int ret;

	if (digest == NULL || digestLength == 0) {
		ret = GAME_ERROR_INVALID_ARGUMENT;
		goto out;
	}

	size = LocalFile::GetFileSize(path.c_str());
	if (size < 0) {
		ret = size;
		goto out;
	}

	ret = gameFileReadAlloc(path.c_str(), 0, &sfo, size);
	if (ret < 0)
		goto out;

	sce_paf_memset(sum, 0, sizeof(sum));
	ret = sceSha256Digest(sfo, size, sum);
	if (ret < 0)
		goto out;

	gameDebugDump("sceSha256Digest() digest", sum, sizeof(sum));
	if (sce_paf_strncmp(digest, (const char *)sum, digestLength) != 0) {
		ret = GAME_ERROR_BAD_DATA;
		goto out;
	}

	ret = gameSfoLoadParams(sfo, size, params, count, info, true);
	if (ret < 0)
		goto out;

	ret = 0;
out:
	if (sfo != NULL)
		sce_paf_free(sfo);

	return ret;
}

/*
 * A file written whole; on a failed write the file is removed. On either failure detail is
 * left the path's detail (0x810105D0).
 */
int gameFileWrite(const string &path, const void *buf, size_t size, string *detail)
{
	int ret = 0;
	common::SharedPtr<File> file =
		LocalFile::Open(path.c_str(), SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6, &ret);

	if (ret < 0) {
		gamePathDetail(path, detail);
		return ret;
	}

	ret = file->Write(buf, size);
	if (ret < 0) {
		file->Close();
		LocalFile::RemoveFile(path.c_str());
		gamePathDetail(path, detail);
		return ret;
	}

	file->Close();

	return 0;
}

/*
 * A file read whole into buf; on either failure detail is left the path's detail
 * (0x81010754).
 */
int gameFileRead(const string &path, void *buf, size_t size, string *detail)
{
	int ret = 0;
	common::SharedPtr<File> file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);

	if (ret < 0) {
		gamePathDetail(path, detail);
		return ret;
	}

	ret = file->Read(buf, size);
	if (ret < 0) {
		file->Close();
		gamePathDetail(path, detail);
		return ret;
	}

	file->Close();

	return 0;
}

/* topmenu_plugin told that an application's registration has ended (0x810108D2) */
int gameAppRegisterEnd(const LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_TITLE_ID);

	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));

	Plugin *topmenu = Plugin::Find("topmenu_plugin");

	if (topmenu == NULL)
		return GAME_ERROR_NO_PLUGIN;

	ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

	if (iface == NULL)
		return GAME_ERROR_NO_PLUGIN;

	((void (*)(const char *, int))iface->func[TOPMENU_PLUGIN_FUNC_REGISTER_END])(
		titleId.c_str(), 0);

	return 0;
}

/*
 * A registered application's information handed to applauncher_plugin again, topmenu_plugin told
 * the registration has ended when notifyTopmenu is set (0x810109C4).
 */
int gameAppUpdate(const LsdbAppInfoPtr *info, bool flag, bool notifyTopmenu)
{
	int ret;
	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	if (applauncher == NULL) {
		ret = GAME_ERROR_NO_PLUGIN;
	} else {
		ShellPluginInterface *iface = (ShellPluginInterface *)applauncher->GetInterface(1);

		ret = GAME_ERROR_NO_PLUGIN;
		if (iface != NULL)
			ret = ((int (*)(const LsdbAppInfoPtr *, bool))
			       iface->func[APPLAUNCHER_PLUGIN_FUNC_UPDATE])(info, flag);
	}

	if (notifyTopmenu)
		gameAppRegisterEnd(info);

	return ret;
}

/* a theme's record set from its theme.xml's title and provider (0x81010A14) */
int gameThemeSetInfo(const LsdbTheme *theme, const common::SharedPtr<LsdbThemeInfo> *info)
{
	AppInfoSClient client;
	int ret;

	SceLsdb_D322A870(&client);
	ret = SceLsdb_01790EDE(&client, theme, &(*info)->m_infomationProperty.m_title,
			       &(*info)->m_infomationProperty.m_provider);
	SceLsdb_56D8DCF0(&client);

	return ret;
}

/* whether two applications' APP_VER differ, a missing one read as empty (0x81010A5E) */
bool gameAppVersionDiffers(const LsdbAppInfoPtr *info, const LsdbAppInfoPtr *other)
{
	string version;
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_APP_VER);

	if (it != (*info)->end())
		version = LsdbValueText(it->second);

	string otherVersion;

	it = (*other)->find(LSDB_KEY_APP_VER);
	if (it != (*other)->end())
		otherVersion = LsdbValueText(it->second);

	bool same = version == otherVersion;

	return !same;
}

/*
 * The parameter tables and their lengths (0x81010BC2..0x81010C28). Sony's linker keeps one
 * copy of functions with the same code, so several lengths share another's.
 */
const GameSfoParam *gameSfoGetVersionParams(void)
{
	return s_sfoVersionParams;
}

unsigned int gameSfoGetVersionParamCount(void)
{
	return sizeof(s_sfoVersionParams) / sizeof(s_sfoVersionParams[0]);
}

const GameSfoParam *gameSfoGetAppVerParams(void)
{
	return s_sfoAppVerParams;
}

unsigned int gameSfoGetAppVerParamCount(void)
{
	return sizeof(s_sfoAppVerParams) / sizeof(s_sfoAppVerParams[0]);
}

const GameSfoParam *gameSfoGetCheckParams(void)
{
	return s_sfoCheckParams;
}

unsigned int gameSfoGetCheckParamCount(void)
{
	return sizeof(s_sfoCheckParams) / sizeof(s_sfoCheckParams[0]);
}

const GameSfoParam *gameSfoGetAppParams(void)
{
	return s_sfoAppParams;
}

unsigned int gameSfoGetAppParamCount(void)
{
	return sizeof(s_sfoAppParams) / sizeof(s_sfoAppParams[0]);
}

const GameSfoParam *gameSfoGetAttributeParams(void)
{
	return s_sfoAttributeParams;
}

unsigned int gameSfoGetAttributeParamCount(void)
{
	return sizeof(s_sfoAttributeParams) / sizeof(s_sfoAttributeParams[0]);
}

const GameSfoParam *gameSfoGetTitleParams(void)
{
	return s_sfoTitleParams;
}

unsigned int gameSfoGetTitleParamCount(void)
{
	return sizeof(s_sfoTitleParams) / sizeof(s_sfoTitleParams[0]);
}

const GameSfoParam *gameSfoGetSaveDataParams(void)
{
	return s_sfoSaveDataParams;
}

unsigned int gameSfoGetSaveDataParamCount(void)
{
	return sizeof(s_sfoSaveDataParams) / sizeof(s_sfoSaveDataParams[0]);
}

const unsigned int *gameGetAddcontDirKeys(void)
{
	return s_addcontDirKeys;
}

unsigned int gameGetAddcontDirKeyCount(void)
{
	return sizeof(s_addcontDirKeys) / sizeof(s_addcontDirKeys[0]);
}

/* whether a category is a game's, compared over the category's own length (0x81010C28) */
bool gameCategoryIsGameName(const string &category)
{
	unsigned int i;

	for (i = 0; i < sizeof(s_gameCategoryNames) / sizeof(s_gameCategoryNames[0]); i++) {
		if (sce_paf_strncmp(category.c_str(), s_gameCategoryNames[i], category.length()) == 0)
			return true;
	}

	return false;
}

/*
 * Whether an application counts as on ux0: (0x81010C5A): any _apptype but 0 and 2 does, else
 * its original path must begin with ux0:. PLACEHOLDER: what the application types are is not
 * read yet.
 */
int gameAppIsOnUx0(const LsdbAppInfoPtr *info, bool *onUx0)
{
	*onUx0 = false;

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_APPTYPE);

	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	int type = (int)gameLsdbValueInteger(it->second);

	if (type != 2 && type != 0) {
		*onUx0 = true;
	} else {
		it = (*info)->find(LSDB_KEY_ORG_PATH);
		if (it == (*info)->end())
			return GAME_PAF_ERROR_NOT_FOUND;
		if (sce_paf_strncmp("ux0:", LsdbValueText(it->second), 4) == 0)
			*onUx0 = true;
	}

	return 0;
}

/*
 * Whether two categories belong to one title: the same, or a game and its patch (gd and gp,
 * gdc and gpc, gdd and gpd, either way round) (0x81010D48).
 */
bool gameCategoryMatches(const string &category, const string &other)
{
	if (category == other)
		return true;
	if (category == "gd" && other == "gp")
		return true;
	if (category == "gp" && other == "gd")
		return true;
	if (category == "gdc" && other == "gpc")
		return true;
	if (category == "gpc" && other == "gdc")
		return true;
	if (category == "gdd" && other == "gpd")
		return true;
	if (category == "gpd" && other == "gdd")
		return true;

	return false;
}

/* whether a category pair is a gdc/gpc title's and its gdd/gpd one's (0x81010EB2) */
bool gameCategoryIsDataPair(const string &category, const string &other)
{
	return (category == "gdc" && other == "gdd") || (category == "gpc" && other == "gpd");
}

/*
 * Where a path is: 1 a game card (gro0:; 2 with bit 0 of flags), 4 pd0:, 3 ux0:, else 0
 * (0x81010F28). PLACEHOLDER: what the flag selects is not read yet.
 */
int gamePathGetStorage(const string &path, SceUInt64 flags)
{
	if (path.find("gro0:") != string::npos)
		return (flags & 1) ? 2 : 1;
	if (path.find("pd0:") != string::npos)
		return 4;
	if (path.find("ux0:") != string::npos)
		return 3;

	return 0;
}

/*
 * Where a patch's path is, for a gp or gpc category: 1 a game card (gro0:; 2 with bit 0 of
 * flags), 3 ux0:, else 0 (0x8101108E).
 */
int gamePatchPathGetStorage(const string &path, SceUInt64 flags, const string &category)
{
	if (category == "gp" || category == "gpc") {
		if (path.find("gro0:") != string::npos)
			return (flags & 1) ? 2 : 1;
		if (path.find("ux0:") != string::npos)
			return 3;
	}

	return 0;
}

/*
 * Where an application is installed, from its attribute and original path (0x810111C0); 0
 * (no storage) is an error.
 */
int gameAppGetStorage(const LsdbAppInfoPtr *info, int *storage)
{
	SceUInt64 attr = 0;
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_ATTRIBUTE);

	if (it != (*info)->end())
		attr = gameLsdbValueInteger(it->second);

	it = (*info)->find(LSDB_KEY_ORG_PATH);
	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	{
		string path(LsdbValueText(it->second));

		*storage = gamePathGetStorage(path, attr);
	}

	if (*storage == 0)
		return GAME_ERROR_NO_STORAGE;

	return 0;
}

/* where the application titleId is installed, from its record (0x810112FE) */
int gameTitleGetStorage(const char *titleId, int *storage)
{
	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = gameAppGetInfo(titleId, &info);

	if (ret < 0)
		return ret;

	return gameAppGetStorage(&info, storage);
}

/*
 * An application's original and SELF paths (_org_path, _self_path) with their beginning
 * replaced: each that contains from has its first from.length() bytes replaced by to
 * (0x8101145A).
 */
int gameAppInfoReplacePaths(LsdbAppInfoPtr *info, const string &from, const string &to)
{
	LsdbAppInfo::iterator end = (*info)->end();
	static const unsigned int keys[] = {
		LsdbKeyHasher<9>::Hash("_org_path"),
		LsdbKeyHasher<10>::Hash("_self_path"),
	};
	unsigned int i;

	for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		LsdbAppInfo::iterator it = (*info)->find(keys[i]);

		if (it == end)
			return GAME_ERROR_NO_VALUE;

		string path(LsdbValueText(it->second));

		if (path.find(from) != string::npos) {
			path.replace(0, from.length(), to);
			gameLsdbValueSetText((**info)[keys[i]], path);
		}
	}

	return 0;
}

/*
 * A registration's original and SELF paths with their beginning replaced (0x8101167C), when it
 * has a title ID. Sony's code copies the title ID and does nothing with it.
 */
int gameAppRegistrationReplacePaths(GameAppRegistration *app, const string &from,
				    const string &to)
{
	int ret = gameAppInfoReplacePaths(&app->info, from, to);

	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator it = app->info->find(LSDB_KEY_TITLE_ID);

	if (it == app->info->end())
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));

	return 0;
}

/*
 * The param.sfo parameters an application's category loads (0x81011740): the application
 * table for games, patches and their data, the attribute table for gdd and gpd.
 */
int gameAppInfoGetSfoParams(const LsdbAppInfoPtr *info, const GameSfoParam **params,
			    unsigned int *count)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);

	if (it == (*info)->end())
		return GAME_ERROR_BAD_DATA;

	string category(LsdbValueText(it->second));

	if (category == "gd" || category == "gdc") {
		*params = s_sfoAppParams;
		*count = 80;
	} else if (category == "gdd") {
		*params = s_sfoAttributeParams;
		*count = 6;
	} else if (category == "gp" || category == "gpc") {
		*params = s_sfoAppParams;
		*count = 80;
	} else if (category == "gpd") {
		*params = s_sfoAttributeParams;
		*count = 6;
	} else if (category == "gda" || category == "gdb" || category == "wda") {
		*params = s_sfoAppParams;
		*count = 80;
	} else {
		return GAME_ERROR_NO_SFO_PARAMS;
	}

	return 0;
}

/*
 * An application's version parameters loaded from its param.sfo, and the parameter table its
 * category loads (0x8101196C).
 */
int gameSfoLoadAppVer(const string &path, LsdbAppInfoPtr *info, const GameSfoParam **params,
		      unsigned int *count)
{
	int ret = gameSfoLoad(path, s_sfoAppVerParams, 3, info);

	if (ret < 0)
		return ret;

	ret = gameAppInfoGetSfoParams(info, params, count);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * gameSfoLoadAppVer once the param.sfo's SHA-256 checks against digest (0x81011A10).
 */
int gameSfoLoadAppVerVerified(const string &path, LsdbAppInfoPtr *info,
			      const GameSfoParam **params, unsigned int *count,
			      const char *digest, SceSize digestLength)
{
	int ret = gameSfoLoadVerified(path, s_sfoAppVerParams, 3, info, digest, digestLength);

	if (ret < 0)
		return ret;

	ret = gameAppInfoGetSfoParams(info, params, count);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * Whether a param.sfo's ATTRIBUTE fits its category (0x81011A4E): a game (gd, gdc) takes no
 * value above 1 in bits 1-2, and bit 20 needs bit 0. PLACEHOLDER: what the bits mean is not
 * read yet.
 */
int gameAttributeCheck(const string &category, SceInt64 attribute)
{
	int level = (int)((attribute & 6) >> 1);

	if ((category == "gd" || category == "gdc") && level >= 2)
		return GAME_ERROR_BAD_DATA;

	if ((attribute & 0x100000) != 0 && (attribute & 1) == 0)
		return GAME_ERROR_BAD_DATA;

	return 0;
}
