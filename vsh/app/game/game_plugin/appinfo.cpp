/*
 * The game plugin's application information: an application's livespace_db parameters, read
 * from its param.sfo and copied from its old record (the times, the live area path and the
 * shell's own keys, which a reinstalled or updated application keeps). The file's and every
 * name here are ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <rtc/rtc.h>
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
