/*
 * np_manager_core.h: internal declarations for SceNpManager account/session core,
 * SceNpUpdateClock, and SceNpCommon service (0x81326690..0x8132A067).
 */
#ifndef SHELL_NP_MANAGER_CORE_H
#define SHELL_NP_MANAGER_CORE_H

#include <stddef.h>
#include <scetypes.h>
#include <np/np_common.h>
#include <std/string>
#include "../ipc/np_ipc_internal.h"

/* SceNpManager internal error codes */
#define NP_MGR_ERR_ALREADY_INIT		-2141911807	/* 0x80550501 */
#define NP_MGR_ERR_NOT_INIT		-2141911806	/* 0x80550502 */
#define NP_MGR_ERR_ACCOUNT_MISMATCH	-2141911800	/* 0x80550508 */
#define NP_MGR_ERR_SIGNED_OUT		-2141911798	/* 0x8055050A */
#define NP_MGR_ERR_NO_TICKET		-2141911797	/* 0x8055050B */
#define NP_MGR_ERR_INTERNAL		-2141911680	/* 0x80550580 */
#define NP_MGR_ERR_CREDENTIAL_MISMATCH	-2141911674	/* 0x80550586 */
#define NP_MGR_ERR_NO_ONLINE_ID		-2141911673	/* 0x80550587 */
#define NP_MGR_ERR_NO_CACHED_PROFILE	-2141911669	/* 0x8055058B */
#define NP_MGR_ERR_BUFFER_TOO_SMALL	-2141911667	/* 0x8055058D */

#define NP_MGR_ERR_SYS_UPDATE_REQUIRED	-2141912046	/* 0x80550412 */
#define NP_MGR_ERR_CLOCK_SKEW_AUTH	-2141912044	/* 0x80550414 */
#define NP_MGR_ERR_CLOCK_SKEW_SSL	-2145054718	/* 0x80251002 */

/* NP account acquisition option flags */
#define NP_MGR_ACQUIRE_FORCE_REFRESH	0x00000001
#define NP_MGR_ACQUIRE_SKIP_ENABLE_CHK	0x00000002
#define NP_MGR_ACQUIRE_SKIP_PROFILE	0x00000004
#define NP_MGR_ACQUIRE_NET_INTERNAL	0x00000008

/* Cached NP settings (60 bytes = 0x3C, matches SceNpCommon NpCommonSettings) */
struct NpCommonSettings {
	int fullTest;			/* +0x00 */
	char titleId[40];		/* +0x04 */
	int navOnly;			/* +0x2C */
	int adClockDiff;		/* +0x30 */
	int geoFiltering;		/* +0x34 */
	int debugFlag;			/* +0x38 */
};

/* ux0:id.dat update descriptor passed to SceDbsl writer (0x81361F44) */
struct NpIdDatUpdateInfo {
	paf::string memoryCardId;	/* +0x00 */
	paf::string deviceId;		/* +0x0C */
	SceUInt64 accountId;		/* +0x18 */
	paf::string onlineId;		/* +0x20 */
	SceUInt32 sysVersion;		/* +0x2C */
	SceUInt8 hasDigest;		/* +0x30 */
};

/* Cached profile file layout persisted to <userDir>/np/myprofile.dat (328 bytes) */
struct NpCachedMyProfileFile {
	SceNpId npId;			/* +0x000 (36 bytes) */
	SceNpOnlineId onlineId;		/* +0x024 (20 bytes) */
	char aboutMe[128];		/* +0x038 (128 bytes) */
	char avatarUrl[144];		/* +0x0B8 (144 bytes) */
};

/* Cached profile block kept inside NpAccountSession (456 bytes = 0x1C8) */
struct NpAccountProfileCache {
	SceNpId npId;			/* +0x000 (36 bytes) */
	SceUInt8 profileHeader[52];	/* +0x024 (52 bytes) */
	char aboutMe[128];		/* +0x058 (128 bytes) */
	SceUInt32 profileSeparator;	/* +0x0D8 (4 bytes) */
	SceUInt8 languages[16];		/* +0x0DC (16 bytes) */
	SceUInt32 panelColor;		/* +0x0EC (4 bytes) */
	char panelId[64];		/* +0x0F0 (64 bytes) */
	char avatarUrl[144];		/* +0x130 (144 bytes) */
	SceUInt8 profileFooter[8];	/* +0x1C0 (8 bytes) */
};

/* Profile output with trailing status word (460 bytes, used by npAccountGetProfileAndMeta) */
struct NpAccountProfileWithStatus {
	NpAccountProfileCache profile;	/* +0x000 (456 bytes) */
	SceUInt32 extraStatus;		/* +0x1C8 (4 bytes) */
};

/* Account metadata block (16 bytes, used by npAccountGetProfileAndMeta) */
struct NpAccountMetaInfo {
	SceUInt64 accountId;		/* +0x00 */
	SceUInt8 regionInfo[4];		/* +0x08 */
	SceUInt8 contentRatingFlags[4];	/* +0x0C */
};

/* Callback signatures */
typedef void (*NpAccountEventCallback)(int eventType, void *userArg);

/* Manager core lifecycle & helpers (0x81326690..0x813278F9) */
unsigned int npManagerCoreIsInitialized(void);
int npManagerGetCachedCredentials(void *outLoginId, void *outPassword, SceUInt64 *outAccountId);
int npManagerCoreInit(void);
int npAccountGetTicketForService(int reqHandle, void *ticketBuf, SceSize *ioSize, int flags);
void npAccountNotifyBootSignIn(void);
int npAccountClearSessionWithResult(int resultCode);
int npAccountUpdateCachedProfile(const void *languages, const void *panelId,
				 const SceUInt32 *panelColor, const void *avatarUrl,
				 const void *aboutMe);
int npAccountRegisterNetCallback(NpAccountEventCallback callback, void *userArg);
int npAccountUnregisterNetCallback(int callbackId);

/* SceNpUpdateClock init (0x81328E68) */
int npUpdateClockInit(void);

/* SceNpCommon service helpers (0x8132A030..0x8132A067) */
int npCommonServiceLoadAccountId(void);
int npCommonServiceUpdateAccountId(SceUInt64 accountId);

#endif /* SHELL_NP_MANAGER_CORE_H */
