/*
 * SceNotificationUtil's parameters (no SDK header has them: the library is the system's own).
 * The shell reads them whole; this module checks a notification's text and string and keeps a
 * progress notification's event handler. The names are ours.
 */
#ifndef NOTIFICATION_UTIL_NOTIFICATION_UTIL_H
#define NOTIFICATION_UTIL_NOTIFICATION_UTIL_H

#include <scetypes.h>

/* a notification the parameters do not describe */
#define SCE_NOTIFICATION_UTIL_ERROR_INVALID_ARGUMENT	-2146409727	/* 0x80106301 */

#define SCE_NOTIFICATION_UTIL_TEXT_LENGTH	0x40	/* characters, with the terminator */
#define SCE_NOTIFICATION_UTIL_DATA_SIZE		1000

typedef struct SceNotificationUtilSendParam {
	SceWChar16 text[SCE_NOTIFICATION_UTIL_TEXT_LENGTH];	/* one line, or two split by '\n' */
	char data[SCE_NOTIFICATION_UTIL_DATA_SIZE];		/* a string the shell receives */
	SceUInt32 reserved[2];					/* must be 0 */
} SceNotificationUtilSendParam;

_Static_assert(sizeof(SceNotificationUtilSendParam) == 0x470, "SceNotificationUtilSendParam");

/* called with its argument when the shell reports an event of the progress notification */
typedef int (*SceNotificationUtilProgressEventHandler)(void *pArg);

typedef struct SceNotificationUtilProgressBeginParam {
	SceUInt8 notification[0x4e8];	/* the notification the shell shows, read by the shell only */
	SceNotificationUtilProgressEventHandler eventHandler;
	void *pEventHandlerArg;
	SceUInt8 options[0x80];		/* read by the shell only */
} SceNotificationUtilProgressBeginParam;

_Static_assert(sizeof(SceNotificationUtilProgressBeginParam) == 0x570,
	       "SceNotificationUtilProgressBeginParam");

typedef struct SceNotificationUtilProgressUpdateParam {
	SceUInt8 notification[0x104];	/* read by the shell only */
} SceNotificationUtilProgressUpdateParam;

typedef struct SceNotificationUtilProgressFinishParam {
	SceUInt8 notification[0x4e8];	/* read by the shell only */
} SceNotificationUtilProgressFinishParam;

#endif
