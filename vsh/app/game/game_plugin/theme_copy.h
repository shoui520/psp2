/*
 * A theme's extracted package moved into place for its installer (theme_copy.cpp).
 */

#ifndef GAME_PLUGIN_THEME_COPY_H
#define GAME_PLUGIN_THEME_COPY_H

#include <scetypes.h>
#include <std/string>

#include "game_plugin.h"

/*
 * A theme's extracted package moved into ux0:theme, the theme it replaces kept in
 * ux0:temp/promote until it is done, and its license written (theme_copy.cpp,
 * 0x8103C120..0x8103C5CC), 32 bytes. Each step's part of the progress is a share of the caller's.
 */
class GameThemeCopier {
public:
	GameThemeCopier(SceNpDrmInstallInfo *info, paf::string *errorTitleId);
	virtual ~GameThemeCopier();
	virtual int Install(const paf::string &path, const paf::string &themeId,
			    paf::string *themeDir, paf::string *contentPath,
			    GameJobProgress *progress, float share, int arg6, int arg7);

	int Commit(paf::string *themeDir, paf::string *contentPath, GameJobProgress *progress,
		   float share, int arg4, int arg5);

	SceNpDrmInstallInfo *m_info;
	paf::string m_themeId;
	paf::string *m_errorTitleId;
	float m_moveShare;			/* the move's part of the step */
	float m_licenseShare;			/* the license's */
};

typedef char GameThemeCopierSize[sizeof(GameThemeCopier) == 32 ? 1 : -1];

/*
 * PLACEHOLDER: a content's license written into dir under the name of its content ID, the file
 * left in path (0x8103ECEE, in a file of the plugin not written yet).
 */
int gameLicenseWrite(SceUInt32 contentType, const void *license, SceSize licenseSize,
		     const paf::string *dir, paf::string *path, paf::string *detail, int arg6,
		     int arg7);

#endif
