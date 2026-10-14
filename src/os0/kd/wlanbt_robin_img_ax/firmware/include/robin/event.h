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

enum {
	MAC_EVENT_PRIMARY_FLAGS = 0x41,
	MAC_EVENT_SECONDARY_FLAGS = 0x82,
};

int macGetEventPolicy(struct MacEventPolicy *policy);
int macGetEventPathConfiguration(uint32_t path,
				 struct MacEventPathConfiguration *configuration);
uint32_t macApplyEventRegisterDefaults(volatile void *registerBase);

#endif /* ROBIN_EVENT_H */
