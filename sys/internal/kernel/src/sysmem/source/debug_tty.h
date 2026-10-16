#ifndef SYSMEM_DEBUG_TTY_H
#define SYSMEM_DEBUG_TTY_H

#include <stdint.h>

/* the TTY ring: the next write position, then the characters */
struct DebugTty {
	volatile uint32_t index;
	char ring[4096];
};

extern struct DebugTty s_debugTty;

#endif
