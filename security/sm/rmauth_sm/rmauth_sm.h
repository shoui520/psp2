/*
 * rmauth_sm.h: Internal types and cross-unit declarations for PlayStation Vita
 * removable media authentication secure module (os0:sm/rmauth_sm.self).
 *
 * Internal function, structure, and constant names (except sm_main) are ours.
 */

#ifndef RMAUTH_SM_H
#define RMAUTH_SM_H

#include "sm_common.h"

#define SCE_SBL_ERROR_RMAUTH_INVALID_ARG	((int)0x800f1916u)
#define SCE_SBL_ERROR_RMAUTH_INIT_FAILED	((int)0x800f1928u)

#define RMAUTH_CMD_GET_STATUS			1u
#define RMAUTH_CMD_SETUP_KEY			2u
#define RMAUTH_CMD_CLEAR_KEY			3u

#define RMAUTH_KEYRING_SLOT			28u

/* Mailbox communication packet at 0x80bd00 (size 0x60) */
typedef struct SceRmauthCommPacket {
	uint32_t size;				/* 0x00: 0x60 */
	uint32_t cmdId;				/* 0x04: 1, 2, or 3 */
	int32_t result;				/* 0x08 */
	uint32_t reserved0c[13];		/* 0x0c..0x3f */
	uint8_t userKey[16];			/* 0x40..0x4f */
	uint32_t statusOut;			/* 0x50 */
	uint32_t reserved54[3];			/* 0x54..0x5f */
} SceRmauthCommPacket;

_Static_assert(sizeof(SceRmauthCommPacket) == 0x60, "SceRmauthCommPacket size");

int rmauthHookInvoke(void *closure, void *packetBuf);
int rmauthAddHook(SmServiceContext *srvCtx, uint32_t cmdId, void *mgr,
		const SmMemberFnPtr *fnPtr, SmServiceHookNode **outNode);
int rmauthCmd1GetStatus(void *mgr, void *packetBuf);
int rmauthWriteKeyringSlot28(void *mgr, uint32_t slot, const void *src,
		uint32_t size);
int rmauthCmd3ClearKey(void *mgr, void *packetBuf);
int rmauthAes128CbcEncryptZeroIv(void *mgr, const void *key, void *dst,
		const void *src, uint32_t size);
int rmauthCmd2SetupKey(void *mgr, void *packetBuf);

int bigmacStartAes128CbcEncrypt(void *dst, const void *keyOrSlot, uint32_t size,
		const void *src, const void *iv, const uint32_t *bmCtrl);
int bigmacStartAes128CbcDecrypt(void *dst, const void *keyOrSlot, uint32_t size,
		const void *src, const void *iv, const uint32_t *bmCtrl);

#endif /* RMAUTH_SM_H */
