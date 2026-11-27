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
#include <buffer/buffer.h>
#include <common/char_encoding.h>
#include <datetime/datetime.h>
#include <std/stdlib.h>
#include <kernel/processmgr.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_stat.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <npdrm/npdrm.h>
#include <regmgr/regmgr.h>

#include "shell.h"
#include "theme.h"
#include "src/nsx/nsx_internal.h"
#include "src/livespace_db/livespace_db.h"
#include "src/livespace_db/lsdb_keys.h"

extern const char *const *LsdbThemeGetSystemTitleIds(unsigned int *count);

const char **shellThemeGetIconTitles(SceUInt32 *count)
{
	return const_cast<const char **>(LsdbThemeGetSystemTitleIds(count));
}

/* 0x813BC6F4 */
void shellThemeInfoGetFilePath(const LsdbThemeInfo *info, paf::string *path,
			       const paf::string &name)
{
	if (!name.empty()) {
		paf::string combined = info->path;
		combined.append(name.c_str(), name.size());
		*path = combined;
	} else {
		*path = name;
	}
}

using namespace paf;

/* the themes' folders: the default, the preset one, the installed ones and the shell's copy
   of the current one */
#define SHELL_THEME_DEFAULT		"vs0:data/internal/theme"
#define SHELL_THEME_PRESET		"pd0:theme"
#if !defined(FW_TYPE_CEX)
#define SHELL_THEME_HOST		"host0:theme"
#endif
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
#if !defined(FW_TYPE_CEX)
	ret = shellThemeSet(SHELL_THEME_HOST);
	if (ret == 0) {
		s_themeIconsEnabled = true;
		return 0;
	}
#endif
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
		ret = SHELL_ERROR_NO_MEMORY;

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
			return ret;
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
#if !defined(FW_TYPE_CEX)
			if (sce_paf_strncmp(path, SHELL_THEME_HOST, sizeof(SHELL_THEME_HOST)) != 0)
				s_themeIconsEnabled = false;
#else
			s_themeIconsEnabled = false;
#endif
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

/* --------------------------------------------------------------------------
 * LsdbThemeInfo & ShellThemeReader (0x813BC14E..0x813BFA32)
 * -------------------------------------------------------------------------- */

static const int kThemeReaderErrorNoMemory = -2146397952;	/* 0x80109100 */
static const int kThemeReaderErrorOpenFailed = -2146397951;	/* 0x80109101 */
static const int kThemeReaderErrorTooLarge = -2146397950;	/* 0x80109102 */
static const int kThemeReaderErrorParseFailed = -2146397949;	/* 0x80109103 */

static const char *const kThemeLangTagNames[LSDB_THEME_LANGUAGES] = {
	"m_ja", "m_en", "m_fr", "m_es", "m_de", "m_it", "m_nl", "m_pt", "m_ru", "m_ko",
	"m_ch", "m_zh", "m_fi", "m_sv", "m_da", "m_no", "m_pl", "m_pt_br", "m_en_gb", "m_tr",
};

/* (0x813BC4E6) */
InfomationProperty::InfomationProperty()
	: m_contentVer(0)
{
}

/* (0x813BC572) */
LsdbThemeInfo::LsdbThemeInfo()
	: m_slot(-1), m_status(0), m_lastModifiedTime(0)
{
	for (int i = 0; i < LSDB_THEME_PAGES; i++) {
		m_homeProperty.m_bgParam[i].m_waveType = -1;
		m_homeProperty.m_bgParam[i].m_fontColor = (unsigned int)-1;
		m_homeProperty.m_bgParam[i].m_fontShadow = true;
	}

	IconParam *const icons[] = {
		&m_homeProperty.m_browser, &m_homeProperty.m_video, &m_homeProperty.m_music,
		&m_homeProperty.m_ps3Link, &m_homeProperty.m_party, &m_homeProperty.m_trophy,
		&m_homeProperty.m_map, &m_homeProperty.m_near, &m_homeProperty.m_hostCollabo,
		&m_homeProperty.m_welcomePark, &m_homeProperty.m_ps4Link, &m_homeProperty.m_friend,
		&m_homeProperty.m_email, &m_homeProperty.m_calendar, &m_homeProperty.m_store,
		&m_homeProperty.m_message, &m_homeProperty.m_parental, &m_homeProperty.m_camera,
		&m_homeProperty.m_settings, &m_homeProperty.m_power,
	};
	for (unsigned int i = 0; i < sizeof(icons) / sizeof(icons[0]); i++) {
		icons[i]->m_pagePos = -1;
		icons[i]->m_iconPos = -1;
	}

	m_startScreenProperty.m_dateColor = 0;
	m_startScreenProperty.m_dateLayout = -1;
	m_startScreenProperty.m_notifyBgColor = 0;
	m_startScreenProperty.m_notifyBorderColor = -1;
	m_startScreenProperty.m_notifyFontColor = -1;

	m_infomationBarProperty.m_barColor = 0xFF;
	m_infomationBarProperty.m_indicatorColor = -1;
	m_infomationBarProperty.m_noticeFontColor = -1;
	m_infomationBarProperty.m_noticeGlowColor = -18888;
}

static unsigned int themeSwapRgbaColor(unsigned int rawColor)
{
	return (rawColor & 0xFF000000U) | ((rawColor & 0xFFU) << 16) |
	       (rawColor & 0xFF00U) | ((rawColor >> 16) & 0xFFU);
}

/* (0x813BC670) */
static int themeParseVersionNumber(const paf::string &verStr)
{
	if (verStr.size() != 5)
		return -1;
	const char *s = verStr.c_str();
	if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9' || s[2] != '.' ||
	    s[3] < '0' || s[3] > '9' || s[4] < '0' || s[4] > '9')
		return -1;
	return (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[3] - '0') * 10 + (s[4] - '0');
}

/* (0x813BD3D6) relative filename check: no '/', at most 32 bytes */
static bool themeValidateFileName(const paf::string &name)
{
	if (name.find("/") != paf::string::npos)
		return false;
	return name.size() <= 32U;
}

static bool themeParseHexColor(const paf::string &text, int *outColor)
{
	char *endPtr = NULL;
	unsigned int parsed = (unsigned int)sce_paf_strtoul(text.c_str(), &endPtr, 16);
	if (endPtr == NULL || *endPtr != '\0')
		return false;
	*outColor = (int)themeSwapRgbaColor(parsed);
	return true;
}

/* (0x813BD44A) */
static bool themeParseIconParam(LsdbThemeInfo *info, NsxXmlNode &node,
				IconParam *outIcon, bool allowFilePath)
{
	for (paf::list<NsxXmlNode>::iterator it = node.children.begin();
	     it != node.children.end(); ++it) {
		if (it->name == "m_iconFilePath") {
			if (allowFilePath) {
				if (!themeValidateFileName(it->text))
					return false;
				outIcon->m_iconFilePath = it->text;
			}
		} else if (it->name == "m_pagePos") {
			if (info->m_slot == 1) {
				char *endPtr = NULL;
				int val = (int)sce_paf_strtol(it->text.c_str(), &endPtr, 10);
				if (endPtr == NULL || *endPtr != '\0')
					return false;
				outIcon->m_pagePos = val;
			}
		} else if (it->name == "m_iconPos") {
			if (info->m_slot == 1) {
				char *endPtr = NULL;
				int val = (int)sce_paf_strtol(it->text.c_str(), &endPtr, 10);
				if (endPtr == NULL || *endPtr != '\0')
					return false;
				outIcon->m_iconPos = val;
			}
		}
	}
	return true;
}

/* (0x813BD564) */
static bool themeParseBackgroundParam(int formatVer, NsxXmlNode &node,
				      BackgroundParam *outBg)
{
	for (paf::list<NsxXmlNode>::iterator it = node.children.begin();
	     it != node.children.end(); ++it) {
		if (it->name == "m_imageFilePath") {
			if (!themeValidateFileName(it->text))
				return false;
			outBg->m_imageFilePath = it->text;
		} else if (it->name == "m_thumbnailFilePath") {
			if (!themeValidateFileName(it->text))
				return false;
			outBg->m_thumbnailFilePath = it->text;
		} else if (it->name == "m_waveType") {
			char *endPtr = NULL;
			int val = (int)sce_paf_strtol(it->text.c_str(), &endPtr, 10);
			if (endPtr == NULL || *endPtr != '\0')
				return false;
			outBg->m_waveType = val;
		} else if (it->name == "m_fontColor") {
			if (formatVer >= 100) {
				int color = 0;
				if (!themeParseHexColor(it->text, &color))
					return false;
				outBg->m_fontColor = (unsigned int)color;
			}
		} else if (it->name == "m_fontShadow") {
			if (formatVer >= 100) {
				char *endPtr = NULL;
				unsigned int val = (unsigned int)sce_paf_strtoul(it->text.c_str(),
										 &endPtr, 10);
				if (endPtr == NULL || *endPtr != '\0')
					return false;
				if (val == 0)
					outBg->m_fontShadow = false;
				else if (val == 1)
					outBg->m_fontShadow = true;
				else
					return false;
			}
		}
	}
	return true;
}

/* (0x813BD77E) */
static bool themeParseHomeProperty(LsdbThemeInfo *info, int formatVer, NsxXmlNode &node)
{
	HomeProperty &home = info->m_homeProperty;
	for (paf::list<NsxXmlNode>::iterator it = node.children.begin();
	     it != node.children.end(); ++it) {
		if (it->name == "m_bgParam") {
			int bgIdx = 0;
			for (paf::list<NsxXmlNode>::iterator bgIt = it->children.begin();
			     bgIt != it->children.end(); ++bgIt) {
				if (bgIt->name == "BackgroundParam" && bgIdx < LSDB_THEME_PAGES) {
					if (!themeParseBackgroundParam(formatVer, *bgIt,
								       &home.m_bgParam[bgIdx]))
						return false;
					++bgIdx;
				}
			}
		} else if (it->name == "m_browser") {
			if (!themeParseIconParam(info, *it, &home.m_browser, true)) return false;
		} else if (it->name == "m_video") {
			if (!themeParseIconParam(info, *it, &home.m_video, true)) return false;
		} else if (it->name == "m_music") {
			if (!themeParseIconParam(info, *it, &home.m_music, true)) return false;
		} else if (it->name == "m_ps3Link") {
			if (!themeParseIconParam(info, *it, &home.m_ps3Link, true)) return false;
		} else if (it->name == "m_party") {
			if (!themeParseIconParam(info, *it, &home.m_party, true)) return false;
		} else if (it->name == "m_trophy") {
			if (!themeParseIconParam(info, *it, &home.m_trophy, true)) return false;
		} else if (it->name == "m_map") {
			if (!themeParseIconParam(info, *it, &home.m_map, false)) return false;
		} else if (it->name == "m_near") {
			if (!themeParseIconParam(info, *it, &home.m_near, true)) return false;
		} else if (it->name == "m_hostCollabo") {
			if (!themeParseIconParam(info, *it, &home.m_hostCollabo, true)) return false;
		} else if (it->name == "m_welcomePark") {
			if (!themeParseIconParam(info, *it, &home.m_welcomePark, false)) return false;
		} else if (it->name == "m_ps4Link") {
			if (!themeParseIconParam(info, *it, &home.m_ps4Link, true)) return false;
		} else if (it->name == "m_friend") {
			if (!themeParseIconParam(info, *it, &home.m_friend, true)) return false;
		} else if (it->name == "m_email") {
			if (!themeParseIconParam(info, *it, &home.m_email, true)) return false;
		} else if (it->name == "m_calendar") {
			if (!themeParseIconParam(info, *it, &home.m_calendar, true)) return false;
		} else if (it->name == "m_store") {
			if (!themeParseIconParam(info, *it, &home.m_store, false)) return false;
		} else if (it->name == "m_message") {
			if (!themeParseIconParam(info, *it, &home.m_message, true)) return false;
		} else if (it->name == "m_parental") {
			if (!themeParseIconParam(info, *it, &home.m_parental, true)) return false;
		} else if (it->name == "m_camera") {
			if (!themeParseIconParam(info, *it, &home.m_camera, true)) return false;
		} else if (it->name == "m_settings") {
			if (!themeParseIconParam(info, *it, &home.m_settings, true)) return false;
		} else if (it->name == "m_power") {
			if (!themeParseIconParam(info, *it, &home.m_power, true)) return false;
		} else if (it->name == "m_bgmFilePath") {
			if (!themeValidateFileName(it->text)) return false;
			home.m_bgmFilePath = it->text;
		} else if (it->name == "m_basePageFilePath") {
			if (formatVer >= 100) {
				if (!themeValidateFileName(it->text)) return false;
				home.m_basePageFilePath = it->text;
			}
		} else if (it->name == "m_curPageFilePath") {
			if (formatVer >= 100) {
				if (!themeValidateFileName(it->text)) return false;
				home.m_curPageFilePath = it->text;
			}
		}
	}
	return true;
}

/* (0x813BDD66 & 0x813BDE32) */
static bool themeParseThemeText(Locale locale, bool loadAllLangs, NsxXmlNode &node,
				LsdbThemeText *outText)
{
	for (paf::list<NsxXmlNode>::iterator it = node.children.begin();
	     it != node.children.end(); ++it) {
		if (it->name == "m_default") {
			if (it->text.size() > 128U)
				return false;
			paf::wstring converted;
			common::Utf8ToUtf16(it->text, &converted, 0);
			outText->m_default = converted;
		} else if (it->name == "m_param") {
			if (loadAllLangs) {
				for (paf::list<NsxXmlNode>::iterator langIt = it->children.begin();
				     langIt != it->children.end(); ++langIt) {
					for (int idx = 0; idx < LSDB_THEME_LANGUAGES; idx++) {
						if (langIt->name == kThemeLangTagNames[idx]) {
							if (!langIt->text.empty()) {
								if (langIt->text.size() > 128U)
									return false;
								paf::wstring converted;
								common::Utf8ToUtf16(langIt->text, &converted, 0);
								outText->m_language[idx] = converted;
							}
							break;
						}
					}
				}
			} else {
				int langIdx = (int)locale;
				if (langIdx >= 0 && langIdx < LSDB_THEME_LANGUAGES) {
					for (paf::list<NsxXmlNode>::iterator langIt = it->children.begin();
					     langIt != it->children.end(); ++langIt) {
						if (langIt->name == kThemeLangTagNames[langIdx]) {
							if (langIt->text.size() > 128U)
								return false;
							paf::wstring converted;
							common::Utf8ToUtf16(langIt->text, &converted, 0);
							outText->m_language[langIdx] = converted;
							break;
						}
					}
				}
			}
		}
	}
	return true;
}

/* (0x813BDFB4) */
static bool themeParseInfomationProperty(LsdbThemeInfo *info, Locale locale, bool loadAllLangs,
					 NsxXmlNode &node)
{
	InfomationProperty &prop = info->m_infomationProperty;
	for (paf::list<NsxXmlNode>::iterator it = node.children.begin();
	     it != node.children.end(); ++it) {
		if (it->name == "m_contentVer") {
			int ver = themeParseVersionNumber(it->text);
			prop.m_contentVer = ver;
			if (ver < 0)
				return false;
		} else if (it->name == "m_title") {
			if (!themeParseThemeText(locale, loadAllLangs, *it, &prop.m_title))
				return false;
		} else if (it->name == "m_provider") {
			if (!themeParseThemeText(locale, loadAllLangs, *it, &prop.m_provider))
				return false;
		} else if (it->name == "m_packageImageFilePath") {
			if (!themeValidateFileName(it->text))
				return false;
			prop.m_packageImageFilePath = it->text;
		} else if (it->name == "m_startPreviewFilePath") {
			if (!themeValidateFileName(it->text))
				return false;
			prop.m_startPreviewFilePath = it->text;
		} else if (it->name == "m_homePreviewFilePath") {
			if (!themeValidateFileName(it->text))
				return false;
			prop.m_homePreviewFilePath = it->text;
		}
	}
	return true;
}

/* (0x813BE144) */
static bool themeParseInfomationBarProperty(LsdbThemeInfo *info, int formatVer,
					    NsxXmlNode &node)
{
	InfomationBarProperty &bar = info->m_infomationBarProperty;
	for (paf::list<NsxXmlNode>::iterator it = node.children.begin();
	     it != node.children.end(); ++it) {
		if (it->name == "m_barColor") {
			if (!themeParseHexColor(it->text, &bar.m_barColor)) return false;
		} else if (it->name == "m_indicatorColor") {
			if (!themeParseHexColor(it->text, &bar.m_indicatorColor)) return false;
		} else if (it->name == "m_noticeFontColor") {
			if (formatVer >= 100 && !themeParseHexColor(it->text, &bar.m_noticeFontColor))
				return false;
		} else if (it->name == "m_noticeGlowColor") {
			if (formatVer >= 100 && !themeParseHexColor(it->text, &bar.m_noticeGlowColor))
				return false;
		} else if (it->name == "m_noNoticeFilePath") {
			if (formatVer >= 100) {
				if (!themeValidateFileName(it->text)) return false;
				bar.m_noNoticeFilePath = it->text;
			}
		} else if (it->name == "m_newNoticeFilePath") {
			if (formatVer >= 100) {
				if (!themeValidateFileName(it->text)) return false;
				bar.m_newNoticeFilePath = it->text;
			}
		}
	}
	return true;
}

/* (0x813BE388) */
static bool themeParseStartScreenProperty(LsdbThemeInfo *info, NsxXmlNode &node)
{
	StartScreenProperty &start = info->m_startScreenProperty;
	for (paf::list<NsxXmlNode>::iterator it = node.children.begin();
	     it != node.children.end(); ++it) {
		if (it->name == "m_filePath") {
			if (!themeValidateFileName(it->text)) return false;
			start.m_filePath = it->text;
		} else if (it->name == "m_dateColor") {
			if (!themeParseHexColor(it->text, &start.m_dateColor)) return false;
		} else if (it->name == "m_dateLayout") {
			char *endPtr = NULL;
			int val = (int)sce_paf_strtoul(it->text.c_str(), &endPtr, 10);
			if (endPtr == NULL || *endPtr != '\0') return false;
			start.m_dateLayout = val;
		} else if (it->name == "m_notifyBgColor") {
			if (!themeParseHexColor(it->text, &start.m_notifyBgColor)) return false;
		} else if (it->name == "m_notifyBorderColor") {
			if (!themeParseHexColor(it->text, &start.m_notifyBorderColor)) return false;
		} else if (it->name == "m_notifyFontColor") {
			if (!themeParseHexColor(it->text, &start.m_notifyFontColor)) return false;
		}
	}
	return true;
}

/* (0x813BD2C0) */
ShellThemeReader::ShellThemeReader(const char *path, const Locale &locale)
	: m_path(path != NULL ? path : ""),
	  m_locale(locale),
	  m_state(0),
	  m_loaded(false),
	  m_loadOption(false)
{
	m_path += "/";
}

/* (0x813BD362) */
ShellThemeReader::~ShellThemeReader()
{
}

/* (0x813BFA2E) */
void ShellThemeReader::SetLoadOption(bool on)
{
	m_loadOption = on;
}

/* (0x811F2B20) */
common::SharedPtr<LsdbThemeInfo> &ShellThemeReader::GetThemeInfo()
{
	return m_info;
}

/* (0x813BE860) */
int ShellThemeReader::Load()
{
	common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(0x2000);
	if (buf.get() == NULL)
		return kThemeReaderErrorNoMemory;

	string xmlPath = m_path;
	xmlPath += SHELL_THEME_FILE;

	int openErr = 0;
	common::SharedPtr<File> file = LocalFile::Open(xmlPath.c_str(), SCE_O_RDONLY, 0, &openErr);
	if (file.get() == NULL)
		return kThemeReaderErrorOpenFailed;

	if (file->GetFileSize() > 0x8000)
		return kThemeReaderErrorTooLarge;

	m_info.reset(new LsdbThemeInfo());
	if (m_info.get() == NULL)
		return kThemeReaderErrorNoMemory;

	m_info->path = m_path;

	FileStat stat;
	if (file->Getstat(&stat) == 0)
		m_info->m_lastModifiedTime = datetime::DateTime::ConvertDateTimeToRtcTick(stat.st_mtime);

	NsxXmlNode rootNode;
	NsxXmlSaxParser saxParser;
	shellXmlSaxCreate(&saxParser, &rootNode, true, true);

	bool checkedHeader = false;
	bool isFinal = false;
	while (!isFinal) {
		int bytesRead = file->Read(buf->GetAddr(), buf->GetSize());
		if (bytesRead < 0) {
			m_info.reset();
			shellXmlSaxDestroy(&saxParser);
			return kThemeReaderErrorParseFailed;
		}
		isFinal = (bytesRead < (int)buf->GetSize());
		if (shellXmlSaxParse(&saxParser, (const char *)buf->GetAddr(),
				     (SceSize)bytesRead, isFinal) != 0) {
			m_info.reset();
			shellXmlSaxDestroy(&saxParser);
			return kThemeReaderErrorParseFailed;
		}

		if (!checkedHeader) {
			checkedHeader = true;
			if (rootNode.name != "theme") {
				m_info.reset();
				shellXmlSaxDestroy(&saxParser);
				return kThemeReaderErrorParseFailed;
			}
			bool hasFormatVer = false;
			for (paf::map<paf::string, paf::string>::iterator aIt =
				     rootNode.attrs.begin();
			     aIt != rootNode.attrs.end(); ++aIt) {
				if (aIt->first == "format-ver") {
					int ver = themeParseVersionNumber(aIt->second);
					if (ver != 99 && ver != 100) {
						m_info.reset();
						shellXmlSaxDestroy(&saxParser);
						return kThemeReaderErrorParseFailed;
					}
					m_info->m_contentPath = aIt->second;
					m_info->m_status = ver;
					m_state = ver;
					hasFormatVer = true;
				} else if (aIt->first == "package") {
					char *endPtr = NULL;
					unsigned int pkg = (unsigned int)sce_paf_strtol(
						aIt->second.c_str(), &endPtr, 10);
					if (endPtr == NULL || *endPtr != '\0' || pkg > 1U) {
						m_info.reset();
						shellXmlSaxDestroy(&saxParser);
						return kThemeReaderErrorParseFailed;
					}
					m_info->m_slot = (int)pkg;
				}
			}
			if (!hasFormatVer || m_info->m_slot == -1) {
				m_info.reset();
				shellXmlSaxDestroy(&saxParser);
				return kThemeReaderErrorParseFailed;
			}
		}

		paf::list<NsxXmlNode>::iterator it = rootNode.children.begin();
		while (it != rootNode.children.end()) {
			if (!isFinal && rootNode.children.size() == 1)
				break;
			bool ok = true;
			if (it->name == "StartScreenProperty")
				ok = themeParseStartScreenProperty(m_info.get(), *it);
			else if (it->name == "InfomationBarProperty")
				ok = themeParseInfomationBarProperty(m_info.get(), m_state, *it);
			else if (it->name == "InfomationProperty")
				ok = themeParseInfomationProperty(m_info.get(), m_locale,
								  m_loadOption, *it);
			else if (it->name == "HomeProperty")
				ok = themeParseHomeProperty(m_info.get(), m_state, *it);
			if (!ok) {
				m_info.reset();
				shellXmlSaxDestroy(&saxParser);
				return kThemeReaderErrorParseFailed;
			}
			paf::list<NsxXmlNode>::iterator nextIt = it;
			++nextIt;
			rootNode.children.erase(it);
			it = nextIt;
		}
	}

	shellXmlSaxDestroy(&saxParser);
	return 0;
}
