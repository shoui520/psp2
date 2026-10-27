/*
 * qaf.c: Quality Assurance Flag (QAF) token RSA-2048 & HMAC-SHA256 verification,
 * DevKit DIP switch queries, and Syscon NVS boot/hardware flag accessors
 * (0x802044..0x8023a1).
 *
 * Function and variable names here are ours (descriptive).
 */

#include "second_loader.h"

/* ============================================================================
 * .rodata (0x80bdc8..0x80bf13)
 * ============================================================================ */

static const uint8_t s_qafRsaModulus[256] = {
	0xcfu, 0xe6u, 0x7bu, 0xbbu, 0xb3u, 0x5fu, 0x31u, 0x91u,
	0x5cu, 0xf5u, 0xb4u, 0x1bu, 0x57u, 0x95u, 0xa3u, 0xa3u,
	0x5bu, 0x2cu, 0x76u, 0xb3u, 0x97u, 0x85u, 0xf3u, 0x4du,
	0xa8u, 0xf4u, 0x72u, 0x67u, 0xa3u, 0x07u, 0x01u, 0xbcu,
	0xcdu, 0xbfu, 0x75u, 0xa2u, 0xadu, 0x94u, 0x67u, 0xbdu,
	0xe0u, 0xc4u, 0x19u, 0x9au, 0xa0u, 0x2au, 0xb8u, 0x7fu,
	0xafu, 0x37u, 0x93u, 0x7fu, 0x3bu, 0x7du, 0xbcu, 0x57u,
	0xf6u, 0xcau, 0x62u, 0xd9u, 0x68u, 0x55u, 0xb8u, 0x6au,
	0xf6u, 0x97u, 0xa8u, 0x71u, 0x78u, 0xb3u, 0x7au, 0x26u,
	0x4eu, 0x4fu, 0x81u, 0x47u, 0x12u, 0x41u, 0xe9u, 0x10u,
	0x04u, 0xe5u, 0x9fu, 0x19u, 0x1au, 0x31u, 0x2eu, 0xb9u,
	0x29u, 0x14u, 0x2au, 0x90u, 0x79u, 0xceu, 0x89u, 0x6bu,
	0x3du, 0xfeu, 0xdfu, 0xe9u, 0x58u, 0x87u, 0xb1u, 0x9du,
	0xdfu, 0xb1u, 0x00u, 0x59u, 0xdau, 0x1fu, 0x61u, 0x00u,
	0x55u, 0xd5u, 0xc4u, 0xefu, 0xaeu, 0x92u, 0x7au, 0x18u,
	0x7eu, 0x85u, 0x81u, 0x5cu, 0x32u, 0x6fu, 0xd1u, 0xa1u,
	0x2au, 0x2bu, 0x56u, 0xb7u, 0x84u, 0xdau, 0x71u, 0x2bu,
	0x54u, 0xcau, 0xdcu, 0x01u, 0x9cu, 0x2bu, 0x13u, 0x16u,
	0x53u, 0xd5u, 0xd4u, 0x49u, 0x6cu, 0xf7u, 0xc3u, 0xdeu,
	0xf2u, 0xe6u, 0x9eu, 0x8fu, 0x97u, 0x26u, 0x6eu, 0x2du,
	0x84u, 0xd0u, 0x07u, 0x1au, 0x80u, 0x15u, 0x0fu, 0x0au,
	0x4fu, 0x9eu, 0x3bu, 0xd2u, 0x7fu, 0x4du, 0x79u, 0xd8u,
	0xe5u, 0x8fu, 0xd7u, 0x73u, 0x62u, 0x7bu, 0x4du, 0xddu,
	0x93u, 0xceu, 0xabu, 0x90u, 0xd5u, 0x9fu, 0xebu, 0xb0u,
	0x2eu, 0x81u, 0x3fu, 0xbau, 0x9du, 0x5au, 0xdbu, 0xa0u,
	0x88u, 0x72u, 0xabu, 0x92u, 0xc5u, 0xb2u, 0x5au, 0xeau,
	0x96u, 0xf0u, 0xc2u, 0xddu, 0xaeu, 0x1fu, 0xfdu, 0x82u,
	0x21u, 0x5fu, 0x26u, 0x2au, 0x34u, 0x11u, 0xc7u, 0x3bu,
	0x09u, 0x33u, 0x16u, 0x1fu, 0xabu, 0xfau, 0xbau, 0xd3u,
	0x29u, 0x1au, 0x29u, 0x4au, 0xd6u, 0x5eu, 0xadu, 0xa1u,
	0xa7u, 0xedu, 0x6du, 0x30u, 0x3fu, 0x17u, 0xd1u, 0x76u,
	0x61u, 0x8eu, 0x52u, 0x2eu, 0x09u, 0x4bu, 0xdcu, 0xc3u,
};

static const uint32_t s_qafRsaMeta[3] = {
	0x00000100u, 0x01000100u, 0x00000004u,
};

static const uint8_t s_qafKeySeed506[32] = {
	0x0eu, 0x04u, 0xe9u, 0x18u, 0x05u, 0xb1u, 0xbau, 0x0eu,
	0x07u, 0x47u, 0x4du, 0xd2u, 0x57u, 0xa6u, 0x06u, 0x3au,
	0xb6u, 0x0du, 0x12u, 0x96u, 0xb6u, 0xdau, 0xc5u, 0x72u,
	0xacu, 0x3eu, 0x03u, 0x6du, 0x88u, 0x77u, 0x0fu, 0x2cu,
};

static const uint8_t s_qafKeySeed507[32] = {
	0x45u, 0x62u, 0x86u, 0xf8u, 0x24u, 0xdau, 0xb4u, 0x99u,
	0xbcu, 0xfdu, 0x78u, 0x92u, 0x8cu, 0x1bu, 0xa9u, 0x3fu,
	0xd6u, 0x02u, 0x6du, 0x50u, 0x9au, 0x65u, 0xceu, 0xa5u,
	0x50u, 0x10u, 0x52u, 0xc0u, 0xdbu, 0x7fu, 0x4cu, 0xe2u,
};

/* ============================================================================
 * .data (0x80c874..0x80c87b) and .bss (0x80cd54..0x80cd5f)
 * ============================================================================ */

static uint8_t s_hwFlagsCache[8] = { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };

static uint32_t s_devkitFeatureEnabled;
static int8_t s_nvsBootFlags[8];

/* ============================================================================
 * Functions (0x802044..0x8023a1)
 * ============================================================================ */

/* 0x802044 */
void enableDevkitFeatureFlag(void)
{
	s_devkitFeatureEnabled = 1u;
	setDebugUartEnabledFlag(1u);
}

/* 0x802050 */
uint32_t isDevkitFeatureEnabled(void)
{
	return s_devkitFeatureEnabled;
}

/* 0x802056 */
int readDipswFromScratchpad(uint32_t *outDipsw8)
{
	int status;

	status = sceSysconReadScratchpad(0xe0u, outDipsw8, 32u);
	if (status != 0)
		slMemset(outDipsw8, 0, 32u);
	return status;
}

/* 0x802088 */
int checkDipswBit(const uint32_t *dipswWords, unsigned int bit)
{
	return (dipswWords[bit >> 5] & (1u << (bit & 0x1fu))) != 0;
}

/* 0x8020a0 */
static void keyringReadSlotLocal(unsigned int slot, uint32_t *dst)
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

/* 0x8020ea */
int initQafSeedsInKeyring(void)
{
	(void)s_qafRsaMeta;
	writeKeyringSlotBytes(0x506u, s_qafKeySeed506, sizeof(s_qafKeySeed506));
	return writeKeyringSlotBytes(0x507u, s_qafKeySeed507, sizeof(s_qafKeySeed507));
}

/* 0x802112 */
int verifyQafToken(uint8_t *outQaFlags16)
{
	uint8_t rsaSig[256];
	uint8_t decryptedQaf[128];
	uint8_t encryptedQaf[128];
	uint8_t snvsSectorBuf[32];
	uint8_t snvsSectorData[32];
	uint8_t snvsMgmtFlags[32];
	uint8_t pscodeSlot[32];
	RsaPublicKeyParam pubKey;
	uint32_t nvsOffset;
	uint8_t *ptr;
	int status;

	if (!isQafEnabledInNvs())
		return (int)0x800f0002;

	slMemset(encryptedQaf, 0, sizeof(encryptedQaf));
	ptr = encryptedQaf;
	for (nvsOffset = 0x400u; ; nvsOffset += 0x20u, ptr += 0x20u) {
		status = sceSysconNvsReadData((uint16_t)nvsOffset, ptr, 0x20u);
		if (status != 0)
			return status;
		if (nvsOffset == 0x460u)
			break;
	}

	slMemset(rsaSig, 0, sizeof(rsaSig));
	ptr = rsaSig;
	for (nvsOffset = 0x5a0u; ; nvsOffset += 0x20u, ptr += 0x20u) {
		status = sceSysconNvsReadData((uint16_t)nvsOffset, ptr, 0x20u);
		if (status != 0)
			return status;
		if (nvsOffset == 0x680u)
			break;
	}

	pubKey.modulus = (const uint32_t *)s_qafRsaModulus;
	pubKey.modulusWordCount = 64u;
	pubKey.exponent = (const uint32_t *)(s_qafRsaModulus + 260u);
	pubKey.exponentWordCount = 1u;

	if (qafVerifyRsaSignature(encryptedQaf, rsaSig, &pubKey) != 0)
		return (int)0x800f0027;

	slMemset(decryptedQaf, 0, sizeof(decryptedQaf));
	if (qafDecryptToken(encryptedQaf, decryptedQaf, s_qafKeySeed506, s_qafKeySeed507) != 0)
		return (int)0x800f0027;
	if (qafVerifyTokenCmac(decryptedQaf, s_qafKeySeed506) != 0)
		return (int)0x800f0027;

	slMemset(pscodeSlot, 0, sizeof(pscodeSlot));
	keyringReadSlotLocal(0x509u, (uint32_t *)pscodeSlot);
	status = qafCheckPsCodeAndOpenPsId(decryptedQaf, pscodeSlot);
	if (status != 0)
		return status;

	if (!isSnvsInitialized())
		return (int)0x800f0026;

	slMemset(snvsMgmtFlags, 0, sizeof(snvsMgmtFlags));
	keyringReadSlotLocal(0x50bu, (uint32_t *)snvsMgmtFlags);
	if ((~snvsMgmtFlags[4] & 2u) == 0)
		return (int)0x800f0024;

	slMemset(snvsSectorData, 0, sizeof(snvsSectorData));
	slMemset(snvsSectorBuf, 0, sizeof(snvsSectorBuf));
	status = readSnvsSectorData(21, snvsSectorData, snvsSectorBuf, 1);
	if (status != 0)
		return status;

	if (!qafVerifySnvsSector21Cmac(snvsSectorData, s_qafKeySeed506))
		return (int)0x800f0039;
	if (!qafCompareTokenDigest(decryptedQaf, snvsSectorData))
		return (int)0x800f0037;

	slMemcpy(outQaFlags16, decryptedQaf + 0x30u, 16u);
	return 0;
}

/* 0x8022e2 */
int checkBitFlagInArray(const uint8_t *array, int byteOffset, uint8_t mask)
{
	return (array[byteOffset] & mask) != 0;
}

/* 0x8022f4 */
void clearBitFlagInArray(uint8_t *array, int byteOffset, uint8_t mask)
{
	array[byteOffset] &= (uint8_t)~mask;
}

/* 0x802302 */
void clearBootFlagsState(void)
{
	slMemset(s_hwFlagsCache, 0xff, sizeof(s_hwFlagsCache));
	slMemset(s_nvsBootFlags, 0xff, sizeof(s_nvsBootFlags));
}

/* 0x80232a */
int loadBootFlagsFromNvs(void)
{
	s_hwFlagsCache[4] = 0u;
	return sceSysconNvsReadData(0x480u, s_nvsBootFlags, sizeof(s_nvsBootFlags));
}

/* 0x802346 */
int loadHardwareFlagsFromNvs(void)
{
	return sceSysconNvsReadData(0x4a0u, &KBL_PARAM->boot_flags[0], 1u);
}

/* 0x802358 */
int getHwFlagByte4(void)
{
	return (int)(int8_t)s_hwFlagsCache[4];
}

/* 0x802362 */
int isQafEnabledInNvs(void)
{
	return (s_nvsBootFlags[0] & 1) == 0;
}

/* 0x802370 */
int getNvsBootFlagByte1(void)
{
	return (int)s_nvsBootFlags[1];
}

/* 0x80237a */
void setNvsBootFlagByte1(uint8_t val)
{
	s_nvsBootFlags[1] = (int8_t)val;
}

/* 0x802384 */
int getNvsBootFlagByte3(void)
{
	return (int)s_nvsBootFlags[3];
}

/* 0x80238e */
int getNvsBootFlagByte7(void)
{
	return (int)s_nvsBootFlags[7];
}

/* 0x802398 */
int getNvsBootFlagByte6(void)
{
	return (int)s_nvsBootFlags[6];
}
