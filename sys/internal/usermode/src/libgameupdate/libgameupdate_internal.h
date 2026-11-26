/*
 * libgameupdate_internal.h: internal structures, constants, and declarations for
 * SceLibGameUpdate (vs0:sys/external/libgameupdate.suprx).
 */
#ifndef LIBGAMEUPDATE_INTERNAL_H
#define LIBGAMEUPDATE_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <libhmac.h>
#include <rtc.h>
#include <regmgr/regmgr.h>
#include <np_common/np_common.h>
#include <libgameupdate.h>

/* Internal SceGameUpdate error codes (0x804120xx) */
#define GAMEUPDATE_ERROR_INVALID_STATE		(-2143213440)	/* 0x80412080 */
#define GAMEUPDATE_ERROR_NO_MEMORY		(-2143213439)	/* 0x80412081 */

/* Target platform modes */
#define GAMEUPDATE_PLATFORM_DEFAULT		0
#define GAMEUPDATE_PLATFORM_PSP			1
#define GAMEUPDATE_PLATFORM_PSP2		2

/* Internal check result status values */
#define GAMEUPDATE_STATUS_DOWNLOADING		0
#define GAMEUPDATE_STATUS_UP_TO_DATE		1
#define GAMEUPDATE_STATUS_AVAILABLE		2
#define GAMEUPDATE_STATUS_DISCONTINUED		3
#define GAMEUPDATE_STATUS_SYSUPDATE_NEEDED	4
#define GAMEUPDATE_STATUS_READY_TO_INSTALL	5

/* Info flags */
#define GAMEUPDATE_INFO_FLAG_SIGNOFF		0x00000002u
#define GAMEUPDATE_INFO_FLAG_DEBUG_MOCK		0x00000080u

/* Init option flags */
#define GAMEUPDATE_OPT_SKIP_BGDL_CHECK		0x00000001u

/* Package distribution types */
#define GAMEUPDATE_PKG_CUMULATIVE		0
#define GAMEUPDATE_PKG_INCREMENTAL		1

/* Download client error codes (0x80103Axx / 0x80103Bxx) */
#define DOWNLOAD_CLIENT_ERROR_INVALID_ARG	(-2146420215)	/* 0x80103A09 */
#define DOWNLOAD_SVC_ERROR_INTERNAL		(-2146419968)	/* 0x80103B00 */
#define DOWNLOAD_SVC_ERROR_ALREADY_OPEN		(-2146419966)	/* 0x80103B02 */
#define DOWNLOAD_SVC_ERROR_NOT_OPEN		(-2146419964)	/* 0x80103B04 */
#define DOWNLOAD_SVC_ERROR_INVALID_ARG		(-2146419962)	/* 0x80103B06 */
#define DOWNLOAD_SVC_ERROR_ALLOC_FAILED		(-2146419961)	/* 0x80103B07 */
#define DOWNLOAD_SVC_ERROR_NOT_INITIALIZED	(-2146419960)	/* 0x80103B08 */
#define DOWNLOAD_SVC_ERROR_CONNECT_REFUSED	(-2146419935)	/* 0x80103B21 */
#define DOWNLOAD_SVC_ERROR_NO_MEMORY		(-2146419920)	/* 0x80103B30 */

/* Download task status (0x130 = 304 bytes) */
struct DownloadTaskStatus {
	SceUInt32 taskState;			/* +0x000 */
	SceUInt32 taskType;			/* +0x004 */
	SceInt32 errorCode;			/* +0x008 */
	SceUInt16 versionMajor;			/* +0x00C */
	SceUInt16 versionMinor;			/* +0x00E */
	char titleId[32];			/* +0x010 */
	char contentName[256];			/* +0x030 */

	DownloadTaskStatus();
	~DownloadTaskStatus();
};

/* Patch-check cache entry sent to SceShellSvc (16 bytes) */
struct PatchCheckCacheEntry {
	SceUInt64 networkTick;
	SceInt32 status;
	SceUInt32 flags;
};

/* download_client.cpp */
int downloadFindPatchTask(const char *titleId, DownloadTaskStatus *outStatus);
int downloadDeletePatchTask(const char *titleId);

/* shell_svc_patchcheck.cpp */
int shellSvcPatchCheckSetCache(const char *titleId, const PatchCheckCacheEntry *entry,
			       int updateMask);

#endif /* LIBGAMEUPDATE_INTERNAL_H */
