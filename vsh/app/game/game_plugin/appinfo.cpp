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
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <common/shared_ptr.h>

#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "../../shell/plugins.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the errors (values only) */
#define GAME_ERROR_NO_PLUGIN	-2146430719	/* 0x80101101: a plugin the call needs is not loaded */
#define GAME_ERROR_CATEGORY	-2146430705	/* 0x8010110F: a category the plugin does not know */
#define GAME_ERROR_NO_STORAGE	-2146430663	/* 0x80101139: a path on no storage the plugin knows */
#define GAME_ERROR_NO_SFO_PARAMS	-2146430672	/* 0x80101130: a category with no param.sfo table */
#define GAME_ERROR_NO_BOOT_APP	-2146430677	/* 0x8010112B: a BOOT_INSTALL_DIR no application is installed in */
#define GAME_ERROR_APP_RUNNING	-2146430700	/* 0x80101114: an application that is running */
#define GAME_ERROR_BAD_PBP	-2146430686	/* 0x80101122: a PBP file without its header */
#define GAME_ERROR_PSP_NOT_LISTED	-2146430689	/* 0x8010111F: a PSP title the emulator's list does not allow */
#define GAME_ERROR_PATCH_DATA	-2146430673	/* 0x8010112F: a patch whose category is data */

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

/* the sum of values[0..last] of count values, as a percentage (0x810103E6) */
float gameSumPercent(int last, const float *values, unsigned int count)
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

/* PLACEHOLDER: what is checked of a PSP game (the disc ID under TITLE_ID) */
static GameSfoParam s_sfoPspCheckParams[] = {
	{ "BOOTABLE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "CATEGORY", 0, GAME_SFO_FORMAT_TEXT, 3, 4, 0 },
	{ "UPDATER_VER", 0, GAME_SFO_FORMAT_TEXT, 5, 8, 0 },
	{ "APP_VER", 0, GAME_SFO_FORMAT_TEXT, 6, 8, 0 },
	{ "GAMEDATA_ID", 0, GAME_SFO_FORMAT_TEXT, 13, 16, 0 },
	{ "DISC_ID", LsdbKeyHash("TITLE_ID"), GAME_SFO_FORMAT_TEXT, 13, 16, 0 },
};

/* a PSP game on a memory card (MG) */
static const GameSfoParam s_sfoPspGameParams[] = {
	{ "TITLE", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "PARENTAL_LEVEL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "USE_USB", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "GAMEDATA_ID", 0, GAME_SFO_FORMAT_TEXT, 13, 16, 0 },
};

/* a PSP application on a memory card (MA); the same as a game's, one copy in Sony's binary */
static const GameSfoParam s_sfoPspAppParams[] = {
	{ "TITLE", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "PARENTAL_LEVEL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "USE_USB", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "GAMEDATA_ID", 0, GAME_SFO_FORMAT_TEXT, 13, 16, 0 },
};

/* a PS1 classic (ME) */
static GameSfoParam s_sfoPspPs1Params[] = {
	{ "TITLE", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "PARENTAL_LEVEL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "DISC_ID", LsdbKeyHash("TITLE_ID"), GAME_SFO_FORMAT_TEXT, 13, 16, 0 },
	{ "BOOTABLE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "DRIVER_PATH", 0, GAME_SFO_FORMAT_TEXT, 64, 64, 0 },
	{ "PSP_SYSTEM_VER", 0, GAME_SFO_FORMAT_TEXT, 5, 8, 0 },
	{ "CATEGORY", 0, GAME_SFO_FORMAT_TEXT, 3, 4, 0 },
	{ "USE_USB", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "GAMEDATA_ID", 0, GAME_SFO_FORMAT_TEXT, 13, 16, 0 },
};

/* a PSP disc game image (EG) */
static GameSfoParam s_sfoPspDiscGameParams[] = {
	{ "TITLE", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "PARENTAL_LEVEL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "DISC_ID", LsdbKeyHash("TITLE_ID"), GAME_SFO_FORMAT_TEXT, 13, 16, 0 },
	{ "DISC_TOTAL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "DISC_NUMBER", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "BOOTABLE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "DRIVER_PATH", 0, GAME_SFO_FORMAT_TEXT, 64, 64, 0 },
	{ "PSP_SYSTEM_VER", 0, GAME_SFO_FORMAT_TEXT, 5, 8, 0 },
	{ "CATEGORY", 0, GAME_SFO_FORMAT_TEXT, 3, 4, 0 },
	{ "USE_USB", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "ATTRIBUTE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "APP_VER", 0, GAME_SFO_FORMAT_TEXT, 6, 8, 0 },
	{ "HRKGMP_VER", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "GAMEDATA_ID", 0, GAME_SFO_FORMAT_TEXT, 13, 16, 0 },
};

/* PLACEHOLDER: a PSP game on a memory card, the case without its title (MG) */
static const GameSfoParam s_sfoPspGameBootParams[] = {
	{ "PARENTAL_LEVEL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "BOOTABLE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "CATEGORY", 0, GAME_SFO_FORMAT_TEXT, 3, 4, 0 },
	{ "USE_USB", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
};

/* a PSP game patch (PG) */
static const GameSfoParam s_sfoPspPatchParams[] = {
	{ "TITLE", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "PARENTAL_LEVEL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "BOOTABLE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "CATEGORY", 0, GAME_SFO_FORMAT_TEXT, 3, 4, 0 },
	{ "USE_USB", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "ATTRIBUTE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "APP_VER", 0, GAME_SFO_FORMAT_TEXT, 6, 8, 0 },
	{ "TARGET_APP_VER", 0, GAME_SFO_FORMAT_TEXT, 6, 8, 0 },
};

/* a PSP updater (MG) */
static const GameSfoParam s_sfoPspUpdaterParams[] = {
	{ "TITLE", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "PARENTAL_LEVEL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "BOOTABLE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "DRIVER_PATH", 0, GAME_SFO_FORMAT_TEXT, 64, 64, 0 },
	{ "UPDATER_VER", 0, GAME_SFO_FORMAT_TEXT, 5, 8, 0 },
};

/* PLACEHOLDER: a PSP category WG */
static GameSfoParam s_sfoPspWgParams[] = {
	{ "TITLE", 0, GAME_SFO_FORMAT_TEXT, 128, 128, 0 },
	{ "PARENTAL_LEVEL", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "DISC_ID", LsdbKeyHash("TITLE_ID"), GAME_SFO_FORMAT_TEXT, 13, 16, 0 },
	{ "BOOTABLE", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
	{ "DRIVER_PATH", 0, GAME_SFO_FORMAT_TEXT, 64, 64, 0 },
	{ "PSP_SYSTEM_VER", 0, GAME_SFO_FORMAT_TEXT, 5, 8, 0 },
	{ "CATEGORY", 0, GAME_SFO_FORMAT_TEXT, 3, 4, 0 },
	{ "USE_USB", 0, GAME_SFO_FORMAT_INTEGER, 4, 4, 0 },
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

/* the keys an application's information must keep from the one it replaces */
static unsigned int s_parentalKeys[] = {
	LsdbKeyHash("PARENTAL_LEVEL"),
	LsdbKeyHash("REGION_DENY"),
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
		gamePathGetMountedDrive(path, detail);
		return ret;
	}

	ret = file->Write(buf, size);
	if (ret < 0) {
		file->Close();
		LocalFile::RemoveFile(path.c_str());
		gamePathGetMountedDrive(path, detail);
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
		gamePathGetMountedDrive(path, detail);
		return ret;
	}

	ret = file->Read(buf, size);
	if (ret < 0) {
		file->Close();
		gamePathGetMountedDrive(path, detail);
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
__attribute__((noinline)) int gameAttributeCheck(const string &category, SceInt64 attribute)
{
	int level = (int)((attribute & 6) >> 1);

	if ((category == "gd" || category == "gdc") && level >= 2)
		return GAME_ERROR_BAD_DATA;

	if ((attribute & 0x100000) != 0 && (attribute & 1) == 0)
		return GAME_ERROR_BAD_DATA;

	return 0;
}

/*
 * Whether an application's param.sfo values are ones the shell accepts (0x81011ADA): a
 * category, a parental level up to 11, an attribute that fits the category, a title ID,
 * versions in their formats, a LiveArea type below 4, and for game and wireless data (gda,
 * wda) a BOOT_INSTALL_DIR naming another installed application.
 */
int gameAppInfoCheckSfo(const LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator end = (*info)->end();
	string category;
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);
	const char *text;
	int ret;

	if (it == end)
		return GAME_ERROR_BAD_DATA;

	text = LsdbValueText(it->second);
	if (text == NULL || *text == '\0')
		return GAME_ERROR_BAD_DATA;

	category = text;

	it = (*info)->find(LSDB_KEY_PARENTAL_LEVEL);
	if (it != end && (SceUInt32)gameLsdbValueInteger(it->second) > 11)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_ATTRIBUTE);
	if (it != end) {
		ret = gameAttributeCheck(category, gameLsdbValueInteger(it->second));
		if (ret < 0)
			return ret;
	}

	it = (*info)->find(LSDB_KEY_TITLE_ID_SFO);
	if (it == end)
		return GAME_ERROR_BAD_DATA;

	string titleId(LsdbValueText(it->second));

	if (titleId.length() == 0)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_DISP_VER);
	if (it != end && gameSystemVersionCheck(LsdbValueText(it->second)) < 0)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_APP_VER);
	if (it != end && gameVersionCheck(LsdbValueText(it->second)) < 0)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_TARGET_APP_VER);
	if (it != end && gameVersionCheck(LsdbValueText(it->second)) < 0)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_VERSION);
	if (it != end && gameVersionCheck(LsdbValueText(it->second)) < 0)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_LAREA_TYPE);
	if (it != end) {
		switch ((SceUInt32)gameLsdbValueInteger(it->second)) {
		case 0:
		case 1:
		case 2:
		case 3:
			break;
		default:
			return GAME_ERROR_BAD_DATA;
		}
	}

	it = (*info)->find(LSDB_KEY_BOOT_INSTALL_DIR);
	if (it == end)
		return 0;

	if (!(category == "gda" || category == "wda"))
		return GAME_ERROR_BAD_DATA;

	string bootDir(LsdbValueText(it->second));

	if (bootDir.length() == 0 || bootDir == titleId)
		return GAME_ERROR_BAD_DATA;

	LsdbAppInfoPtr boot(new LsdbAppInfo);

	if (boot.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameAppGetInfo(bootDir.c_str(), &boot);
	if (ret < 0) {
		if (ret == (int)LSDB_ERROR_NOT_FOUND)
			return GAME_ERROR_NO_BOOT_APP;
		return ret;
	}

	return 0;
}

/*
 * Whether two param.sfo informations agree in ATTRIBUTE bits 0 and 20 (0x8101243C); an empty
 * information agrees with any. PLACEHOLDER: what the bits mean is not read yet.
 */
int gameAttributeCompare(const LsdbAppInfoPtr *info, const LsdbAppInfoPtr *other)
{
	if (info->get() != NULL && (*info)->size() == 0)
		return 0;

	SceUInt32 attribute = 0;
	SceUInt32 otherAttribute = 0;
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_ATTRIBUTE);

	if (it != (*info)->end())
		attribute = (SceUInt32)gameLsdbValueInteger(it->second);

	it = (*other)->find(LSDB_KEY_ATTRIBUTE);
	if (it != (*other)->end())
		otherAttribute = (SceUInt32)gameLsdbValueInteger(it->second);

	if (((attribute ^ otherAttribute) & 0x100001) != 0)
		return GAME_ERROR_BAD_DATA;

	return 0;
}

/*
 * Whether an additional content's information fits the application it belongs to and the
 * other contents installed with it (0x81012522): its category must match the application's,
 * pair with each other's, and its ATTRIBUTE bits 0 and 20 and INSTALL_DIR_ADDCONT_ADD_n
 * directories must agree with theirs.
 */
int gameAddcontCheck(const LsdbAppInfoPtr *app, const LsdbAppInfoPtr *info,
		     const vector<LsdbAppInfoPtr> *related)
{
	if (related->size() == 0)
		return 0;

	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string category(LsdbValueText(it->second));
	char dirs[7][12];
	unsigned int i;

	for (i = 0; i < 7; i++) {
		dirs[i][0] = '\0';
		it = (*info)->find(s_addcontDirKeys[i]);
		if (it != end) {
			sce_paf_strncpy(dirs[i], LsdbValueText(it->second), 11);
			dirs[i][11] = '\0';
		}
	}

	it = (*info)->find(LSDB_KEY_TITLE_ID);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));
	SceInt64 attribute = 0;

	it = (*info)->find(LSDB_KEY_ATTRIBUTE);
	if (it != end)
		attribute = gameLsdbValueInteger(it->second);

	if (app->get() != NULL && (*app)->size() != 0) {
		LsdbAppInfo::iterator appEnd = (*app)->end();

		it = (*app)->find(LSDB_KEY_CATEGORY);
		if (it == appEnd)
			return GAME_ERROR_NO_VALUE;

		bool matches;
		{
			string appCategory(LsdbValueText(it->second));

			matches = gameCategoryMatches(appCategory, category);
		}
		if (!matches)
			return GAME_ERROR_BAD_DATA;

		SceInt64 appAttribute = 0;

		it = (*app)->find(LSDB_KEY_ATTRIBUTE);
		if (it != appEnd)
			appAttribute = gameLsdbValueInteger(it->second);

		if (((appAttribute ^ attribute) & 0x100001) != 0)
			return GAME_ERROR_BAD_DATA;
	}

	unsigned int n;

	for (n = 0; n < related->size(); n++) {
		LsdbAppInfo::iterator otherEnd = (*related)[n]->end();

		it = (*related)[n]->find(LSDB_KEY_CATEGORY);
		if (it == otherEnd)
			return GAME_ERROR_NO_VALUE;

		bool pairs;
		{
			string otherCategory(LsdbValueText(it->second));

			pairs = gameCategoryIsDataPair(category, otherCategory);
		}
		if (!pairs)
			return GAME_ERROR_BAD_DATA;

		SceInt64 otherAttribute = 0;

		it = (*related)[n]->find(LSDB_KEY_ATTRIBUTE);
		if (it != otherEnd)
			otherAttribute = gameLsdbValueInteger(it->second);

		if (((attribute ^ otherAttribute) & 0x100001) != 0)
			return GAME_ERROR_BAD_DATA;

		for (i = 0; i < 7; i++) {
			it = (*related)[n]->find(s_addcontDirKeys[i]);
			if (it != otherEnd &&
			    sce_paf_strncmp(dirs[i], LsdbValueText(it->second),
					    gameStrnlen(dirs[i], 12) + 1) != 0)
				return GAME_ERROR_BAD_DATA;
		}
	}

	return 0;
}

/*
 * Whether a patch's param.sfo values are ones the shell accepts (0x81012D74): a category that
 * is not data, an attribute that fits it, a title ID and versions in their format.
 */
int gameAppInfoCheckPatchSfo(const LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);
	int ret;

	if (it == end)
		return GAME_ERROR_BAD_DATA;

	string category(LsdbValueText(it->second));

	if (category.length() == 0)
		return GAME_ERROR_BAD_DATA;

	if (category == "gda" || category == "wda")
		return GAME_ERROR_PATCH_DATA;

	it = (*info)->find(LSDB_KEY_ATTRIBUTE);
	if (it != end) {
		ret = gameAttributeCheck(category, gameLsdbValueInteger(it->second));
		if (ret < 0)
			return ret;
	}

	it = (*info)->find(LSDB_KEY_TITLE_ID_SFO);
	if (it == end)
		return GAME_ERROR_BAD_DATA;

	string titleId(LsdbValueText(it->second));

	if (titleId.length() == 0)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_APP_VER);
	if (it != end && gameVersionCheck(LsdbValueText(it->second)) < 0)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_TARGET_APP_VER);
	if (it != end && gameVersionCheck(LsdbValueText(it->second)) < 0)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_VERSION);
	if (it != end && gameVersionCheck(LsdbValueText(it->second)) < 0)
		return GAME_ERROR_BAD_DATA;

	return 0;
}

/*
 * A SELF's eboot attribute and memory sizes stored into an application's information
 * (0x810131E8): zeros when the SELF cannot be read, whose error is returned.
 */
int gameAppInfoSetEbootParam(const string &selfPath, LsdbAppInfoPtr info)
{
	SceUInt32 attribute = 0;
	SceUInt32 phyMemsize = 0;
	SceUInt32 appMemsize = 0;
	GameEbootParam *param = new GameEbootParam;

	if (param == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = param->Read(selfPath);

	if (ret >= 0) {
		attribute = param->data.attribute;
		phyMemsize = param->data.phyMemsize;
		appMemsize = param->data.appMemsize;
		ret = 0;
	}

	gameLsdbValueSetInteger((*info)[LSDB_KEY_EBOOT_ATTR], attribute);
	gameLsdbValueSetInteger((*info)[LSDB_KEY_PHY_MEMSIZE], phyMemsize);
	gameLsdbValueSetInteger((*info)[LSDB_KEY_APP_MEMSIZE], appMemsize);

	delete param;

	return ret;
}

/* an application's eboot attribute and memory sizes copied to another's (0x8101336E) */
int gameAppInfoCopyEbootParam(const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst)
{
	LsdbAppInfo::iterator end = (*src)->end();
	LsdbAppInfo::iterator it = (*src)->find(LSDB_KEY_EBOOT_ATTR);

	if (it == end)
		return GAME_ERROR_NO_VALUE;
	gameLsdbValueSetInteger((**dst)[LSDB_KEY_EBOOT_ATTR],
				(SceUInt32)gameLsdbValueInteger(it->second));

	it = (*src)->find(LSDB_KEY_PHY_MEMSIZE);
	if (it == end)
		return GAME_ERROR_NO_VALUE;
	gameLsdbValueSetInteger((**dst)[LSDB_KEY_PHY_MEMSIZE],
				(SceUInt32)gameLsdbValueInteger(it->second));

	it = (*src)->find(LSDB_KEY_APP_MEMSIZE);
	if (it == end)
		return GAME_ERROR_NO_VALUE;
	gameLsdbValueSetInteger((**dst)[LSDB_KEY_APP_MEMSIZE],
				(SceUInt32)gameLsdbValueInteger(it->second));

	return 0;
}

/*
 * An application's eboot attribute and memory sizes set from its SELF (0x810135C0): zeros
 * without a SELF path; for game and wireless data (gda, wda) with a BOOT_INSTALL_DIR whose
 * SELF is not there, copied from the boot application's information.
 */
int gameAppInfoUpdateEbootParam(const string &selfPath, LsdbAppInfoPtr *info, string *detail)
{
	if (info->get() == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	if (selfPath.length() == 0) {
		gameLsdbValueSetInteger((**info)[LSDB_KEY_EBOOT_ATTR], 0);
		gameLsdbValueSetInteger((**info)[LSDB_KEY_PHY_MEMSIZE], 0);
		gameLsdbValueSetInteger((**info)[LSDB_KEY_APP_MEMSIZE], 0);
		return 0;
	}

	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);
	int ret;

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	bool isData;
	{
		string category(LsdbValueText(it->second));

		isData = category == "gda" || category == "wda";
	}

	if (!isData) {
		ret = gameAppInfoSetEbootParam(selfPath, *info);
		if (ret < 0) {
			gamePathGetMountedDrive(selfPath, detail);
			return ret;
		}
		return 0;
	}

	it = (*info)->find(LSDB_KEY_BOOT_INSTALL_DIR);
	if (it == end) {
		gameAppInfoSetEbootParam(selfPath, *info);
		return 0;
	}

	string bootDir(LsdbValueText(it->second));

	it = (*info)->find(LSDB_KEY_TITLE_ID);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));

	if (bootDir.length() == 0 || bootDir == titleId)
		return GAME_ERROR_BAD_DATA;

	LocalFileStat stat;

	ret = LocalFile::Getstat(selfPath.c_str(), &stat);
	if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND)
		return ret;

	if (ret == 0) {
		gameAppInfoSetEbootParam(selfPath, *info);
		return 0;
	}

	LsdbAppInfoPtr boot(new LsdbAppInfo);

	if (boot.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameAppGetInfo(bootDir.c_str(), &boot);
	if (ret < 0) {
		if (ret == (int)LSDB_ERROR_NOT_FOUND)
			return GAME_ERROR_NO_BOOT_APP;
		return ret;
	}

	ret = gameAppInfoCopyEbootParam(&boot, info);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * An application's eboot attribute and memory sizes set from a saved copy of its eboot
 * parameters (0x81013E8E): zeros when there is none.
 */
int gameAppInfoLoadEbootParam(const string &path, LsdbAppInfoPtr *info, string *detail)
{
	GameEbootParam *param = new GameEbootParam;

	if (param == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = param->Load(path);

	if (ret < 0) {
		delete param;
		if (ret == GAME_PAF_ERROR_NOT_FOUND) {
			gameLsdbValueSetInteger((**info)[LSDB_KEY_EBOOT_ATTR], 0);
			gameLsdbValueSetInteger((**info)[LSDB_KEY_PHY_MEMSIZE], 0);
			gameLsdbValueSetInteger((**info)[LSDB_KEY_APP_MEMSIZE], 0);
			return 0;
		}
		gamePathGetMountedDrive(path, detail);
		return ret;
	}

	gameLsdbValueSetInteger((**info)[LSDB_KEY_EBOOT_ATTR], param->data.attribute);
	gameLsdbValueSetInteger((**info)[LSDB_KEY_PHY_MEMSIZE], param->data.phyMemsize);
	gameLsdbValueSetInteger((**info)[LSDB_KEY_APP_MEMSIZE], param->data.appMemsize);

	delete param;

	return 0;
}

/*
 * An application's INSTALL_DIR_SAVEDATA, set to dir when it has none or an empty one
 * (0x81014128).
 */
void gameAppInfoSetSaveDataDir(LsdbAppInfoPtr *info, const string &dir)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_INSTALL_DIR_SAVEDATA);

	if (it == (*info)->end()) {
		gameLsdbValueSetText((**info)[LSDB_KEY_INSTALL_DIR_SAVEDATA], dir);
		return;
	}

	const char *saveDataDir = LsdbValueText(it->second);

	if (saveDataDir != NULL && *saveDataDir != '\0')
		gameLsdbValueSetText((**info)[LSDB_KEY_INSTALL_DIR_SAVEDATA], saveDataDir);
	else
		gameLsdbValueSetText((**info)[LSDB_KEY_INSTALL_DIR_SAVEDATA], dir.c_str());
}

/*
 * An application's INSTALL_DIR_ADDCONT, set to dir when it has none or an empty one
 * (0x810142C8).
 */
void gameAppInfoSetAddcontDir(LsdbAppInfoPtr *info, const string &dir)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_INSTALL_DIR_ADDCONT);

	if (it == (*info)->end()) {
		gameLsdbValueSetText((**info)[LSDB_KEY_INSTALL_DIR_ADDCONT], dir);
		return;
	}

	const char *addcontDir = LsdbValueText(it->second);

	if (addcontDir != NULL && *addcontDir != '\0')
		gameLsdbValueSetText((**info)[LSDB_KEY_INSTALL_DIR_ADDCONT], addcontDir);
	else
		gameLsdbValueSetText((**info)[LSDB_KEY_INSTALL_DIR_ADDCONT], dir.c_str());
}

/*
 * An application's INSTALL_DIR_CACHE (stored as INSTALL_DIR_CACHE0), set to dir when it has
 * none or an empty one (0x81014468).
 */
void gameAppInfoSetCacheDir(LsdbAppInfoPtr *info, const string &dir)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_INSTALL_DIR_CACHE);

	if (it == (*info)->end()) {
		gameLsdbValueSetText((**info)[LSDB_KEY_INSTALL_DIR_CACHE], dir);
		return;
	}

	const char *cacheDir = LsdbValueText(it->second);

	if (cacheDir != NULL && *cacheDir != '\0')
		gameLsdbValueSetText((**info)[LSDB_KEY_INSTALL_DIR_CACHE], cacheDir);
	else
		gameLsdbValueSetText((**info)[LSDB_KEY_INSTALL_DIR_CACHE], dir);
}

/* An application's IMPORT_SAVEDATA_INFO emptied, when it has one (0x810145EE). */
void gameAppInfoClearImportSaveDataInfo(LsdbAppInfoPtr *info)
{
	if ((*info)->find(LSDB_KEY_IMPORT_SAVEDATA_INFO) != (*info)->end())
		gameLsdbValueSetText((**info)[LSDB_KEY_IMPORT_SAVEDATA_INFO], "");
}

/* An application's #appconf, 0 without one (0x810146BE). */
SceInt64 gameAppInfoGetAppconf(const LsdbAppInfoPtr *info)
{
	if (info->get() == NULL)
		return 0;

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_APPCONF);

	if (it == (*info)->end())
		return 0;

	return gameLsdbValueInteger(it->second);
}

/*
 * The low 4 bits of a patch's #appconf when both the patch's ATTRIBUTE and flags have bit 24;
 * 15 when only flags has it, 0 otherwise (0x81014724).
 */
SceUInt64 gamePatchAppconfLow(const LsdbAppInfoPtr *info, const string &category,
			     SceUInt64 flags)
{
	SceUInt64 ret = 0;

	if (info->get() != NULL && (category == "gp" || category == "gpc")) {
		LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_ATTRIBUTE);
		SceUInt64 attribute = 0;

		if (it != (*info)->end())
			attribute = gameLsdbValueInteger(it->second);

		if ((attribute & 0x1000000) != 0 && (flags & 0x1000000) != 0)
			return gameAppInfoGetAppconf(info) & 0xF;
		if ((attribute & 0x1000000) == 0 && (flags & 0x1000000) != 0)
			return 15;
		if ((attribute & 0x1000000) != 0 && (flags & 0x1000000) == 0)
			return 0;
	} else if ((flags & 0x1000000) != 0) {
		return 15;
	}

	return ret;
}

/*
 * Bits 5-7 of a patch's #appconf when both the patch's ATTRIBUTE and flags have bit 25; 32
 * when only flags has it, 0 otherwise (0x810148D8).
 */
SceUInt64 gamePatchAppconfHigh(const LsdbAppInfoPtr *info, const string &category,
			     SceUInt64 flags)
{
	SceUInt64 ret = 0;

	if (info->get() != NULL && (category == "gp" || category == "gpc")) {
		LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_ATTRIBUTE);
		SceUInt64 attribute = 0;

		if (it != (*info)->end())
			attribute = gameLsdbValueInteger(it->second);

		if ((attribute & 0x2000000) != 0 && (flags & 0x2000000) != 0)
			return gameAppInfoGetAppconf(info) & 0xE0;
		if ((attribute & 0x2000000) == 0 && (flags & 0x2000000) != 0)
			return 32;
		if ((attribute & 0x2000000) != 0 && (flags & 0x2000000) == 0)
			return 0;
	} else if ((flags & 0x2000000) != 0) {
		return 32;
	}

	return ret;
}

/*
 * A patch's information made ready to register over its application app: #LIBLOCATION from
 * the attribute's level (bits 1-2), ATTRIBUTE2 folded into ATTRIBUTE's upper word, and the
 * #appconf bits the patch keeps from the application (0x81014A8E).
 */
int gameAppInfoSetPatchAttribute(const LsdbAppInfoPtr *app, LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);

	if (it == end)
		return GAME_ERROR_BAD_DATA;

	string category(LsdbValueText(it->second));

	it = (*info)->find(LSDB_KEY_ATTRIBUTE);

	SceUInt64 attribute = 0;

	if (it != end)
		attribute = gameLsdbValueInteger(it->second);

	SceUInt32 level = (SceUInt32)((attribute & 6) >> 1);

	if (app->get() != NULL && (category == "gp" || category == "gpc")) {
		LsdbAppInfo::iterator appIt = (*app)->find(LSDB_KEY_ATTRIBUTE);
		SceUInt64 appAttribute = 0;

		if (appIt != (*app)->end())
			appAttribute = gameLsdbValueInteger(appIt->second);

		SceUInt32 appLevel = (SceUInt32)((appAttribute & 6) >> 1);

		if (appLevel == level)
			gameLsdbValueSetInteger((**info)[LSDB_KEY_LIBLOCATION], appLevel);
		else
			gameLsdbValueSetInteger((**info)[LSDB_KEY_LIBLOCATION], level);
	} else if (level != 0) {
		gameLsdbValueSetInteger((**info)[LSDB_KEY_LIBLOCATION], level);
	}

	it = (*info)->find(LSDB_KEY_ATTRIBUTE2);
	if (it != end) {
		SceUInt32 attribute2 = (SceUInt32)gameLsdbValueInteger(it->second);

		gameLsdbValueSetInteger((**info)[LSDB_KEY_ATTRIBUTE],
					attribute | ((SceUInt64)attribute2 << 32));
		(*info)->erase(it);
	}

	SceUInt64 appconf = gamePatchAppconfLow(app, category, attribute) |
			    gamePatchAppconfHigh(app, category, attribute);

	appconf |= gameAppInfoGetAppconf(app) & ~(SceUInt64)0xEF;
	gameLsdbValueSetInteger((**info)[LSDB_KEY_APPCONF], appconf);

	return 0;
}

/*
 * For an application installed in another one's folder (BOOT_INSTALL_DIR): selfPath and path
 * set to that application's _self_path when selfPath is not there (0x81014EDA).
 */
int gameAppInfoUseBootSelfPath(const LsdbAppInfoPtr *info, string *selfPath, string *path)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_BOOT_INSTALL_DIR);

	if (it == (*info)->end())
		return 0;

	if (*LsdbValueText(it->second) == '\0')
		return 0;

	string bootDir(LsdbValueText(it->second));

	if (bootDir.length() == 0)
		return 0;

	LsdbAppInfoPtr boot(new LsdbAppInfo);
	int ret = gameAppGetInfo(bootDir.c_str(), &boot);

	if (ret < 0) {
		if (ret == (int)LSDB_ERROR_NOT_FOUND)
			return GAME_ERROR_NO_BOOT_APP;
		return ret;
	}

	LsdbAppInfo::iterator end = boot->end();

	it = boot->find(LSDB_KEY_SELF_PATH);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	LocalFileStat stat;

	if (LocalFile::Getstat(selfPath->c_str(), &stat) == GAME_PAF_ERROR_NOT_FOUND) {
		string bootSelfPath(LsdbValueText(it->second));

		*selfPath = bootSelfPath;
		*path = bootSelfPath;

		if (boot->find(LSDB_KEY_TITLE_ID) == end)
			return GAME_ERROR_NO_VALUE;
	}

	return 0;
}

/* the categories of a game application (by their first four bytes) */
static const char *s_gameAppCategories[] = { "gd", "gp", "gdc", "gdd", "gpc", "gpd" };

/*
 * An application's kind (0x810153C8): 1 for a game, 2 for the data application NPXS10998, 0
 * for other data; kind is left alone without a category or with another one.
 */
void gameAppInfoGetGameKind(const LsdbAppInfoPtr *info, const string &titleId, int *kind)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);

	if (it == (*info)->end())
		return;

	const char *category = LsdbValueText(it->second);

	if (sce_paf_strncmp(category, "gda", 4) == 0 || sce_paf_strncmp(category, "gdb", 4) == 0 ||
	    sce_paf_strncmp(category, "wda", 4) == 0) {
		if (sce_paf_strncmp(titleId.c_str(), "NPXS10998", 10) == 0)
			*kind = 2;
		else
			*kind = 0;
		return;
	}

	unsigned int i;

	for (i = 0; i < sizeof(s_gameAppCategories) / sizeof(s_gameAppCategories[0]); i++) {
		if (sce_paf_strncmp(category, s_gameAppCategories[i], 4) == 0) {
			*kind = 1;
			return;
		}
	}
}

/*
 * An application's _bg_title_id set to its background application's title ID, when it has
 * one (0x810154C0).
 */
void gameAppRegistrationSetBgTitleId(GameAppRegistration *app)
{
	if (app->related.size() == 0)
		return;

	static const unsigned int keys[] = { LsdbKeyHash("_bg_title_id") };
	unsigned int i;

	for (i = 0; i < app->related.size() && i < sizeof(keys) / sizeof(keys[0]); i++) {
		const LsdbAppInfoPtr *bgApp = &app->related[i];
		LsdbAppInfo::iterator it = (*bgApp)->find(LSDB_KEY_TITLE_ID);

		if (it != (*bgApp)->end())
			gameLsdbValueSetText((*app->info)[keys[i]], LsdbValueText(it->second));
	}
}

/*
 * The drive an application's data goes to, from where the application is (0x81015614): grw0:
 * for a game card (ux0: with toUx0), ux0: for ux0: and pd0:.
 */
int gamePathGetDataDrive(const string &path, bool toUx0, string *drive)
{
	if (path.find("gro0:") != string::npos) {
		if (toUx0)
			*drive = "ux0:";
		else
			*drive = "grw0:";
		return 0;
	}

	if (path.find("ux0:") != string::npos || path.find("pd0:") != string::npos) {
		*drive = "ux0:";
		return 0;
	}

	return GAME_ERROR_NO_DRIVE;
}

/*
 * Whether an application's information differs from other's in what an update must keep
 * (0x810157BA): the system version and the parental keys. Sony's code looks the system
 * version up in info twice and tests the second against other's end.
 */
int gameAppInfoParentalDiffers(const LsdbAppInfoPtr *info, const LsdbAppInfoPtr *other)
{
	if (info->get() == NULL || (*info)->size() == 0)
		return 1;

	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator otherEnd = (*other)->end();
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_SYSTEM_VER);
	LsdbAppInfo::iterator otherIt = (*info)->find(LSDB_KEY_SYSTEM_VER);

	if (it != end && otherIt != otherEnd &&
	    (SceUInt32)gameLsdbValueInteger(otherIt->second) !=
	    (SceUInt32)gameLsdbValueInteger(it->second))
		return 1;

	unsigned int i;

	for (i = 0; i < sizeof(s_parentalKeys) / sizeof(s_parentalKeys[0]); i++) {
		it = (*info)->find(s_parentalKeys[i]);
		otherIt = (*other)->find(s_parentalKeys[i]);

		switch (it->second.type) {
		case LSDB_VALUE_INTEGER: {
			int value = 0, otherValue = 0;

			if (it != end)
				value = (int)it->second.integer;
			if (otherIt != otherEnd)
				otherValue = (int)gameLsdbValueInteger(otherIt->second);
			if (value != otherValue)
				return 1;
			break;
		}
		case LSDB_VALUE_TEXT: {
			string text, otherText;

			if (it != end)
				text = LsdbValueText(it->second);
			if (otherIt != otherEnd)
				otherText = LsdbValueText(otherIt->second);
			if (!(text == otherText))
				return 1;
			break;
		}
		}
	}

	return 0;
}

/*
 * What an update must keep copied from src to dst (0x81015A7E): the system version with its
 * display string when dst has none or an older one, and the parental keys.
 */
int gameAppInfoCopyParental(const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst)
{
	LsdbAppInfo::iterator end = (*src)->end();
	LsdbAppInfo::iterator it = (*src)->find(LSDB_KEY_SYSTEM_VER);
	LsdbAppInfo::iterator dstIt = (*dst)->find(LSDB_KEY_SYSTEM_VER);

	if (it != end) {
		SceUInt32 version = (SceUInt32)gameLsdbValueInteger(it->second);

		if (dstIt == (*dst)->end()) {
			gameLsdbValueSetInteger((**dst)[LSDB_KEY_SYSTEM_VER], version);
			it = (*src)->find(LSDB_KEY_DISP_VER);
			if (it != end)
				gameLsdbValueSetText((**dst)[LSDB_KEY_DISP_VER],
						     LsdbValueText(it->second));
		} else if ((SceUInt32)gameLsdbValueInteger(dstIt->second) < version) {
			gameLsdbValueSetInteger((**dst)[LSDB_KEY_SYSTEM_VER], version);
			it = (*src)->find(LSDB_KEY_DISP_VER);
			if (it != end)
				gameLsdbValueSetText((**dst)[LSDB_KEY_DISP_VER],
						     LsdbValueText(it->second));
		}
	}

	unsigned int i;

	for (i = 0; i < sizeof(s_parentalKeys) / sizeof(s_parentalKeys[0]); i++) {
		it = (*src)->find(s_parentalKeys[i]);
		if (it == end)
			continue;

		switch (it->second.type) {
		case LSDB_VALUE_INTEGER:
			gameLsdbValueSetInteger((**dst)[s_parentalKeys[i]],
						(SceUInt32)gameLsdbValueInteger(it->second));
			break;
		case LSDB_VALUE_TEXT:
			gameLsdbValueSetText((**dst)[s_parentalKeys[i]], LsdbValueText(it->second));
			break;
		}
	}

	return 0;
}

/*
 * GAME_ERROR_APP_RUNNING when an application or its background application is running
 * (0x81015ED8).
 */
int gameAppCheckNotRunning(const string &titleId, const LsdbAppInfoPtr *info)
{
	SceAppMgrAppStatus status;

	sce_paf_memset(&status, 0, sizeof(status));
	if (sceAppMgrGetStatusByName(titleId.c_str(), &status) == 0)
		return GAME_ERROR_APP_RUNNING;

	if (info->get() == NULL || (*info)->size() == 0)
		return 0;

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_BG_TITLE_ID);

	if (it == (*info)->end())
		return 0;

	const char *bgTitleId = LsdbValueText(it->second);

	sce_paf_memset(&status, 0, sizeof(status));
	if (sceAppMgrGetStatusByName(bgTitleId, &status) == 0)
		return GAME_ERROR_APP_RUNNING;

	return 0;
}

/* Bit 11 (0x800) of an application's #appconf set or cleared (0x81015FA2). */
void gameAppInfoSetAppconfBit11(LsdbAppInfoPtr *info, bool set)
{
	if (set) {
		SceUInt64 appconf = gameAppInfoGetAppconf(info) | 0x800;

		gameLsdbValueSetInteger((**info)[LSDB_KEY_APPCONF], appconf);
		return;
	}

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_APPCONF);

	if (it != (*info)->end()) {
		SceUInt64 appconf = gameLsdbValueInteger(it->second) & ~(SceUInt64)0x800;

		gameLsdbValueSetInteger((**info)[LSDB_KEY_APPCONF], appconf);
	}
}

/* Bit 10 (0x400) of an application's #appconf set or cleared (0x810160C6). */
void gameAppInfoSetAppconfBit10(LsdbAppInfoPtr *info, bool set)
{
	if (set) {
		SceUInt64 appconf = gameAppInfoGetAppconf(info) | 0x400;

		gameLsdbValueSetInteger((**info)[LSDB_KEY_APPCONF], appconf);
		return;
	}

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_APPCONF);

	if (it != (*info)->end()) {
		SceUInt64 appconf = gameLsdbValueInteger(it->second) & ~(SceUInt64)0x400;

		gameLsdbValueSetInteger((**info)[LSDB_KEY_APPCONF], appconf);
	}
}

/*
 * A game card application's information moved to ux0: (0x810161EA): its paths from gro0: to
 * ux0:, bit 0 of its attribute set, bit 1 of #_promote_attr set when dir has a manual, bits 10
 * and 11 of #appconf cleared; registered again with doRegister (and ended with notify).
 */
int gameAppInfoMoveToUx0(LsdbAppInfoPtr *info, bool doRegister, bool notify, const string *dir)
{
	GameAppRegistration app;
	int ret;

	app.info = *info;
	if (app.entry.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameAppRegistrationReplacePaths(&app, string("gro0:"), string("ux0:"));
	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_ATTRIBUTE);
	SceUInt64 attribute = 0;

	if (it != end)
		attribute = gameLsdbValueInteger(it->second);
	gameLsdbValueSetInteger((**info)[LSDB_KEY_ATTRIBUTE], attribute | 1);

	it = (*info)->find(LSDB_KEY_PROMOTE_ATTR);
	if (it != end) {
		SceUInt32 promoteAttr = (SceUInt32)gameLsdbValueInteger(it->second);
		string manual = *dir + string("/sce_sys/manual/001.png");
		LocalFileStat stat;

		if (LocalFile::Getstat(manual.c_str(), &stat) == 0)
			promoteAttr |= 2;
		gameLsdbValueSetInteger((**info)[LSDB_KEY_PROMOTE_ATTR], promoteAttr);
	}

	gameAppInfoSetAppconfBit10(info, false);
	gameAppInfoSetAppconfBit11(info, false);

	if (doRegister) {
		char param[4];

		param[0] = 0;
		app.liveAreaState = 0;
		ret = gameAppRegister(&app, 1, false, notify, param);
		if (ret < 0)
			return ret;
		if (notify && (ret = gameAppRegisterEnd(&app.info)) < 0)
			return ret;
	}

	return 0;
}

/* An application's PARENTAL_LEVEL, 0 without one (0x81016A74). */
int gameAppInfoGetParentalLevel(const LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_PARENTAL_LEVEL);

	if (it == (*info)->end())
		return 0;

	return (int)gameLsdbValueInteger(it->second);
}

/* Whether a folder has any folder in it other than the dot ones (0x81016ACC). */
bool gameDirHasSubDirs(const string &path)
{
	int count = 0;
	bool ret = false;

	if (!Dir::IsExist(path.c_str()))
		return false;

	Dir dir;

	if (dir.Open(path.c_str()) < 0)
		return false;

	DirEnt entry;

	while (dir.Read(&entry) == 0) {
		if (entry.type == DirEnt::Type_Dir && sce_paf_strncmp(entry.name.c_str(), ".", 1) != 0)
			count++;
	}
	dir.Close();

	if (count != 0)
		ret = true;

	return ret;
}

/* Bit 12 (0x1000) of an application's #appconf set or cleared (0x81016B8E). */
void gameAppInfoSetAppconfBit12(LsdbAppInfoPtr *info, bool set)
{
	if (set) {
		SceUInt64 appconf = gameAppInfoGetAppconf(info) | 0x1000;

		gameLsdbValueSetInteger((**info)[LSDB_KEY_APPCONF], appconf);
		return;
	}

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_APPCONF);

	if (it != (*info)->end()) {
		SceUInt64 appconf = gameLsdbValueInteger(it->second) & ~(SceUInt64)0x1000;

		gameLsdbValueSetInteger((**info)[LSDB_KEY_APPCONF], appconf);
	}
}

/* Bit 2 (0x4) of an application's #_promote_attr set or cleared (0x81016CB2). */
void gameAppInfoSetPromoteAttrBit2(LsdbAppInfoPtr *info, bool set)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_PROMOTE_ATTR);

	if (set) {
		SceUInt32 promoteAttr = 0;

		if (it != (*info)->end())
			promoteAttr = (SceUInt32)gameLsdbValueInteger(it->second);
		gameLsdbValueSetInteger((**info)[LSDB_KEY_PROMOTE_ATTR], promoteAttr | 4);
	} else if (it != (*info)->end()) {
		SceUInt32 promoteAttr = (SceUInt32)gameLsdbValueInteger(it->second);

		gameLsdbValueSetInteger((**info)[LSDB_KEY_PROMOTE_ATTR], promoteAttr & ~4u);
	}
}

/* Whether bit 2 (0x4) of an application's #_promote_attr is set (0x81016E02). */
bool gameAppInfoPromoteAttrBit2(const LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_PROMOTE_ATTR);

	if (it != (*info)->end() && (gameLsdbValueInteger(it->second) & 4) != 0)
		return true;

	return false;
}

/* Whether ux0:license/app/<titleId>/ has the free trial license file (0x81016E6C). */
bool gameAppHasFttLicense(const string &titleId)
{
	string name;

	if (gameFttLicenseName(&name) < 0)
		return false;

	string path = string("ux0:license/app/") + titleId;

	path += "/";
	path.append(name);

	return LocalFile::Exists(path.c_str());
}

/* The title ID of the game card's application: the first folder in gro0:app (0x810170A0). */
string gameCardGetTitleId(void)
{
	string titleId("");

	if (!Dir::IsExist("gro0:"))
		return string("");

	string path = string("gro0:") + string("app");
	Dir dir;

	if (dir.Open(path.c_str()) < 0)
		return string("");

	DirEnt entry;

	while (dir.Read(&entry) == 0) {
		if (entry.type == DirEnt::Type_Dir && sce_paf_strncmp(entry.name.c_str(), ".", 1) != 0) {
			titleId = entry.name;
			break;
		}
	}
	dir.Close();

	return titleId;
}

/* A folder's statistics read into a cleared stat (0x8101731A). */
int gameDirGetStat(const string &path, SceNpDrmInstallInfo *stat)
{
	sce_paf_memset(stat, 0, sizeof(*stat));

	int ret = gameDirReadStat(path, stat);

	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The ATTRIBUTE_MINOR flags: valid when bit 4 is set, then whether bits 2 and
 * 3 are clear (0x8101733E).
 */
int gameAppInfoGetAttributeMinor(const LsdbAppInfoPtr *info, bool *valid,
				 bool *bit2Clear, bool *bit3Clear)
{
	*valid = false;
	*bit2Clear = false;
	*bit3Clear = false;

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_ATTRIBUTE_MINOR);

	if (it != (*info)->end()) {
		SceUInt64 minor = gameLsdbValueInteger(it->second);

		if ((minor & 0x10) != 0) {
			*valid = true;
			*bit2Clear = (minor & 4) == 0;
			*bit3Clear = (minor & 8) == 0;
		}
	}

	return 0;
}

/*
 * The PSP parameter tables and their lengths (0x810173C0..0x81017420); Sony's linker keeps one
 * copy of the lengths with the same code.
 */
const GameSfoParam *gameSfoGetPspCheckParams(void)
{
	return s_sfoPspCheckParams;
}

unsigned int gameSfoGetPspCheckParamCount(void)
{
	return sizeof(s_sfoPspCheckParams) / sizeof(s_sfoPspCheckParams[0]);
}

const GameSfoParam *gameSfoGetPspGameParams(void)
{
	return s_sfoPspGameParams;
}

unsigned int gameSfoGetPspGameParamCount(void)
{
	return sizeof(s_sfoPspGameParams) / sizeof(s_sfoPspGameParams[0]);
}

const GameSfoParam *gameSfoGetPspAppParams(void)
{
	return s_sfoPspAppParams;
}

unsigned int gameSfoGetPspAppParamCount(void)
{
	return sizeof(s_sfoPspAppParams) / sizeof(s_sfoPspAppParams[0]);
}

const GameSfoParam *gameSfoGetPspPs1Params(void)
{
	return s_sfoPspPs1Params;
}

unsigned int gameSfoGetPspPs1ParamCount(void)
{
	return sizeof(s_sfoPspPs1Params) / sizeof(s_sfoPspPs1Params[0]);
}

const GameSfoParam *gameSfoGetPspDiscGameParams(void)
{
	return s_sfoPspDiscGameParams;
}

unsigned int gameSfoGetPspDiscGameParamCount(void)
{
	return sizeof(s_sfoPspDiscGameParams) / sizeof(s_sfoPspDiscGameParams[0]);
}

const GameSfoParam *gameSfoGetPspGameBootParams(void)
{
	return s_sfoPspGameBootParams;
}

unsigned int gameSfoGetPspGameBootParamCount(void)
{
	return sizeof(s_sfoPspGameBootParams) / sizeof(s_sfoPspGameBootParams[0]);
}

const GameSfoParam *gameSfoGetPspPatchParams(void)
{
	return s_sfoPspPatchParams;
}

unsigned int gameSfoGetPspPatchParamCount(void)
{
	return sizeof(s_sfoPspPatchParams) / sizeof(s_sfoPspPatchParams[0]);
}

const GameSfoParam *gameSfoGetPspUpdaterParams(void)
{
	return s_sfoPspUpdaterParams;
}

unsigned int gameSfoGetPspUpdaterParamCount(void)
{
	return sizeof(s_sfoPspUpdaterParams) / sizeof(s_sfoPspUpdaterParams[0]);
}

const GameSfoParam *gameSfoGetPspWgParams(void)
{
	return s_sfoPspWgParams;
}

unsigned int gameSfoGetPspWgParamCount(void)
{
	return sizeof(s_sfoPspWgParams) / sizeof(s_sfoPspWgParams[0]);
}

/*
 * A PBP file's header, and whether its data.psar is a PS1 multi-disc image
 * (PSTITLEIMG) when multiDisc is given (0x81017422).
 */
int gamePbpReadHeader(const char *path, GamePbpHeader *header, bool *multiDisc)
{
	int ret = 0;
	SceUInt8 *buf = (SceUInt8 *)sce_paf_malloc(0x200);

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	common::SharedPtr<File> file = LocalFile::Open(path, SCE_O_RDONLY, 0, &ret);

	if (ret != 0) {
		sce_paf_free(buf);
		return ret;
	}

	SceInt64 size = file->GetFileSize();

	if (size < 0) {
		sce_paf_free(buf);
		return (int)size;
	}

	ret = file->Read(buf, 0x200);
	if (ret < 0) {
		sce_paf_free(buf);
		file->Close();
		return ret;
	}

	if (buf[0] != 0 || buf[1] != 'P' || buf[2] != 'B' || buf[3] != 'P') {
		sce_paf_free(buf);
		file->Close();
		return GAME_ERROR_BAD_PBP;
	}

	sce_paf_memcpy(header, buf, sizeof(*header));

	if (multiDisc != NULL) {
		char magic[17];

		if (size < header->dataPsarOffset) {
			sce_paf_free(buf);
			file->Close();
			return GAME_ERROR_BAD_PBP;
		}

		SceInt64 pos = file->Seek(header->dataPsarOffset, SCE_SEEK_SET);

		if (pos < 0) {
			sce_paf_free(buf);
			file->Close();
			return (int)pos;
		}

		ret = file->Read(magic, sizeof(magic));
		if (ret < 0) {
			sce_paf_free(buf);
			file->Close();
			return ret;
		}

		*multiDisc = false;
		if (sce_paf_memcmp(magic, "PSTITLEIMG000000", 16) == 0)
			*multiDisc = true;
	}

	file->Close();
	sce_paf_free(buf);

	return 0;
}

/* GAME_ERROR_APP_RUNNING when the application in the foreground (-3) is titleId (0x81017850). */
int gameAppCheckNotForeground(const string &titleId)
{
	SceAppMgrAppStatus status;

	sce_paf_memset(&status, 0, sizeof(status));
	if (sceAppMgrGetStatusByAppId(-3, &status) == 0 &&
	    sce_paf_strncmp(status.appName, titleId.c_str(), titleId.length()) == 0)
		return GAME_ERROR_APP_RUNNING;

	return 0;
}

/*
 * A PSP game's GAMEDATA_ID, else its TITLE_ID; the three ULJS00167..9 discs share the game
 * data of NPJH90038 (0x810178A8).
 */
int gamePspGetGameDataId(const LsdbAppInfoPtr *info, string *gameDataId)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_GAMEDATA_ID);

	if (it != (*info)->end()) {
		*gameDataId = LsdbValueText(it->second);
		return 0;
	}

	it = (*info)->find(LSDB_KEY_TITLE_ID_SFO);
	if (it == (*info)->end())
		return GAME_ERROR_BAD_DATA;

	string titleId(LsdbValueText(it->second));

	if (sce_paf_strncmp(titleId.c_str(), "ULJS00167", 10) == 0 ||
	    sce_paf_strncmp(titleId.c_str(), "ULJS00168", 10) == 0 ||
	    sce_paf_strncmp(titleId.c_str(), "ULJS00169", 10) == 0)
		*gameDataId = "NPJH90038";

	return 0;
}

/*
 * GAME_ERROR_APP_RUNNING when the application in the foreground (-3) is titleId, or is a PSP
 * game that shares its game data (0x81017A32).
 */
int gamePspCheckGameDataNotRunning(const string &titleId, const LsdbAppInfoPtr *info)
{
	SceAppMgrAppStatus status;

	sce_paf_memset(&status, 0, sizeof(status));
	if (sceAppMgrGetStatusByAppId(-3, &status) != 0)
		return 0;

	if (sce_paf_strncmp(status.appName, titleId.c_str(), titleId.length()) == 0)
		return GAME_ERROR_APP_RUNNING;

	string gameDataId;
	int ret = gamePspGetGameDataId(info, &gameDataId);

	if (ret < 0)
		return ret;

	if (gameDataId.length() == 0)
		return 0;

	LsdbAppInfoPtr running(new LsdbAppInfo);

	if (running.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameAppGetInfo(status.appName, &running);
	if (ret < 0)
		return ret;

	string runningId;

	ret = gamePspGetGameDataId(&running, &runningId);
	if (ret < 0)
		return ret;

	if (runningId == gameDataId)
		return GAME_ERROR_APP_RUNNING;

	return 0;
}

/* A PSP game's boot file in dir: EBOOT.PBP, else PBOOT.PBP, else PARAM.PBP (0x81017D80). */
int gamePspGetBootPath(const string &dir, string *path)
{
	*path = dir + string("/EBOOT.PBP");

	LocalFileStat stat;

	if (LocalFile::Getstat(path->c_str(), &stat) < 0) {
		*path = dir + string("/PBOOT.PBP");
		if (LocalFile::Getstat(path->c_str(), &stat) < 0) {
			*path = dir + string("/PARAM.PBP");

			int ret = LocalFile::Getstat(path->c_str(), &stat);

			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

/*
 * Whether a PSP content ID (1 to 47 characters) names a title the emulator's list allows: its
 * title ID is the 9 characters after the 7 of the service ID (0x8101810A).
 */
int gamePspCheckContentId(const char *contentId)
{
	unsigned int len;
	char titleId[12];

	if (contentId == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	for (len = 0; len < 48; len++) {
		if (contentId[len] == '\0')
			break;
	}

	if (len == 0 || len >= 48)
		return GAME_ERROR_INVALID_ARGUMENT;

	sce_paf_memset(titleId, 0, sizeof(titleId));
	sce_paf_strncpy(titleId, contentId + 7, 9);
	titleId[9] = '\0';

	if (!gamePspEmuListAllows(titleId))
		return GAME_ERROR_PSP_NOT_LISTED;

	return 0;
}

/* Whether a content ID is the PocketStation application's (0x81018184). */
bool gameIsPocketStation(const string &contentId)
{
	return contentId == string("JA0003-PCSC80018_00-POCKETSTATION001");
}
