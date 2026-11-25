/*
 * game_update.h: SceGameUpdate client statically linked into SceShell
 * (0x812A9708-0x812AB957). Fetches title patch metadata (*-ver.xml) over HTTPS
 * with HMAC-SHA256 signed URLs and parses package/hybrid/changeinfo metadata.
 */
#ifndef SHELL_GAME_UPDATE_H
#define SHELL_GAME_UPDATE_H

#include <stddef.h>
#include <scetypes.h>

/* SceGameUpdate error codes (0x804120xx) */
#define SCE_GAMEUPDATE_ERROR_NOT_INITIALIZED	(-2143213567)	/* 0x80412001 */
#define SCE_GAMEUPDATE_ERROR_INVALID_ARGUMENT	(-2143213565)	/* 0x80412003 */
#define SCE_GAMEUPDATE_ERROR_HTTP_FAILED	(-2143213560)	/* 0x80412008 */
#define SCE_GAMEUPDATE_ERROR_XML_SYNTAX		(-2143213559)	/* 0x80412009 */
#define SCE_GAMEUPDATE_ERROR_XML_VALUE		(-2143213558)	/* 0x8041200A */
#define SCE_GAMEUPDATE_ERROR_ALREADY_RUNNING	(-2143213557)	/* 0x8041200B */
#define SCE_GAMEUPDATE_ERROR_ABORTED		(-2143213556)	/* 0x8041200C */
#define SCE_GAMEUPDATE_ERROR_INVALID_STATE	(-2143213440)	/* 0x80412080 */
#define SCE_GAMEUPDATE_ERROR_NO_MEMORY		(-2143213439)	/* 0x80412081 */
#define SCE_GAMEUPDATE_ERROR_NO_PACKAGE		(-2143213438)	/* 0x80412082 */

/* Target platform modes for SceGameUpdateInitParam::targetPlatform */
#define SCE_GAMEUPDATE_PLATFORM_DEFAULT		0	/* normalized to PSP2 */
#define SCE_GAMEUPDATE_PLATFORM_PSP		1
#define SCE_GAMEUPDATE_PLATFORM_PSP2		2

/* Check result status values in SceGameUpdateResult::status */
#define SCE_GAMEUPDATE_STATUS_DOWNLOADING	0
#define SCE_GAMEUPDATE_STATUS_UP_TO_DATE	1
#define SCE_GAMEUPDATE_STATUS_AVAILABLE		2
#define SCE_GAMEUPDATE_STATUS_DISCONTINUED	3
#define SCE_GAMEUPDATE_STATUS_SYSUPDATE_NEEDED	4
#define SCE_GAMEUPDATE_STATUS_READY_TO_INSTALL	5

/* Result info flag bits in SceGameUpdateInfo::flags */
#define SCE_GAMEUPDATE_INFO_FLAG_SIGNOFF	0x00000002u
#define SCE_GAMEUPDATE_INFO_FLAG_DEBUG_MOCK	0x00000080u

/* Init param option flags */
#define SCE_GAMEUPDATE_OPT_SKIP_BGDL_CHECK	0x00000001u

/* Package distribution types */
#define SCE_GAMEUPDATE_PKG_CUMULATIVE		0
#define SCE_GAMEUPDATE_PKG_INCREMENTAL		1

struct GameUpdateCore;
struct GameUpdateHttpWork;
struct GameUpdateXmlWork;

/* Initialization parameters passed to sceGameUpdateInit (112 bytes) */
struct SceGameUpdateInitParam {
	void *(*allocFunc)(SceSize size);
	void (*freeFunc)(void *ptr);
	SceInt32 targetPlatform;
	char titleId[16];
	char currentAppVer[8];
	char contentId[48];
	SceUInt32 currentPsp2SysVer;
	SceUInt32 currentPspSysVer;
	char patchTitleId[16];
	SceUInt32 flags;
};

/* Opaque handle holding core, HTTP, and XML parser work areas (12 bytes) */
struct SceGameUpdateContext {
	GameUpdateCore *core;
	GameUpdateHttpWork *http;
	GameUpdateXmlWork *xml;
};

/* Main patch-check outcome written by sceGameUpdateRun (16 bytes) */
struct SceGameUpdateResult {
	SceSize size;
	SceInt32 status;
	char latestVersion[8];
};

/* Detailed patch metadata written by sceGameUpdateRun (412 bytes) */
struct SceGameUpdateInfo {
	char tagName[20];
	char titleName[128];
	char changeInfoUrl[256];
	SceUInt32 packageCount;
	SceUInt32 flags;
};

/* Per-package download descriptor returned by sceGameUpdateGetPackageInfo (300 bytes) */
struct SceGameUpdatePackageInfo {
	SceUInt64 packageSize;
	SceUInt8 sha1Digest[20];
	char packageUrl[268];
	SceInt32 packageType;
};

int sceGameUpdateInit(SceGameUpdateContext *context, int option,
		      const SceGameUpdateInitParam *param);
int sceGameUpdateInitEx(SceGameUpdateContext *context, int option,
			const SceGameUpdateInitParam *param);
int sceGameUpdateRun(SceGameUpdateContext *context, SceGameUpdateResult *result,
		     SceGameUpdateInfo *info);
int sceGameUpdateRunEx(SceGameUpdateContext *context, SceGameUpdateResult *result,
		       SceGameUpdateInfo *info);
int sceGameUpdateGetPackageInfo(SceGameUpdateContext *context, SceUInt32 packageIndex,
				SceGameUpdatePackageInfo *outPackage);
int sceGameUpdateAbort(SceGameUpdateContext *context);
int sceGameUpdateTerm(SceGameUpdateContext *context);
int sceGameUpdateRecordCacheStatus(const char *titleId, SceInt32 status);

#endif
