/*
 * qaf_sm.h: Internal types, structures, and function declarations for
 * PlayStation Vita QA Flag Secure Module (qaf_sm.self).
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#ifndef QAF_SM_H
#define QAF_SM_H

#include "sm_common.h"

/* ============================================================================
 * QAF Error Codes & Command IDs
 * ============================================================================ */

#define SCE_SBL_QAF_BOOT_CMD_VERIFY_REC		4
#define SCE_SBL_QAF_BOOT_CMD_GEN_REC		5
#define SCE_SBL_QAF_BOOT_CMD_CHECK_BIT1		6
#define SCE_SBL_QAF_BOOT_CMD_CLEAR_BIT1		7
#define SCE_SBL_QAF_BOOT_CMD_SET_BIT1		8
#define SCE_SBL_QAF_BOOT_CMD_SERVICE		0xff

#define SCE_SBL_QAF_CMD_SNVS_SLOT0		1u
#define SCE_SBL_QAF_CMD_SNVS_ALL_SLOTS		2u
#define SCE_SBL_QAF_CMD_VERIFY_TOKEN		12u
#define SCE_SBL_QAF_CMD_APPLY_TOKEN		13u

#define SCE_SBL_ERROR_QAF_INVALID_VALUE		((int)0x800f0716u)
#define SCE_SBL_ERROR_QAF_BAD_MAGIC		((int)0x800f0724u)
#define SCE_SBL_ERROR_QAF_BIT1_SET		((int)0x800f0726u)
#define SCE_SBL_ERROR_QAF_BAD_MAC		((int)0x800f0727u)
#define SCE_SBL_ERROR_QAF_CID_ERROR		((int)0x800f0728u)
#define SCE_SBL_ERROR_QAF_CID_MISMATCH		((int)0x800f0739u)

/* ============================================================================
 * QAF Structures (names ours)
 * ============================================================================ */

typedef struct QafKeyContext {
	uint8_t slot506[32];			/* 0x00..0x1f */
	uint8_t slot507[16];			/* 0x20..0x2f */
	uint8_t consoleId[16];			/* 0x30..0x3f */
} QafKeyContext;

_Static_assert(sizeof(QafKeyContext) == 0x40, "QafKeyContext size");

typedef struct QafSnvsCipher {
	const void *vptr;			/* 0x00 */
	const uint8_t *dataKey;			/* 0x04 */
	const uint8_t *tweakKey;		/* 0x08 */
	const uint8_t *cmacKey;			/* 0x0c */
	uint32_t dataKeyBits;			/* 0x10 */
	uint32_t cmacKeyBits;			/* 0x14 */
} QafSnvsCipher;

_Static_assert(sizeof(QafSnvsCipher) == 0x18, "QafSnvsCipher size");

/* ============================================================================
 * Function Declarations (names ours except sm_main)
 * ============================================================================ */

void qafReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8);
int sm_main(void);
void qafHookInvoke(void *self, void *commBuf);
int qafRegisterHook(SmServiceContext *srvCtx, uint32_t cmdId,
		QafKeyContext *keyCtx, const SmMemberFnPtr *fnPtr,
		SmServiceHookNode **outNode);
void qafReadHardwareCounter2240(uint8_t *out4);
void qafKeyCtxInitC2(QafKeyContext *ctx, const void *slot506,
		const void *slot507, const void *cid);
void qafKeyCtxInitC1(QafKeyContext *ctx, const void *slot506,
		const void *slot507, const void *cid);
void qafKeyCtxDtorD2(QafKeyContext *ctx);
void qafKeyCtxDtorD1(QafKeyContext *ctx);
int qafUnwrapAndVerifyToken(QafKeyContext *ctx, const uint8_t *inToken);
int qafCmd12VerifyToken(void *ctxPtr, void *commBuf);
int qafBuildNormalizedToken(const QafKeyContext *ctx, uint8_t *outToken128);
int qafCmd13VerifyAndApplyToken(void *ctxPtr, void *commBuf);
int qafBootCmd4VerifyRecord(QafKeyContext *ctx, uint8_t *inOutRec32,
		const void *cmacKey);
int qafBootCmd5GenerateRecord(QafKeyContext *ctx, uint8_t *inOutRec32,
		const void *cmacKey);
int qafBootCmd6CheckFlagBit1(QafKeyContext *ctx, const uint8_t *inRec32);
int qafBootCmd7ClearFlagBit1(QafKeyContext *ctx, uint8_t *inOutRec32);
int qafBootCmd8SetFlagBit1(QafKeyContext *ctx, uint8_t *inOutRec32);
int qafCmd1HandleSnvsSlot0(void *ctxPtr, void *commBuf);
int qafCmd2HandleSnvsAllSlots(void *ctxPtr, void *commBuf);
void qafReadKeyringSlot32Alias(uint32_t slotIndex, uint32_t *outWords8);
void qafStaticInitOrDtor(int isInit);
int qafEnsureSnvsCryptoInitialized(void);
int qafReadSnvsRecord21(void *outWord0, void *outWord1, const void *inSector32);
int qafValidateSectorRange(uint32_t sectorCount, uint32_t startSector);
int qafDecryptSnvsSectors(uint32_t startSector, const void *src,
		void *dst, uint32_t sectorCount);
uint32_t qafIsSnvsInitialized(void);
QafSnvsCipher *qafGetSnvsCipher(void);
const uint8_t *qafGetZeroIv16(void);
int qafBuildSnvsMacChallenge(uint32_t counter, uint32_t sectorIdx,
		uint8_t *outChallenge16);
int qafVerifyAndUnwrapSnvsSectorWrite(uint32_t *inOutCounter,
		uint32_t *outSectorIdx, uint8_t *outSectorData32,
		uint32_t sectorSize, uint8_t *inOutPacket48);
uint32_t qafIncrementSnvsCounter(uint32_t counter);
void qafStaticInitThunk(void);
void qafStaticDtorThunk(void);
void qafSnvsCipherDtorD2(QafSnvsCipher *c);
void qafSnvsCipherDtorD0(QafSnvsCipher *c);
void qafSnvsCipherCtorC2(QafSnvsCipher *c);
void qafSnvsCipherCtorC1(QafSnvsCipher *c);
void qafSnvsCipherDtorD1(QafSnvsCipher *c);
int qafSnvsCipherInit(QafSnvsCipher *c, const uint8_t *dataKey,
		const uint8_t *tweakKey, const uint8_t *cmacKey,
		uint32_t dataKeyBits, uint32_t cmacKeyBits);
int qafSnvsSignRecordCmac(const QafSnvsCipher *c, uint8_t *inOutRec32);
int qafFormatSectorTweak(const QafSnvsCipher *c, uint32_t sectorIdx,
		uint64_t *outTweak16);
int qafSnvsDecryptSectors(const QafSnvsCipher *c, const void *src,
		void *dst, uint32_t totalBytes, uint32_t startSector);
void qafSnvsCipherNoop1(void);
int qafXtsDecryptSector(const uint8_t *tweakKey, const uint8_t *dataKey,
		const uint8_t *tweakIv, uint32_t keyBits, uint32_t sectorBytes,
		const uint8_t *src, uint8_t *dst);
int qafIncrementTweak128(uint8_t *tweak, uint32_t len);
int qafXtsDecryptMultiSectors(const uint8_t *tweakIv, const uint8_t *dataKey,
		const uint8_t *tweakKey, uint32_t keyBits, uint32_t totalBytes,
		uint32_t wordsPerSector, const uint8_t *src, uint8_t *dst);
void qafReadHardwareKey2220(uint32_t *outWords8);
int qafEncryptSnvsSectors(uint32_t startSector, const void *src,
		void *dst, uint32_t sectorCount);
int qafWriteSnvsRecord21(const void *inWord0, const void *inWord1,
		void *outSector32);
int qafWrapSnvsSectorReadResponse(uint32_t counter, uint32_t sectorIdx,
		const void *sectorData32, uint32_t sectorSize,
		uint8_t *outPacket48);
int qafVerifySnvsCommitPacket(uint32_t *inOutCounter, uint32_t *outSectorIdx,
		uint8_t *inOutPacket16);
int qafSnvsEncryptSectors(const QafSnvsCipher *c, const void *src,
		void *dst, uint32_t totalBytes, uint32_t startSector);
int qafXtsEncryptSector(const uint8_t *tweakKey, const uint8_t *dataKey,
		const uint8_t *tweakIv, uint32_t keyBits, uint32_t sectorBytes,
		const uint8_t *src, uint8_t *dst);
int qafXtsEncryptMultiSectors(const uint8_t *tweakIv, const uint8_t *dataKey,
		const uint8_t *tweakKey, uint32_t keyBits, uint32_t totalBytes,
		uint32_t wordsPerSector, const uint8_t *src, uint8_t *dst);
int qafSnvsCipherNoop2(void);
int qafDecryptQaToken(const uint8_t *inToken128, uint8_t *outToken128,
		const void *aesKey32, const void *aesIv16);
int qafVerifyQaTokenCmac(uint8_t *inOutToken128, const void *cmacKey32);
int qafVerifyQaTokenRsaSignature(const void *token128, const void *sig256,
		const BignumPublicKey *pubKey);
int qafVerifySnvsRecordCmac(const uint8_t *rec32, const void *cmacKey32);
int qafCompareByte5WithByte4(const uint8_t *a, const uint8_t *b);
int qafVerifyTokenConsoleId(const uint8_t *token128, const uint8_t *cid16);
int qafFormatConsoleIdForToken(uint8_t *outCid16, const uint8_t *inCid16);
int qafDecodeInvertedBytes(uint8_t *dst, const uint8_t *src, uint32_t len);
int qafEncryptDefaultQaToken(uint8_t *inOutToken128, const void *aesKey32,
		const void *aesIv16);
int qafBuildDefaultNoFlagsToken(uint8_t *outToken128, const void *cmacKey32,
		const uint8_t *cid16);
int qafBuildDefaultSnvsRecord(uint8_t *outRec32, uint8_t flagByte,
		const void *cmacKey32);

#endif /* QAF_SM_H */
