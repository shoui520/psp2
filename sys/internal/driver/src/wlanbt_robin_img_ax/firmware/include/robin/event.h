/* Complete three-byte policy returned by the MAC event-policy query. */
#ifndef ROBIN_EVENT_H
#define ROBIN_EVENT_H

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
 * State of the MAC event engine. Only the fields the readers use are named;
 * the state is 13 once the engine runs.
 */
struct MacEventState {
	uint8_t state;
	uint8_t reserved0[10];
	uint8_t level;			/* reported while both paths are in state 3 */
	uint8_t reserved0b;
	uint8_t mode;
	uint8_t reserved1[3];
	uint8_t payload;		/* bit 0: pattern pending, bits 4-5: payload mode */
	uint8_t pathStates;		/* primary path in the low nibble, secondary in the high */
	uint8_t reserved2;
	uint8_t flags;
	uint8_t policyPathStates;
	struct MacEventPathConfiguration configuration;
};

_Static_assert(sizeof(struct MacEventState) == 31, "event engine state");

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
