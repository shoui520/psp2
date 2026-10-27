/*
 * rmauth_sm.c: PlayStation Vita removable media authentication secure module
 * (os0:sm/rmauth_sm.self) service entry point and command handlers
 * (0x80b09c..0x80b424, .rodata 0x80bb88..0x80bca8).
 *
 * Identical across 4.00-DEBUGGER, 3.650.011-TOOL, 3.630.011-DEBUGGER, and
 * 3.65-CEX.
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#include "rmauth_sm.h"

/* ============================================================================
 * .rodata (0x80bb88..0x80bca8)
 * ============================================================================ */

/* 0x80bb88: C++ vtable for concrete hook node (vtable pointer points to +8 = 0x80bb90) */
static const uintptr_t s_rmauthHookVtable[4] = {
	0u,
	0u,
	(uintptr_t)rmauthHookInvoke,
	0u
};

/* 0x80bb98: C++ vtable for abstract hook base node */
static const uintptr_t s_rmauthBaseHookVtable[4] __attribute__((used)) = {
	0u,
	0u,
	(uintptr_t)__cxa_pure_virtual,
	0u
};

/* 0x80bba8..0x80bca8: 256-byte removable media key derivation seed table */
static const uint8_t s_rmauthSeedTable[256] = {
	0xd4u, 0x19u, 0xa2u, 0xebu, 0x9du, 0x61u, 0xa5u, 0x2fu,
	0x4fu, 0xa2u, 0x8bu, 0x27u, 0xe3u, 0x2fu, 0xcdu, 0xd7u,
	0xe0u, 0x04u, 0x8du, 0x44u, 0x3du, 0x63u, 0xc9u, 0x2cu,
	0x0bu, 0x27u, 0x13u, 0x55u, 0x41u, 0xd9u, 0x2eu, 0xc4u,
	0xc1u, 0xb3u, 0x64u, 0x9bu, 0x15u, 0x82u, 0x14u, 0xa8u,
	0xa3u, 0x7du, 0x2du, 0x20u, 0xd6u, 0x5fu, 0x1bu, 0x20u,
	0xa8u, 0x64u, 0x4eu, 0x9fu, 0xd3u, 0xe8u, 0x50u, 0x48u,
	0xc2u, 0x5cu, 0x86u, 0x58u, 0x9bu, 0x32u, 0x88u, 0xa9u,
	0xb7u, 0x30u, 0xb5u, 0xb7u, 0x92u, 0x2bu, 0xe0u, 0xfeu,
	0x14u, 0xb5u, 0x2cu, 0xdeu, 0xcbu, 0x2cu, 0x18u, 0x12u,
	0x88u, 0x2eu, 0x74u, 0x42u, 0x8au, 0x1au, 0xf0u, 0xc5u,
	0x6cu, 0x40u, 0xb4u, 0x9cu, 0x0eu, 0x70u, 0xc6u, 0x35u,
	0x28u, 0xb2u, 0x38u, 0x80u, 0xa9u, 0x59u, 0xf6u, 0x76u,
	0x67u, 0x0eu, 0xb1u, 0xa4u, 0x0eu, 0x62u, 0xe6u, 0x17u,
	0xe8u, 0xbfu, 0x46u, 0x73u, 0xccu, 0xe9u, 0x5eu, 0xdbu,
	0x7au, 0x99u, 0xccu, 0xd2u, 0x74u, 0xcdu, 0xa4u, 0x73u,
	0x9du, 0x49u, 0x67u, 0x01u, 0x33u, 0x0bu, 0x12u, 0x3fu,
	0x8du, 0x90u, 0x38u, 0xe6u, 0xb9u, 0x71u, 0x1eu, 0x37u,
	0x83u, 0x84u, 0xd4u, 0x77u, 0x8du, 0x64u, 0x62u, 0x57u,
	0xc5u, 0x74u, 0x27u, 0x49u, 0x23u, 0x33u, 0x59u, 0x28u,
	0xb0u, 0x03u, 0xa0u, 0x21u, 0x75u, 0xebu, 0x5cu, 0x44u,
	0xa6u, 0x10u, 0x22u, 0xc2u, 0x6fu, 0xb2u, 0xc5u, 0xa8u,
	0xcdu, 0x1eu, 0x9cu, 0xd4u, 0xfcu, 0xaau, 0x4eu, 0x10u,
	0xf9u, 0xdcu, 0xfdu, 0x5bu, 0xa1u, 0xabu, 0x62u, 0x15u,
	0x42u, 0xdbu, 0xb1u, 0xcdu, 0xf8u, 0x21u, 0xd4u, 0x63u,
	0x58u, 0xe8u, 0x7cu, 0xb0u, 0x3fu, 0xe5u, 0xb5u, 0x4eu,
	0x3cu, 0x7au, 0x44u, 0x82u, 0x80u, 0xe0u, 0xb9u, 0xdbu,
	0xd0u, 0x3eu, 0x05u, 0x28u, 0x07u, 0xd8u, 0x89u, 0xf1u,
	0x5eu, 0xe9u, 0x79u, 0x48u, 0xacu, 0x31u, 0x4bu, 0x73u,
	0x32u, 0x3cu, 0x48u, 0x2du, 0x17u, 0x35u, 0x47u, 0xd5u,
	0xefu, 0xeau, 0xd7u, 0xb2u, 0xecu, 0xbau, 0x5eu, 0x31u,
	0x43u, 0x1eu, 0xbdu, 0x16u, 0x79u, 0x62u, 0x99u, 0x32u
};

/* ============================================================================
 * Service Entry & Command Handlers (0x80b09c..0x80b424)
 * ============================================================================ */

/* 0x80b09c */
int sm_main(void)
{
	SmServiceContext srvCtx;
	SmMemberFnPtr fnPtr;
	SmServiceHookNode *outNode;
	uint8_t mgrDummy;
	int ret;

	smSrvInit(&srvCtx);

	fnPtr.fn = rmauthCmd2SetupKey;
	fnPtr.adj = 0;
	ret = SCE_SBL_ERROR_RMAUTH_INIT_FAILED;
	if (rmauthAddHook(&srvCtx, RMAUTH_CMD_SETUP_KEY, &mgrDummy, &fnPtr, &outNode) == 0) {
		fnPtr.fn = rmauthCmd1GetStatus;
		fnPtr.adj = 0;
		if (rmauthAddHook(&srvCtx, RMAUTH_CMD_GET_STATUS, &mgrDummy, &fnPtr, &outNode) == 0) {
			fnPtr.fn = rmauthCmd3ClearKey;
			fnPtr.adj = 0;
			if (rmauthAddHook(&srvCtx, RMAUTH_CMD_CLEAR_KEY, &mgrDummy, &fnPtr, &outNode) == 0) {
				smSrvRunLoop(&srvCtx);
				ret = 0x55555555;
			}
		}
	}

	smSrvTeardown(&srvCtx);
	return ret;
}

/* 0x80b158 (name ours) */
int rmauthHookInvoke(void *closure, void *packetBuf)
{
	uintptr_t *words = (uintptr_t *)closure;
	uint8_t *obj = (uint8_t *)words[1];
	uintptr_t fnVal = words[2];
	int32_t adj = (int32_t)words[3];
	SmCmdMethod fn;

	if ((fnVal & 1u) != 0u) {
		uintptr_t vptr = *(const uintptr_t *)(const void *)(obj + adj);

		fn = *(const SmCmdMethod *)(const void *)(vptr + fnVal - 1u);
	} else {
		fn = (SmCmdMethod)fnVal;
	}
	return fn(obj + adj, packetBuf);
}

/* 0x80b18c (name ours) */
int rmauthAddHook(SmServiceContext *srvCtx, uint32_t cmdId, void *mgr,
		const SmMemberFnPtr *fnPtr, SmServiceHookNode **outNode)
{
	SmServiceHookNode *node;

	*outNode = NULL;
	node = smSrvAllocNode(srvCtx);
	if (node == NULL)
		return SCE_SBL_ERROR_NO_MEMORY;

	node->cmdId = cmdId;
	node->next = NULL;
	if (&node->vtable != NULL) {
		node->vtable = &s_rmauthHookVtable[2];
		node->handler = mgr;
		node->arg0 = (void *)(uintptr_t)fnPtr->fn;
		node->adj = fnPtr->adj;
	}
	*outNode = node;
	node->next = srvCtx->head;
	srvCtx->head = node;
	return 0;
}

/* 0x80b206 (name ours; mailbox command 1) */
int rmauthCmd1GetStatus(void *mgr, void *packetBuf)
{
	SceRmauthCommPacket *pkt = (SceRmauthCommPacket *)packetBuf;
	uint32_t status = 0u;

	(void)mgr;
	memcpy(&pkt->statusOut, &status, sizeof(status));
	pkt->result = 0;
	return 0;
}

/* 0x80b230 (name ours) */
int rmauthWriteKeyringSlot28(void *mgr, uint32_t slot, const void *src,
		uint32_t size)
{
	uint32_t bmCtrl[2];
	uint8_t keyBuf[32];
	int ret;

	(void)mgr;
	if (slot != RMAUTH_KEYRING_SLOT || size > 32u)
		return SCE_SBL_ERROR_RMAUTH_INVALID_ARG;

	bigmacInitChannel0(bmCtrl);
	REG32(0xe0050104u) = 1u;

	memset(keyBuf, 0, sizeof(keyBuf));
	memcpy(keyBuf, src, size);

	ret = bigmacStartMemcpy((void *)(uintptr_t)0xe04e0380u, keyBuf, 32u, bmCtrl);
	if (ret == 0) {
		ret = bigmacWaitChannelDone(bmCtrl);
		if (ret == 0) {
			REG32(0xe0050104u) = 0u;
			return 0;
		}
	}

	REG32(0xe0050104u) = 0u;
	return ret;
}

/* 0x80b2d2 (name ours; mailbox command 3) */
int rmauthCmd3ClearKey(void *mgr, void *packetBuf)
{
	SceRmauthCommPacket *pkt = (SceRmauthCommPacket *)packetBuf;
	uint8_t zeroKey[32];
	int ret;

	memset(zeroKey, 0, sizeof(zeroKey));
	ret = rmauthWriteKeyringSlot28(mgr, RMAUTH_KEYRING_SLOT, zeroKey, sizeof(zeroKey));
	pkt->result = ret;
	return ret;
}

/* 0x80b31e (name ours) */
int rmauthAes128CbcEncryptZeroIv(void *mgr, const void *key, void *dst,
		const void *src, uint32_t size)
{
	uint32_t bmCtrl[2];
	uint8_t zeroIv[16];
	int ret;

	(void)mgr;
	bigmacInitChannel0(bmCtrl);
	if (key == NULL || dst == NULL || size < 16u || (size & 0xfu) != 0u)
		return SCE_SBL_ERROR_RMAUTH_INVALID_ARG;

	memset(zeroIv, 0, sizeof(zeroIv));
	ret = bigmacStartAes128CbcEncrypt(dst, key, size, src, zeroIv, bmCtrl);
	if (ret == 0)
		ret = bigmacWaitChannelDone(bmCtrl);
	return ret;
}

/* 0x80b39e (name ours; mailbox command 2) */
int rmauthCmd2SetupKey(void *mgr, void *packetBuf)
{
	SceRmauthCommPacket *pkt = (SceRmauthCommPacket *)packetBuf;
	uint8_t seedBuf[32];
	uint8_t derivedKey[32];
	uint8_t userKey[16];
	int ret;

	memcpy(seedBuf, s_rmauthSeedTable, sizeof(seedBuf));
	memcpy(userKey, pkt->userKey, sizeof(userKey));

	ret = rmauthAes128CbcEncryptZeroIv(mgr, userKey, derivedKey, seedBuf, 16u);
	if (ret == 0) {
		ret = rmauthAes128CbcEncryptZeroIv(mgr, userKey, derivedKey + 16u,
				seedBuf + 16u, 16u);
		if (ret == 0)
			ret = rmauthWriteKeyringSlot28(mgr, RMAUTH_KEYRING_SLOT,
					derivedKey, sizeof(derivedKey));
	}

	pkt->result = ret;
	return ret;
}
