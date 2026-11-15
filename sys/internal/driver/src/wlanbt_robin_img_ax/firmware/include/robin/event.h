/* Complete three-byte policy returned by the MAC event-policy query. */
#ifndef ROBIN_EVENT_H
#define ROBIN_EVENT_H

#include <stddef.h>
#include <stdint.h>

struct MacEventPolicy {
	uint8_t mode;
	uint8_t flags;
	uint8_t pathStates;
};

typedef char MacEventPolicySize[(sizeof(struct MacEventPolicy) == 3) ? 1 : -1];

/* Nine-byte MAC configuration copied by the event-path query. */
struct MacEventPathConfiguration {
	uint8_t controlPattern;
	uint8_t controlTogglePattern;
	uint8_t secondaryPattern;
	uint8_t primaryPattern;
	uint8_t primaryTogglePattern;
	uint8_t secondaryTogglePattern;
	uint8_t primaryControlPattern;
	uint8_t secondaryControlPattern;
	uint8_t toggleMask;
};

typedef char MacEventPathConfigurationSize[
	(sizeof(struct MacEventPathConfiguration) == 9) ? 1 : -1];

/*
 * State of the MAC event engine: the calibration data's main annex (type 12,
 * or 13 once the engine runs), 64 bytes loaded by calDataHeaderLoad()
 * (sqram/caltable.c). Only the fields the readers use are named.
 */
struct MacEventState {
	uint8_t state;			/* the annex type */
	uint8_t reserved1;
	uint16_t length;
	uint32_t next;			/* image offset of the next annex */
	uint8_t reserved8[3];
	uint8_t level;			/* reported while both paths are in state 3 */
	uint8_t pathFlags;		/* bit 0: path 0's calibration flag, bit 4: path 1's (calDataFlagsGet()) */
	uint8_t mode;
	uint8_t reserved14[2];
	uint8_t regionCode;		/* the region code GET_HW_SPEC reports */
	uint8_t payload;		/* bit 0: pattern pending, bits 4-5: payload mode, bit 6: calibration flag */
	uint8_t pathStates;		/* primary path in the low nibble, secondary in the high */
	uint8_t reserved2;
	uint8_t flags;
	uint8_t policyPathStates;
	struct MacEventPathConfiguration configuration;
	uint8_t reserved31[24];
	uint8_t regionVersion;		/* bits 2-5: calRegionMinor, bits 6-7: calRegionMajor */
	uint8_t reserved56[8];
};

_Static_assert(offsetof(struct MacEventState, configuration) == 22, "event engine state");
_Static_assert(sizeof(struct MacEventState) == 64, "event engine state");

#define MAC_EVENT_STATE_LOADED		12
#define MAC_EVENT_STATE_RUNNING		13
#define MAC_EVENT_PATH_STATE_UNKNOWN	3
#define MAC_EVENT_LEVEL_DEFAULT		0x51

enum {
	MAC_EVENT_PRIMARY_FLAGS = 0x41,
	MAC_EVENT_SECONDARY_FLAGS = 0x82,
};

extern struct MacEventState macEventState;
extern uint8_t macEventProfileActive;

int macGetEventPrimaryPathState(void);
int macGetEventSecondaryPathState(void);
unsigned int macGetEventPayloadMode(void);
int macGetEventLevel(void);
int macGetEventPolicy(struct MacEventPolicy *policy);
int macGetEventPathConfiguration(uint32_t path,
				 struct MacEventPathConfiguration *configuration);
uint32_t macApplyEventRegisterDefaults(volatile void *registerBase);

#endif /* ROBIN_EVENT_H */
