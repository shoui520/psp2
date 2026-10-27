/*
 * mgkm_sm.c: PlayStation Vita MagicGate key manager secure module
 * (os0:sm/mgkm_sm.self) service entry point and command handlers
 * (0x80b09c..0x80b4b6, .rodata 0x80c118..0x80c158).
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#include "mgkm_sm.h"

/* ============================================================================
 * .rodata (0x80c118..0x80c158)
 * ============================================================================ */

/* 0x80c118: C++ vtable for concrete hook node (vtable pointer points to +8 = 0x80c120) */
static const uintptr_t s_mgkmHookVtable[4] = {
	0u,
	0u,
	(uintptr_t)mgkmHookInvoke,
	0u
};

/* 0x80c128: C++ vtable for abstract hook base node */
static const uintptr_t s_mgkmBaseHookVtable[4] __attribute__((used)) = {
	0u,
	0u,
	(uintptr_t)__cxa_pure_virtual,
	0u
};

/* 0x80c138..0x80c148: 16-byte Console ID SHA-256 derivation salt */
static const uint8_t s_mgkmConsoleHashSalt[16] = {
	0x72u, 0xb6u, 0x27u, 0xfdu, 0x20u, 0x00u, 0xdfu, 0x4cu,
	0xfau, 0x5fu, 0x9fu, 0x0du, 0x9au, 0xf8u, 0xd6u, 0x5fu
};

/* 0x80c148..0x80c158: 16-byte keyring slot 31 seed (differs between debug and release) */
static const uint8_t s_mgkmSlot31Seed[16] = {
#if FW_DEBUG
	0xbcu, 0x91u, 0x66u, 0x43u, 0x94u, 0x12u, 0xc4u, 0xf9u,
	0xceu, 0xfbu, 0xe9u, 0x0eu, 0x16u, 0x70u, 0xc3u, 0xd5u
#else
	0xa4u, 0x8eu, 0xb0u, 0xdcu, 0x31u, 0x2bu, 0xacu, 0x8du,
	0xd5u, 0x49u, 0x3du, 0xb5u, 0xb9u, 0xc8u, 0xffu, 0x0cu
#endif
};

/* ============================================================================
 * Service Entry & Command Handlers (0x80b09c..0x80b4b6)
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

	fnPtr.fn = mgkmCmd1SetSlot31Key;
	fnPtr.adj = 0;
	ret = SCE_SBL_ERROR_MGKM_INIT_FAILED;
	if (mgkmAddHook(&srvCtx, MGKM_CMD_SET_SLOT31_KEY, &mgrDummy, &fnPtr, &outNode) == 0) {
		fnPtr.fn = mgkmCmd2DeriveAndSetSlot30Key;
		fnPtr.adj = 0;
		if (mgkmAddHook(&srvCtx, MGKM_CMD_DERIVE_SLOT30_KEY, &mgrDummy, &fnPtr, &outNode) == 0) {
			fnPtr.fn = mgkmCmd3ClearSlot31Key;
			fnPtr.adj = 0;
			if (mgkmAddHook(&srvCtx, MGKM_CMD_CLEAR_SLOT31_KEY, &mgrDummy, &fnPtr, &outNode) == 0) {
				fnPtr.fn = mgkmCmd4ClearSlot30Key;
				fnPtr.adj = 0;
				if (mgkmAddHook(&srvCtx, MGKM_CMD_CLEAR_SLOT30_KEY, &mgrDummy, &fnPtr, &outNode) == 0) {
					smSrvRunLoop(&srvCtx);
					ret = 0x55555555;
				}
			}
		}
	}

	smSrvTeardown(&srvCtx);
	return ret;
}

/* 0x80b170 (name ours) */
int mgkmHookInvoke(void *closure, void *packetBuf)
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

/* 0x80b1a4 (name ours) */
int mgkmAddHook(SmServiceContext *srvCtx, uint32_t cmdId, void *mgr,
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
		node->vtable = &s_mgkmHookVtable[2];
		node->handler = mgr;
		node->arg0 = (void *)(uintptr_t)fnPtr->fn;
		node->adj = fnPtr->adj;
	}
	*outNode = node;
	node->next = srvCtx->head;
	srvCtx->head = node;
	return 0;
}

/* 0x80b21e (name ours) */
int mgkmWriteKeyringSlot(void *mgr, uint32_t slot, const void *src,
		uint32_t size)
{
	uint32_t bmCtrl[2];
	uint8_t keyBuf[32];
	int ret;

	(void)mgr;
	if (slot - 29u > 2u || size > 32u)
		return SCE_SBL_ERROR_MGKM_INVALID_ARG;

	bigmacInitChannel0(bmCtrl);
	REG32(0xe0050104u) = 1u;

	memset(keyBuf, 0, sizeof(keyBuf));
	memcpy(keyBuf, src, size);

	ret = bigmacStartMemcpy((void *)(uintptr_t)(0xe04e0000u + (slot << 5)),
			keyBuf, 32u, bmCtrl);
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

/* 0x80b2cc (name ours; mailbox command 4) */
int mgkmCmd4ClearSlot30Key(void *mgr, void *packetBuf)
{
	SceMgkmCommPacket *pkt = (SceMgkmCommPacket *)packetBuf;
	uint8_t zeroKey[32];
	int ret;

	memset(zeroKey, 0, sizeof(zeroKey));
	ret = mgkmWriteKeyringSlot(mgr, 30u, zeroKey, sizeof(zeroKey));
	pkt->result = ret;
	return ret;
}

/* 0x80b318 (name ours; mailbox command 3) */
int mgkmCmd3ClearSlot31Key(void *mgr, void *packetBuf)
{
	SceMgkmCommPacket *pkt = (SceMgkmCommPacket *)packetBuf;
	uint8_t zeroKey[32];
	int ret;

	memset(zeroKey, 0, sizeof(zeroKey));
	ret = mgkmWriteKeyringSlot(mgr, 31u, zeroKey, sizeof(zeroKey));
	pkt->result = ret;
	return ret;
}

/* 0x80b364 (name ours; mailbox command 1) */
int mgkmCmd1SetSlot31Key(void *mgr, void *packetBuf)
{
	SceMgkmCommPacket *pkt = (SceMgkmCommPacket *)packetBuf;
	uint8_t keyBuf[32];
	int ret;

	memset(keyBuf, 0, sizeof(keyBuf));
	memcpy(keyBuf, s_mgkmSlot31Seed, sizeof(s_mgkmSlot31Seed));
	ret = mgkmWriteKeyringSlot(mgr, 31u, keyBuf, sizeof(keyBuf));
	pkt->result = ret;
	return ret;
}

/* 0x80b3bc (name ours) */
int mgkmDeriveConsoleSeedSha256(void *mgr, void *outDigest32, uint32_t outSize)
{
	uint8_t rawCid[16];
	uint8_t hashBuf[32];
	uint8_t cid[16];
	int ret;

	(void)mgr;
	if (outDigest32 == NULL || outSize != 32u)
		return SCE_SBL_ERROR_MGKM_INVALID_ARG;

	aimgrReadConsoleIdAlias1(rawCid);
	ret = aimgrGetConsoleId(rawCid, cid);
	if (ret == 0) {
		memset(hashBuf, 0, sizeof(hashBuf));
		memcpy(hashBuf, cid, 16u);
		memcpy(hashBuf + 16u, s_mgkmConsoleHashSalt, 16u);

		if (bigmacSha256(hashBuf, hashBuf, sizeof(hashBuf)) == 0)
			memcpy(outDigest32, hashBuf, sizeof(hashBuf));
		else
			ret = SCE_SBL_ERROR_BAD_DIGEST;
	}
	aimgrClearConsoleId2(rawCid);
	return ret;
}

/* 0x80b460 (name ours; mailbox command 2) */
int mgkmCmd2DeriveAndSetSlot30Key(void *mgr, void *packetBuf)
{
	SceMgkmCommPacket *pkt = (SceMgkmCommPacket *)packetBuf;
	uint8_t derivedKey[32];
	int ret;

	memset(derivedKey, 0, sizeof(derivedKey));
	ret = mgkmDeriveConsoleSeedSha256(mgr, derivedKey, sizeof(derivedKey));
	if (ret == 0)
		ret = mgkmWriteKeyringSlot(mgr, 30u, derivedKey, sizeof(derivedKey));
	pkt->result = ret;
	return ret;
}
