#ifndef LIBKERNEL_LIFECYCLE_H
#define LIBKERNEL_LIFECYCLE_H

#include <stdint.h>
#include <systimer/timer_regs.h>


union ProcessTimeBase {
	uint64_t clock;
	struct {
		uint32_t low;
		uint32_t high;
	} words;
};

struct LibkernelState {
	uint32_t processSdkVersion;
	SceLongTimerRegisters *timer;	/* the LT5, read-only */
	union ProcessTimeBase processTimeBase;
	unsigned int backtraceAvailable;
	uint32_t pmUserEnable;
	uint32_t exitStarted;
};

extern struct LibkernelState libkernelState;

#endif
