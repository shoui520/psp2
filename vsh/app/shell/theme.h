/*
 * theme.h: the shell's theme (theme.cpp) and the theme reader it loads a theme's theme.xml
 * with. The reader's code is the shell's (0x813BD2C0...), shared with the settings
 * application. Every name here is ours (no record names them).
 */
#ifndef _SHELL_THEME_H
#define _SHELL_THEME_H

#include <scetypes.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <framework.h>

#include "src/livespace_db/livespace_db_internal.h"

/* reads a theme's folder: its theme.xml into a LsdbThemeInfo */
class ShellThemeReader {
public:
	/* the folder, and the locale the texts are read in */
	ShellThemeReader(const char *path, const Locale &locale);	/* (0x813BD2C0) */
	~ShellThemeReader();						/* (0x813BD362) */

	void SetLoadOption(bool on);					/* (0x813BFA2E) */
	int Load();							/* (0x813BE860) */
	paf::common::SharedPtr<LsdbThemeInfo> &GetThemeInfo();		/* (0x811F2B20) */

	paf::string m_path;
	Locale m_locale;
	paf::common::SharedPtr<LsdbThemeInfo> m_info;
	int m_state;
	bool m_loaded;
	bool m_loadOption;
};

/* a file of the theme: its folder and name, or nothing for no name (0x813BC6F4) */
void shellThemeInfoGetFilePath(const LsdbThemeInfo *info, paf::string *path, const paf::string &name);

/* the shell's theme: the current one, its change, delete and icons */
void shellThemeCheckLicense(void);			/* an ux0 theme's activation (0x810055B6) */
int shellThemeChange(const char *path);			/* NULL: the default (0x81005F1A) */
int shellThemeDelete(const char *path);			/* an ux0 theme (0x810063B8) */
int shellThemeGetType(void);				/* 100 ux0, 200 pd0, 300 vs0 (0x8100660A) */
int shellThemeGetCount(void);				/* the themes installed (0x810066EA) */
bool shellThemeIconsEnabled(void);			/* (0x810066FE) */
bool shellThemeIsPending(void);				/* (0x8100670A) */
paf::common::SharedPtr<LsdbThemeInfo> *shellThemeGetInfo(void);	/* (0x8100672A) */
bool shellThemeGetIconPath(const char *titleId, paf::string *path, int type);	/* (0x81006734) */

#endif
