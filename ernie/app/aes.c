/*
 * AES (FIPS-197): the key expansion, the cipher and its inverse on one 16-byte block, and the
 * modes over a buffer of whole blocks: CBC, CFB, OFB and CTR (a 16-bit counter in the last two
 * bytes of the block). The state is the block in RAM, column by column (state[r + 4 * c]); the
 * round keys are the words of the key schedule, byte 0 the first.
 */
#if !defined(ERNIE_UPDATER) && ERNIE_FW >= 0x0100060B
/* memcpy expanded in place, as Sony's 0x0100060B build does */
#pragma inline
#endif
#include <stddef.h>
#include <string.h>

#include "ernie.h"
#include "aes.h"
#if !defined(ERNIE_UPDATER) && FW_SINCE(0x0100060B)
#include "aestable.h"
#else
#include "gf.h"
#endif

static const unsigned char aesSbox[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
	0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
	0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
	0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
	0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
	0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
	0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
	0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
	0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
	0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
	0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
	0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
	0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
	0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
	0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
	0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
	0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

static const unsigned char aesInvSbox[256] = {
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38,
	0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
	0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
	0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
	0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d,
	0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2,
	0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
	0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
	0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
	0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda,
	0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a,
	0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
	0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
	0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
	0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea,
	0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85,
	0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
	0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
	0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
	0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20,
	0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31,
	0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
	0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
	0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
	0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0,
	0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26,
	0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d,
};

/* the round constants: x^(i - 1) in GF(2^8), in the first byte of the word */
static const unsigned char aesRcon[14] = {
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
	0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d,
};

#ifdef ERNIE_UPDATER
/* the updater's revision carries no revision string, and tests the near blocks against 0 */
#define AES_NULL	0
#else
#if FW_SINCE(0x0100060B)
static const char aesRevision[] = "$Rev: 4488 $";
#elif FW_SINCE(0x00090903)
static const char aesRevision[] = "$Rev: 2587 $";
#elif FW_SINCE(0x00090206)
static const char aesRevision[] = "$Rev: 2166 $";
#else
static const char aesRevision[] = "$Rev: 1987 $";
#endif

/* the application's tests them against NULL, a far null */
#define AES_NULL	NULL
#endif

/* 0x0100060B (rev 4488): table-driven rounds (aestable.c), each round one function, the block
   length fixed at 4 words, and only the CBC mode */
#if !defined(ERNIE_UPDATER) && FW_SINCE(0x0100060B)
#define AES_TABLES	1
#else
#define AES_TABLES	0
#endif

/* a round's key: its 4 words of 4 bytes (the offset in char arithmetic, as the original) */
#define ROUND_KEY(ks, round)	((const unsigned char *)(ks)->w + (round) * 4 * 4)

/* byte n of a word, in memory order */
#define WORD_BYTE(word, n)	(((unsigned char *)&(word))[n])

static unsigned long aesRotWord(unsigned long word);
static unsigned long aesSubWord(unsigned long word);
static void aesAddRoundKey(unsigned char __near *state, const unsigned char *key);
#if AES_TABLES
static void aesSubBytesShiftRows(unsigned char __near *state);
static void aesRound(unsigned char __near *state);
static void aesInvRound(unsigned char __near *state);
static void aesInvShiftRowsSubBytes(unsigned char __near *state);
static void aesXor(unsigned char __near *dst, const unsigned char __near *src);
#else
static void aesSubBytes(unsigned char __near *state);
static void aesShiftRows(unsigned char __near *state);
static void aesMixColumns(unsigned char __near *state);
static void aesInvShiftRows(unsigned char __near *state);
static void aesInvSubBytes(unsigned char __near *state);
static void aesInvMixColumns(unsigned char __near *state);
static void aesXor(unsigned char __near *dst, const unsigned char __near *a,
		   const unsigned char __near *b, unsigned char len);
static void aesIncrementCounter(unsigned char __near *counter);
#endif

int aesKeyExpansion(const unsigned long *key, struct AesKey __near *ks)
{
	unsigned long temp;
	unsigned char i, nk, nr;

	if (key == NULL || ks->w == NULL)
		return AES_ERROR_NULL;
	if (ks->nk < 4 || ks->nk > 8)
		return AES_ERROR_KEY_LENGTH;
#if AES_TABLES
	if (ks->nb != 4)
		return AES_ERROR_BLOCK_LENGTH;
	nk = ks->nk;
	nr = nk + 6;
#else
	if (ks->nb < 4 || ks->nb > 8)
		return AES_ERROR_BLOCK_LENGTH;
	nk = ks->nk;
	if (nk >= 4)
		nr = nk + 6;
	else
		nr = 10;
#endif
	for (i = 0; i < nk; i++)
		ks->w[i] = key[i];
	if (nk < 7) {
		for (i = nk; i < (nr + 1) * 4; i++) {
			temp = *(ks->w + i - 1);
			if (i % nk == 0) {
				temp = aesRotWord(temp);
				temp = aesSubWord(temp);
				WORD_BYTE(temp, 0) ^= aesRcon[i / nk - 1];
			}
			ks->w[i] = *(ks->w + i - nk) ^ temp;
		}
	} else {
		for (i = nk; i < (nr + 1) * 4; i++) {
			temp = *(ks->w + i - 1);
			if (i % nk == 0) {
				temp = aesRotWord(temp);
				temp = aesSubWord(temp);
				WORD_BYTE(temp, 0) ^= aesRcon[i / nk - 1];
			} else if (i % nk == 4) {
				temp = aesSubWord(temp);
			}
			ks->w[i] = *(ks->w + i - nk) ^ temp;
		}
	}
	return 0;
}

static unsigned long aesRotWord(unsigned long word)
{
	unsigned char first;

	first = WORD_BYTE(word, 0);
	WORD_BYTE(word, 0) = WORD_BYTE(word, 1);
	WORD_BYTE(word, 1) = WORD_BYTE(word, 2);
	WORD_BYTE(word, 2) = WORD_BYTE(word, 3);
	WORD_BYTE(word, 3) = first;
	return word;
}

static unsigned long aesSubWord(unsigned long word)
{
	WORD_BYTE(word, 0) = aesSbox[WORD_BYTE(word, 0)];
	WORD_BYTE(word, 1) = aesSbox[WORD_BYTE(word, 1)];
	WORD_BYTE(word, 2) = aesSbox[WORD_BYTE(word, 2)];
	WORD_BYTE(word, 3) = aesSbox[WORD_BYTE(word, 3)];
	return word;
}

#if AES_TABLES
int aesEncrypt(const unsigned char __near *in, unsigned char __near *out, struct AesKey __near *ks)
{
	unsigned char round, nr;

	if (in == AES_NULL || out == AES_NULL || ks->w == NULL)
		return AES_ERROR_NULL;
	if (ks->nk < 4 || ks->nk > 8)
		return AES_ERROR_KEY_LENGTH;
	if (ks->nb != 4)
		return AES_ERROR_BLOCK_LENGTH;
	nr = ks->nk + 6;
	memcpy(out, in, AES_BLOCK_SIZE);
	aesAddRoundKey(out, (const unsigned char *)ks->w);
	for (round = 1; round < nr; round++) {
		aesRound(out);
		aesAddRoundKey(out, ROUND_KEY(ks, round));
	}
	aesSubBytesShiftRows(out);
	aesAddRoundKey(out, ROUND_KEY(ks, round));
	return 0;
}

int aesDecrypt(const unsigned char __near *in, unsigned char __near *out, struct AesKey __near *ks)
{
	unsigned char round, nr;

	if (out == AES_NULL || in == AES_NULL || ks->w == NULL)
		return AES_ERROR_NULL;
	if (ks->nk < 4 || ks->nk > 8)
		return AES_ERROR_KEY_LENGTH;
	if (ks->nb != 4)
		return AES_ERROR_BLOCK_LENGTH;
	nr = ks->nk + 6;
	memcpy(out, in, AES_BLOCK_SIZE);
	aesAddRoundKey(out, ROUND_KEY(ks, nr));
	aesInvShiftRowsSubBytes(out);
	for (round = nr - 1; round != 0; round--) {
		aesAddRoundKey(out, ROUND_KEY(ks, round));
		aesInvRound(out);
	}
	aesAddRoundKey(out, ROUND_KEY(ks, round));
	return 0;
}

static void aesAddRoundKey(unsigned char __near *state, const unsigned char *key)
{
	state[0] ^= key[0];
	state[1] ^= key[1];
	state[2] ^= key[2];
	state[3] ^= key[3];
	state[4] ^= key[4];
	state[5] ^= key[5];
	state[6] ^= key[6];
	state[7] ^= key[7];
	state[8] ^= key[8];
	state[9] ^= key[9];
	state[10] ^= key[10];
	state[11] ^= key[11];
	state[12] ^= key[12];
	state[13] ^= key[13];
	state[14] ^= key[14];
	state[15] ^= key[15];
}

/* SubBytes and ShiftRows, the last round's */
static void aesSubBytesShiftRows(unsigned char __near *state)
{
	unsigned char t;

	state[0] = aesSbox[state[0]];
	state[4] = aesSbox[state[4]];
	state[8] = aesSbox[state[8]];
	state[12] = aesSbox[state[12]];
	t = aesSbox[state[1]];
	state[1] = aesSbox[state[5]];
	state[5] = aesSbox[state[9]];
	state[9] = aesSbox[state[13]];
	state[13] = t;
	t = aesSbox[state[2]];
	state[2] = aesSbox[state[10]];
	state[10] = t;
	t = aesSbox[state[6]];
	state[6] = aesSbox[state[14]];
	state[14] = t;
	t = aesSbox[state[3]];
	state[3] = aesSbox[state[15]];
	state[15] = aesSbox[state[11]];
	state[11] = aesSbox[state[7]];
	state[7] = t;
}

/* SubBytes, ShiftRows and MixColumns: each column from the S-box times 1, 2 and 3 */
static void aesRound(unsigned char __near *state)
{
	unsigned char t[AES_BLOCK_SIZE];

	t[0] = aesSboxMul2[state[0]] ^ aesSboxMul3[state[5]] ^
		aesSbox[state[10]] ^ aesSbox[state[15]];
	t[1] = aesSbox[state[0]] ^ aesSboxMul2[state[5]] ^
		aesSboxMul3[state[10]] ^ aesSbox[state[15]];
	t[2] = aesSbox[state[0]] ^ aesSbox[state[5]] ^
		aesSboxMul2[state[10]] ^ aesSboxMul3[state[15]];
	t[3] = aesSboxMul3[state[0]] ^ aesSbox[state[5]] ^
		aesSbox[state[10]] ^ aesSboxMul2[state[15]];
	t[4] = aesSboxMul2[state[4]] ^ aesSboxMul3[state[9]] ^
		aesSbox[state[14]] ^ aesSbox[state[3]];
	t[5] = aesSbox[state[4]] ^ aesSboxMul2[state[9]] ^
		aesSboxMul3[state[14]] ^ aesSbox[state[3]];
	t[6] = aesSbox[state[4]] ^ aesSbox[state[9]] ^
		aesSboxMul2[state[14]] ^ aesSboxMul3[state[3]];
	t[7] = aesSboxMul3[state[4]] ^ aesSbox[state[9]] ^
		aesSbox[state[14]] ^ aesSboxMul2[state[3]];
	t[8] = aesSboxMul2[state[8]] ^ aesSboxMul3[state[13]] ^
		aesSbox[state[2]] ^ aesSbox[state[7]];
	t[9] = aesSbox[state[8]] ^ aesSboxMul2[state[13]] ^
		aesSboxMul3[state[2]] ^ aesSbox[state[7]];
	t[10] = aesSbox[state[8]] ^ aesSbox[state[13]] ^
		aesSboxMul2[state[2]] ^ aesSboxMul3[state[7]];
	t[11] = aesSboxMul3[state[8]] ^ aesSbox[state[13]] ^
		aesSbox[state[2]] ^ aesSboxMul2[state[7]];
	t[12] = aesSboxMul2[state[12]] ^ aesSboxMul3[state[1]] ^
		aesSbox[state[6]] ^ aesSbox[state[11]];
	t[13] = aesSbox[state[12]] ^ aesSboxMul2[state[1]] ^
		aesSboxMul3[state[6]] ^ aesSbox[state[11]];
	t[14] = aesSbox[state[12]] ^ aesSbox[state[1]] ^
		aesSboxMul2[state[6]] ^ aesSboxMul3[state[11]];
	t[15] = aesSboxMul3[state[12]] ^ aesSbox[state[1]] ^
		aesSbox[state[6]] ^ aesSboxMul2[state[11]];
	state[0] = t[0];
	state[1] = t[1];
	state[2] = t[2];
	state[3] = t[3];
	state[4] = t[4];
	state[5] = t[5];
	state[6] = t[6];
	state[7] = t[7];
	state[8] = t[8];
	state[9] = t[9];
	state[10] = t[10];
	state[11] = t[11];
	state[12] = t[12];
	state[13] = t[13];
	state[14] = t[14];
	state[15] = t[15];
}

/* InvMixColumns, InvShiftRows and InvSubBytes, the inverse rounds before the last */
static void aesInvRound(unsigned char __near *state)
{
	unsigned char t[AES_BLOCK_SIZE];

	t[0] = aesMul14[state[0]] ^ aesMul11[state[1]] ^
		aesMul13[state[2]] ^ aesMul9[state[3]];
	t[5] = aesMul9[state[0]] ^ aesMul14[state[1]] ^
		aesMul11[state[2]] ^ aesMul13[state[3]];
	t[10] = aesMul13[state[0]] ^ aesMul9[state[1]] ^
		aesMul14[state[2]] ^ aesMul11[state[3]];
	t[15] = aesMul11[state[0]] ^ aesMul13[state[1]] ^
		aesMul9[state[2]] ^ aesMul14[state[3]];
	t[4] = aesMul14[state[4]] ^ aesMul11[state[5]] ^
		aesMul13[state[6]] ^ aesMul9[state[7]];
	t[9] = aesMul9[state[4]] ^ aesMul14[state[5]] ^
		aesMul11[state[6]] ^ aesMul13[state[7]];
	t[14] = aesMul13[state[4]] ^ aesMul9[state[5]] ^
		aesMul14[state[6]] ^ aesMul11[state[7]];
	t[3] = aesMul11[state[4]] ^ aesMul13[state[5]] ^
		aesMul9[state[6]] ^ aesMul14[state[7]];
	t[8] = aesMul14[state[8]] ^ aesMul11[state[9]] ^
		aesMul13[state[10]] ^ aesMul9[state[11]];
	t[13] = aesMul9[state[8]] ^ aesMul14[state[9]] ^
		aesMul11[state[10]] ^ aesMul13[state[11]];
	t[2] = aesMul13[state[8]] ^ aesMul9[state[9]] ^
		aesMul14[state[10]] ^ aesMul11[state[11]];
	t[7] = aesMul11[state[8]] ^ aesMul13[state[9]] ^
		aesMul9[state[10]] ^ aesMul14[state[11]];
	t[12] = aesMul14[state[12]] ^ aesMul11[state[13]] ^
		aesMul13[state[14]] ^ aesMul9[state[15]];
	t[1] = aesMul9[state[12]] ^ aesMul14[state[13]] ^
		aesMul11[state[14]] ^ aesMul13[state[15]];
	t[6] = aesMul13[state[12]] ^ aesMul9[state[13]] ^
		aesMul14[state[14]] ^ aesMul11[state[15]];
	t[11] = aesMul11[state[12]] ^ aesMul13[state[13]] ^
		aesMul9[state[14]] ^ aesMul14[state[15]];
	state[0] = aesInvSbox[t[0]];
	state[1] = aesInvSbox[t[1]];
	state[2] = aesInvSbox[t[2]];
	state[3] = aesInvSbox[t[3]];
	state[4] = aesInvSbox[t[4]];
	state[5] = aesInvSbox[t[5]];
	state[6] = aesInvSbox[t[6]];
	state[7] = aesInvSbox[t[7]];
	state[8] = aesInvSbox[t[8]];
	state[9] = aesInvSbox[t[9]];
	state[10] = aesInvSbox[t[10]];
	state[11] = aesInvSbox[t[11]];
	state[12] = aesInvSbox[t[12]];
	state[13] = aesInvSbox[t[13]];
	state[14] = aesInvSbox[t[14]];
	state[15] = aesInvSbox[t[15]];
}

/* InvShiftRows and InvSubBytes, the first inverse round's */
static void aesInvShiftRowsSubBytes(unsigned char __near *state)
{
	unsigned char t;

	state[0] = aesInvSbox[state[0]];
	state[4] = aesInvSbox[state[4]];
	state[8] = aesInvSbox[state[8]];
	state[12] = aesInvSbox[state[12]];
	t = aesInvSbox[state[13]];
	state[13] = aesInvSbox[state[9]];
	state[9] = aesInvSbox[state[5]];
	state[5] = aesInvSbox[state[1]];
	state[1] = t;
	t = aesInvSbox[state[2]];
	state[2] = aesInvSbox[state[10]];
	state[10] = t;
	t = aesInvSbox[state[6]];
	state[6] = aesInvSbox[state[14]];
	state[14] = t;
	t = aesInvSbox[state[3]];
	state[3] = aesInvSbox[state[7]];
	state[7] = aesInvSbox[state[11]];
	state[11] = aesInvSbox[state[15]];
	state[15] = t;
}

/* dst ^= src, a block, from the last byte; never called */
static void aesXor(unsigned char __near *dst, const unsigned char __near *src)
{
	unsigned char i;

	i = AES_BLOCK_SIZE;
	while (i != 0) {
		i--;
		dst[i] ^= src[i];
	}
}

int aesCbcEncrypt(const unsigned char __near *in, unsigned char __near *out,
		  struct AesKey __near *ks, const unsigned char *iv, unsigned int len)
{
	char ret;
	unsigned char block[AES_BLOCK_SIZE];

	if (len % AES_BLOCK_SIZE)
		return AES_ERROR_LENGTH;
	memcpy((unsigned char __near *)block, iv, AES_BLOCK_SIZE);
	while (len != 0) {
		block[0] ^= in[0];
		block[1] ^= in[1];
		block[2] ^= in[2];
		block[3] ^= in[3];
		block[4] ^= in[4];
		block[5] ^= in[5];
		block[6] ^= in[6];
		block[7] ^= in[7];
		block[8] ^= in[8];
		block[9] ^= in[9];
		block[10] ^= in[10];
		block[11] ^= in[11];
		block[12] ^= in[12];
		block[13] ^= in[13];
		block[14] ^= in[14];
		block[15] ^= in[15];
		ret = aesEncrypt(block, out, ks);
		memcpy((unsigned char __near *)block, out, AES_BLOCK_SIZE);
		in += AES_BLOCK_SIZE;
		out += AES_BLOCK_SIZE;
		len -= AES_BLOCK_SIZE;
	}
	return ret;
}

int aesCbcDecrypt(const unsigned char __near *in, unsigned char __near *out,
		  struct AesKey __near *ks, const unsigned char *iv, unsigned int len)
{
	char ret;
	unsigned char block[AES_BLOCK_SIZE];

	if (len % AES_BLOCK_SIZE)
		return AES_ERROR_LENGTH;
	memcpy((unsigned char __near *)block, iv, AES_BLOCK_SIZE);
	while (len != 0) {
		ret = aesDecrypt(in, out, ks);
		out[0] ^= block[0];
		out[1] ^= block[1];
		out[2] ^= block[2];
		out[3] ^= block[3];
		out[4] ^= block[4];
		out[5] ^= block[5];
		out[6] ^= block[6];
		out[7] ^= block[7];
		out[8] ^= block[8];
		out[9] ^= block[9];
		out[10] ^= block[10];
		out[11] ^= block[11];
		out[12] ^= block[12];
		out[13] ^= block[13];
		out[14] ^= block[14];
		out[15] ^= block[15];
		memcpy((unsigned char __near *)block, in, AES_BLOCK_SIZE);
		out += AES_BLOCK_SIZE;
		in += AES_BLOCK_SIZE;
		len -= AES_BLOCK_SIZE;
	}
	return ret;
}
#else
int aesEncrypt(const unsigned char __near *in, unsigned char __near *out, struct AesKey __near *ks)
{
	unsigned char round, nk, nr;
#if !FW_SINCE(0x00090804)
	unsigned long unused;		/* 0x00090206 keeps four more bytes in the frame */
#endif

	if (in == AES_NULL || out == AES_NULL || ks->w == NULL)
		return AES_ERROR_NULL;
	if (ks->nk < 4 || ks->nk > 8)
		return AES_ERROR_KEY_LENGTH;
	if (ks->nb < 4 || ks->nb > 8)
		return AES_ERROR_BLOCK_LENGTH;
	nk = ks->nk;
	if (nk >= 4)
		nr = nk + 6;
	else
		nr = 10;
	memcpy(out, in, AES_BLOCK_SIZE);
	aesAddRoundKey(out, (const unsigned char *)ks->w);
	for (round = 1; round < nr; round++) {
		aesSubBytes(out);
		aesShiftRows(out);
		aesMixColumns(out);
		aesAddRoundKey(out, ROUND_KEY(ks, round));
	}
	aesSubBytes(out);
	aesShiftRows(out);
	aesAddRoundKey(out, ROUND_KEY(ks, nr));
	return 0;
}

int aesDecrypt(const unsigned char __near *in, unsigned char __near *out, struct AesKey __near *ks)
{
	unsigned char round, nk, nr;
#if !FW_SINCE(0x00090804)
	unsigned long unused;		/* 0x00090206 keeps four more bytes in the frame */
#endif

	if (out == AES_NULL || in == AES_NULL || ks->w == NULL)
		return AES_ERROR_NULL;
	if (ks->nk < 4 || ks->nk > 8)
		return AES_ERROR_KEY_LENGTH;
	if (ks->nb < 4 || ks->nb > 8)
		return AES_ERROR_BLOCK_LENGTH;
	nk = ks->nk;
	if (nk >= 4)
		nr = nk + 6;
	else
		nr = 10;
	memcpy(out, in, AES_BLOCK_SIZE);
	aesAddRoundKey(out, ROUND_KEY(ks, nr));
	for (round = nr - 1; round != 0; round--) {
		aesInvShiftRows(out);
		aesInvSubBytes(out);
		aesAddRoundKey(out, ROUND_KEY(ks, round));
		aesInvMixColumns(out);
	}
	aesInvShiftRows(out);
	aesInvSubBytes(out);
	aesAddRoundKey(out, (const unsigned char *)ks->w);
	return 0;
}

int aesCbcEncrypt(const unsigned char __near *in, unsigned char __near *out,
		  struct AesKey __near *ks, const unsigned char *iv, unsigned int len)
{
	char ret;
	unsigned char block[AES_BLOCK_SIZE];

	if (len % AES_BLOCK_SIZE)
		return AES_ERROR_LENGTH;
	memcpy(block, iv, AES_BLOCK_SIZE);
	while (len != 0) {
		aesXor(block, block, in, AES_BLOCK_SIZE);
		ret = aesEncrypt(block, out, ks);
		memcpy(block, out, AES_BLOCK_SIZE);
		in += AES_BLOCK_SIZE;
		out += AES_BLOCK_SIZE;
		len -= AES_BLOCK_SIZE;
	}
	return ret;
}

int aesCbcDecrypt(const unsigned char __near *in, unsigned char __near *out,
		  struct AesKey __near *ks, const unsigned char *iv, unsigned int len)
{
	char ret;
	unsigned char block[AES_BLOCK_SIZE];

	if (len % AES_BLOCK_SIZE)
		return AES_ERROR_LENGTH;
	memcpy(block, iv, AES_BLOCK_SIZE);
	while (len != 0) {
		ret = aesDecrypt(in, out, ks);
		aesXor(out, out, block, AES_BLOCK_SIZE);
		memcpy(block, in, AES_BLOCK_SIZE);
		out += AES_BLOCK_SIZE;
		in += AES_BLOCK_SIZE;
		len -= AES_BLOCK_SIZE;
	}
	return ret;
}

int aesCfbEncrypt(const unsigned char __near *in, unsigned char __near *out,
		  struct AesKey __near *ks, const unsigned char *iv, unsigned int len)
{
	char ret;
	unsigned char block[AES_BLOCK_SIZE];

	if (len % AES_BLOCK_SIZE)
		return AES_ERROR_LENGTH;
	memcpy(block, iv, AES_BLOCK_SIZE);
	while (len != 0) {
		ret = aesEncrypt(block, out, ks);
		aesXor(out, out, in, AES_BLOCK_SIZE);
		memcpy(block, out, AES_BLOCK_SIZE);
		in += AES_BLOCK_SIZE;
		out += AES_BLOCK_SIZE;
		len -= AES_BLOCK_SIZE;
	}
	return ret;
}

int aesCfbDecrypt(const unsigned char __near *in, unsigned char __near *out,
		  struct AesKey __near *ks, const unsigned char *iv, unsigned int len)
{
	char ret;
	unsigned char block[AES_BLOCK_SIZE];

	if (len % AES_BLOCK_SIZE)
		return AES_ERROR_LENGTH;
	memcpy(block, iv, AES_BLOCK_SIZE);
	while (len != 0) {
		ret = aesEncrypt(block, out, ks);
		aesXor(out, out, in, AES_BLOCK_SIZE);
		memcpy(block, in, AES_BLOCK_SIZE);
		out += AES_BLOCK_SIZE;
		in += AES_BLOCK_SIZE;
		len -= AES_BLOCK_SIZE;
	}
	return ret;
}

int aesOfb(const unsigned char __near *in, unsigned char __near *out, struct AesKey __near *ks,
	   const unsigned char *iv, unsigned int len)
{
	char ret;
	unsigned char block[AES_BLOCK_SIZE];

	if (len % AES_BLOCK_SIZE)
		return AES_ERROR_LENGTH;
	memcpy(block, iv, AES_BLOCK_SIZE);
	while (len != 0) {
		ret = aesEncrypt(block, out, ks);
		memcpy(block, out, AES_BLOCK_SIZE);
		aesXor(out, out, in, AES_BLOCK_SIZE);
		in += AES_BLOCK_SIZE;
		out += AES_BLOCK_SIZE;
		len -= AES_BLOCK_SIZE;
	}
	return ret;
}

int aesCtr(const unsigned char __near *in, unsigned char __near *out, struct AesKey __near *ks,
	   const unsigned char *iv, unsigned int len)
{
	char ret;
	unsigned char block[AES_BLOCK_SIZE];

	if (len % AES_BLOCK_SIZE)
		return AES_ERROR_LENGTH;
	memcpy(block, iv, AES_BLOCK_SIZE);
	while (len != 0) {
		ret = aesEncrypt(block, out, ks);
		aesXor(out, out, in, AES_BLOCK_SIZE);
		aesIncrementCounter(&block[AES_BLOCK_SIZE - 2]);
		in += AES_BLOCK_SIZE;
		out += AES_BLOCK_SIZE;
		len -= AES_BLOCK_SIZE;
	}
	return ret;
}

static void aesAddRoundKey(unsigned char __near *state, const unsigned char *key)
{
	unsigned char i;

	for (i = 0; i < AES_BLOCK_SIZE; i++)
		state[i] ^= key[i];
}

static void aesSubBytes(unsigned char __near *state)
{
	unsigned char i;

	for (i = 0; i < AES_BLOCK_SIZE; i++)
		state[i] = aesSbox[state[i]];
}

/* row r rotated left by r */
static void aesShiftRows(unsigned char __near *state)
{
	unsigned char i;
	unsigned char saved[3];

	saved[0] = state[1];
	for (i = 1; i < 13; i += 4)
		state[i] = state[i + 4];
	state[i] = saved[0];

	saved[0] = state[2];
	saved[1] = state[6];
	for (i = 2; i < 10; i += 4)
		state[i] = state[i + 8];
	state[i] = saved[0];
	state[i + 4] = saved[1];

	saved[0] = state[3];
	saved[1] = state[7];
	saved[2] = state[11];
	for (i = 3; i < 7; i += 4)
		state[i] = state[i + 12];
	state[i] = saved[0];
	state[i + 4] = saved[1];
	state[i + 8] = saved[2];
}

static void aesMixColumns(unsigned char __near *state)
{
	unsigned char column[4];
	unsigned char i;

	for (i = 0; i < AES_BLOCK_SIZE; i += 4) {
		column[0] = gfMul2(state[i]);
		column[0] ^= gfMul3(state[i + 1]);
		column[0] ^= state[i + 2];
		column[0] ^= state[i + 3];
		column[1] = state[i];
		column[1] ^= gfMul2(state[i + 1]);
		column[1] ^= gfMul3(state[i + 2]);
		column[1] ^= state[i + 3];
		column[2] = state[i];
		column[2] ^= state[i + 1];
		column[2] ^= gfMul2(state[i + 2]);
		column[2] ^= gfMul3(state[i + 3]);
		column[3] = gfMul3(state[i]);
		column[3] ^= state[i + 1];
		column[3] ^= state[i + 2];
		column[3] ^= gfMul2(state[i + 3]);
		state[i] = column[0];
		state[i + 1] = column[1];
		state[i + 2] = column[2];
		state[i + 3] = column[3];
	}
}

/* row r rotated right by r */
static void aesInvShiftRows(unsigned char __near *state)
{
	unsigned char i;
	unsigned char saved[3];

	saved[0] = state[13];
	for (i = 13; i >= 2; i -= 4)
		state[i] = state[i - 4];
	state[1] = saved[0];

	saved[0] = state[14];
	saved[1] = state[10];
	for (i = 14; i >= 8; i -= 4)
		state[i] = state[i - 8];
	state[6] = saved[0];
	state[2] = saved[1];

	saved[0] = state[15];
	saved[1] = state[11];
	saved[2] = state[7];
	for (i = 15; i >= 12; i -= 4)
		state[i] = state[i - 12];
	state[11] = saved[0];
	state[7] = saved[1];
	state[3] = saved[2];
}

static void aesInvSubBytes(unsigned char __near *state)
{
	unsigned char i;

	for (i = 0; i < AES_BLOCK_SIZE; i++)
		state[i] = aesInvSbox[state[i]];
}

static void aesInvMixColumns(unsigned char __near *state)
{
	unsigned char column[4];
	unsigned char i;

	for (i = 0; i < AES_BLOCK_SIZE; i += 4) {
		column[0] = gfMul14(state[i]);
		column[0] ^= gfMul11(state[i + 1]);
		column[0] ^= gfMul13(state[i + 2]);
		column[0] ^= gfMul9(state[i + 3]);
		column[1] = gfMul9(state[i]);
		column[1] ^= gfMul14(state[i + 1]);
		column[1] ^= gfMul11(state[i + 2]);
		column[1] ^= gfMul13(state[i + 3]);
		column[2] = gfMul13(state[i]);
		column[2] ^= gfMul9(state[i + 1]);
		column[2] ^= gfMul14(state[i + 2]);
		column[2] ^= gfMul11(state[i + 3]);
		column[3] = gfMul11(state[i]);
		column[3] ^= gfMul13(state[i + 1]);
		column[3] ^= gfMul9(state[i + 2]);
		column[3] ^= gfMul14(state[i + 3]);
		state[i] = column[0];
		state[i + 1] = column[1];
		state[i + 2] = column[2];
		state[i + 3] = column[3];
	}
}

/* dst = a ^ b, len bytes, from the last */
static void aesXor(unsigned char __near *dst, const unsigned char __near *a,
		   const unsigned char __near *b, unsigned char len)
{
	while (len != 0) {
		len--;
		dst[len] = a[len] ^ b[len];
	}
}

/* the 16-bit big-endian counter at counter[0..1] incremented */
static void aesIncrementCounter(unsigned char __near *counter)
{
	unsigned char low;

	low = ++counter[1];
	if (low == 0)
		counter[0]++;
}
#endif
