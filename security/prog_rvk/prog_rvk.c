/*
 * prog_rvk.c: PS Vita Program Revocation List (slb2:prog_rvk.srvk) for
 * 4.00-DEBUGGER, loaded by secure_kernel into Cmep SRAM at 0x809000..0x809380
 * and consulted by secure_kernel (srvk.c) and kprx_auth_sm when authenticating
 * Secure Modules and kernel PRX modules.
 *
 * Structure and field names are ours, matching secure_kernel/srvk.c.
 */

#ifndef _Static_assert
#define SRVK_JOIN_INNER(a, b) a##b
#define SRVK_JOIN(a, b) SRVK_JOIN_INNER(a, b)
#define _Static_assert(condition, message) \
	typedef char SRVK_JOIN(srvk_static_assert_, __COUNTER__)[(condition) ? 1 : -1]
#endif

#include <stdint.h>

/* Comparison operators used in SceSrvkVersionEntry::cmpOp (names ours) */
#define SCE_SRVK_CMP_EQ		0u	/* revoke if version == versionRef */
#define SCE_SRVK_CMP_NE		1u	/* revoke if version != versionRef */
#define SCE_SRVK_CMP_LT		2u	/* revoke if version <  versionRef */
#define SCE_SRVK_CMP_LE		3u	/* revoke if version <= versionRef */
#define SCE_SRVK_CMP_GT		4u	/* revoke if version >  versionRef */
#define SCE_SRVK_CMP_GE		5u	/* revoke if version >= versionRef */

/* Entry types in prog_rvk.srvk (names ours) */
#define SCE_SRVK_ENTRY_VERSION_REVOKE	1u
#define SCE_SRVK_ENTRY_DIGEST_REVOKE	2u
#define SCE_SRVK_ENTRY_VERSION_ALLOW	3u

typedef struct SceSrvkHeader {
	uint32_t magic;				/* 0x00: 5 */
	uint32_t version;			/* 0x04: 1 */
	uint16_t reserved08;			/* 0x08 */
	uint32_t socVersion;			/* 0x0a: unaligned 32-bit SoC version */
	uint16_t reserved0e;			/* 0x0e */
	uint32_t entryCount;			/* 0x10: 27 */
	uint32_t reserved14[3];			/* 0x14..0x1f */
} __attribute__((packed)) SceSrvkHeader;

_Static_assert(sizeof(SceSrvkHeader) == 0x20, "SceSrvkHeader size");

typedef struct SceSrvkVersionEntry {
	uint16_t type;				/* 0x00: SCE_SRVK_ENTRY_* */
	uint16_t reserved02;			/* 0x02 */
	uint64_t targetAuthId;			/* 0x04 */
	uint64_t authIdMask;			/* 0x0c */
	uint64_t versionRef;			/* 0x14 */
	uint16_t cmpOp;				/* 0x1c: SCE_SRVK_CMP_* */
	uint16_t reserved1e;			/* 0x1e */
} __attribute__((packed)) SceSrvkVersionEntry;

_Static_assert(sizeof(SceSrvkVersionEntry) == 0x20, "SceSrvkVersionEntry size");

#define SRVK_SYS_VERSION	((uint64_t)(SRVK_FW_VERSION) << 16)

/*
 * Certified File Section 0 (type=1, index=1, 0x20 bytes):
 * Loaded at 0x809000 by secure_kernel::srvkAuthenticateAndLoad.
 */
const SceSrvkHeader g_progRvkHeader __attribute__((section(".srvk_hdr"), used)) = {
	.magic      = 5u,
	.version    = 1u,
	.reserved08 = 0u,
	.socVersion = SRVK_FW_VERSION,
	.reserved0e = 0u,
	.entryCount = 27u,
	.reserved14 = { 0u, 0u, 0u },
};

/*
 * Certified File Section 1 (type=2, index=2, 27 * 0x20 = 0x360 bytes):
 * Loaded at 0x809020 by secure_kernel::srvkAuthenticateAndLoad.
 */
const SceSrvkVersionEntry g_progRvkEntries[27] __attribute__((section(".srvk_body"), used)) = {
	/* [ 0] +0x000: 0x2000-series authId (== 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2000000000000000ULL, 0xfff0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_EQ, 0u },
	/* [ 1] +0x020: 0x2000-series authId (!= 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2000000000000000ULL, 0xfff0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_NE, 0u },
	/* [ 2] +0x040: 0x2300-series authId (== 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2300000000000000ULL, 0xfff0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_EQ, 0u },
	/* [ 3] +0x060: 0x2300-series authId (!= 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2300000000000000ULL, 0xfff0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_NE, 0u },
	/* [ 4] +0x080: 0x2400..0x2700-series authId (== 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2400000000000000ULL, 0xfcf0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_EQ, 0u },
	/* [ 5] +0x0a0: 0x2400..0x2700-series authId (!= 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2400000000000000ULL, 0xfcf0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_NE, 0u },
	/* [ 6] +0x0c0: 0x2800-series system modules (> 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2800000000000000ULL, 0x2ff7800000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_GT, 0u },
	/* [ 7] +0x0e0: 0x2800-series system modules (< 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2800000000000000ULL, 0x2ff7800000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_LT, 0u },
	/* [ 8] +0x100: 0x2802-series authId (== 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2802000000000000ULL, 0xfff6000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_EQ, 0u },
	/* [ 9] +0x120: 0x2802-series authId (!= 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2802000000000000ULL, 0xfff6000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_NE, 0u },
	/* [10] +0x140: 0x2804-series authId (== 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2804000000000000ULL, 0xfff6000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_EQ, 0u },
	/* [11] +0x160: 0x2804-series authId (!= 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2804000000000000ULL, 0xfff6000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_NE, 0u },
	/* [12] +0x180: 0x2806-series authId (== 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2806000000000000ULL, 0xfff6000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_EQ, 0u },
	/* [13] +0x1a0: 0x2806-series authId (!= 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2806000000000000ULL, 0xfff6000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_NE, 0u },
	/* [14] +0x1c0: 0x2900-series authId (== 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2900000000000000ULL, 0xfff0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_EQ, 0u },
	/* [15] +0x1e0: 0x2900-series authId (!= 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2900000000000000ULL, 0xfff0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_NE, 0u },
	/* [16] +0x200: 0x2a00..0x2b00-series authId (== 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2a00000000000000ULL, 0xfef0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_EQ, 0u },
	/* [17] +0x220: 0x2a00..0x2b00-series authId (!= 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2a00000000000000ULL, 0xfef0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_NE, 0u },
	/* [18] +0x240: 0x2c00..0x2d00-series authId (== 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2c00000000000000ULL, 0xfef0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_EQ, 0u },
	/* [19] +0x260: 0x2c00..0x2d00-series authId (!= 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2c00000000000000ULL, 0xfef0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_NE, 0u },
	/* [20] +0x280: 0x2e00-series authId (!= 4.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2e00000000000000ULL, 0x2ff0000000000000ULL, SRVK_SYS_VERSION, SCE_SRVK_CMP_NE, 0u },
	/* [21] +0x2a0: Specific revoked program 0x210000101cd20007 (>= 0.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x210000101cd20007ULL, 0xffffffffffffffffULL, 0x0000000000000000ULL, SCE_SRVK_CMP_GE, 0u },
	/* [22] +0x2c0: Specific revoked program 0x210000101cd20009 (>= 0.00) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x210000101cd20009ULL, 0xffffffffffffffffULL, 0x0000000000000000ULL, SCE_SRVK_CMP_GE, 0u },
	/* [23] +0x2e0: Specific module 0x2800c0101cd2000b (< 101.22) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2800c0101cd2000bULL, 0xffffffffffffffffULL, 0x0001012200000000ULL, SCE_SRVK_CMP_LT, 0u },
	/* [24] +0x300: Specific module 0x2808000000000101 (< 2.05) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2808000000000101ULL, 0xffffffffffffffffULL, 0x0000020500000000ULL, SCE_SRVK_CMP_LT, 0u },
	/* [25] +0x320: Specific SM 0x2800000000000013 (< 2.05) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2800000000000013ULL, 0xffffffffffffffffULL, 0x0000020500000000ULL, SCE_SRVK_CMP_LT, 0u },
	/* [26] +0x340: Specific module 0x2808000000000100 (< 2.05) */
	{ SCE_SRVK_ENTRY_VERSION_REVOKE, 0u, 0x2808000000000100ULL, 0xffffffffffffffffULL, 0x0000020500000000ULL, SCE_SRVK_CMP_LT, 0u },
};
