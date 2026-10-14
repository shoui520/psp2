/* SceKernelThreadMgr's complete VFP register save area. */
#pragma once

#include <stddef.h>
#include <stdint.h>

struct ThreadmgrVfpContext {
	uint32_t s[64];				/* d0-d31 */
	uint32_t fpscr;
	uint32_t fpexc;
	uint8_t reserved[0x18];
};

_Static_assert(offsetof(struct ThreadmgrVfpContext, fpscr) == 0x100 &&
	       offsetof(struct ThreadmgrVfpContext, fpexc) == 0x104,
	       "thread VFP register layout");
_Static_assert(sizeof(struct ThreadmgrVfpContext) == 0x120, "thread VFP context size");
