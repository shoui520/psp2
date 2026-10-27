/*
 * mgkm_sm.h: Internal types and cross-unit declarations for PlayStation Vita
 * MagicGate key manager secure module (os0:sm/mgkm_sm.self).
 *
 * Internal function, structure, and constant names (except sm_main) are ours.
 */

#ifndef MGKM_SM_H
#define MGKM_SM_H

#include "sm_common.h"

#define SCE_SBL_ERROR_BAD_DIGEST		((int)0x800f0027u)
#define SCE_SBL_ERROR_MGKM_INVALID_ARG		((int)0x800f1616u)
#define SCE_SBL_ERROR_MGKM_INIT_FAILED		((int)0x800f1628u)

#define MGKM_CMD_SET_SLOT31_KEY			1u
#define MGKM_CMD_DERIVE_SLOT30_KEY		2u
#define MGKM_CMD_CLEAR_SLOT31_KEY		3u
#define MGKM_CMD_CLEAR_SLOT30_KEY		4u

/* Mailbox communication packet at 0x80c180 (size 0x40) */
typedef struct SceMgkmCommPacket {
	uint32_t size;				/* 0x00: 0x40 */
	uint32_t cmdId;				/* 0x04: 1..4 */
	int32_t result;				/* 0x08 */
	uint32_t reserved0c[13];		/* 0x0c..0x3f */
} SceMgkmCommPacket;

_Static_assert(sizeof(SceMgkmCommPacket) == 0x40, "SceMgkmCommPacket size");

int mgkmHookInvoke(void *closure, void *packetBuf);
int mgkmAddHook(SmServiceContext *srvCtx, uint32_t cmdId, void *mgr,
		const SmMemberFnPtr *fnPtr, SmServiceHookNode **outNode);
int mgkmWriteKeyringSlot(void *mgr, uint32_t slot, const void *src,
		uint32_t size);
int mgkmCmd4ClearSlot30Key(void *mgr, void *packetBuf);
int mgkmCmd3ClearSlot31Key(void *mgr, void *packetBuf);
int mgkmCmd1SetSlot31Key(void *mgr, void *packetBuf);
int mgkmDeriveConsoleSeedSha256(void *mgr, void *outDigest32, uint32_t outSize);
int mgkmCmd2DeriveAndSetSlot30Key(void *mgr, void *packetBuf);

int bigmacSha256(void *outDigest, const void *src, uint32_t size);
int bigmacStartHash(void *outDigest, const void *src, uint32_t size,
		uint32_t hashAlgo, uint32_t waitMode, const uint32_t *bmCtrl);

#endif /* MGKM_SM_H */
