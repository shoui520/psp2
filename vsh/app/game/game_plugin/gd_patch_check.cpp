/*
 * The game plugin's check of an application's patch against the application and against the
 * patch it applies on (0x8108A680..0x8108A81C). The file's name and the function's are ours (no
 * record names them).
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/libkernel.h>
#include <std/string>

#include "game_plugin.h"

/* PLACEHOLDER names: what the check returns when the patch does not fit (values only) */
#define GAME_PATCH_ERROR_APP_MISMATCH		-2138629116	/* 0x80870C04 */
#define GAME_PATCH_ERROR_BASE_MISMATCH		-2138629115	/* 0x80870C05 */
#define GAME_PATCH_ERROR_BASE_VERSION		-2138629114	/* 0x80870C06 */
#define GAME_PATCH_ERROR_NO_BASE		-2138629113	/* 0x80870C07 */
#define GAME_PATCH_ERROR_FLAGS			-2138629112	/* 0x80870C08 */

/* what step reads which package, for a caller that reports the one that failed */
#define GAME_PATCH_STEP_APP			1
#define GAME_PATCH_STEP_BASE			2
#define GAME_PATCH_STEP_PATCH			3

/*
 * The patch at patchPath checked against the application at appPath and, when it applies on
 * another patch, against that one at basePath (NULL when none is installed), each path ending in
 * '/', with buf as work memory: 1 for a patch that applies on the application, 2 for one that
 * applies on the installed patch. step says which package was read last.
 */
int gamePatchCheck(const char *appPath, const char *basePath, const char *patchPath,
		   int reserved, void *buf, SceSize bufSize, int *step)
{
	GamePackageHeadInfo app;
	GamePackageHeadInfo base;
	GamePackageHeadInfo patch;
	int ret;

	if (step != NULL)
		*step = GAME_PATCH_STEP_APP;

	ret = gameNpDrmGetPackageHeader(&app, sizeof(app), appPath, buf, bufSize);
	if (ret < 0)
		return ret;

	if (step != NULL)
		*step = GAME_PATCH_STEP_PATCH;

	ret = gameNpDrmGetPackageHeader(&patch, sizeof(patch), patchPath, buf, bufSize);
	if (ret < 0)
		return ret;

	if (app.info.headerFields[0] != patch.info.headerFields[0])
		return GAME_PATCH_ERROR_APP_MISMATCH;
	if (sceClibMemcmp(app.info.contentId, patch.info.contentId,
			  sizeof(app.info.contentId)) != 0)
		return GAME_PATCH_ERROR_APP_MISMATCH;
	if (sceClibMemcmp(app.keyDigest, patch.keyDigest, sizeof(app.keyDigest)) != 0)
		return GAME_PATCH_ERROR_APP_MISMATCH;
	if (sceClibMemcmp(app.digest, patch.appDigest, sizeof(app.digest)) != 0)
		return GAME_PATCH_ERROR_APP_MISMATCH;
	if ((app.info.flags ^ patch.info.flags) & GAME_PACKAGE_FLAG_CARD)
		return GAME_PATCH_ERROR_FLAGS;

	if (patch.baseVersion == 0)
		return 1;

	if (step != NULL)
		*step = GAME_PATCH_STEP_BASE;

	if (basePath == NULL)
		return GAME_PATCH_ERROR_NO_BASE;

	ret = gameNpDrmGetPackageHeader(&base, sizeof(base), basePath, buf, bufSize);
	if (ret < 0)
		return ret;

	if (base.version != patch.baseVersion)
		return GAME_PATCH_ERROR_BASE_VERSION;
	if (base.info.headerFields[0] != patch.info.headerFields[0])
		return GAME_PATCH_ERROR_BASE_MISMATCH;
	if (sceClibMemcmp(base.info.contentId, patch.info.contentId,
			  sizeof(base.info.contentId)) != 0)
		return GAME_PATCH_ERROR_BASE_MISMATCH;
	if (sceClibMemcmp(base.keyDigest, patch.keyDigest, sizeof(base.keyDigest)) != 0)
		return GAME_PATCH_ERROR_BASE_MISMATCH;
	if (sceClibMemcmp(base.digest, patch.baseDigest, sizeof(base.digest)) != 0)
		return GAME_PATCH_ERROR_BASE_MISMATCH;
	if ((base.info.flags ^ patch.info.flags) & GAME_PACKAGE_FLAG_CARD)
		return GAME_PATCH_ERROR_FLAGS;

	return 2;
}
