#ifndef SCE_LED_H
#define SCE_LED_H

#include <stddef.h>
#include <psp2common/types.h>

/* ksceLedSetMode's option for every mode but SCE_LED_MODE_SCRIPT. */
typedef struct LedModeParam {
	SceSize size;
	union {
		SceUInt32 sysconIndex;		/* SCE_LED_MODE_ON */
		SceUInt32 sysconLevel;		/* SCE_LED_MODE_LEVEL */
		struct {
			SceInt32 onTicks;
			SceInt32 offTicks;
			SceInt32 repeatCount;	/* -1: forever */
			SceInt32 endState;	/* on (nonzero) or off once the repeats end */
		} blink;			/* SCE_LED_MODE_BLINK */
	};
} LedModeParam;

_Static_assert(sizeof(LedModeParam) == 20, "LED mode parameter size");
_Static_assert(offsetof(LedModeParam, size) == 0, "LED parameter size field");
_Static_assert(offsetof(LedModeParam, sysconIndex) == 4, "LED pattern index");
_Static_assert(offsetof(LedModeParam, sysconLevel) == 4, "LED level");
_Static_assert(offsetof(LedModeParam, blink.onTicks) == 4, "LED on ticks");
_Static_assert(offsetof(LedModeParam, blink.offTicks) == 8, "LED off ticks");
_Static_assert(offsetof(LedModeParam, blink.repeatCount) == 12, "LED repeat count");
_Static_assert(offsetof(LedModeParam, blink.endState) == 16, "LED final state");

#endif
