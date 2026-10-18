#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

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
int sceSblAuthMgrAuthHeader(int handle, const void *header, uint32_t size, SceAuthInfo *auth);
int sceSblAuthMgrClose(int handle);
int sceSblAuthMgrOpen(int *handle);
int sceSblAuthMgrVerifySpsfo(struct SpsfoContext *context);
