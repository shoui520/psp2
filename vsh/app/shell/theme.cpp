/*
 * The shell's theme. The current theme (/CONFIG/THEME current_theme) is read at start: an ux0
 * theme from its copy in ur0:shell/theme, a preset one from pd0:theme, else the default in
 * vs0:data/internal/theme, and handed to livespace_db, which gives the pages and the system
 * applications' icons the theme's images. A theme changed to is mounted, copied to
 * ur0:shell/theme and recorded in livespace_db's theme table. The file's and every name here
 * are ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <framework.h>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <kernel/processmgr.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_stat.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <npdrm/npdrm.h>
#include <regmgr/regmgr.h>

#include "shell.h"
#include "theme.h"
#include "src/livespace_db/livespace_db.h"
#include "src/livespace_db/lsdb_keys.h"

using namespace paf;

/* the themes' folders: the default, the preset one, the installed ones and the shell's copy
   of the current one */
#define SHELL_THEME_DEFAULT		"vs0:data/internal/theme"
#define SHELL_THEME_PRESET		"pd0:theme"
#define SHELL_THEME_INSTALLED		"ux0:theme/"
#define SHELL_THEME_CACHE		"ur0:shell/theme"
#define SHELL_THEME_LICENSE		"ux0:license/theme/"
#define SHELL_THEME_FILE		"theme.xml"

#define SHELL_THEME_CATEGORY		"/CONFIG/THEME"

/* /CONFIG/THEME wallpaper: the user's own (kept), a theme's or none */
#define SHELL_WALLPAPER_NONE		0
#define SHELL_WALLPAPER_USER		1
#define SHELL_WALLPAPER_THEME		2

/* what shellThemeGetType gives for the current theme */
#define SHELL_THEME_TYPE_INSTALLED	100
#define SHELL_THEME_TYPE_PRESET		200
#define SHELL_THEME_TYPE_DEFAULT	300

/* the theme reader found no theme (value only) */
#define SHELL_THEME_ERROR_NO_THEME	0x801091FF
/* a theme deleted that is not an installed one (value only) */
#define SHELL_THEME_ERROR_NOT_INSTALLED	0x80109122
/* no memory for the default theme (value only) */
#define SHELL_THEME_ERROR_NO_MEMORY	0x80AF0022

/* livespace_db's error that says the default theme is pending (value only) */
#define SHELL_LSDB_THEME_DEFAULT	0x801010F5

/* sceNpDrmCheckActData: the activation has expired (value only) */
#define SHELL_NPDRM_ERROR_EXPIRED	0x80870006

/* the copy's access rights (rw for others, as the original's) */
#define SHELL_THEME_FILE_MODE		0006

/* shellHomeUpdateRequest's kind after a theme change */
#define SHELL_HOME_UPDATE_THEME		3

/* the icon of the power button (HomeProperty::m_power) */
#define SHELL_THEME_ICON_POWER		6

static common::SharedPtr<LsdbThemeInfo> s_theme;
static bool s_themeIconsEnabled;
static string s_pendingTheme;
static bool s_pendingIsDefault;

int shellNoticeServiceStart(void)
{
	return shellNoticeServiceInit();
}

/* the theme in path, read into s_theme; the default's is none */
static int shellThemeSet(const char *path)
{
	int ret = 0;

	if (sce_paf_strncmp(path, SHELL_THEME_DEFAULT, sizeof(SHELL_THEME_DEFAULT)) == 0) {
		s_theme.reset();
	} else {
		if (sce_paf_strncmp(path, SHELL_THEME_INSTALLED, sizeof(SHELL_THEME_INSTALLED) - 1) == 0)
			path = SHELL_THEME_CACHE;

		ShellThemeReader reader(path, Framework::Instance()->GetLocale());

		reader.SetLoadOption(true);
		ret = reader.Load();
		if (ret == 0) {
			s_theme = reader.GetThemeInfo();
			if (s_theme.get() == NULL)
				ret = SHELL_THEME_ERROR_NO_THEME;
		}
	}
	return ret;
}

/* /CONFIG/THEME current_theme, empty when it cannot be read */
static inline string shellThemeGetCurrent(void)
{
	char current[256];

	if (sceRegMgrGetKeyStr(SHELL_THEME_CATEGORY, "current_theme", current, sizeof(current)) != 0)
		return string("");
	return string(current);
}

int shellThemeLoad(void)
{
	int ret = 0;
	string current = shellThemeGetCurrent();

	if (current.size() != 0 && (ret = shellThemeSet(current.c_str())) == 0) {
		int err = SceLsdb_B0E42B67();

		if (SHELL_LSDB_TRIGGER_TABLE_STALE(err) || err == (int)SHELL_LSDB_THEME_DEFAULT) {
			s_pendingTheme = current;
			if (err == (int)SHELL_LSDB_THEME_DEFAULT)
				s_pendingIsDefault = true;
		}
		return err;
	}

	const char *path;

	if (!shellStartupHasPresetTheme() || shellThemeSet(path = SHELL_THEME_PRESET) != 0) {
		path = SHELL_THEME_DEFAULT;
		shellThemeSet(path);
	}
	s_pendingTheme = path;
	if (sce_paf_strncmp(s_pendingTheme.c_str(), SHELL_THEME_DEFAULT, sizeof(SHELL_THEME_DEFAULT)) == 0)
		s_pendingIsDefault = true;

	int wallpaper;

	if (sceRegMgrGetKeyInt(SHELL_THEME_CATEGORY, "wallpaper", &wallpaper) == 0 &&
	    wallpaper != SHELL_WALLPAPER_USER) {
		wallpaper = s_theme.get() != NULL ? SHELL_WALLPAPER_THEME : SHELL_WALLPAPER_NONE;
		sceRegMgrSetKeyInt(SHELL_THEME_CATEGORY, "wallpaper", wallpaper);
	}
	return ret;
}

void shellThemeCheckLicense(void)
{
	string current = shellThemeGetCurrent();
	int actType;
	int versionFlag;
	SceUInt64 accountId;
	SceUInt64 startTime;
	SceUInt64 expirationTime;

	if (sce_paf_strncmp(current.c_str(), SHELL_THEME_INSTALLED, sizeof(SHELL_THEME_INSTALLED) - 1) == 0 &&
	    sceNpDrmCheckActData(&actType, &versionFlag, &accountId, &startTime, &expirationTime) ==
	    (int)SHELL_NPDRM_ERROR_EXPIRED)
		shellSystemNoticeRequest(0, -1, NULL);
}

/* s_theme (the default's when none) given to livespace_db, the wallpaper and current_theme
   set; isDefault applies only the icons */
static int shellThemeApply(const char *path, bool isDefault, bool resetWallpaper)
{
	int ret;
	unsigned int flags = isDefault ? LSDB_SET_PAGE_THEME_ICONS : ~0u;

	if (s_theme.get() != NULL) {
		ret = SceLsdb_6F73E332(g_shellLsdbClient->client, &s_theme, flags);
	} else {
		ret = SHELL_THEME_ERROR_NO_MEMORY;

		common::SharedPtr<LsdbThemeInfo> theme(new LsdbThemeInfo);

		if (theme.get() != NULL) {
			for (int i = 0; i < LSDB_THEME_PAGES; i++)
				theme->m_homeProperty.m_bgParam[i].m_waveType = shellStartupGetPageColor();
			ret = SceLsdb_6F73E332(g_shellLsdbClient->client, &theme, flags);
		}
	}
	if (ret == 0) {
		int wallpaper;

		ret = sceRegMgrGetKeyInt(SHELL_THEME_CATEGORY, "wallpaper", &wallpaper);
		if (ret == 0 && (wallpaper != SHELL_WALLPAPER_USER || resetWallpaper)) {
			wallpaper = s_theme.get() != NULL ? SHELL_WALLPAPER_THEME : SHELL_WALLPAPER_NONE;
			ret = sceRegMgrSetKeyInt(SHELL_THEME_CATEGORY, "wallpaper", wallpaper);
		}
		if (ret == 0)
			ret = sceRegMgrSetKeyStr(SHELL_THEME_CATEGORY, "current_theme", path,
				sce_paf_strlen(path) + 1);
	}
	return ret;
}

/* the activation table's callback: checked again on success */
static void shellThemeOnActivation(int result, void *arg);

int shellThemeApplyPending(void)
{
	int ret = 0;

	if (s_pendingTheme.size() != 0) {
		sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
		ret = shellThemeApply(s_pendingTheme.c_str(), s_pendingIsDefault, false);
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
		s_pendingTheme.clear();
		s_pendingIsDefault = false;
	}
	shellCallbackTableAdd(shellThemeOnActivation, NULL);
	shellThemeCheckLicense();
	return ret;
}

/* a file copied; theme.xml keeps its times */
static __attribute__((noinline)) int shellThemeCopyFile(const char *src, const char *dst)
{
	int err = 0;
	char buf[4096];
	common::SharedPtr<File> in = LocalFile::Open(src, SCE_O_RDONLY, 0, &err);

	if (err == 0) {
		common::SharedPtr<File> out = LocalFile::Open(dst, SCE_O_WRONLY | SCE_O_CREAT,
			SHELL_THEME_FILE_MODE, &err);

		if (err == 0) {
			do {
				int size = in->Read(buf, sizeof(buf));

				if (size > 0)
					size = out->Write(buf, size);
				if (size <= 0) {
					err = size;
					break;
				}
			} while (err == 0);
		}
	}
	if (err == 0 && sce_paf_private_strtok(dst, SHELL_THEME_FILE) != NULL) {
		LocalFileStat stat;

		err = in->Getstat((FileStat *)&stat);
		if (err == 0) {
			LocalFileStat times;

			times.st_ctime = stat.st_ctime;
			times.st_atime = stat.st_atime;
			times.st_mtime = stat.st_mtime;
			err = LocalFile::Chstat(dst, &times, SCE_CST_CT | SCE_CST_AT | SCE_CST_MT);
		}
	}
	return err;
}

/* ur0:shell/theme emptied, and an installed theme's files (mounted at dir) copied to it */
static int shellThemeCopy(const char *path, const char *dir)
{
	int ret = 0;

	Dir::RemoveRecursive(SHELL_THEME_CACHE);
	if (sce_paf_strncmp(path, SHELL_THEME_INSTALLED, sizeof(SHELL_THEME_INSTALLED) - 1) == 0) {
		Dir::CreateRecursive(SHELL_THEME_CACHE);

		Dir folder;

		for (ret = folder.Open(dir); ret == 0;) {
			DirEnt entry;

			ret = folder.Read(&entry);
			if (ret == 0 && entry.type == DirEnt::Type_File) {
				string from = string(dir) + entry.name;
				string to = string(SHELL_THEME_CACHE) + "/" + entry.name;

				ret = shellThemeCopyFile(from.c_str(), to.c_str());
			}
		}
		if (ret == SCE_PAF_ERROR_DIR_NO_MORE_ENTRY)
			ret = 0;
	}
	return ret;
}

/* the last part of a path, empty for none */
static string shellThemeGetName(const char *path)
{
	string name(path);
	string::size_type pos = name.rfind('/');

	if (pos == string::npos || pos >= name.size() - 1)
		return "";
	return name.substr(pos + 1, string::npos);
}

int shellThemeChange(const char *path)
{
	int ret;
	string dir;
	char mountPoint[16];

	sce_paf_memset(mountPoint, 0, sizeof(mountPoint));
	if (sce_paf_strncmp(path, SHELL_THEME_INSTALLED, sizeof(SHELL_THEME_INSTALLED) - 1) == 0) {
		ret = sceAppMgrThemeDataMount(shellThemeGetName(path).c_str(), NULL, NULL, mountPoint);
		if (ret != 0)
			goto out;
		dir = mountPoint;
	} else {
		dir = string(path) + "/";
	}

	sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
	ret = sceRegMgrSetKeyStr(SHELL_THEME_CATEGORY, "current_theme", "", 1);
	if (ret == 0)
		ret = shellThemeCopy(path, dir.c_str());
	if (mountPoint[0] != '\0')
		sceAppMgrUmount(mountPoint);
	if (ret == 0) {
		if (path != NULL && path[0] != '\0') {
			ret = shellThemeSet(path);
			if (ret == 0) {
				AppInfoQClient client;

				SceLsdb_A6D8772C(&client);
				if (SceLsdb_E90B81CD(&client) == 0) {
					LsdbTheme theme;
					int err = SceLsdb_64DD44C2(&client, &string(path),
						Framework::Instance()->GetLocale(), &theme);

					if (err == 0 && theme.lastModifiedTime == 0) {
						theme.packageImageFilePath = s_theme.get()->m_infomationProperty.m_packageImageFilePath;
						theme.homePreviewFilePath = s_theme.get()->m_infomationProperty.m_homePreviewFilePath;
						theme.startPreviewFilePath = s_theme.get()->m_infomationProperty.m_startPreviewFilePath;
						theme.titleDefault = s_theme.get()->m_infomationProperty.m_title.m_default;
						theme.providerDefault = s_theme.get()->m_infomationProperty.m_provider.m_default;
						theme.contentVer = s_theme.get()->m_infomationProperty.m_contentVer;
						theme.lastModifiedTime = s_theme.get()->m_lastModifiedTime;
						SceLsdb_01790EDE(g_shellLsdbClient->client, &theme,
							&s_theme.get()->m_infomationProperty.m_title,
							&s_theme.get()->m_infomationProperty.m_provider);
					}
					SceLsdb_DEF5C3F9(&client);
				}
				SceLsdb_E2101B03(&client);
			}
		} else if (!shellStartupHasPresetTheme() || shellThemeSet(path = SHELL_THEME_PRESET) != 0) {
			path = SHELL_THEME_DEFAULT;
			shellThemeSet(path);
		}
		if (ret == 0) {
			s_themeIconsEnabled = false;
			ret = shellThemeApply(path, false, true);
		}
	}
	sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
	if (ret != 0) {
		if (!shellStartupHasPresetTheme() || shellThemeSet(SHELL_THEME_PRESET) != 0)
			shellThemeSet(SHELL_THEME_DEFAULT);

		int wallpaper;

		if (sceRegMgrGetKeyInt(SHELL_THEME_CATEGORY, "wallpaper", &wallpaper) == 0 &&
		    wallpaper != SHELL_WALLPAPER_USER) {
			wallpaper = s_theme.get() != NULL ? SHELL_WALLPAPER_THEME : SHELL_WALLPAPER_NONE;
			sceRegMgrSetKeyInt(SHELL_THEME_CATEGORY, "wallpaper", wallpaper);
		}
	}
	shellHomeUpdateRequest(SHELL_HOME_UPDATE_THEME);
out:
	return ret;
}

int shellThemeDelete(const char *path)
{
	int ret = 0;

	if (sce_paf_strncmp(path, SHELL_THEME_INSTALLED, sizeof(SHELL_THEME_INSTALLED) - 1) != 0)
		return SHELL_THEME_ERROR_NOT_INSTALLED;

	sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);

	bool isCurrent = path == shellThemeGetCurrent();

	if (!isCurrent || (ret = shellThemeChange(NULL)) == 0) {
		string license = SHELL_THEME_LICENSE + shellThemeGetName(path);

		Dir::RemoveRecursive(license.c_str());
		Dir::RemoveRecursive(path);
		SceLsdb_54F188A5(g_shellLsdbClient->client, &string(path));
	}
	sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
	return ret;
}

int shellThemeGetType(void)
{
	int type = 0;
	string current = shellThemeGetCurrent();

	if (sce_paf_strncmp(current.c_str(), SHELL_THEME_DEFAULT, sizeof(SHELL_THEME_DEFAULT)) == 0)
		type = SHELL_THEME_TYPE_DEFAULT;
	else if (sce_paf_strncmp(current.c_str(), SHELL_THEME_PRESET, sizeof(SHELL_THEME_PRESET)) == 0)
		type = SHELL_THEME_TYPE_PRESET;
	else if (sce_paf_strncmp(current.c_str(), SHELL_THEME_INSTALLED, sizeof(SHELL_THEME_INSTALLED) - 1) == 0)
		type = SHELL_THEME_TYPE_INSTALLED;
	return type;
}

int shellThemeGetCount(void)
{
	return SceLsdb_637814FE(g_shellLsdbClient->client);
}

bool shellThemeIconsEnabled(void)
{
	return s_themeIconsEnabled;
}

bool shellThemeIsPending(void)
{
	return s_pendingTheme.size() != 0 || s_themeIconsEnabled;
}

common::SharedPtr<LsdbThemeInfo> *shellThemeGetInfo(void)
{
	return &s_theme;
}

bool shellThemeGetIconPath(const char *titleId, string *path, int type)
{
	if (!s_themeIconsEnabled)
		return false;

	LsdbThemeInfo *info = s_theme.get();

	if (info == NULL || titleId == NULL)
		return false;

	/* the theme's icon of a system application (or of the power button, type 6) */
	const HomeProperty *home = &info->m_homeProperty;
	const IconParam *icon;

	if (sce_paf_strcmp("NPXS10003", titleId) == 0) {
		icon = &home->m_browser;
	} else if (sce_paf_strcmp("NPXS10010", titleId) == 0) {
		icon = &home->m_video;
	} else if (sce_paf_strcmp("NPXS10009", titleId) == 0) {
		icon = &home->m_music;
	} else if (sce_paf_strcmp("NPXS10012", titleId) == 0) {
		icon = &home->m_ps3Link;
	} else if (sce_paf_strcmp("NPXS10001", titleId) == 0) {
		icon = &home->m_party;
	} else if (sce_paf_strcmp("NPXS10008", titleId) == 0) {
		icon = &home->m_trophy;
	} else if (sce_paf_strcmp("NPXS10005", titleId) == 0) {
		icon = &home->m_map;
	} else if (sce_paf_strcmp("NPXS10000", titleId) == 0) {
		icon = &home->m_near;
	} else if (sce_paf_strcmp("NPXS10026", titleId) == 0) {
		icon = &home->m_hostCollabo;
	} else if (sce_paf_strcmp("NPXS10007", titleId) == 0) {
		icon = &home->m_welcomePark;
	} else if (sce_paf_strcmp("NPXS10013", titleId) == 0 || sce_paf_strcmp("NPXS10098", titleId) == 0) {
		icon = &home->m_ps4Link;
	} else if (sce_paf_strcmp("NPXS10006", titleId) == 0) {
		icon = &home->m_friend;
	} else if (sce_paf_strcmp("NPXS10072", titleId) == 0) {
		icon = &home->m_email;
	} else if (sce_paf_strcmp("NPXS10091", titleId) == 0) {
		icon = &home->m_calendar;
	} else if (sce_paf_strcmp("NPXS10002", titleId) == 0) {
		icon = &home->m_store;
	} else if (sce_paf_strcmp("NPXS10014", titleId) == 0) {
		icon = &home->m_message;
	} else if (sce_paf_strcmp("NPXS10094", titleId) == 0) {
		icon = &home->m_parental;
	} else if (sce_paf_strcmp("NPXS10004", titleId) == 0) {
		icon = &home->m_camera;
	} else if (sce_paf_strcmp("NPXS10015", titleId) == 0) {
		icon = &home->m_settings;
	} else if (type == SHELL_THEME_ICON_POWER) {
		icon = &home->m_power;
	} else {
		return false;
	}
	if (icon->m_iconFilePath.size() == 0)
		return false;
	shellThemeInfoGetFilePath(s_theme.get(), path, icon->m_iconFilePath);
	return true;
}

static void shellThemeOnActivation(int result, void *arg)
{
	if (result == 0)
		shellThemeCheckLicense();
}
