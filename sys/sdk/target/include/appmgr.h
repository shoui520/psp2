/*
 * appmgr.h: the application manager's error codes, system events and application state, under
 * Sony's names and with Sony's definitions (SDK Release 03.570.011, target/include/appmgr.h).
 * SceAppMgr's system calls take these; the library's functions come with the user-mode SDK
 * headers.
 */
#ifndef _SCE_APPMGR_APPMGR_H
#define _SCE_APPMGR_APPMGR_H

#include <scetypes.h>

#if !defined(_LANGUAGE_ASSEMBLY)

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_APPMGR_MAX_APP_NAME_LENGTH		(31)

/* system events */
#define SCE_APPMGR_SYSTEMEVENT_ON_RESUME		(0x10000003)
#define SCE_APPMGR_SYSTEMEVENT_ON_STORE_PURCHASE	(0x10000004)
#define SCE_APPMGR_SYSTEMEVENT_ON_NP_MESSAGE_ARRIVED	(0x10000005)
#define SCE_APPMGR_SYSTEMEVENT_ON_STORE_REDEMPTION	(0x10000006)

/* error codes */
#define SCE_APPMGR_ERROR_BUSY			-2139086848	/* 0x80802000 */
#define SCE_APPMGR_ERROR_STATE			-2139086829	/* 0x80802013 */
#define SCE_APPMGR_ERROR_NULL_POINTER		-2139086826	/* 0x80802016 */
#define SCE_APPMGR_ERROR_INVALID		-2139086822	/* 0x8080201A */
#define SCE_APPMGR_ERROR_INVALID_SELF_PATH	-2139086818	/* 0x8080201E */
#define SCE_APPMGR_ERROR_TOO_LONG_ARGV		-2139086819	/* 0x8080201D */
#define SCE_APPMGR_ERROR_BGM_PORT_BUSY		-2139082752	/* 0x80803000 */

/* the information bar */
#define SCE_APPMGR_INFOBAR_VISIBILITY_INVISIBLE		(0)
#define SCE_APPMGR_INFOBAR_VISIBILITY_VISIBLE		(1)
#define SCE_APPMGR_INFOBAR_COLOR_BLACK			(0)
#define SCE_APPMGR_INFOBAR_COLOR_WHITE			(1)
#define SCE_APPMGR_INFOBAR_TRANSPARENCY_OPAQUE		(0)
#define SCE_APPMGR_INFOBAR_TRANSPARENCY_TRANSLUCENT	(1)

typedef struct SceAppMgrSystemEvent {
	SceInt32 systemEvent;	/* SCE_APPMGR_SYSTEMEVENT_* */
	union {
		SceUInt8 reserved[60];
	} data;
} SceAppMgrSystemEvent;

typedef struct SceAppMgrAppState {
	SceUInt32 systemEventNum;
	SceUInt32 appEventNum;
	SceBool isSystemUiOverlaid;
	SceUInt8 reserved[128 - sizeof(SceUInt32) * 2 - sizeof(SceBool)];
} SceAppMgrAppState;

typedef struct SceAppMgrNetworkDisconnectionWarningDialogStateOptParam {
	SceUInt8 reserved[128];
} SceAppMgrNetworkDisconnectionWarningDialogStateOptParam;

typedef struct SceAppMgrLoadExecOptParam {
	int reserved[256 / 4];
} SceAppMgrLoadExecOptParam;

/*
 * not in the SDK: the status of a running application (NULL: the caller's own; the layout is
 * SceAppMgrAppStatus in <appmgr/appmgr.h>), and whether a process is the shell and crashed
 */
int sceAppMgrGetStatusByName(const char *name, void *status);
int sceAppMgrIsPidShellAndCrashed(SceUID pid);

#ifdef __cplusplus
}
#endif

#endif /* !_LANGUAGE_ASSEMBLY */

#endif /* _SCE_APPMGR_APPMGR_H */
