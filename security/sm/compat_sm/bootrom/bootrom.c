/*
 * bootrom.c: Stage 2 C implementation (0x80010000..0x80010E34) of the 4,096-byte
 * ePSP (Allegrex) BootROM payload embedded in os0:sm/compat_sm.self.
 *
 * Function, structure, and variable names (except sceSysregSync) are ours.
 */

#include "bootrom.h"

/* ============================================================================
 * Pinned Data Section (0x80010D80..0x80010E34, LMA 0x0F00..0x0FB4)
 * ============================================================================ */

/* 0x80010d80: 64-byte scratch buffer for initial SRAM challenge */
u32 g_challengeBuf[16] __attribute__((section(".data.pinned"))) = { 0 };

/* 0x80010dc0: 64-byte expected response transferred to 0xBFC00FC0 during handshake */
u8 g_expectedResponse[64] __attribute__((section(".data.pinned"))) = {
	0x59u, 0xadu, 0x29u, 0xd3u, 0xe6u, 0x62u, 0x79u, 0xf1u,
	0xafu, 0x53u, 0x2cu, 0x62u, 0x79u, 0x92u, 0xdeu, 0xcbu,
	0x56u, 0xa8u, 0xb9u, 0x9cu, 0x68u, 0xa5u, 0x09u, 0x58u,
	0x18u, 0xf3u, 0x52u, 0xdcu, 0x9bu, 0xc7u, 0xfbu, 0x8fu,
	0x3du, 0x43u, 0x70u, 0x7du, 0x2fu, 0xbbu, 0x72u, 0x3cu,
	0x12u, 0x36u, 0x0cu, 0x8eu, 0x81u, 0xbeu, 0x03u, 0x1eu,
	0x01u, 0x2fu, 0x20u, 0xd2u, 0x68u, 0xdau, 0x7cu, 0xcdu,
	0x20u, 0x21u, 0xd5u, 0x56u, 0x0fu, 0xe6u, 0x27u, 0x02u
};

/* 0x80010e00: 32-byte scramble key seed placed at 0xBFC00200 */
u8 g_scrambleSeed[32] __attribute__((section(".data.pinned"))) = {
#if FW_VERSION == 0x03630011
	0xb2u, 0xb6u, 0x63u, 0x50u, 0x08u, 0x3au, 0x61u, 0xb4u,
	0x85u, 0x23u, 0x09u, 0xf5u, 0x1bu, 0x74u, 0x0cu, 0x29u,
#else
	0x78u, 0xaau, 0x2du, 0xdbu, 0x9cu, 0x9cu, 0x99u, 0x2eu,
	0xf4u, 0x9eu, 0x0eu, 0x96u, 0x49u, 0x8eu, 0x3eu, 0x7eu,
#endif
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
};

/* 0x80010e20: SHA-1 initial digest state */
u32 g_sha1InitState[5] __attribute__((section(".data.pinned"))) = {
	0x67452301u,
	0xefcdab89u,
	0x98badcfeu,
	0x10325476u,
	0xc3d2e1f0u
};

/* ============================================================================
 * Stage 2 Code (0x80010000..0x80010D18)
 * ============================================================================ */

/* 0x80010000 (name ours) */
void __attribute__((noreturn, section(".text.entry"))) bootRomMain(void)
{
	u32 bootParam = HW(BOOTROM_PARAM_ADDR);
	int i;

	if (bootParam != 0u) {
		HW(BOOTROM_PARAM_ADDR) = 0u;
		resumeMain(bootParam);
		goto clear_and_halt;
	}

	HW(SYSREG_BUS_CLOCK) |= 0x80u;
	sync();
	delayLoop(1u);
	HW(SYSREG_BUS_CLOCK) ^= 0x80u;
	HW(SYSREG_RESET_ENABLE) = (HW(SYSREG_RESET_ENABLE) | 0x400u) ^ 0x400u;
	HW(SYSREG_BUS_CLOCK) |= 0x80u;
	sync();

	memcpyWords(g_challengeBuf, (const u32 *)BOOTROM_CHALLENGE_ADDR, 64);
	memcpyWords((u32 *)BOOTROM_SRAM_BASE, (const u32 *)IPL_UNCACHED_BASE,
			BOOTROM_SRAM_SIZE);

	for (i = 0; i < 16; i++) {
		u32 w = ((volatile u32 *)BOOTROM_SRAM_BASE)[i];
		u32 c = g_challengeBuf[i];

		g_challengeBuf[i] = 0u;
		((volatile u32 *)BOOTROM_SRAM_BASE)[i] = w ^ c;
	}

	if (kirkDecryptCmd1((void *)BOOTROM_SRAM_BASE,
			(const void *)BOOTROM_SRAM_BASE) == 0) {
		memcpyWords((u32 *)IPL_UNCACHED_BODY,
				(const u32 *)BOOTROM_SRAM_BASE, 3936);
		memsetBytes((void *)BOOTROM_SRAM_BASE, 0, BOOTROM_SRAM_SIZE);
		if (kirkDecryptCmd1((void *)IPL_UNCACHED_BASE,
				(const void *)IPL_UNCACHED_BODY) == 0) {
			dcacheWritebackInvalidateAll();
			icacheInvalidateAll();
			sceSysregSync(1u);
			((void (*)(void))IPL_CACHED_ENTRY)();
		}
	}

clear_and_halt:
	for (;;)
		memsetBytes((void *)BOOTROM_SRAM_BASE, 0, BOOTROM_SRAM_SIZE);
}

/* 0x80010160 (name ours) */
void icacheInvalidateAll(void)
{
	u32 size = 4096u << ((cp0Config() >> 9) & 7u);
	u32 addr;

	__asm__ __volatile__(
		"mtc0 $zero, $28\n\t"
		"mtc0 $zero, $29"
	);
	for (addr = 0u; addr != size; addr += 64u) {
		__asm__ __volatile__(
			"cache 0x01, 0(%0)\n\t"
			"cache 0x03, 0(%0)"
			:: "r"(addr)
		);
	}
}

/* 0x80010198 (name ours) */
void dcacheWritebackInvalidateAll(void)
{
	u32 size = 2048u << ((cp0Config() >> 6) & 7u);
	u32 addr;

	for (addr = 0u; addr != size; addr += 64u) {
		__asm__ __volatile__(
			"cache 0x14, 0(%0)\n\t"
			"cache 0x14, 0(%0)"
			:: "r"(addr)
		);
	}
	sync();
}

/* 0x800101c8 (name ours) */
int kirkDecryptCmd1(void *dst, const void *src)
{
	const u32 *hdr = (const u32 *)src;
	u32 status;

	if ((hdr[0x64 / 4] & 1u) == 0u || hdr[0x70 / 4] < 256u)
		return -1;

	KIRK_CMD = KIRK_CMD_DECRYPT_PRIVATE;
	KIRK_SRC = PHYS(src);
	KIRK_DST = PHYS(dst);
	KIRK_PHASE = 1u;

	do {
		status = KIRK_STATUS & 0x11u;
	} while (status == 0u);

	if ((status & 0x10u) == 0u) {
		KIRK_STATUS_ACK = status;
		return (int)KIRK_RESULT;
	}

	KIRK_PHASE = 2u;
	do {
		status = KIRK_STATUS & 0x02u;
	} while (status == 0u);

	KIRK_STATUS_ACK = status;
	sync();
	return -1;
}

/* 0x80010250 (name ours) */
u32 memcpyWords(u32 *dst, const u32 *src, int size)
{
	u32 sum = 0u;

	do {
		u32 v = *src++;

		size -= 4;
		*dst++ = v;
		sum += v;
	} while (size > 0);
	return sum;
}

/* 0x80010278 (name ours) */
void *memsetBytes(void *dst, int val, int size)
{
	u8 *p = (u8 *)dst;

	do {
		size--;
		*p++ = (u8)val;
	} while (size > 0);
	return dst;
}

/* 0x80010294 (name ours) */
void delayLoop(u32 count)
{
	u32 loops = (count * 3u) << 5;

	__asm__ __volatile__(
		"1:\n\t"
		"bnez %0, 1b\n\t"
		"addiu %0, %0, -1"
		: "+r"(loops)
	);
}

/* 0x800102b0 (name ours) */
void bootRomPanicHook(void)
{
}

/* 0x800102b8 (name ours) */
void bootRomDefaultCallback(void)
{
}

/* 0x800102c0 (name ours) */
void __attribute__((noreturn)) resumeDispatch(u32 bootParam)
{
	SceCompatResumeBootInfo bootInfo;
	u8 seed[8];
	u32 digest[5];

	__asm__ __volatile__(
		"ctc0 $zero, $0\n\t"
		"ctc0 $zero, $1\n\t"
		"ctc0 $zero, $2\n\t"
		"ctc0 $zero, $3\n\t"
		"ctc0 $zero, $4\n\t"
		"ctc0 $zero, $5\n\t"
		"ctc0 $zero, $6\n\t"
		"ctc0 $zero, $7\n\t"
		"ctc0 $zero, $8\n\t"
		"ctc0 $zero, $9\n\t"
		"ctc0 $zero, $10\n\t"
		"ctc0 $zero, $11\n\t"
		"ctc0 $zero, $12\n\t"
		"ctc0 $zero, $13\n\t"
		"ctc0 $zero, $14\n\t"
		"ctc0 $zero, $15\n\t"
		"ctc0 $zero, $16\n\t"
		"cfc0 $zero, $17\n\t"
		"ctc0 $zero, $18\n\t"
		"ctc0 $zero, $19\n\t"
		"ctc0 $zero, $20\n\t"
		"ctc0 $zero, $21\n\t"
		"ctc0 $zero, $22\n\t"
		"ctc0 $zero, $23\n\t"
		"ctc0 $zero, $24\n\t"
		"ctc0 $zero, $25\n\t"
		"ctc0 $zero, $26\n\t"
		"ctc0 $zero, $27\n\t"
		"ctc0 $zero, $28\n\t"
		"ctc0 $zero, $29\n\t"
		"ctc0 $zero, $30\n\t"
		"ctc0 $zero, $31"
		::: "memory"
	);

	memsetBytes(&bootInfo, 0, sizeof(bootInfo));
	bootInfo.size = sizeof(bootInfo);
	bootInfo.callback = bootRomDefaultCallback;
	bootInfo.modProtId = -1;
	bootInfo.moduleApiDefaultMode = 2u;
	bootInfo.maxModules = 1200u;
	bootInfo.memBase = 0u;
	bootInfo.bootFlags = 0u;
	bootInfo.numProtects = 0u;
	bootInfo.memSize = 0u;
	bootInfo.reserved18 = 0u;
	bootInfo.buildVersion = 0u;

	seed[0] = 0x12u;
	seed[1] = 0x34u;
	seed[2] = 0x56u;
	seed[3] = 0x78u;
	seed[4] = 0x9au;
	seed[5] = 0x00u;
	seed[6] = 0xdeu;
	seed[7] = 0xf0u;
	sha1Digest(seed, sizeof(seed), digest);

	digest[0] ^= HW(SYSREG_FUSE_ID_LOW);
	digest[1] ^= HW(SYSREG_FUSE_ID_HIGH);
	sha1Digest(digest, sizeof(digest), digest);

	bootParam ^= digest[0] ^ digest[1] ^ digest[2] ^ digest[3] ^ digest[4];

	/* Target must lie within 0x88000000..0x89FFFFFF */
	if ((bootParam + 0x78000000u) > 0x01ffffffu) {
		bootRomPanicHook();
		for (;;)
			;
	}

	if ((bootParam & 3u) == 0u) {
		((void (*)(SceCompatResumeBootInfo *))bootParam)(&bootInfo);
	} else if ((bootParam & 3u) == 1u) {
		const u32 *hdr = (const u32 *)(bootParam & ~3u);

		if (hdr[0] >= 128u) {
			bootRomPanicHook();
			for (;;)
				;
		}
		if (hdr[1] != 0x06060110u) {
			bootRomPanicHook();
			for (;;)
				;
		}
		((void (*)(SceCompatResumeBootInfo *))hdr[0x30 / 4])(&bootInfo);
	}

	for (;;)
		;
}

/* 0x800104fc (name ours) */
int resumeMain(u32 bootParam)
{
	initDdrAndHandshake(0u, 0u, 0u, 0u, 0u, 0u, 0u);
	resumeDispatch(bootParam);
	return 0;
}

/* 0x80010548 (name ours) */
void *transferBootKey(void)
{
	const u32 *src = (const u32 *)g_expectedResponse;
	u32 *dst = (u32 *)BOOTROM_CHALLENGE_ADDR;
	const u32 *end = src + 16;

	do {
		u32 v0 = src[0];
		u32 v1 = src[1];
		u32 v2 = src[2];
		u32 v3 = src[3];

		dst[0] = v0;
		dst[1] = v1;
		dst[2] = v2;
		dst[3] = v3;
		src += 4;
		dst += 4;
	} while (src != end);

	memsetBytes(g_expectedResponse, 0, sizeof(g_expectedResponse));

	HW(KERMIT_HANDSHAKE_REG) = 1u;
	while (HW(KERMIT_HANDSHAKE_REG) != 2u)
		;
	HW(KERMIT_HANDSHAKE_REG) = 3u;
	while (HW(KERMIT_HANDSHAKE_REG) != 0u)
		;

	return memsetBytes((void *)BOOTROM_CHALLENGE_ADDR, 0,
			sizeof(g_expectedResponse));
}

/* 0x80010654 (name ours) */
int setupKeySeedAndVerify(void)
{
	u8 *seed = (u8 *)BOOTROM_SCRAMBLE_SEED;
	u32 block[3];

	memsetBytes((void *)BOOTROM_SRAM_BASE, 0, 512);
	memsetBytes((void *)(BOOTROM_SRAM_BASE + 0x220u), 0, 3552);
	transferBootKey();

	__builtin_memcpy(block, g_scrambleSeed, 12);
	__builtin_memcpy(seed, block, 12);
	__builtin_memcpy(block, g_scrambleSeed + 12, 12);
	__builtin_memcpy(seed + 12, block, 12);
	__builtin_memcpy(block, g_scrambleSeed + 24, 8);
	__builtin_memcpy(seed + 24, block, 8);

	if ((HW(SYSREG_MEMORY_CONFIG) & 0xff000000u) != 0xf0000000u) {
		HW(SYSREG_MEMORY_CONFIG) |= 3u;
		sync();
		memsetBytes((void *)BOOTROM_SRAM_BASE, 0, BOOTROM_SRAM_SIZE);
		for (;;)
			;
	}
	return 0;
}

/* 0x8001076c (name ours) */
int initDdrAndHandshake(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6)
{
	(void)a0;
	(void)a1;
	(void)a2;
	(void)a3;
	(void)a4;
	(void)a5;
	(void)a6;

	HW(DDR_FLUSH_PATTERN + 0x00u) = 0xccccccccu;
	HW(DDR_FLUSH_PATTERN + 0x04u) = 0xccccccccu;
	HW(DDR_FLUSH_PATTERN + 0x08u) = 0xccccccccu;
	HW(DDR_FLUSH_PATTERN + 0x0cu) = 0xccccccccu;
	HW(DDR_FLUSH_REQUEST + 0x00u) = 0u;
	HW(DDR_FLUSH_REQUEST + 0x04u) = 0u;
	HW(DDR_FLUSH_REQUEST + 0x08u) = 0u;
	HW(DDR_FLUSH_REQUEST + 0x0cu) = 0u;
	HW(DDR_FLUSH_REQUEST + 0x10u) = 0u;
	HW(DDR_FLUSH_CONTROL) &= ~0x60u;
	HW(SYSREG_MEMORY_CONFIG) |= 2u;
	sync();
	HW(SYSREG_BUS_CLOCK) |= 0x4080u;
	setupKeySeedAndVerify();
	return 0;
}

/* 0x80010838 (name ours) */
void __attribute__((noreturn)) abortClearBootRam(void)
{
	HW(SYSREG_MEMORY_CONFIG) |= 3u;
	sync();
	memsetBytes((void *)BOOTROM_SRAM_BASE, 0, BOOTROM_SRAM_SIZE);
	for (;;)
		;
}

/* 0x80010870 (name ours) */
void sha1Transform(const u32 *block, u32 *state)
{
	u32 W[16];
	int i;

	for (i = 0; i < 16; i++)
		W[i] = block[i];
	sha1Compress(W, state);
}

/* 0x800108e8 (name ours) */
void sha1Compress(u32 *W, u32 *state)
{
	u32 a = state[0];
	u32 b = state[1];
	u32 c = state[2];
	u32 d = state[3];
	u32 e = state[4];
	u32 t;

	for (t = 0u; t < 80u; t++) {
		u32 s = t & 15u;
		u32 w = W[s];
		u32 f;
		u32 k;
		u32 temp;

		if (t >= 16u) {
			w ^= W[(t + 2u) & 15u] ^ W[(t + 8u) & 15u] ^
			     W[(t + 13u) & 15u];
			w = ror32(w, 31u);
			W[s] = w;
		}

		if (t < 20u) {
			f = (b & c) | (~b & d);
			k = 0x5a827999u;
		} else if (t < 40u) {
			f = b ^ c ^ d;
			k = 0x6ed9eba1u;
		} else if (t < 60u) {
			f = (b & c) | (b & d) | (c & d);
			k = 0x8f1bbcdcu;
		} else {
			f = b ^ c ^ d;
			k = 0xca62c1d6u;
		}

		temp = ror32(a, 27u) + f + e + k + w;
		e = d;
		d = c;
		c = ror32(b, 2u);
		b = a;
		a = temp;
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
}

/* 0x80010a40 (name ours) */
int sha1Digest(const void *data, u32 len, void *outDigest)
{
	const u8 *src = (const u8 *)data;
	u32 bitLen = len << 3;
	u32 state[5];
	u32 W[16];
	int i;

	state[0] = g_sha1InitState[0];
	state[1] = g_sha1InitState[1];
	state[2] = g_sha1InitState[2];
	state[3] = g_sha1InitState[3];
	state[4] = g_sha1InitState[4];

	if ((((u32)src & 0x0fu) | (len & 0x3fu)) == 0u) {
		const u32 *s32 = (const u32 *)src;

		while (len != 0u) {
			for (i = 0; i < 16; i++)
				W[i] = wsbw32(*s32++);
			len -= 64u;
			sha1Compress(W, state);
		}
		for (i = 0; i < 16; i++)
			W[i] = 0u;
		W[0] = 0x80000000u;
		W[15] = bitLen;
		sha1Compress(W, state);
	} else {
		while (len >= 64u) {
			for (i = 0; i < 16; i++) {
				u32 v = load32Ua(src);

				src += 4;
				W[i] = wsbw32(v);
			}
			len -= 64u;
			sha1Compress(W, state);
		}

		for (i = 0; i < 16; i++)
			W[i] = 0u;

		if (len >= 56u) {
			u8 *dst8 = (u8 *)W;

			for (i = 0; (u32)i < len; i++)
				dst8[i] = *src++;
			dst8[len] = 0x80u;
			for (i = 0; i < 16; i++)
				W[i] = wsbw32(W[i]);
			sha1Compress(W, state);
			len -= 64u;
			for (i = 0; i < 16; i++)
				W[i] = 0u;
		}

		W[15] = bitLen;
		if ((s32)len > 0) {
			u8 *dst8 = (u8 *)W;

			for (i = 0; (u32)i < len; i++)
				dst8[i] = *src++;
			dst8[len] = 0x80u;
			for (i = 0; i < 14; i++)
				W[i] = wsbw32(W[i]);
		} else {
			W[0] = (len < 1u) ? 0x80000000u : 0u;
		}
		sha1Compress(W, state);
	}

	for (i = 0; i < 5; i++)
		store32Ua((u8 *)outDigest + i * 4, wsbw32(state[i]));
	return 0;
}

/* 0x80010cf0 (Sony name from sys661/ipl) */
int sceSysregSync(u32 mask)
{
	sync();
	mask &= 5u;
	HW(SYSREG_BUS_SYNC) = mask;
	while ((HW(SYSREG_BUS_SYNC) & mask) != 0u)
		;
	return 0;
}
