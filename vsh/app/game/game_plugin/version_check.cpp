/*
 * SceShell's game plugin: the versions an application needs, checked against the system's,
 * the PSP emulator's and the PSM runtime's. The file's and every name here are ours (no record
 * names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
extern "C" {
#include <compat/compat.h>
}
#include <regmgr/regmgr.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string>
#include <file/localfile.h>
#include <common_gui_dialog/common_gui_dialog.h>

#include "../../shell/sysupchk.h"
#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the title ID of the PSM runtime, and the version of it the system carries */
#define GAME_PSM_RUNTIME_TITLE_ID	"PCSI00011"
#define GAME_PSM_RUNTIME_VERSION_FILE	"vs0:vsh/game/psm/runtime_version.txt"

/* the version checks' errors (values only) */
#define GAME_ERROR_INVALID_TITLE_ID	-2146430573	/* 0x80101193 */
#define GAME_ERROR_SYSTEM_TOO_OLD	-2146430687	/* 0x80101121: the system is older than needed */
#define GAME_ERROR_NO_PSM_RUNTIME	-2146430971	/* 0x80101005 */
#define GAME_ERROR_PSM_NOT_INSTALLED	-2146430641	/* 0x8010114F */
#define GAME_ERROR_PSM_TOO_OLD		-2146430643	/* 0x8010114D */
#define GAME_ERROR_PSM_REVOKED		-2146430644	/* 0x8010114C: the application's license is revoked */

/* the parental controls' errors (values only) */
#define GAME_ERROR_PARENTAL		-2146430685	/* 0x80101123: restricted by the parental controls */
#define GAME_ERROR_NO_REGION		-2146430645	/* 0x8010114B */
#define GAME_ERROR_REGION_KR		-2146430646	/* 0x8010114A: not in Korea */
#define GAME_ERROR_REGION_DENIED	-2146430684	/* 0x80101124: not in this region */

/* the regions an application's REGION_DENY bits name, by bit (Hong Kong counts as Asia) */
#define LSDB_KEY_REGION_DENY		0xCC003704u	/* "REGION_DENY" */
#define GAME_REGION_HONG_KONG		11
#define GAME_REGION_ASIA		2

static const SceUInt16 s_denyRegions[] = { 1, 2, 5, 6, 4, 10, 3, 7, 8, 9, 7, 7 };

/* the parental controls in the registry, and the level that is never allowed */
#define GAME_PARENTAL_CATEGORY		"/CONFIG/SECURITY/PARENTAL"
#define GAME_PARENTAL_LEVEL_NEVER	99

/*
 * PLACEHOLDER name, defined in another file of the shell: the PSM runtime the system has
 * installed, 0 without one (0x810FCBA6). The version checks are sfo_param.cpp's (game_appinfo.h).
 */
int shellPsmGetRuntimeVersion(SceUInt16 *version);

/* A version's text, "N.NN" or "NN.NN", as a number (0x81028480). */
__attribute__((noinline)) int gameVersionParse(const string &text, SceUInt32 *version)
{
	char buf[8];
	int ret;

	sce_paf_memset(buf, 0, sizeof(buf));
	sce_paf_strncpy(buf, text.c_str(), sizeof(buf));

	if (buf[1] == '.') {
		buf[4] = '\0';
		ret = gameVersionCheckShort(buf);
		if (ret < 0)
			return ret;

		ret = gameVersionToNumber(buf, sizeof(buf), version);
		if (ret < 0)
			return ret;
	} else {
		buf[5] = '\0';
		ret = gameVersionCheck(buf);
		if (ret < 0)
			return ret;

		ret = gameVersionToNumber(buf, sizeof(buf), version);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* The version of the PSM runtime installed (0x81028506). */
__attribute__((noinline)) int gamePsmGetRuntimeVersion(SceUInt32 *version)
{
	*version = 0;

	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = gameAppGetInfo(GAME_PSM_RUNTIME_TITLE_ID, &info);

	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator it = info->find(LSDB_KEY_APP_VER);

	if (it == info->end())
		return GAME_ERROR_NO_VALUE;

	ret = gameVersionParse(string(LsdbValueText(it->second)), version);
	if (ret < 0)
		return ret;

	return 0;
}

/* The version of the PSM runtime the system carries, as a number and as "N.NN" (0x810287C4). */
__attribute__((noinline)) int gamePsmGetSystemRuntimeVersion(SceUInt32 *version, string *text)
{
	int ret = 0;

	*version = 0;

	common::SharedPtr<File> file = LocalFile::Open(GAME_PSM_RUNTIME_VERSION_FILE, SCE_O_RDONLY,
						       0, &ret);

	if (ret < 0)
		return ret;

	SceInt64 size = file->GetFileSize();

	if (size < 0)
		return size;

	if ((SceUInt32)size >= 8)
		return GAME_ERROR_NO_MEMORY;

	char buf[8];

	sce_paf_memset(buf, 0, sizeof(buf));

	ret = file->Read(buf, 7);
	if (ret < 0) {
		file->Close();
		return ret;
	}

	buf[7] = '\0';
	file->Close();

	ret = gameVersionParse(string(buf), version);
	if (ret < 0)
		return ret;

	string number(buf);
	string::size_type dot = number.find(".");
	string major = number.substr(0, dot);
	string minor = number.substr(dot + 1, string::npos);
	char t[8];

	sce_paf_memset(t, 0, sizeof(t));
	sce_paf_snprintf(t, sizeof(t), "%d.%s", sce_paf_atoi(major.c_str()), minor.c_str());
	*text = string(t);

	return 0;
}

/* The system version an application needs, as a number and as "N.NN" (0x81028BF8). */
__attribute__((noinline)) int gameAppGetSystemVersion(const LsdbAppInfoPtr *info, SceUInt32 *version, string *text)
{
	*version = 0xFFFFFFFF;

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_SYSTEM_VER);

	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	*version = gameLsdbValueInteger(it->second);

	char buf[8];

	sce_paf_memset(buf, 0, sizeof(buf));

	if (*version >= 100) {
		sce_paf_snprintf(buf, sizeof(buf), "%d.%d", *version / 100, *version % 100);
		*text = string(buf);
	} else {
		sce_paf_snprintf(buf, sizeof(buf), "0.%d", *version);
		*text = string(buf);
	}

	return 0;
}

/*
 * Whether the system runs the application of info, by its category: the PSP emulator's
 * version for a PSP game, the system's for a game, and the PSM runtime's for a PSM
 * application (0x81028D50).
 */
__attribute__((noinline)) int gameAppCheckSystemVersion(const LsdbAppInfoPtr *info, string *version)
{
	*version = "";

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);

	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	const LsdbValue &value = it->second;

	if (*LsdbValueText(value) == '\0')
		return GAME_ERROR_NO_VALUE;

	string category(LsdbValueText(value));

	if (category == "ME" || category == "EG") {
		LsdbAppInfo::iterator ver = (*info)->find(LSDB_KEY_SYSTEM_VER);

		if (ver == (*info)->end())
			return GAME_ERROR_NO_VALUE;

		int required = gameLsdbValueInteger(ver->second);

		if (sceCompatGetPspSystemSoftwareVersion() < required)
			return GAME_ERROR_SYSTEM_TOO_OLD;
	} else if (category == "gc" || category == "gd" || category == "gp" ||
		   category == "gdc" || category == "gpc" || category == "gda" ||
		   category == "gdb") {
		SceUInt32 required;

		if (category == "gc") {
			LsdbAppInfo::iterator ver = (*info)->find(LSDB_KEY_SYSTEM_ROOT_VER);

			if (ver == (*info)->end())
				return GAME_ERROR_NO_VALUE;

			required = gameLsdbValueInteger(ver->second);
		} else {
			LsdbAppInfo::iterator ver = (*info)->find(LSDB_KEY_SYSTEM_VER);

			if (ver == (*info)->end())
				return GAME_ERROR_NO_VALUE;

			required = gameLsdbValueInteger(ver->second);
		}

		int older = 1;
		int ret = shellSystemVersionCheck(required, &older);

		if (ret != 0)
			return ret;

		if (older == 1)
			return GAME_ERROR_SYSTEM_TOO_OLD;
	} else if (category == "mb") {
		LsdbAppInfo::iterator id = (*info)->find(LSDB_KEY_TITLE_ID);

		if (id == (*info)->end())
			return GAME_ERROR_NO_VALUE;

		string titleId(LsdbValueText(id->second));
		bool allowed = false;
		int model = 0;
		int ret = gamePluginPsmAppIsAllowed(titleId, &allowed, &model);

		if (ret < 0)
			return ret;

		if (!allowed)
			return 0;

		SceUInt32 runtime = 0;

		ret = gamePsmGetRuntimeVersion(&runtime);
		if (ret < 0) {
			if (ret == GAME_ERROR_NO_PSM_RUNTIME)
				ret = GAME_ERROR_PSM_NOT_INSTALLED;
			return ret;
		}

		SceUInt32 bundled = 0;
		string bundledText(s_empty_name);

		ret = gamePsmGetSystemRuntimeVersion(&bundled, &bundledText);
		if (ret < 0)
			return ret;

		if (runtime < bundled) {
			SceUInt16 installed[2];

			sce_paf_memset(installed, 0, sizeof(installed));

			if (shellPsmGetRuntimeVersion(installed) == 0)
				return GAME_ERROR_PSM_TOO_OLD;

			if (installed[1] + 100 * (SceUInt32)installed[0] < bundled)
				return GAME_ERROR_PSM_TOO_OLD;
		}

		SceUInt32 required = 0;
		string requiredText(s_empty_name);

		ret = gameAppGetSystemVersion(info, &required, &requiredText);
		if (ret < 0)
			return ret;

		if (runtime >= required)
			return 0;

		SceUInt16 installed[2];

		sce_paf_memset(installed, 0, sizeof(installed));

		if (shellPsmGetRuntimeVersion(installed) == 0 ||
		    installed[1] + 100 * (SceUInt32)installed[0] < required)
			return GAME_ERROR_PSM_TOO_OLD;

		return 0;
	} else if (category == "mba") {
		LsdbAppInfo::iterator ver = (*info)->find(LSDB_KEY_SYSTEM_VER);

		if (ver == (*info)->end())
			return GAME_ERROR_NO_VALUE;

		int older = 1;
		int ret = shellSystemVersionCheck((SceUInt32)gameLsdbValueInteger(ver->second), &older);

		if (ret != 0)
			return ret;

		if (older == 1)
			return GAME_ERROR_SYSTEM_TOO_OLD;
	} else if (!(category == "wda")) {
		return GAME_ERROR_UNKNOWN_CATEGORY;
	}

	return 0;
}

/*
 * Whether the parental controls let the application of info start: the browser's, the
 * store's and email's own switches, the region for two system applications, and the
 * content level (0x81029756).
 */
__attribute__((noinline)) int gameAppCheckStartControl(const LsdbAppInfoPtr *info)
{
	const char *titleId = sceLsdbGetName(info);

	if (titleId == NULL)
		return GAME_ERROR_NO_VALUE;

	if (sce_paf_strncmp(titleId, "NPXS10003", sce_paf_strlen("NPXS10003")) == 0) {
		int control;
		int ret = sceRegMgrGetKeyInt(GAME_PARENTAL_CATEGORY, "browser_start_control",
					     &control);

		if (ret != 0)
			return ret;

		if (control != 0)
			return GAME_ERROR_PARENTAL;
	} else if (sce_paf_strncmp(titleId, "NPXS10002", sce_paf_strlen("NPXS10002")) == 0) {
		int control = 0;

		if (sceRegMgrGetKeyInt(GAME_PARENTAL_CATEGORY, "store_start_control",
				       &control) == 0 && control != 0)
			return GAME_ERROR_PARENTAL;
	} else if (sce_paf_strncmp(titleId, "NPXS10072", sce_paf_strlen("NPXS10072")) == 0) {
		int control = 0;

		if (sceRegMgrGetKeyInt(GAME_PARENTAL_CATEGORY, "email_start_control",
				       &control) == 0 && control != 0)
			return GAME_ERROR_PARENTAL;
	} else if (sce_paf_strncmp(titleId, "NPXS10000", sce_paf_strlen("NPXS10000")) == 0 ||
		   sce_paf_strncmp(titleId, "NPXS10005", sce_paf_strlen("NPXS10005")) == 0) {
		char region[4];

		if (sceRegMgrSrvGetRegionStr(region, sizeof(region)) <= 0)
			return GAME_ERROR_NO_REGION;

		if (sce_paf_strncmp(region, "KR2", 3) == 0) {
			if (sce_paf_strncmp(titleId, "NPXS10005", 10) != 0)
				return GAME_ERROR_REGION_KR;
			else
				return GAME_ERROR_REGION_DENIED;
		}
	}

	int level = 0;
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_PARENTAL_LEVEL);

	if (it != (*info)->end())
		level = gameLsdbValueInteger(it->second);

	int control = 0;
	int ret = sceRegMgrGetKeyInt(GAME_PARENTAL_CATEGORY, "content_start_control", &control);

	if (ret < 0)
		return ret;

	if (level == GAME_PARENTAL_LEVEL_NEVER || (control != 0 && level != 0 && level > control))
		return GAME_ERROR_PARENTAL;

	return 0;
}

/* Whether the license of the PSM application of info is revoked (0x81029988). */
__attribute__((noinline)) int gamePsmCheckRevoked(const LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);

	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	const LsdbValue &value = it->second;

	if (*LsdbValueText(value) == '\0')
		return GAME_ERROR_NO_VALUE;

	string category(LsdbValueText(value));

	if (category == "mb" || category == "mba") {
		LsdbAppInfo::iterator id = (*info)->find(LSDB_KEY_TITLE_ID);

		if (id == (*info)->end())
			return GAME_ERROR_NO_VALUE;

		string titleId(LsdbValueText(id->second));
		int ret = gameCheckTitleId(titleId.c_str());

		if (ret < 0)
			return GAME_ERROR_INVALID_TITLE_ID;

		string path = string("ux0:psm/") + titleId + string("/RW/System/revoked");
		common::SharedPtr<File> file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);

		if (ret == 0)
			return GAME_ERROR_PSM_REVOKED;
	}

	return 0;
}

/*
 * Whether the application of info may start: its region, the versions it needs (the PSM
 * runtime's into version when that is too old or missing), the parental controls and its
 * license (0x81029E56).
 */
int gameAppCheckStart(const LsdbAppInfoPtr *info, char *version)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_REGION_DENY);

	if (it != (*info)->end()) {
		int deny = gameLsdbValueInteger(it->second);

		if (deny != 0) {
			int region = sceRegMgrSrvGetRegion();

			if (region < 0)
				return region;

			if (region == GAME_REGION_HONG_KONG)
				region = GAME_REGION_ASIA;

			int bit = 1;
			unsigned int i;

			for (i = 0; i < sizeof(s_denyRegions) / sizeof(s_denyRegions[0]); i++) {
				if ((deny & bit) != 0 && region == s_denyRegions[i])
					break;
				bit *= 2;
			}

			if (i < sizeof(s_denyRegions) / sizeof(s_denyRegions[0]))
				return GAME_ERROR_REGION_DENIED;
		}
	}

	string required("");

	sce_paf_memset(version, 0, 8);

	int ret = gameAppCheckSystemVersion(info, &required);

	if (ret < 0) {
		if ((ret == GAME_ERROR_PSM_TOO_OLD || ret == GAME_ERROR_PSM_NOT_INSTALLED) &&
		    required.length() != 0)
			sce_paf_strncpy(version, required.c_str(), 8);
		return ret;
	}

	ret = gameAppCheckStartControl(info);
	if (ret < 0)
		return ret;

	ret = gamePsmCheckRevoked(info);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The system update check loaded for topmenu, for the system the application of info needs,
 * with callback told of the result: the check's SetParam result (0x81029FF4).
 */
__attribute__((noinline)) int gameAppStartSysupchk(const LsdbAppInfoPtr *info, void (*callback)(int result, void *arg),
			 void *arg, bool gameCard, int dialogMode, int priority)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_SYSTEM_VER);

	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	SceInt64 required = gameLsdbValueInteger(it->second);
	char text[16];

	sce_paf_strncpy(text, "-", sizeof(text) - 1);
	text[sizeof(text) - 1] = '\0';

	LsdbAppInfo::iterator disp = (*info)->find(LSDB_KEY_DISP_VER);

	if (disp == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	const char *ver = LsdbValueText(disp->second);

	if (ver[0] >= '0' && ver[0] <= '9' && ver[1] >= '0' && ver[1] <= '9' && ver[2] == '.' &&
	    ver[3] >= '0' && ver[3] <= '9' && ver[4] >= '0' && ver[4] <= '9' &&
	    ver[5] >= '0' && ver[5] <= '9') {
		if (ver[0] == '0') {
			sce_paf_strncpy(text, ver + 1, 4);
			text[4] = '\0';
		} else {
			sce_paf_strncpy(text, ver, 5);
			text[5] = '\0';
		}
	}

	bool card = gameCard;
	bool ux0 = false;

	if (!gameCard) {
		LsdbAppInfo::iterator path = (*info)->find(LSDB_KEY_SELF_PATH);

		if (path == (*info)->end())
			return GAME_ERROR_NO_VALUE;

		string selfPath(LsdbValueText(path->second));

		if (selfPath.find("gro0:") != string::npos)
			card = true;

		ux0 = selfPath.find("ux0:") != string::npos;
	}

	ShellSysupchkParam param;

	param.caller = "topmenu_plugin";
	param.callback = callback;
	param.arg = arg;
	param.checkMode = 1;
	param.dialogMode = dialogMode;
	if (card)
		param.launchMode = 2;
	else if (ux0)
		param.launchMode = 3;
	else
		param.launchMode = 1;
	param.acceptedFlag = 0;
	param.requiredVersion = required;
	param.requiredVersionString = text;
	shellSysupchkInterface = &shellSysupchkDefaultInterface;

	CommonGuiLayoutDialogParam dialog;

	dialog.priority = priority;
	param.msgDlgParam = &dialog;

	ShellSysupchkInitParam init;
	int ret = init.SetParam(param);

	init.Load(NULL, 0);

	return ret;
}

/*
 * The application's update checked for, with callback told of the result: the dialog mode
 * given, else 3 for the application types 2, 4 and 5 and 2 for the others (0x8102A4AC).
 */
int gameAppCheckUpdate(const LsdbAppInfoPtr *info, void (*callback)(int result, void *arg),
		       void *arg, bool gameCard, int mode, int priority)
{
	int dialogMode = 2;

	if (mode != 0) {
		dialogMode = mode;
	} else {
		int type = sceLsdbGetType(info);

		if (type != 1 && (type == 2 || type == 4 || type == 5))
			dialogMode = 3;
	}

	return gameAppStartSysupchk(info, callback, arg, gameCard, dialogMode, priority);
}
