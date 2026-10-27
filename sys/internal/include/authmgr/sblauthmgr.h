#pragma once

#include <scetypes.h>
#include <stdint.h>

/*
 * SceSblAuthMgr's errors. SCE_SBL_ERROR_AM_EINVAL is Sony's (sceSblAuthMgrAuthHeader's message);
 * names unknown for the others, named after it by their errno (the low byte) or by what returns them.
 */
#define SCE_SBL_ERROR_AM_EPERM			((int)0x800f0501)	/* a SELF is open already */
#define SCE_SBL_ERROR_AM_EBADF			((int)0x800f0509)
#define SCE_SBL_ERROR_AM_ENOMEM			((int)0x800f050c)
#define SCE_SBL_ERROR_AM_EINVAL			((int)0x800f0516)
#define SCE_SBL_ERROR_AM_INVALID_SPFSO		((int)0x800f0524)
#define SCE_SBL_ERROR_AM_NO_PHYSICAL_ADDRESS	((int)0x800f0528)
#define SCE_SBL_ERROR_AM_SM_NOT_STOPPED		((int)0x800f0529)
#define SCE_SBL_ERROR_AM_SM_FAILED		((int)0x800f052a)	/* its status */
#define SCE_SBL_ERROR_AM_NEWER_SYSTEM_VERSION	((int)0x800f0537)

/*
 * A SELF's authentication information, as the secure kernel returns it (Sony's "SelfAuthInfo":
 * modulemgr's "get SelfAuthInfo failed", processmgr's sceKernelGetProcessSelfAuthInfo).
 */
/*
 * a capability or attribute set (names unknown): authmgr stores its words, acmgr tests its bits
 * byte by byte, and both copy it whole
 */
typedef union SceSelfAuthBits {
	SceUInt64 bits[4];
	SceUInt8 bytes[0x20];
} SceSelfAuthBits;

/* a shared secret, copied whole */
typedef struct SceSelfAuthSecret {
	SceUInt32 data[4];
} SceSelfAuthSecret;

typedef struct SceSelfAuthInfo {
	SceUInt64 programAuthorityId;
	SceUInt64 spfsoInfo;			/* media 13 and 14: the SPFSO's word 3 << 16 */
	SceSelfAuthBits capability;
	SceSelfAuthBits attribute;
	SceSelfAuthSecret sharedSecret0;
	SceUInt32 klicensee[4];
	SceUInt8 sharedSecret2[0x10];
	SceUInt32 sharedSecret3[4];
} SceSelfAuthInfo;

/* what a SELF is authenticated with: the caller's information and the module's, as returned */
typedef struct SelfAuthRequest {
	SceUInt32 reserved0;
	SceUInt32 selfType;			/* bits 0-3: 0 kernel, 1 user, 2 secure module */
	SceSelfAuthInfo caller;
	SceSelfAuthInfo module;			/* filled by the secure kernel */
	SceUInt32 mediaType;
	SceUInt32 reserved1;
} SelfAuthRequest;

#ifndef __cplusplus
_Static_assert(sizeof(SceSelfAuthInfo) == 0x90, "SceSelfAuthInfo");
_Static_assert(sizeof(SelfAuthRequest) == 0x130, "SelfAuthRequest");
#endif

struct SceSblSpfsoCtx;

int sceSblAuthMgrOpen(int *handle);
int sceSblAuthMgrClose(int handle);
int sceSblAuthMgrAuthHeader(int handle, const void *header, SceSize size, SelfAuthRequest *auth);
int sceSblAuthMgrSetupAuthSegment(int handle, SceUInt32 segment);
int sceSblAuthMgrAuthSegment(int handle, void *buffer, SceSize size, SceUInt32 reservedLow,
				     int reservedHigh);
int sceSblAuthMgrGetEKc(void *buffer, SceSize size, SceUInt32 type);
int sceSblAuthMgrSetDmac5Key(const void *key, SceSize size, int slot, int keyId);
int sceSblAuthMgrClearDmac5Key(int slot, int value);
int sceSblAuthMgrDecBindData(void *input, SceSize inputSize, void *output, SceSize outputSize,
			     int flags);
int sceSblAuthMgrVerifySpsfo(struct SceSblSpfsoCtx *ctx);
#if AUTHMGR_NSKBL
/* NSKBL's: its boot loader starts and stops the module itself */
int sceSblAuthMgrStart(void);
int sceSblAuthMgrStopSm(void);
#endif
/* vitasdk: ksceSblAuthMgrCompareSwVersion */
int SceSblAuthMgrForKernel_ABAB8466(SceUInt32 version);
int SceSblAuthMgrForKernel_2A83A012(void);
