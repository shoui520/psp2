#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel_boot_loader/kbl_param.h>

struct SysrootState;

/* the boot loader's context; Sysmem reads only these fields */
struct SysmemBootContext {
	uint8_t reserved0[0x2c];
	SceKblParam *kblParam;
	uint8_t reserved1[0xd0];
	struct SysrootState *sysroot;
	uint8_t reserved2[0x1f8];
	uintptr_t debugPutchar;			/* 0x2fc: the boot loader's putchar */
	uint8_t reserved3[0x8];
	uint32_t debugPrintLevel;
};

_Static_assert(offsetof(struct SysmemBootContext, kblParam) == 0x2c,
	       "Sysmem boot KBL pointer offset");
_Static_assert(offsetof(struct SysmemBootContext, sysroot) == 0x100,
	       "Sysmem boot Sysroot pointer offset");
_Static_assert(offsetof(struct SysmemBootContext, debugPutchar) == 0x2fc,
	       "Sysmem boot putchar offset");
_Static_assert(offsetof(struct SysmemBootContext, debugPrintLevel) == 0x308,
	       "Sysmem boot debug print level offset");
