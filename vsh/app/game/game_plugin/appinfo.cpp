/*
 * The game plugin's application information: an application's livespace_db parameters, read
 * from its param.sfo and copied from its old record (the times, the live area path and the
 * shell's own keys, which a reinstalled or updated application keeps). The file's and every
 * name here are ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <rtc/rtc.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <plugin.h>
#include <common/shared_ptr.h>

#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "../../shell/plugins.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the errors (values only) */
#define GAME_ERROR_NO_PLUGIN	-2146430719	/* 0x80101101: a plugin the call needs is not loaded */
#define GAME_ERROR_NO_VALUE	-2146430713	/* 0x80101107: a key the record does not have */
#define GAME_ERROR_CATEGORY	-2146430705	/* 0x8010110F: a category the plugin does not know */
#define GAME_ERROR_NO_DRIVE	-2146430682	/* 0x80101126: a path without a drive */

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
};

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
