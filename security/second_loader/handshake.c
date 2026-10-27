/*
 * handshake.c — Ernie (Syscon) Cryptographic Handshake Packet Builder & Verifier
 *
 * Address range: 0x808e3e..0x809421
 *
 * Builds and verifies the 40-byte challenge/response packets for Ernie (Syscon)
 * mutual authentication (command 0x30 subcommands 0..5, key slots 0x0b and 0x0f),
 * using Bigmac AES-128-CBC and the hardware RNG at 0xe005003c.
 */

#include "second_loader.h"

/* .rodata: 0x80c430..0x80c4df */
static const u8 s_handshake0fZeroIv[16] = { 0 };

static const u8 s_handshake0fMagicChallenge[16] = {
	0xb0, 0x11, 0x03, 0xb0, 0x62, 0x38, 0x32, 0xd6,
	0x25, 0x40, 0xb5, 0x63, 0x33, 0xd6, 0xe1, 0x1d
};

static const u8 s_handshakeZeroIv[16] = { 0 };

static const u8 s_handshakeKeySlot0b[64] = {
	0xbb, 0x64, 0x47, 0x21, 0xcb, 0x4c, 0x55, 0x07,
	0x2e, 0x83, 0x17, 0x7b, 0xeb, 0x3b, 0xbe, 0xe9,
	0xdc, 0x6b, 0x6e, 0xe0, 0xf4, 0x57, 0xdf, 0x0e,
	0x7b, 0xad, 0x1c, 0x5e, 0xa3, 0x38, 0x02, 0x7f,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xcf, 0x2e, 0x93, 0xe9, 0xf9, 0x4e, 0x28, 0xcc,
	0xa4, 0x80, 0x26, 0x13, 0x4c, 0x7c, 0x77, 0xce
};

static const u8 s_handshakeKeySlot0f[64] = {
	0x50, 0xe4, 0xc3, 0xa7, 0x72, 0x64, 0x16, 0x7c,
	0x40, 0x9c, 0x72, 0xa9, 0xb5, 0x7a, 0x86, 0x09,
	0x9e, 0x34, 0x08, 0x7c, 0x48, 0x98, 0x5b, 0x4b,
	0x35, 0x1a, 0x63, 0x57, 0x2d, 0x9b, 0x48, 0x1b,
	0xeb, 0xe3, 0x46, 0x0d, 0x84, 0xa4, 0x17, 0x54,
	0xac, 0x44, 0x13, 0x68, 0xcf, 0x02, 0x00, 0xd8,
	0xc8, 0x6b, 0x51, 0xfb, 0x01, 0x9a, 0x20, 0x7f,
	0x32, 0x11, 0x8e, 0x55, 0x46, 0x2d, 0x50, 0x08
};

/* .data: 0x80ccc4..0x80cd07 */
u8 *s_handshakeKeyTable[16] = {
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, (u8 *)s_handshakeKeySlot0b,
	NULL, NULL, NULL, (u8 *)s_handshakeKeySlot0f
};

static u32 s_handshakeReservedData __attribute__((used)) = 0;

/* 0x808e3e: constantTimeMemcmp (name ours) */
int constantTimeMemcmp(const void *s1, const void *s2, u32 len)
{
	const u8 *a = (const u8 *)s1;
	const u8 *b = (const u8 *)s2;
	int res = 0;
	u32 i;

	for (i = 0; i < len; i++) {
		if (a[i] != b[i])
			res = -1;
	}
	return res;
}

/* 0x808e5c: handshakeMapKeySlot (name ours) */
int handshakeMapKeySlot(u32 keySlot, int revision, u32 *outMappedSlot)
{
	int res = handshakeCheckKeySlotValid(keySlot);

	if (res == 0) {
		if (keySlot == 6 && revision != 1)
			*outMappedSlot = 14;
		else
			*outMappedSlot = keySlot;
	}
	return res;
}

/* 0x808e90: handshakeBuildStartPacket (name ours) */
int handshakeBuildStartPacket(u8 *pkt, u32 keySlot)
{
	int res = handshakeCheckKeySlotValid(keySlot);

	if (res == 0) {
		pkt[0] = 0x30;
		pkt[1] = 0;
		pkt[2] = 0;
		pkt[3] = (u8)keySlot;
		memset(pkt + 4, 0, 4);
		memset(pkt + 8, 0, 0x20);
	}
	return res;
}

/* 0x808ee0: handshakeBuildChallengeResponse (name ours) */
int handshakeBuildChallengeResponse(u8 *outPkt, const u8 *inPkt, u32 keySlot, u32 revision, const void *hostRandom)
{
	u8 status;
	u32 mappedSlot;
	int res;

	res = handshakeCheckKeySlotValid(keySlot);
	if (res != 0)
		return res;

	if (inPkt[0] != 0x30 || inPkt[1] != 1)
		return SCE_SBL_ERROR_SL_EILLEGAL;

	status = inPkt[2];
	if (status >= 6)
		return (status == 0x10) ? 0x800f0027 : SCE_SBL_ERROR_SL_EILLEGAL;
	if (status != 0)
		return 0x800f0027;

	mappedSlot = inPkt[3];
	if (mappedSlot != keySlot)
		return SCE_SBL_ERROR_SL_EILLEGAL;

	outPkt[0] = 0x30;
	outPkt[1] = 2;
	outPkt[2] = 0;
	outPkt[3] = (u8)mappedSlot;
	*(u32 *)(outPkt + 4) = revision;

	if (mappedSlot == 6 && revision != 1)
		mappedSlot = 14;

	memcpy(outPkt + 8, hostRandom, 8);
	memcpy(outPkt + 16, inPkt + 8, 8);
	memcpy(outPkt + 24, s_handshakeKeyTable[mappedSlot] + 0x30, 0x10);

	if (handshakeAes128CbcEncrypt(outPkt + 8, outPkt + 8, 0x20,
				      s_handshakeKeyTable[mappedSlot], s_handshakeZeroIv) != 0)
		return SCE_SBL_ERROR_SL_EIO;

	return 0;
}

/* 0x808fd4: handshakeVerifyAndDeriveSessionKey (name ours) */
int handshakeVerifyAndDeriveSessionKey(const u8 *inPkt, u32 keySlot, u32 revision,
				       const void *sysconChallenge, const void *hostRandom,
				       void *outSessionKey)
{
	u8 decrypted[32];
	u8 status;
	u32 mappedSlot;
	int res;

	res = handshakeCheckKeySlotValid(keySlot);
	if (res != 0)
		return res;

	if (inPkt[0] != 0x30 || inPkt[1] != 3)
		return SCE_SBL_ERROR_SL_EILLEGAL;

	status = inPkt[2];
	if (status >= 6)
		return (status == 0x10) ? 0x800f0027 : SCE_SBL_ERROR_SL_EILLEGAL;
	if (status != 0)
		return 0x800f0027;

	mappedSlot = inPkt[3];
	if (mappedSlot != keySlot)
		return SCE_SBL_ERROR_SL_EILLEGAL;

	if (mappedSlot == 6 && revision != 1)
		mappedSlot = 14;

	if (handshakeAes128CbcDecrypt(decrypted, inPkt + 8, 0x20,
				      s_handshakeKeyTable[mappedSlot] + 0x10,
				      s_handshakeZeroIv) != 0)
		return SCE_SBL_ERROR_SL_EIO;

	if (constantTimeMemcmp(decrypted, sysconChallenge, 8) != 0 ||
	    constantTimeMemcmp(decrypted + 8, hostRandom, 8) != 0)
		return 0x800f0027;

	if (outSessionKey == NULL)
		return 0;

	memcpy(outSessionKey, sysconChallenge, 8);
	memcpy((u8 *)outSessionKey + 8, hostRandom, 8);
	if (handshakeAes128CbcEncrypt(outSessionKey, outSessionKey, 0x10,
				      s_handshakeKeyTable[mappedSlot] + 0x20,
				      s_handshakeZeroIv) != 0)
		return SCE_SBL_ERROR_SL_EIO;

	return 0;
}

/* 0x809108: handshakeWipeKeySlot (name ours) */
int handshakeWipeKeySlot(u32 keySlot, int revision)
{
	int res = handshakeCheckKeySlotValid(keySlot);

	if (res == 0) {
		if (keySlot == 6 && revision != 1)
			keySlot = 14;
		if (keySlot < 16) {
			if (s_handshakeKeyTable[keySlot] == NULL)
				res = SCE_SBL_ERROR_SL_ENOENT;
			else
				memset(s_handshakeKeyTable[keySlot], 0, 0x40);
		} else {
			res = 0x800f0022;
		}
	}
	return res;
}

/* 0x809166: handshakeAes128CbcEncrypt (name ours) */
int handshakeAes128CbcEncrypt(void *dst, const void *src, u32 size, const void *key, const void *iv)
{
	u8 ivBuf[16];

	memcpy(ivBuf, iv, sizeof(ivBuf));
	if (bigmacAes128CbcEncrypt(dst, src, size, key, ivBuf) == 0)
		return 0;
	return SCE_SBL_ERROR_SL_EIO;
}

/* 0x8091c8: handshakeAes128CbcDecrypt (name ours) */
int handshakeAes128CbcDecrypt(void *dst, const void *src, u32 size, const void *key, const void *iv)
{
	u8 ivBuf[16];

	memcpy(ivBuf, iv, sizeof(ivBuf));
	if (bigmacAes128CbcDecrypt(dst, src, size, key, ivBuf) == 0)
		return 0;
	return SCE_SBL_ERROR_SL_EIO;
}

/* 0x80922a: bigmacRngReadBytes (name ours) */
int bigmacRngReadBytes(void *dst, u32 size)
{
	u32 offset = 0;

	while (size != 0) {
		u32 chunk = (size > 4) ? 4 : size;
		u32 rngWord = SCE_BIGMAC_REG(0, SCE_BIGMAC_RNG);

		memcpy((u8 *)dst + offset, &rngWord, chunk);
		offset += chunk;
		size -= chunk;
	}
	return 0;
}

/* 0x809282: handshake0fBuildAuthPacket (name ours) */
int handshake0fBuildAuthPacket(u8 *outPkt, int revision, const void *sessionKey)
{
	u32 mappedSlot = (u32)-1;

	outPkt[0] = 0x30;
	outPkt[1] = 4;
	outPkt[2] = 0;
	outPkt[3] = 0x0f;

	if (handshakeMapKeySlot(0x0f, revision, &mappedSlot) == 0 && mappedSlot == 0x0f) {
		memset(outPkt + 4, 0, 4);
		memcpy(outPkt + 8, s_handshake0fMagicChallenge, 0x10);
		memset(outPkt + 24, 0, 0x10);
		if (handshakeAes128CbcEncrypt(outPkt + 8, outPkt + 8, 0x20,
					      sessionKey, s_handshake0fZeroIv) == 0)
			return 0;
		return SCE_SBL_ERROR_SL_EIO;
	}
	return 0x800f002f;
}

/* 0x80931c: handshake0fVerifyResponsePacket (name ours) */
int handshake0fVerifyResponsePacket(const u8 *inPkt, int revision, const void *sessionKey)
{
	u32 mappedSlot;
	u8 decrypted[32];
	u8 status;

	if (inPkt[0] != 0x30 || inPkt[1] != 5)
		return SCE_SBL_ERROR_SL_EILLEGAL;

	status = inPkt[2];
	if (status >= 6)
		return (status == 0x10) ? 0x800f0027 : SCE_SBL_ERROR_SL_EILLEGAL;
	if (status != 0)
		return 0x800f0027;
	if (inPkt[3] != 0x0f)
		return SCE_SBL_ERROR_SL_EILLEGAL;

	mappedSlot = (u32)-1;
	if (handshakeMapKeySlot(0x0f, revision, &mappedSlot) != 0 || mappedSlot != 0x0f)
		return 0x800f002f;

	if (handshakeAes128CbcDecrypt(decrypted, inPkt + 8, 0x20,
				      sessionKey, s_handshake0fZeroIv) != 0)
		return SCE_SBL_ERROR_SL_EIO;

	if (constantTimeMemcmp(decrypted, s_handshake0fMagicChallenge, 0x10) == 0)
		return 0;
	return 0x800f0027;
}

/* 0x8093f8: handshakeCheckKeySlotValid (name ours) */
int handshakeCheckKeySlotValid(u32 keySlot)
{
	if (keySlot == 0x0b || keySlot == 0x0f) {
		if (s_handshakeKeyTable[keySlot] == NULL)
			return 0x800f0028;
		return 0;
	}
	return SCE_SBL_ERROR_SL_EINVAL;
}
