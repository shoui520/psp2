/* fw1 boot block */
#pragma once

#include <vip.h>

/* the crash records (boot.S), one per core */
#define BOOT_CRASH_RECORDS	0x380
#define BOOT_CRASH_RECORD_SIZE	16

#define BOOT_CACHE_TAGS		0x310000	/* instruction cache tag RAM */
#define BOOT_CACHE_LINE		32
