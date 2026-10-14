/*
 * Readers of the MAC event engine state (the antenna and path selection
 * state). The filename is inferred.
 */
#include <stddef.h>
#include "robin/event.h"
#include "robin/rom.h"
#include "robin/runtime.h"

struct MacEventState macEventState __attribute__((section(".dtcm.bss")));
uint8_t macEventProfileActive __attribute__((section(".dtcm.rw")));

/* State of the secondary path, or 3 while no profile is active. */
int macGetEventSecondaryPathState(void)
{
	if (macEventProfileActive != 0)
		return macEventState.pathStates >> 4;
	return MAC_EVENT_PATH_STATE_UNKNOWN;
}

/* State of the primary path, or 3 while no profile is active. */
int macGetEventPrimaryPathState(void)
{
	if (macEventProfileActive != 0)
		return macEventState.pathStates & 0xf;
	return MAC_EVENT_PATH_STATE_UNKNOWN;
}

/* The engine's level while it runs with flag bit 3 set and both paths unknown, else 0x51. */
int macGetEventLevel(void)
{
	if (macEventProfileActive != 0 && macEventState.state == MAC_EVENT_STATE_RUNNING &&
	    (macEventState.flags & 8) != 0 &&
	    macGetEventPrimaryPathState() == MAC_EVENT_PATH_STATE_UNKNOWN &&
	    macGetEventSecondaryPathState() == MAC_EVENT_PATH_STATE_UNKNOWN)
		return macEventState.level;
	return MAC_EVENT_LEVEL_DEFAULT;
}

unsigned int macGetEventPayloadMode(void)
{
	return (macEventState.payload >> 4) & 3;
}

int macGetEventPolicy(struct MacEventPolicy *policy)
{
	if (policy == NULL || macEventProfileActive == 0 ||
	    macEventState.state != MAC_EVENT_STATE_RUNNING)
		return 0;
	policy->mode = macEventState.mode;
	policy->flags = macEventState.flags;
	policy->pathStates = macEventState.policyPathStates;
	return 1;
}

int macGetEventPathConfiguration(uint32_t path, struct MacEventPathConfiguration *configuration)
{
	if (configuration == NULL || macEventProfileActive == 0 ||
	    macEventState.state != MAC_EVENT_STATE_RUNNING || path != 0)
		return 0;
	__aeabi_memcpy(configuration, &macEventState.configuration, sizeof(*configuration));
	return 1;
}
