/*
 * aimgr_sm.h: Internal types and cross-unit declarations for PlayStation Vita
 * SceSblAIMgr secure module (os0:sm/aimgr_sm.self).
 *
 * Internal function, structure, and constant names (except sm_main) are ours.
 */

#ifndef AIMGR_SM_H
#define AIMGR_SM_H

#include "sm_common.h"

#define SCE_SBL_ERROR_AIMGR_INVALID_ARG		((int)0x800f0c16u)
#define SCE_SBL_ERROR_AIMGR_BAD_DIGEST		((int)0x800f0c27u)
#define SCE_SBL_ERROR_AIMGR_FAILED		((int)0x800f0c28u)
#define SCE_SBL_ERROR_AIMGR_KEY_NOT_READY	((int)0x800f0c29u)

#define AIMGR_CMD_GET_CONSOLE_ID		1u
#define AIMGR_CMD_GET_OPEN_PSID			2u
#define AIMGR_CMD_GET_SLOT_600			3u
#define AIMGR_CMD_EXTRACT_CERT_INFO		4u
#define AIMGR_CMD_GENERATE_TOKEN		5u

/* Mailbox communication packet at 0x80c300 (size up to 0x260) */
typedef struct SceAimgrCommPacket {
	uint32_t size;				/* 0x00 */
	uint32_t cmdId;				/* 0x04: 1..5 */
	int32_t result;				/* 0x08 */
	uint32_t reserved0c[13];		/* 0x0c..0x3f */
	uint8_t payload[0x220];			/* 0x40..0x25f */
} SceAimgrCommPacket;

_Static_assert(sizeof(SceAimgrCommPacket) == 0x260, "SceAimgrCommPacket size");

int aimgrSmHookInvoke(void *closure, void *packetBuf);
int aimgrSmAddHook(SmServiceContext *srvCtx, uint32_t cmdId, void *mgr,
		const SmMemberFnPtr *fnPtr, SmServiceHookNode **outNode);
void aimgrSmReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8);
int aimgrCmd5GenerateSignedToken(void *mgr, void *packetBuf);
int aimgrCmd1GetConsoleId(void *mgr, void *packetBuf);
int aimgrCmd2GetOpenPsId(void *mgr, void *packetBuf);
int aimgrCmd3GetSlot600(void *mgr, void *packetBuf);
int aimgrCmd4ExtractCertTargetInfo(void *mgr, void *packetBuf);
int aimgrBuildAndSignToken(uint8_t *buf);
int aimgrVerifyCertAndExtract16(uint8_t outLen, void *out16,
		const uint8_t *certBuf, uint32_t certSize);
int aimgrVerifyCertAndExtractPayload(const uint8_t *certBuf, void *out80,
		uint32_t outSize);
int aimgrSetupKeyringSlot204(void);
int aimgrComputeCertCmac(void *outMac16, const void *src, uint32_t size);
int aimgrDecryptCertTag(void *outTag16, const void *inTag16, uint32_t size);
int aimgrDecryptAndVerifyCertBlock(const uint8_t *block256, uint32_t size,
		uint32_t unused);
int16_t aimgrExtractSubCodeBe(uint32_t rawWord2);
int aimgrFillRandomBytes(uint8_t *dst, uint32_t size);

int bigmacHmacSha256Sync(void *outMac, const void *src, uint32_t size,
		const void *key, uint32_t keyLen);
int bigmacStartHmacSha256(void *outMac, const void *src, uint32_t size,
		const void *key, uint32_t keyLen, uint32_t flags,
		const uint32_t *bmCtrl);

#endif /* AIMGR_SM_H */
