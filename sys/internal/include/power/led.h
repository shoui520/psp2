/* SceLed (ScePower): the LEDs SceLedForDriver_EA24BE03 sets */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

/* LED error codes (lowio's 0x803f01xx facility; names unknown) */
#define SCE_LED_ERROR_INVALID_LED	0x803f0140
#define SCE_LED_ERROR_INVALID_MODE	0x803f0141

/* SceLedForDriver_EA24BE03's modes */
#define SCE_LED_MODE_OFF	0
#define SCE_LED_MODE_ON		1	/* Syscon pattern index */
#define SCE_LED_MODE_BLINK	2
#define SCE_LED_MODE_SCRIPT	3	/* option is a byte script */
#define SCE_LED_MODE_LEVEL	4	/* Syscon level, LED 2 only */

/* SceLedForDriver_EA24BE03's option for every mode but SCE_LED_MODE_SCRIPT. */
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


/* vitasdk: ksceLedSetMode */
int SceLedForDriver_EA24BE03(uint32_t led, uint32_t mode, const void *option);
