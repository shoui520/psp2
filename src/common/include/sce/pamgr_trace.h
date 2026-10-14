/*
 * The PA trace configuration ScePamgr hands out (os0:kd/pamgr.skprx), read
 * by SceGpuEs4CoreDump for its .pmon dump.
 */

#pragma once

#include <stdint.h>

/* GPU sampling configuration forwarded by AddGpuTraceByKey. */
struct PamgrGpuTraceConfig {
	uint32_t control;
	uint16_t interval0;
	uint16_t interval1;
	uint32_t counterConfig[8];
	uint32_t sampleConfig[8];
};

/* Original five-word primary buffer and trace-record settings. */
struct PamgrPrimaryTraceConfig {
	uint32_t control;
	uint32_t base;
	uint32_t size;
	uint32_t limit;
	uint32_t recordMask;
};

_Static_assert(sizeof(struct PamgrGpuTraceConfig) == 0x48, "PamgrGpuTraceConfig size");
_Static_assert(sizeof(struct PamgrPrimaryTraceConfig) == 0x14, "PamgrPrimaryTraceConfig size");

/* names unknown: copies of the running configuration, 0 when tracing */
int ScePamgrForDriver_02ACD81C(struct PamgrGpuTraceConfig *config);
int ScePamgrForDriver_606C98CD(struct PamgrPrimaryTraceConfig *config);
