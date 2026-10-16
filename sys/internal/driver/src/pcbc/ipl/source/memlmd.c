/*
 * Module decryption (memlmd): KIRK driver by polling, the "~PSP" module
 * decrypter registered with loadelf as its get-length function, and the
 * scramble key it derives its per-boot key from.
 *
 * KIRK (0xBDE00000) works in place on buffers that start with a 0x14-byte
 * header {mode, 0, 0, key code, size}.
 */

#include "ipl.h"
#include "psp_header.h"
#include "ipl_data.h"
#include "memlmd.h"

#define KIRK_BASE		0xBDE00000
#define KIRK_RESET		HW(KIRK_BASE + 0x08)
#define KIRK_PHASE		HW(KIRK_BASE + 0x0C)
#define KIRK_CMD		HW(KIRK_BASE + 0x10)
#define KIRK_RESULT		HW(KIRK_BASE + 0x14)
#define KIRK_STATUS		HW(KIRK_BASE + 0x1C)
#define KIRK_STATUS_ACK		HW(KIRK_BASE + 0x28)
#define KIRK_SRC		HW(KIRK_BASE + 0x2C)
#define KIRK_DST		HW(KIRK_BASE + 0x30)

#define KIRK_STATUS_DONE	0x01
#define KIRK_STATUS_ERROR	0x02
#define KIRK_STATUS_SUSPENDED	0x10
#define KIRK_STATUS_ANY		0x33

#define KIRK_CMD_DECRYPT_PRIVATE	1
#define KIRK_CMD_DECRYPT_IV0		7
#define KIRK_CMD_DECRYPT_FUSE		8
#define KIRK_CMD_SHA1			11

#define KIRK_MODE_DECRYPT_CBC		5

#define DECRYPT_KEY_CODE		0x43	/* KIRK 7 key seed of kernel modules */

/* kseg0 address in main RAM (0x08000000-0x0FFFFFFF physical) */
int IsRamAddress(u32 addr)
{
	return ((addr >> 27) & 3) == 1;
}

/* write back and invalidate the whole data cache before KIRK reads RAM */
static inline void dcacheFlush(void)
{
	u32 size = (1 << (((cp0Config() >> 6) & 7) + 12)) >> 1;
	u32 a;

	for (a = 0; a < size; a += 64) {
		dcacheIndexWritebackInvalidate(a);
		dcacheIndexWritebackInvalidate(a);
	}
}

int sceUtilsBufferCopyByPolling(void *out, void *in, int cmd)
{
	int ret;

	dcacheFlush();
	if (IsRamAddress((u32)in))
		sceSysregSync(5);

	KIRK_CMD = cmd;
	KIRK_SRC = PHYS(in);
	KIRK_DST = PHYS(out);
	KIRK_PHASE = 1;
	while ((KIRK_STATUS & KIRK_STATUS_ANY) == 0)
		;

	if (KIRK_STATUS & 0x22) {
		KIRK_RESET = 1;
		sync();
		ret = -1;
	} else if (KIRK_STATUS & KIRK_STATUS_SUSPENDED) {
		KIRK_PHASE = 2;
		while ((KIRK_STATUS & KIRK_STATUS_ANY) == 0)
			;
		if ((KIRK_STATUS & KIRK_STATUS_ERROR) == 0) {
			KIRK_RESET = 1;
			sync();
		}
		ret = -1;
	} else {
		ret = KIRK_RESULT;
	}

	if (IsRamAddress((u32)in))
		sceSysregSync(5);
	KIRK_STATUS_ACK = KIRK_STATUS;
	sync();
	return ret;
}

static int kirkDecrypt(u32 *buf, SceSize size, u32 keyCode, int cmd)
{
	buf[0] = KIRK_MODE_DECRYPT_CBC;
	buf[3] = keyCode;
	buf[4] = size;
	buf[1] = 0;
	buf[2] = 0;
	g_kirkBuffer = buf;
	return sceUtilsBufferCopyByPolling(buf, buf, cmd) == 0 ? 0 : -1;
}

/* KIRK command 7: AES-CBC decrypt with a key seed */
int kirk7(void *buf, SceSize size, u32 keyCode)
{
	return kirkDecrypt(buf, size, keyCode, KIRK_CMD_DECRYPT_IV0);
}

/* KIRK command 8: AES-CBC decrypt with the fuse-derived key */
int kirk8(void *buf, SceSize size)
{
	return kirkDecrypt(buf, size, 0x100, KIRK_CMD_DECRYPT_FUSE);
}

int memlmdMemcmp(const u8 *a, const u8 *b, SceSize n)
{
	SceSize i;

	for (i = 0; i < n; i++) {
		if (a[i] != b[i])
			return a[i] - b[i];
	}
	return 0;
}

/* 1 if the module's key data matches one of the 16-byte entries of list */
int checkWhitelist(const u8 *prx, const u8 *list, SceSize listSize)
{
	SceSize i;
	int j;

	if (list == NULL || listSize == 0)
		return 0;
	for (i = 0; i < listSize >> 4; i++) {
		const s8 *a = (const s8 *)list + i * 16;
		const s8 *b = (const s8 *)prx + 0x140;

		for (j = 0; j < 16; j++) {
			if (a[j] != b[j])
				break;
		}
		if (j == 16)
			return 1;
	}
	return 0;
}

static void copy(void *dst, const void *src, SceSize n)
{
	u8 *d = dst;
	const u8 *s = src;

	while (n--)
		*d++ = *s++;
}

static void clear(void *dst, SceSize n)
{
	u8 *d = dst;

	while (n--)
		*d++ = 0;
}

/*
 * Decrypt a "~PSP" module in place; *newSize receives the plaintext size.
 * Registered as loadelf's get-length function.
 */
int sceUtilsGetLoadModuleABLengthByPolling(u8 *prx, SceSize size, SceSize *newSize)
{
	u8 *key = g_scrambleKey;
	u8 *list = g_whitelist;
	u8 sigSave[0x28], hashSave[0x14];
	int i, ret = -2;

	if (prx == NULL || newSize == NULL || size < 0x160 || ((u32)prx & 0x3F) != 0)
		goto out;
	if (!IsRamAddress((u32)prx))
		goto out;

	if (memlmdMemcmp(g_moduleTag, prx + 0xD0, 4) != 0) {
		clear(key, 16);
		ret = -3;
		goto out;
	}
	for (i = 0; i < 0x30; i++) {
		if (prx[0xD4 + i] != 0) {
			ret = -5;
			goto out;
		}
	}
	if (checkWhitelist(prx, list, 0) == 1) {
		ret = -4;
		goto out;
	}

	*newSize = prx[0xB0] | prx[0xB1] << 8 | prx[0xB2] << 16 | prx[0xB3] << 24;
	if (*newSize > size - PSP_HEADER_SIZE) {
		ret = -2;
		goto out;
	}

	/* keep the header; derive 9 key blocks from the scramble key */
	copy(g_header, prx, PSP_HEADER_SIZE);
	for (i = 0; i < 9; i++) {
		copy(g_decryptWork + 0x14 + i * 16, key, 16);
		g_decryptWork[0x14 + i * 16] = i;
	}

	copy(g_keySave, prx + 0x104, 0x28);
	clear(prx + 0x104, 0x28);
	copy(sigSave, g_keySave, 0x28);
	*(u32 *)prx = size - 4;
	copy(hashSave, prx, 0x14);
	g_kirkBuffer = (u32 *)prx;
	copy(prx, g_header, 0x20);

	copy(g_decryptHash, g_hashHeaderSeed, 0x28);
	copy(g_decryptHash + 0x28, hashSave, 0x14);
	copy(g_decryptHash + 0x3C, sigSave, 0x28);
	clear(key, 16);

	ret = -1;
	if (kirk7(g_decryptWork, 0x90, DECRYPT_KEY_CODE) < 0)
		goto restore;

	/* rearrange the header for the hash and decryption commands */
	copy(prx + 0x00, g_header + 0xD0, 0x5C);
	copy(prx + 0x5C, g_header + 0x140, 0x10);
	copy(prx + 0x6C, g_header + 0x12C, 0x14);
	copy(prx + 0x80, g_header + 0x80, 0x30);
	copy(prx + 0xB0, g_header + 0xC0, 0x10);
	copy(prx + 0xC0, g_header + 0xB0, 0x10);
	copy(prx + 0xD0, g_header + 0x00, 0x80);
	clear(prx + 0x34, 0x28);

	copy(g_decryptHash + 0x14, prx + 0x5C, 0x60);
	if (kirk7(g_decryptHash, 0x60, DECRYPT_KEY_CODE) < 0)
		goto restore;
	copy(prx + 0x5C, g_decryptHash, 0x60);

	copy(g_decryptHash, prx + 0x6C, 0x14);
	copy(prx + 0x70, prx + 0x5C, 0x10);
	clear(prx + 0x18, 0x58);
	g_kirkBuffer = (u32 *)prx;
	copy(prx + 4, prx, 4);
	*(u32 *)prx = 0x14C;
	copy(prx + 8, g_decryptWork, 0x10);
	clear(g_decryptWork, 0x10);

	/* the SHA-1 of the rearranged header must match the stored one */
	if (sceUtilsBufferCopyByPolling(prx, prx, KIRK_CMD_SHA1) != 0)
		goto restore;
	for (i = 0; i < 0x14; i++) {
		if ((s8)prx[i] != (s8)g_decryptHash[i])
			break;
	}
	if (i != 0x14) {
		ret = -6;
		goto restore;
	}

	for (i = 0; i < 0x40; i++)
		g_decryptHash[0x14 + i] = prx[0x80 + i] ^ g_decryptWork[0x10 + i];
	clear(g_decryptWork + 0x10, 0x40);
	if (kirk7(g_decryptHash, 0x40, DECRYPT_KEY_CODE) < 0)
		goto restore;
	for (i = 0x3F; i >= 0; i--)
		prx[0x40 + i] = g_decryptHash[i] ^ g_decryptWork[0x50 + i];
	clear(g_decryptWork + 0x50, 0x40);

	clear(prx + 0x80, 0x30);
	prx[0xA0] = 1;
	copy(prx + 0xB0, prx + 0xC0, 0x10);
	clear(prx + 0xC0, 0x10);

	/* decrypt the body to prx */
	if (sceUtilsBufferCopyByPolling(prx, prx + 0x40, KIRK_CMD_DECRYPT_PRIVATE) != 0) {
		ret = -7;
		goto restore;
	}
	if (*newSize < PSP_HEADER_SIZE)
		clear(prx + *newSize, PSP_HEADER_SIZE - *newSize);
	ret = 0;
	goto out;

restore:
	copy(prx, g_header, PSP_HEADER_SIZE);
	*newSize = 0;
out:
	clear(key, 16);
	clear(g_decryptWork, 0xA4);
	clear(g_decryptHash, 0xB4);
	return ret;
}

/*
 * Undo the per-console signing of a module header: decrypt 0xD0 bytes of
 * header at 0x80 with KIRK 8 (XOR-masked before and after).
 */
int memlmdUnsign(u8 *prx, SceSize size)
{
	int i, ret;

	if (prx == NULL || size < 0x160 || ((u32)prx & 0x3F) != 0 || !IsRamAddress((u32)prx))
		return -2;

	copy(g_header + 0x14, prx + 0x80, 0xD0);
	for (i = 0; i < 0xD0; i++)
		g_header[0x14 + i] ^= g_unsignXorIn[i & 15];
	ret = kirk8(g_header, 0xD0);
	if (ret < 0)
		return ret;
	for (i = 0; i < 0xD0; i++)
		g_header[i] ^= g_unsignXorOut[i & 15];

	copy(prx + 0x80, g_header + 0x40, 0x90);
	copy(prx + 0x110, g_header, 0x40);
	return 0;
}

/* loadelf's set-mask function: scramble key = hash ^ constant */
int memlmdInitializeScrambleKey(u32 unk, const u8 *hash)
{
	int i;

	if (unk != 0)
		return -2;
	for (i = 0; i < 16; i++) {
		if (hash[i] != 0)
			break;
	}
	if (i == 16)
		return -2;
	for (i = 0; i < 16; i++)
		g_scrambleKey[i] = g_scrambleXor[i] ^ hash[i];
	return 0;
}

int memlmdClearScrambleKey(u32 unk)
{
	int i;

	if (unk != 0)
		return -2;
	for (i = 0; i < 16; i++)
		g_scrambleKey[i] = 0;
	return 0;
}

/* loadelf's sub-type checks on the module tag */
int memlmdCompareSubType(u32 tag)
{
	return (tag & 0xFFFF0000) > 0xF099FFFF;
}

int memlmdCompareLatestSubType(u32 tag)
{
	return (tag & 0xFFFF0000) > 0xF09AFFFF;
}
