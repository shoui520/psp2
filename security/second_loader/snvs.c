/*
 * snvs.c: Secure Non-Volatile Storage (SNVS) sector reading, Ernie handshake
 * mode 0x0f session key establishment, AES-XTS sector decryption, and AES-CMAC
 * sector integrity verification (0x8027e0..0x80363b).
 *
 * Function and variable names here are ours (descriptive), except SpkgSnvsSector
 * from sys/internal/kernel/src/update_mgr/snvs.h.
 */

#include "second_loader.h"

/* ============================================================================
 * .rodata (0x80bf74..0x80bf97)
 * ============================================================================ */

static const uint32_t s_snvsReservedRo74 = 0u;
static const uint8_t s_snvsZeroIv[16] = { 0u };

static const SnvsCryptoVtable s_snvsCryptoVtable = {
	{ 0u, 0u },
	SnvsCrypto_destroy,
	SnvsCrypto_deleteObj
};

/* ============================================================================
 * .data (0x80c8c0..0x80ca07)
 * ============================================================================ */

static uint32_t s_snvsUseSysconProtocol[16] __attribute__((aligned(8))) = { 0u };

/* Primary encrypted SNVS key seeds (for eMMC >= 0x996000 sectors) at 0x80c900 */
static uint8_t s_snvsKeySeedsPrimary[128] = {
	0xe1u, 0x22u, 0xf9u, 0xc4u, 0x7eu, 0x1cu, 0x94u, 0xf0u,
	0x82u, 0x53u, 0x6au, 0x1fu, 0x24u, 0x4bu, 0x9au, 0x9cu,
	0xe0u, 0x8du, 0xa5u, 0xe8u, 0xe1u, 0x22u, 0xf9u, 0xc4u,
	0x7eu, 0x1cu, 0x94u, 0xf0u, 0x82u, 0x53u, 0x6au, 0x1fu,
	0x24u, 0x4bu, 0x9au, 0x9cu, 0xe0u, 0x8du, 0xa5u, 0xe8u,
	0xe1u, 0x22u, 0xf9u, 0xc4u, 0x7eu, 0x1cu, 0x94u, 0xf0u,
	0x82u, 0x53u, 0x6au, 0x1fu, 0x24u, 0x4bu, 0x9au, 0x9cu,
	0xe0u, 0x8du, 0xa5u, 0xe8u, 0xe1u, 0x22u, 0xf9u, 0xc4u,
	0xedu, 0xf4u, 0x81u, 0xefu, 0x0au, 0xffu, 0x55u, 0xcdu,
	0x1au, 0x64u, 0x3eu, 0x8au, 0x7au, 0xf5u, 0x09u, 0x11u,
	0x36u, 0xa7u, 0x74u, 0x9cu, 0xedu, 0xf4u, 0x81u, 0xefu,
	0x0au, 0xffu, 0x55u, 0xcdu, 0x1au, 0x64u, 0x3eu, 0x8au,
};

static uint8_t s_snvsKeySeedsIv[16] = {
	0x7eu, 0x1cu, 0x94u, 0xf0u, 0x82u, 0x53u, 0x6au, 0x1fu,
	0x82u, 0x53u, 0x6au, 0x1fu, 0x24u, 0x4bu, 0x9au, 0x9cu,
};

/* Fallback SNVS key seeds (for eMMC < 0x996000 sectors) at 0x80c990 */
static uint8_t s_snvsKeySeedsFallback[112] = {
	0xe1u, 0x22u, 0xf9u, 0xc4u, 0x7eu, 0x1cu, 0x94u, 0xf0u,
	0x82u, 0x53u, 0x6au, 0x1fu, 0x24u, 0x4bu, 0x9au, 0x9cu,
	0xe0u, 0x8du, 0xa5u, 0xe8u, 0xe1u, 0x22u, 0xf9u, 0xc4u,
	0x7eu, 0x1cu, 0x94u, 0xf0u, 0x82u, 0x53u, 0x6au, 0x1fu,
	0x24u, 0x4bu, 0x9au, 0x9cu, 0xe0u, 0x8du, 0xa5u, 0xe8u,
	0xe1u, 0x22u, 0xf9u, 0xc4u, 0x7eu, 0x1cu, 0x94u, 0xf0u,
	0x82u, 0x53u, 0x6au, 0x1fu, 0x24u, 0x4bu, 0x9au, 0x9cu,
	0xe0u, 0x8du, 0xa5u, 0xe8u, 0xe1u, 0x22u, 0xf9u, 0xc4u,
	0xedu, 0xf4u, 0x81u, 0xefu, 0x0au, 0xffu, 0x55u, 0xcdu,
	0x1au, 0x64u, 0x3eu, 0x8au, 0x7au, 0xf5u, 0x09u, 0x11u,
	0x36u, 0xa7u, 0x74u, 0x9cu, 0xedu, 0xf4u, 0x81u, 0xefu,
	0x0au, 0xffu, 0x55u, 0xcdu, 0x1au, 0x64u, 0x3eu, 0x8au,
};

typedef int (*XtsBlockCryptFn)(void *dst, const void *src, uint32_t size,
		const void *key, BigmacContext *ctx);

static XtsBlockCryptFn s_xtsAesEncryptBlockFn = NULL;
static XtsBlockCryptFn s_xtsAesDecryptBlockFn = NULL;

/* ============================================================================
 * .bss (0x80cd80..0x80cfbf)
 * ============================================================================ */

static uint8_t s_snvsCmdPacket[16] __attribute__((aligned(16)));
static uint8_t s_snvsReplyPacket[48] __attribute__((aligned(16)));
static uint8_t s_snvsHandshakeStartTx[64] __attribute__((aligned(16)));
static uint8_t s_snvsHandshakeChallengeRx[64] __attribute__((aligned(16)));
static uint8_t s_snvsHandshakeReplyTx[64] __attribute__((aligned(16)));
static uint8_t s_snvsHandshakeVerifyRx[64] __attribute__((aligned(16)));
static uint8_t s_snvsHandshakeFinishTx[64] __attribute__((aligned(16)));
static uint8_t s_snvsHandshakeFinalRx[64] __attribute__((aligned(16)));

static SnvsXtsCmacEngine s_snvsCryptoEngine __attribute__((aligned(16)));
static uint32_t s_snvsCryptoInitialized;
static uint32_t s_snvsDataKey[8] __attribute__((aligned(16)));
static uint32_t s_snvsTweakKey[8] __attribute__((aligned(16)));
static uint32_t s_snvsCmacKey[8] __attribute__((aligned(16)));

/* ============================================================================
 * Forward Declarations (internal to snvs.c)
 * ============================================================================ */

static int ensureSnvsCryptoEngineInitialized(void);
static int parseSnvsManagementSector(void *outFlags, void *outStatus, const void *rawSector32);
static int decryptAndVerifySnvsSectors(int sectorIndex, const void *rawSectors,
		void *outPlaintext, int sectorCount);
static int buildEncryptedSnvsReadCmd(uint32_t seqNum, int sectorIndex, uint8_t *outCmd16);
static int decryptAndVerifySnvsReadReply(uint32_t *ioSeqNum, uint32_t *outSectorIdx,
		void *outSectorData32, int expectedDataSize, uint8_t *replyBuf48);
static int advanceSnvsSequenceNumber(int prevSeq);
static int SnvsCrypto_setKeys(SnvsXtsCmacEngine *eng, const void *dataKey,
		const void *tweakKey, int xtsKeyBits, const void *cmacKey, int cmacKeyBits);
static int SnvsCrypto_computeHeaderCmac(SnvsXtsCmacEngine *eng, uint8_t *inOutSector32);
static int SnvsCrypto_decryptSectors(SnvsXtsCmacEngine *eng, const void *src,
		void *dst, uint32_t byteSize, uint32_t startSector);
static int xtsAesDecryptBlocks(const void *initialTweak16, const void *dataKey,
		const void *tweakKey, int keyBits, uint32_t totalBytes,
		uint32_t blocksPerUnit, const uint8_t *src, uint8_t *dst);

/* ============================================================================
 * Functions (0x8027e0..0x80363b)
 * ============================================================================ */

/* 0x8027e0 */
static void keyringWriteSlotLocal(uint32_t slot, const uint32_t *words)
{
	(void)s_snvsReservedRo74;
	KEYRING_CTRL->data = words[0];
	KEYRING_CTRL->reserved0[0] = words[1];
	KEYRING_CTRL->reserved0[1] = words[2];
	KEYRING_CTRL->reserved0[2] = words[3];
	KEYRING_CTRL->reserved0[3] = words[4];
	KEYRING_CTRL->reserved0[4] = words[5];
	KEYRING_CTRL->reserved0[5] = words[6];
	KEYRING_CTRL->reserved0[6] = words[7];
	KEYRING_CTRL->protection = slot & 0xffffu;
}

/* 0x802854 */
static void keyringReadSlotWordBytes(unsigned int slot, uint8_t *dst4)
{
	volatile const uint8_t *src = (volatile const uint8_t *)&KEYRING_SLOTS[slot].words[0];
	unsigned int i;

	for (i = 0; i < 4u; i++)
		dst4[i] = src[i];
}

/* 0x802870 */
int initSnvsBaseKeyringSlots(void)
{
	BigmacContext ctx;
	uint32_t keyslot = 0x216u;
	uint32_t mbrTotalSectors = 0;
	const uint32_t *slot502Src;
	const uint32_t *slot503Src;
	const uint32_t *slot504Src;
	int status;

	bigmacInitContext(&ctx);
	status = bigmacSetChannelIndex(&ctx, 0);
	if (status != 0)
		return status;

	status = bigmacAes128CbcDecryptKeyslot(s_snvsKeySeedsPrimary, s_snvsKeySeedsPrimary,
			0x60u, &keyslot, s_snvsKeySeedsIv, &ctx);
	if (status != 0)
		return status;
	status = bigmacWaitChannel(&ctx);
	if (status != 0)
		return status;

	keyringReadSlotWordBytes(0x50fu, (uint8_t *)&mbrTotalSectors);
	if (mbrTotalSectors < 0x00996000u) {
		slot502Src = (const uint32_t *)(s_snvsKeySeedsFallback + 0x00u);
		slot503Src = (const uint32_t *)(s_snvsKeySeedsFallback + 0x20u);
		slot504Src = (const uint32_t *)(s_snvsKeySeedsFallback + 0x40u);
	} else {
		slot502Src = (const uint32_t *)(s_snvsKeySeedsPrimary + 0x00u);
		slot503Src = (const uint32_t *)(s_snvsKeySeedsPrimary + 0x20u);
		slot504Src = (const uint32_t *)(s_snvsKeySeedsPrimary + 0x40u);
	}
	keyringWriteSlotLocal(0x502u, slot502Src);
	keyringWriteSlotLocal(0x503u, slot503Src);
	keyringWriteSlotLocal(0x504u, slot504Src);
	return 0;
}

/* 0x80290c */
static int readSnvsSectorsFromNvs(int startSector, void *outBuf, int sectorCount)
{
	uint32_t totalBytes = (uint32_t)sectorCount << 5;
	uint32_t offset = 0;

	while (offset < totalBytes) {
		if (sceSysconNvsReadData((uint16_t)((uint32_t)startSector * 0x20u + offset),
				(uint8_t *)outBuf + offset, 0x20u) != 0) {
			return (int)0x800f0005;
		}
		offset += 0x20u;
	}
	return 0;
}

/* 0x802956 */
int runErnieHandshake0F(void)
{
	uint32_t sessionKey[8];
	uint8_t hostRandom[8];
	int status;

	slMemset(sessionKey, 0, sizeof(sessionKey));
	status = ernieHandshakeBuildStartPacket((HandshakePacket *)s_snvsHandshakeStartTx, 0x0fu);
	if (status != 0)
		return status;
	status = sceSysconExchangeHandshakePacket(s_snvsHandshakeStartTx, 40u,
			s_snvsHandshakeChallengeRx, 40u);
	if (status != 0)
		return status;
	status = bigmacGenerateRandom(hostRandom, sizeof(hostRandom));
	if (status != 0)
		return status;
	status = ernieHandshakeProcessChallenge((HandshakePacket *)s_snvsHandshakeReplyTx,
			(const HandshakePacket *)s_snvsHandshakeChallengeRx,
			0x0fu, 1u, hostRandom);
	if (status != 0)
		return status;
	status = sceSysconExchangeHandshakePacket(s_snvsHandshakeReplyTx, 40u,
			s_snvsHandshakeVerifyRx, 40u);
	if (status != 0)
		return status;
	status = ernieHandshakeVerifyAndFinish((const HandshakePacket *)s_snvsHandshakeVerifyRx,
			0x0fu, 1u, s_snvsHandshakeChallengeRx + 8u, hostRandom,
			(uint8_t *)sessionKey);
	if (status != 0)
		return status;
	status = ernieHandshakeBuildFinishPacket((HandshakePacket *)s_snvsHandshakeFinishTx,
			1u, sessionKey);
	if (status != 0)
		return status;
	status = sceSysconExchangeHandshakePacket(s_snvsHandshakeFinishTx, 40u,
			s_snvsHandshakeFinalRx, 40u);
	if (status != 0)
		return status;
	status = ernieHandshakeVerifyFinalAck((const HandshakePacket *)s_snvsHandshakeFinalRx,
			1u, sessionKey);
	if (status != 0)
		return status;
	keyringWriteSlotLocal(0x511u, sessionKey);
	return 0;
}

/* 0x802a30 */
int initSnvsHandshakeOrFallbackKeys(uint32_t resumeValid)
{
	uint32_t zeroKey[8];
	uint32_t baryonVer = 0;
	uint32_t snvsVerInfo = 0;
	uint32_t seqSeed = 0;
	int status;

	if (resumeValid != 0) {
		s_snvsUseSysconProtocol[0] = 1u;
		return 0;
	}

	keyringReadSlotWordBytes(0x508u, (uint8_t *)&baryonVer);
	status = sceSysconGetSnvsVersionInfo(&snvsVerInfo);
	if (status != 0)
		return status;

	if ((baryonVer < 0x00090903u || sceSysconGetBaryonModeBit0() != 0) &&
	    ((snvsVerInfo & 0x00ffffffu) < 0x3600u || sceSysconGetBaryonModeBit0() == 0)) {
		slMemset(zeroKey, 0, sizeof(zeroKey));
		keyringWriteSlotLocal(0x511u, zeroKey);
	} else {
		status = runErnieHandshake0F();
		if (status != 0)
			return status;
		seqSeed = BIGMAC->ch[0].rng;
		s_snvsUseSysconProtocol[0] = 1u;
	}

	KEYRING_CTRL->data = seqSeed;
	KEYRING_CTRL->protection = 0x512u;
	return 0;
}

/* 0x802b0c */
static int readSnvsSectorsViaSyscon(int sectorIndex, void *outBuf, int sectorCount)
{
	uint32_t seqNum = 0;
	uint32_t outSectorIdx = 0;
	int status;

	keyringReadSlotWordBytes(0x512u, (uint8_t *)&seqNum);
	status = buildEncryptedSnvsReadCmd(seqNum, sectorIndex, s_snvsCmdPacket);
	if (status != 0)
		return status;

	status = sceSysconSnvsExchangePacket(s_snvsCmdPacket, 16u, s_snvsReplyPacket, 48u);
	if (status == 0) {
		status = decryptAndVerifySnvsReadReply(&seqNum, &outSectorIdx,
				outBuf, sectorCount << 5, s_snvsReplyPacket);
	}
	advanceSnvsSequenceNumber((int)seqNum);
	return status;
}

/* 0x802b82 */
int readSnvsManagementSector(void *outFlags, void *outStatus)
{
	uint8_t rawSector[32];
	int status;

	status = ensureSnvsCryptoEngineInitialized();
	if (status != 0)
		return status;

	if (s_snvsUseSysconProtocol[0] != 0)
		status = readSnvsSectorsViaSyscon(0, rawSector, 1);
	else
		status = readSnvsSectorsFromNvs(0, rawSector, 1);
	if (status != 0)
		return status;

	return parseSnvsManagementSector(outFlags, outStatus, rawSector);
}

/* 0x802bc6 */
int readSnvsSectorData(int sectorIndex, void *outPlaintext, void *rawSectorScratch, int sectorCount)
{
	int status;

	status = ensureSnvsCryptoEngineInitialized();
	if (status != 0)
		return status;

	if (s_snvsUseSysconProtocol[0] != 0)
		status = readSnvsSectorsViaSyscon(sectorIndex, rawSectorScratch, sectorCount);
	else
		status = readSnvsSectorsFromNvs(sectorIndex, rawSectorScratch, sectorCount);
	if (status != 0)
		return status;

	return decryptAndVerifySnvsSectors(sectorIndex, rawSectorScratch, outPlaintext, sectorCount);
}

/* 0x802c24 */
static void keyringReadSlotWordsSnvs(unsigned int slot, uint32_t *dst)
{
	volatile uint32_t *src = &KEYRING_SLOTS[slot].words[0];

	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
	dst[4] = src[4];
	dst[5] = src[5];
	dst[6] = src[6];
	dst[7] = src[7];
}

/* 0x802c6e */
static void initOrDestroySnvsCryptoEngine(int initialize)
{
	if (initialize == 1)
		SnvsCrypto_init(&s_snvsCryptoEngine);
	else if (initialize == 0)
		SnvsCrypto_destroy(&s_snvsCryptoEngine);
}

/* 0x802c8e */
static int ensureSnvsCryptoEngineInitialized(void)
{
	int status;

	if (s_snvsCryptoInitialized != 0)
		return 0;

	slMemset(s_snvsDataKey, 0, sizeof(s_snvsDataKey));
	slMemset(s_snvsTweakKey, 0, sizeof(s_snvsTweakKey));
	slMemset(s_snvsCmacKey, 0, sizeof(s_snvsCmacKey));

	keyringReadSlotWordsSnvs(0x502u, s_snvsDataKey);
	keyringReadSlotWordsSnvs(0x503u, s_snvsTweakKey);
	keyringReadSlotWordsSnvs(0x504u, s_snvsCmacKey);

	status = SnvsCrypto_setKeys(&s_snvsCryptoEngine, s_snvsDataKey,
			s_snvsTweakKey, 128, s_snvsCmacKey, 128);
	if (status == 0)
		s_snvsCryptoInitialized = 1u;
	return status;
}

/* 0x802d0a */
static int parseSnvsManagementSector(void *outFlags, void *outStatus, const void *rawSector32)
{
	uint8_t decrypted[32];
	uint8_t checkBuf[32];
	int status;

	if (s_snvsCryptoInitialized == 0 || !outFlags || !outStatus || !rawSector32)
		return (int)0x800f0016;

	slMemset(decrypted, 0, sizeof(decrypted));
	status = SnvsCrypto_decryptSectors(&s_snvsCryptoEngine, rawSector32, decrypted, 32u, 0u);
	if (status != 0)
		return status;

	slMemcpy(checkBuf, decrypted, 32u);
	slMemset(checkBuf + 16u, 0, 16u);
	status = SnvsCrypto_computeHeaderCmac(&s_snvsCryptoEngine, checkBuf);
	if (status != 0)
		return status;

	if (slTimingsafeBcmp(checkBuf + 16u, decrypted + 16u, 16u) != 0)
		return (int)0x800f0027;

	slMemcpy(outFlags, checkBuf + 8u, 4u);
	slMemcpy(outStatus, checkBuf + 12u, 4u);
	return 0;
}

/* 0x802dba */
static int validateSnvsSectorRange(int startSector, uint32_t sectorCount)
{
	if ((uint32_t)(startSector - 1) <= 30u && sectorCount < 32u)
		return (uint32_t)startSector + sectorCount < 32u;
	return 0;
}

/* 0x802dd6 */
static int decryptAndVerifySnvsSectors(int sectorIndex, const void *rawSectors,
		void *outPlaintext, int sectorCount)
{
	if (s_snvsCryptoInitialized == 0 || !outPlaintext || !rawSectors)
		return (int)0x800f0016;
	if (!validateSnvsSectorRange(sectorIndex, (uint32_t)sectorCount))
		return (int)0x800f0016;

	return SnvsCrypto_decryptSectors(&s_snvsCryptoEngine, rawSectors,
			outPlaintext, (uint32_t)sectorCount << 5, (uint32_t)sectorIndex);
}

/* 0x802e2c */
uint32_t getSnvsCryptoInitialized(void)
{
	return s_snvsCryptoInitialized;
}

/* 0x802e32 */
SnvsXtsCmacEngine *getSnvsCryptoEngine(void)
{
	return &s_snvsCryptoEngine;
}

/* 0x802e38 */
const uint8_t *getSnvsZeroIv(void)
{
	return s_snvsZeroIv;
}

/* 0x802e3e */
static int buildEncryptedSnvsReadCmd(uint32_t seqNum, int sectorIndex, uint8_t *outCmd16)
{
	uint32_t slot516[8];
	uint32_t sessionKey[8];
	BigmacContext bigmacCtx;
	uint8_t iv[16];
	uint32_t byteOffset;
	uint16_t csum = 0;
	unsigned int i;
	int status;

	keyringReadSlotWordsSnvs(0x516u, slot516);
	if (slot516[0] == 1u)
		return (int)0x800f0029;
	if (!outCmd16 || ((uint32_t)sectorIndex << 5) > 0x3e0u)
		return (int)0x800f0016;

	slMemset(outCmd16, 0, 16u);
	byteOffset = ((uint32_t)sectorIndex << 5) & 0xffffu;
	outCmd16[0] = (uint8_t)byteOffset;
	outCmd16[1] = (uint8_t)(byteOffset >> 8);
	outCmd16[2] = 0x20u;
	outCmd16[3] = 0u;
	outCmd16[4] = (uint8_t)seqNum;
	outCmd16[5] = (uint8_t)(seqNum >> 8);
	outCmd16[6] = (uint8_t)(seqNum >> 16);
	outCmd16[7] = (uint8_t)(seqNum >> 24);

	for (i = 0; i < 14u; i++)
		csum = (uint16_t)(csum + outCmd16[i]);
	csum = (uint16_t)~csum;
	outCmd16[14] = (uint8_t)csum;
	outCmd16[15] = (uint8_t)(csum >> 8);

	keyringReadSlotWordsSnvs(0x511u, sessionKey);
	bigmacInitContext(&bigmacCtx);
	slMemcpy(iv, s_snvsZeroIv, sizeof(iv));
	status = bigmacAes256CbcEncrypt(outCmd16, outCmd16, 16u, sessionKey, iv, &bigmacCtx);
	if (status == 0)
		status = bigmacWaitChannel(&bigmacCtx);
	return status;
}

/* 0x802f40 */
static int decryptAndVerifySnvsReadReply(uint32_t *ioSeqNum, uint32_t *outSectorIdx,
		void *outSectorData32, int expectedDataSize, uint8_t *replyBuf48)
{
	uint32_t sessionKey[8];
	BigmacContext bigmacCtx;
	uint8_t iv[16];
	uint16_t csum = 0;
	uint16_t pktCsum;
	uint32_t pktSeq;
	unsigned int i;
	int status;

	if (!ioSeqNum || !outSectorIdx || !replyBuf48 || !outSectorData32 || expectedDataSize != 32)
		return (int)0x800f0016;

	keyringReadSlotWordsSnvs(0x511u, sessionKey);
	bigmacInitContext(&bigmacCtx);
	slMemcpy(iv, s_snvsZeroIv, sizeof(iv));
	status = bigmacAes256CbcDecrypt(replyBuf48, replyBuf48, 48u, sessionKey, iv, &bigmacCtx);
	if (status != 0)
		return status;
	status = bigmacWaitChannel(&bigmacCtx);
	if (status != 0)
		return status;

	for (i = 0; i < 46u; i++)
		csum = (uint16_t)(csum + replyBuf48[i]);
	csum = (uint16_t)~csum;
	pktCsum = (uint16_t)(((uint16_t)replyBuf48[47] << 8) | replyBuf48[46]);
	if (pktCsum != csum)
		return (int)0x800f0027;

	pktSeq = (uint32_t)replyBuf48[4] |
		 ((uint32_t)replyBuf48[5] << 8) |
		 ((uint32_t)replyBuf48[6] << 16) |
		 ((uint32_t)replyBuf48[7] << 24);
	if (pktSeq != *ioSeqNum)
		return (int)0x800f0027;

	*ioSeqNum = pktSeq;
	*outSectorIdx = (uint32_t)(((uint16_t)replyBuf48[1] << 8) | replyBuf48[0]) >> 5;
	slMemcpy(outSectorData32, replyBuf48 + 14u, 32u);
	return 0;
}

/* 0x80304c */
static int advanceSnvsSequenceNumber(int prevSeq)
{
	uint32_t nextSeq = (uint32_t)(prevSeq + 1);

	KEYRING_CTRL->data = nextSeq;
	KEYRING_CTRL->protection = 0x512u;
	return (int)nextSeq;
}

/* 0x803082: registered in .ctors */
static void __attribute__((constructor)) snvsGlobalCtor(void)
{
	initOrDestroySnvsCryptoEngine(1);
}

/* 0x803088: registered in .dtors */
static void __attribute__((destructor)) snvsGlobalDtor(void)
{
	initOrDestroySnvsCryptoEngine(0);
}

/* 0x80308e */
void SnvsCrypto_destroy(SnvsXtsCmacEngine *eng)
{
	eng->vtable = &s_snvsCryptoVtable;
}

/* 0x803096 */
void SnvsCrypto_deleteObj(SnvsXtsCmacEngine *eng)
{
	eng->vtable = &s_snvsCryptoVtable;
	operatorDeleteNoop(eng);
}

/* 0x8030a0 */
void SnvsCrypto_initBase(SnvsXtsCmacEngine *eng)
{
	eng->vtable = &s_snvsCryptoVtable;
	eng->dataKey = NULL;
	eng->tweakKey = NULL;
	eng->cmacKey = NULL;
	eng->xtsKeyBits = 0;
	eng->cmacKeyBits = 0;
}

/* 0x8030be */
void SnvsCrypto_init(SnvsXtsCmacEngine *eng)
{
	eng->vtable = &s_snvsCryptoVtable;
	eng->dataKey = NULL;
	eng->tweakKey = NULL;
	eng->cmacKey = NULL;
	eng->xtsKeyBits = 0;
	eng->cmacKeyBits = 0;
}

/* 0x8030dc */
void SnvsCrypto_resetVtable(SnvsXtsCmacEngine *eng)
{
	eng->vtable = &s_snvsCryptoVtable;
}

/* 0x8030e4 */
static int SnvsCrypto_setKeys(SnvsXtsCmacEngine *eng, const void *dataKey,
		const void *tweakKey, int xtsKeyBits, const void *cmacKey, int cmacKeyBits)
{
	if (!dataKey || !tweakKey || !cmacKey)
		return (int)0x800f0016;
	if (xtsKeyBits != 128 && xtsKeyBits != 256)
		return (int)0x800f0016;
	if (cmacKeyBits != 128 && cmacKeyBits != 256)
		return (int)0x800f0016;

	eng->dataKey = dataKey;
	eng->tweakKey = tweakKey;
	eng->xtsKeyBits = xtsKeyBits;
	eng->cmacKey = cmacKey;
	eng->cmacKeyBits = cmacKeyBits;
	return 0;
}

/* 0x803136 */
static int SnvsCrypto_computeHeaderCmac(SnvsXtsCmacEngine *eng, uint8_t *inOutSector32)
{
	uint8_t cmacInput[64];
	uint8_t macOut[16];
	BigmacContext bigmacCtx;
	int status;

	if (!inOutSector32)
		return (int)0x800f0016;

	slMemset(macOut, 0, sizeof(macOut));
	slMemset(cmacInput, 0, sizeof(cmacInput));
	slMemcpy(cmacInput, inOutSector32, 32u);
	slMemset(cmacInput + 16u, 0, 16u);

	bigmacInitContext(&bigmacCtx);
	status = bigmacAesCmacWithKey(macOut, cmacInput, sizeof(cmacInput),
			eng->cmacKey, (uint32_t)eng->cmacKeyBits >> 3, &bigmacCtx);
	if (status == 0) {
		status = bigmacWaitChannel(&bigmacCtx);
		if (status == 0)
			slMemcpy(inOutSector32 + 16u, macOut, 16u);
	}
	return status;
}

/* 0x8031e6 */
static int SnvsCrypto_formatSectorTweak(SnvsXtsCmacEngine *eng, uint32_t sectorIndex,
		uint32_t *outTweak4)
{
	uint64_t shiftedIndex;
	uint64_t leTweak;

	(void)eng;
	if (!outTweak4)
		return (int)0x800f0016;

	slMemset(outTweak4, 0, 16u);
	shiftedIndex = (uint64_t)sectorIndex << 1;
	writeLe64(&leTweak, &shiftedIndex);
	outTweak4[0] = (uint32_t)leTweak;
	outTweak4[1] = (uint32_t)(leTweak >> 32);
	return 0;
}

/* 0x803232 */
static int SnvsCrypto_decryptSectors(SnvsXtsCmacEngine *eng, const void *src,
		void *dst, uint32_t byteSize, uint32_t startSector)
{
	uint32_t tweak[4];
	int status;

	if (!eng->dataKey || !eng->tweakKey || !dst || !src || (byteSize & 0xfu) != 0)
		return (int)0x800f0016;

	status = SnvsCrypto_formatSectorTweak(eng, startSector, tweak);
	if (status != 0)
		return status;

	if (xtsAesDecryptBlocks(tweak, eng->dataKey, eng->tweakKey, eng->xtsKeyBits,
			byteSize, 16u, (const uint8_t *)src, (uint8_t *)dst) != 0) {
		return (int)0x800f0027;
	}
	return 0;
}

/* 0x8032c6 */
void snvsNoop(void)
{
}

/* 0x8032c8 */
static int xtsAesDecryptSingleUnit(const void *tweak16, const void *dataKey,
		const void *tweakKey, int keyBits, uint32_t unitBytes,
		const uint8_t *src, uint8_t *dst)
{
	BigmacContext bigmacCtx;
	uint8_t stealBlock[16];
	uint8_t tweakBuf[16];
	uint32_t offset;
	unsigned int i;

	if (unitBytes <= 16u)
		return -1;

	bigmacInitContext(&bigmacCtx);
	if (keyBits == 128) {
		s_xtsAesEncryptBlockFn = bigmacAes128EcbEncrypt;
		s_xtsAesDecryptBlockFn = bigmacAes128EcbDecrypt;
	} else if (keyBits == 256) {
		s_xtsAesEncryptBlockFn = bigmacAes256EcbEncrypt;
		s_xtsAesDecryptBlockFn = bigmacAes256EcbDecrypt;
	} else {
		return -1;
	}

	if (s_xtsAesEncryptBlockFn(tweakBuf, tweak16, 16u, tweakKey, &bigmacCtx) != 0)
		return -1;
	bigmacWaitChannel(&bigmacCtx);

	offset = 0;
	while (offset + 16u <= unitBytes) {
		uint8_t carry = 0;

		for (i = 0; i < 16u; i++)
			dst[offset + i] = src[offset + i] ^ tweakBuf[i];
		if (s_xtsAesDecryptBlockFn(dst + offset, dst + offset, 16u, dataKey, &bigmacCtx) != 0)
			return -1;
		bigmacWaitChannel(&bigmacCtx);
		for (i = 0; i < 16u; i++)
			dst[offset + i] ^= tweakBuf[i];

		/* Multiply tweak by x in GF(2^128) */
		for (i = 0; i < 16u; i++) {
			uint8_t b = tweakBuf[i];

			tweakBuf[i] = (uint8_t)((b << 1) + carry);
			carry = (uint8_t)((int8_t)b >> 7);
		}
		if (carry != 0)
			tweakBuf[0] ^= 0x87u;

		offset += 16u;
	}

	if (offset < unitBytes) {
		uint32_t rem = unitBytes - offset;
		uint8_t *lastFullDst = dst + (offset - 16u);
		const uint8_t *lastFullSrc = src + (offset - 16u);
		uint8_t lowBit;

		for (i = 0; i < 16u; i++)
			lastFullDst[i] = lastFullSrc[i] ^ tweakBuf[i];
		if (s_xtsAesDecryptBlockFn(lastFullDst, lastFullDst, 16u, dataKey, &bigmacCtx) != 0)
			return -1;
		bigmacWaitChannel(&bigmacCtx);
		for (i = 0; i < 16u; i++)
			lastFullDst[i] ^= tweakBuf[i];

		/* Divide tweak by x in GF(2^128) for ciphertext stealing */
		lowBit = tweakBuf[0] & 1u;
		if (lowBit != 0)
			tweakBuf[0] ^= 0x87u;
		for (i = 16u; i > 0u; i--) {
			uint8_t b = tweakBuf[i - 1u];
			uint8_t nextBit = b & 1u;

			tweakBuf[i - 1u] = (uint8_t)((b >> 1) | (lowBit << 7));
			lowBit = nextBit;
		}

		for (i = 0; i < rem; i++) {
			stealBlock[i] = src[offset + i] ^ tweakBuf[i];
			dst[offset + i] = lastFullDst[i];
		}
		for (; i < 16u; i++)
			stealBlock[i] = lastFullDst[i] ^ tweakBuf[i];

		if (s_xtsAesDecryptBlockFn(stealBlock, stealBlock, 16u, dataKey, &bigmacCtx) != 0)
			return -1;
		bigmacWaitChannel(&bigmacCtx);
		for (i = 0; i < 16u; i++)
			lastFullDst[i] = stealBlock[i] ^ tweakBuf[i];
	}
	return 0;
}

/* 0x803552 */
static int incrementLe128Counter(uint8_t *counter, int byteLen)
{
	int i;

	for (i = 0; i < byteLen; i++) {
		if (counter[i] != 0xffu) {
			counter[i]++;
			break;
		}
		counter[i] = 0u;
	}
	return 0;
}

/* 0x803576 */
static int xtsAesDecryptBlocks(const void *initialTweak16, const void *dataKey,
		const void *tweakKey, int keyBits, uint32_t totalBytes,
		uint32_t blocksPerUnit, const uint8_t *src, uint8_t *dst)
{
	uint8_t curTweak[16];
	uint32_t unitSize = blocksPerUnit << 4;
	uint32_t numUnits;
	uint32_t offset = 0;
	uint32_t u;

	slMemcpy(curTweak, initialTweak16, sizeof(curTweak));
	numUnits = (totalBytes / unitSize) + ((totalBytes % unitSize) != 0 ? 1u : 0u);
	for (u = 0; u < numUnits; u++) {
		uint32_t curLen = (u == numUnits - 1u) ? totalBytes : unitSize;

		xtsAesDecryptSingleUnit(curTweak, dataKey, tweakKey, keyBits,
				curLen, src + offset, dst + offset);
		offset += curLen;
		incrementLe128Counter(curTweak, 16);
		totalBytes -= unitSize;
	}
	return 0;
}
