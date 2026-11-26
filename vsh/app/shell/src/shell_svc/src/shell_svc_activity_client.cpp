/*
 * shell_svc_activity_client.cpp: ShellSvc IPC client wrappers for posting
 * unlocked trophies to the Activity feed (0x81362108..0x813623D7).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <shell_svc/shellsvc.h>

#define ACTIVITY_CLIENT_ERR_INVALID_ARG		-2146430975	/* 0x80101001 */

#define SHELL_SVC_METHOD_POST_TROPHY		0x00080001U
#define SHELL_SVC_METHOD_POST_HIDDEN_TROPHY	0x00080002U
#define SHELL_SVC_METHOD_POST_PLATINUM_TROPHY	0x00080003U

static size_t activityClientStrLenWithNull(const char *str)
{
	const char *cur = str;
	while (*cur != '\0')
		++cur;
	return (size_t)(cur - str) + 1U;
}

/* 0x813621B2 */
static int invokePostTrophyStandard(IPMI::Client *client, const char *titleId,
				    const char *gameTitle, const char *trophyName,
				    int grade, const char *uri)
{
	int gradePayload = grade;
	IPMI::DataInfo inBufs[5];
	inBufs[0].pData = &gradePayload;
	inBufs[0].dataSize = sizeof(gradePayload);
	inBufs[1].pData = titleId;
	inBufs[1].dataSize = activityClientStrLenWithNull(titleId);
	inBufs[2].pData = gameTitle;
	inBufs[2].dataSize = activityClientStrLenWithNull(gameTitle);
	inBufs[3].pData = trophyName;
	inBufs[3].dataSize = activityClientStrLenWithNull(trophyName);
	inBufs[4].pData = uri;
	inBufs[4].dataSize = activityClientStrLenWithNull(uri);

	int methodResult = 0;
	int err = client->invokeSyncMethod(SHELL_SVC_METHOD_POST_TROPHY,
					   inBufs, 5, &methodResult, NULL, 0);
	if (err == 0)
		return methodResult;
	return err;
}

/* 0x81362286 */
static int invokePostTrophyHidden(IPMI::Client *client, const char *titleId,
				  const char *gameTitle, const char *uri)
{
	IPMI::DataInfo inBufs[3];
	inBufs[0].pData = titleId;
	inBufs[0].dataSize = activityClientStrLenWithNull(titleId);
	inBufs[1].pData = gameTitle;
	inBufs[1].dataSize = activityClientStrLenWithNull(gameTitle);
	inBufs[2].pData = uri;
	inBufs[2].dataSize = activityClientStrLenWithNull(uri);

	int methodResult = 0;
	int err = client->invokeSyncMethod(SHELL_SVC_METHOD_POST_HIDDEN_TROPHY,
					   inBufs, 3, &methodResult, NULL, 0);
	if (err == 0)
		return methodResult;
	return err;
}

/* 0x8136231E */
static int invokePostTrophyPlatinum(IPMI::Client *client, const char *titleId,
				    const char *gameTitle, const char *uri,
				    const char *trophyName)
{
	IPMI::DataInfo inBufs[4];
	inBufs[0].pData = titleId;
	inBufs[0].dataSize = activityClientStrLenWithNull(titleId);
	inBufs[1].pData = gameTitle;
	inBufs[1].dataSize = activityClientStrLenWithNull(gameTitle);
	inBufs[2].pData = uri;
	inBufs[2].dataSize = activityClientStrLenWithNull(uri);
	inBufs[3].pData = trophyName;
	inBufs[3].dataSize = activityClientStrLenWithNull(trophyName);

	int methodResult = 0;
	int err = client->invokeSyncMethod(SHELL_SVC_METHOD_POST_PLATINUM_TROPHY,
					   inBufs, 4, &methodResult, NULL, 0);
	if (err == 0)
		return methodResult;
	return err;
}

/* 0x81362108 */
int npActivityPostTrophyStandard(const char *titleId, const char *gameTitle,
				 const char *trophyName, int grade,
				 const char *uri)
{
	if (titleId == NULL || gameTitle == NULL || trophyName == NULL || uri == NULL)
		return ACTIVITY_CLIENT_ERR_INVALID_ARG;

	IPMI::Client *client = SceShellSvc_B31E7F1C();
	return invokePostTrophyStandard(client, titleId, gameTitle, trophyName, grade, uri);
}

/* 0x81362148 */
int npActivityPostTrophyHidden(const char *titleId, const char *gameTitle,
			       const char *uri)
{
	if (titleId == NULL || gameTitle == NULL || uri == NULL)
		return ACTIVITY_CLIENT_ERR_INVALID_ARG;

	IPMI::Client *client = SceShellSvc_B31E7F1C();
	return invokePostTrophyHidden(client, titleId, gameTitle, uri);
}

/* 0x81362170 */
int npActivityPostTrophyPlatinum(const char *titleId, const char *gameTitle,
				 const char *uri, const char *trophyName)
{
	if (titleId == NULL || gameTitle == NULL || uri == NULL)
		return ACTIVITY_CLIENT_ERR_INVALID_ARG;

	IPMI::Client *client = SceShellSvc_B31E7F1C();
	if (trophyName == NULL)
		trophyName = "";
	return invokePostTrophyPlatinum(client, titleId, gameTitle, uri, trophyName);
}
