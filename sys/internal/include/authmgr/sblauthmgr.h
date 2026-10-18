#pragma once

#include <scetypes.h>
#include <stdint.h>

/*
 * A SELF's authentication information, as the secure kernel returns it (Sony's "SelfAuthInfo":
 * modulemgr's "get SelfAuthInfo failed", processmgr's sceKernelGetProcessSelfAuthInfo).
 */
typedef struct SceSelfAuthInfo {
	SceUInt64 programAuthorityId;
	SceUInt8 padding[8];
	SceUInt8 capability[0x20];
	SceUInt8 attribute[0x20];
	SceUInt8 sharedSecret0[0x10];
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

struct SpsfoContext;

/* vitasdk: ksceSblAuthMgrDecBindData */
int SceSblAuthMgrForDriver_41DAEA12(void *input, uint32_t inputSize, void *output, uint32_t outputSize, int flags);
/* vitasdk: ksceSblAuthMgrGetEKc */
int SceSblAuthMgrForDriver_868B9E9A(void *buffer, uint32_t size, uint32_t type);
/* vitasdk: ksceSblAuthMgrSetDmac5Key */
int SceSblAuthMgrForKernel_122ACDEA(const void *key, uint32_t size, int slot, int keyId);
int SceSblAuthMgrForKernel_2A83A012(void);
/* vitasdk: ksceSblAuthMgrSetupAuthSegment */
int SceSblAuthMgrForKernel_89CCDA2C(int handle, uint32_t segmentIndex);
/* vitasdk: ksceSblAuthMgrCompareSwVersion */
int SceSblAuthMgrForKernel_ABAB8466(uint32_t version);
/* vitasdk: ksceSblAuthMgrLoadBlock */
int SceSblAuthMgrForKernel_BC422443(int handle, void *buffer, uint32_t size);
/* vitasdk: ksceSblAuthMgrClearDmac5Key */
int SceSblAuthMgrForKernel_F2BB723E(int slot, int value);
int sceSblAuthMgrAuthHeader(int handle, const void *header, uint32_t size, SelfAuthRequest *auth);
int sceSblAuthMgrClose(int handle);
int sceSblAuthMgrOpen(int *handle);
int sceSblAuthMgrVerifySpsfo(struct SpsfoContext *context);
