/* Routing table that binds device events to event sources. */
#ifndef ROBIN_EVENTROUTE_H
#define ROBIN_EVENTROUTE_H

#include <stdint.h>

#define SYS_EVENT_ROUTE_COUNT	4
#define SYS_EVENT_ROUTE_NONE	0xff

struct SysEventRoute {
	uint8_t device;		/* device event number, 0xff when the slot is free */
	uint8_t source;		/* event source it is routed to */
	uint8_t channel;
	uint8_t reserved : 1;
	uint8_t bound : 1;	/* the channel was set up */
	int8_t masked : 1;	/* -1 while the device event is masked (sysEventRouteState() reads it signed) */
	uint8_t gpio : 5;	/* GPIO pin of channel 8 */
};

_Static_assert(sizeof(struct SysEventRoute) == 4, "event route");

extern struct SysEventRoute sysEventRoutes[SYS_EVENT_ROUTE_COUNT];

unsigned int sysEventRouteDevice(unsigned int index);
unsigned int sysEventRouteFind(unsigned int device);
unsigned int sysEventRouteSource(unsigned int index);
unsigned int sysEventRouteSourceOf(unsigned int device);
unsigned int sysEventRouteChannelOf(unsigned int device);
unsigned int sysGetEventSourceMapping(unsigned int source);
int sysEventRouteSetMasked(int device, int enable);
int sysEventRouteState(int device);

#endif /* ROBIN_EVENTROUTE_H */
