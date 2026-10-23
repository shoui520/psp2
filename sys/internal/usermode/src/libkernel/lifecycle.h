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

extern uint32_t processSdkVersion;
extern SceLongTimerRegisters *processTimer;	/* the LT5, read-only */
extern union ProcessTimeBase processTimeBase;
extern unsigned int backtraceAvailable;
extern uint32_t pmUserEnable;
extern uint32_t exitStarted;

#endif
