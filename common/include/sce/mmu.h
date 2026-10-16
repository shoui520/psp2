/* SceSysmem translation context saved and restored by MMUContextSwitch. */
#pragma once

#include <stdint.h>

typedef struct MMUContext {
	uint32_t ttbr1;
	uint32_t dacr;
	uint32_t contextId;
} MMUContext;
