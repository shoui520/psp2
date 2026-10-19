/*
 * The SGX543MP4 register block (SceSGX543Reg). Its registers are IMG's
 * (EUR_CR_* in the vendored DDK's hwdefs/sgx543_v1.164defs.h and
 * sgxmpdefs.h); each core's bank follows the master bank at
 * (core + 2) << 14. What follows is what the DDK headers lack: the block's
 * address and Sony's per-core counter bus, read by writing a selector pair
 * and reading the value (SceGpuEs4's register dump, SceGpuCoreDump).
 */
#pragma once

/* SceSonyRegbus (compat/grab_regs.h), the GPU control region the SGX block
 * sits in */
#include <compat/grab_regs.h>

#define SONY_REGBUS_SIZE		0x1000000

#define SGX_REG_BASE			0xe8400000
#define SGX_REG_SIZE			0x20000

/* a core's bank (the DDK's SGX MP bank selection, services4/include/sgx_mkif_km.h) */
#define SGX_REG_BANK_SHIFT		14
#define SGX_REG_BANK_SIZE		(1U << SGX_REG_BANK_SHIFT)
#define SGX_REG_BANK_BASE_INDEX		2
#define SGX_MP_CORE_SELECT(x, i)	((x) + (((i) + SGX_REG_BANK_BASE_INDEX) * SGX_REG_BANK_SIZE))

/* per-core bank offsets */
#define SCE_SGX_COUNTER_VALUE		0x70
#define SCE_SGX_COUNTER_ENABLE		0x78
#define SCE_SGX_COUNTER_SELECT_FIRST	0x90
#define SCE_SGX_COUNTER_SELECT_SECOND	0x94

/* or'ed with (selector | selector << 16) into the two select words */
#define SCE_SGX_COUNTER_FIRST_SOURCE	0x08000000
#define SCE_SGX_COUNTER_SECOND_SOURCE	0x18001000
