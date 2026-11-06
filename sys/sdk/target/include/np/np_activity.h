/*
 * np/np_activity.h: NP Activity, posting a status to the user's activity feed, under Sony's
 * names and with Sony's definitions (SDK Release 03.570.011, target/include/np/np_activity.h).
 */
#ifndef _SCE_NP_ACTIVITY_H_
#define _SCE_NP_ACTIVITY_H_

#include <np/np_common.h>

/* a status message: at most 256 UTF-8 characters and 8 linefeeds, in 1025 bytes with the NUL */
#define SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_NUM_CHARACTERS	256
#define SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_NUM_LINEFEEDS	8
#define SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_SIZE			1025

/* an action URI, in ASCII, with the NUL */
#define SCE_NP_ACTIVITY_STATUS_ACTION_URI_MAX_SIZE		1025

typedef struct SceNpActivityStatusMessage {
	char data[SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_SIZE];
} SceNpActivityStatusMessage;

typedef struct SceNpActivityStatusActionUri {
	char data[SCE_NP_ACTIVITY_STATUS_ACTION_URI_MAX_SIZE];
} SceNpActivityStatusActionUri;

/* a status whose action starts the application */
#define SCE_NP_ACTIVITY_APP_PARAM_MAX_SIZE		897
#define SCE_NP_ACTIVITY_APP_VER_NOT_REQUIRED		0

typedef struct SceNpActivityStatusAppStartupParam {
	SceUInt32 appVersion;	/* "xx.yy" without the period, or SCE_NP_ACTIVITY_APP_VER_NOT_REQUIRED */
	char escapedParam[SCE_NP_ACTIVITY_APP_PARAM_MAX_SIZE];	/* URI-escaped; "" for none */
	SceUInt8 reserved[3];
} SceNpActivityStatusAppStartupParam;

#ifdef __cplusplus
extern "C" {
#endif

int sceNpActivityInit(void *option);
int sceNpActivityTerm(void);
int sceNpActivityPostStatus(const SceNpActivityStatusMessage *message,
			    const SceNpActivityStatusActionUri *actionUri, void *option);
int sceNpActivityPostAppStartupStatus(const SceNpActivityStatusMessage *message,
				      const SceNpActivityStatusAppStartupParam *param,
				      void *option);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NP_ACTIVITY_H_ */
