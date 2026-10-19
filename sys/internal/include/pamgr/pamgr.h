/*
 * ScePamgrForDriver (the performance analysis manager): a registration (sceKernelPaRegister's
 * key) owns the trace units, which its holder configures and starts.
 */

#pragma once

#include <stdint.h>
#include <pamgr/pamgr_trace.h>

/* called from the trace interrupt with the trace status (bit 0: a buffer filled) */
typedef void (*PamgrTraceInterruptCallback)(uint32_t status, void *arg);

/* Counter trace configuration, with observed width constraints. */
struct PamgrCounterTraceConfig {
	uint16_t selection;
	uint16_t interval;
	uint32_t values[3];
	uint32_t control;
	uint8_t upperByte;
	uint8_t lowerByte;
	uint16_t flags;
};

/* Two bank controls and two groups of four hardware words. */
struct PamgrSampleConfig {
	uint32_t bank0Control;
	uint32_t bank1Control;
	uint32_t traceControls[4];
	uint32_t buffer1Controls[4];
};

struct PamgrArmTraceConfig {
	uint32_t control;
	uint32_t modeAndContext;
	uint16_t timestampPeriod;	/* counter 1 reload: a timestamp each time it expires */
	uint16_t counter2Period;	/* counter 2 reload */
};

struct PamgrTraceBufferConfig {
	uint32_t base;
	uint32_t size;
	uint16_t eventMask0;
	uint16_t eventMask1;
};

_Static_assert(sizeof(struct PamgrCounterTraceConfig) == 0x18, "PamgrCounterTraceConfig size");
_Static_assert(sizeof(struct PamgrSampleConfig) == 0x28, "PamgrSampleConfig size");
_Static_assert(sizeof(struct PamgrArmTraceConfig) == 0xc, "PamgrArmTraceConfig size");
_Static_assert(sizeof(struct PamgrTraceBufferConfig) == 0xc, "PamgrTraceBufferConfig size");

/* pamgr.c */
int ScePamgrForDriver_6005FDAA(unsigned int mask, uint32_t *selection);
int ScePamgrForDriver_16A24732(unsigned int bank, unsigned int index, uint32_t value);
uint32_t ScePamgrForDriver_80CA3B39(void);
int ScePamgrForDriver_7F3814A4(uint32_t *values);
uint32_t ScePamgrForDriver_019BD60F(unsigned int buffer);
uint64_t ScePamgrForDriver_FE9BFD27(void);	/* a 64-bit timestamp */
unsigned int ScePamgrForDriver_EB70556B(void);
unsigned int ScePamgrForDriver_FB46C3CD(void);
unsigned int ScePamgrForDriver_3ACF4316(void);	/* state; bit 0: tracing */

/* pamgr_wrapper.c */
int ScePamgrForDriver_5D3D46E4(uint32_t registration, const struct PamgrArmTraceConfig *config);
int ScePamgrForDriver_49A43754(uint32_t registration, const struct PamgrGpuTraceConfig *config);
int ScePamgrForDriver_9935D8EF(uint32_t registration);
int ScePamgrForDriver_17C81A92(uint32_t registration,
		const struct PamgrCounterTraceConfig *config);
int ScePamgrForDriver_0B7F6F53(uint32_t registration);
int ScePamgrForDriver_F2D5105A(uint32_t registration,
		const struct PamgrPrimaryTraceConfig *config);
int ScePamgrForDriver_A1E448E7(uint32_t registration, const struct PamgrSampleConfig *config);
int ScePamgrForDriver_E8FF4812(uint32_t registration,
		const struct PamgrTraceBufferConfig *config);
int ScePamgrForDriver_0BCD4966(uint32_t registration, unsigned int mask, uint32_t selection);
int ScePamgrForDriver_95F4079E(uint32_t registration, uint32_t pointer);
int ScePamgrForDriver_B9EE1620(uint32_t registration);
int ScePamgrForDriver_81764B21(uint32_t registration);
int ScePamgrForDriver_DB62FEFE(uint32_t registration);
int ScePamgrForDriver_D9EFF812(uint32_t registration, uint32_t pointer);
int ScePamgrForDriver_3C12A0CD(uint32_t registration, PamgrTraceInterruptCallback callback,
		void *arg);
int ScePamgrForDriver_FB04BBEF(uint32_t registration);
int sceKernelPaRegister(void);
int sceKernelPaUnregister(uint32_t registration);
/* the shared registration of the 64-bit owner id key, < 0: none */
int ScePamgrForDriver_0D04B52A(uint64_t key);
int ScePamgrForDriver_A708D62A(uint32_t registration);
int ScePamgrForDriver_88462719(uint32_t registration);	/* stops its trace, 0: done */
int ScePamgrForDriver_1A7733E0(uint32_t registration);
