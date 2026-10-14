/* Routing table that binds device events to event sources. */
#ifndef ROBIN_EVENTROUTE_H
#define ROBIN_EVENTROUTE_H

#include <stdint.h>

#define SYS_EVENT_ROUTE_COUNT	4
#define SYS_EVENT_ROUTE_NONE	0xff

/* Bits of SysEventRoute.flags; the bits above hold a value set with the route. */
#define SYS_EVENT_ROUTE_BOUND	0x02
#define SYS_EVENT_ROUTE_MASKED	0x04

struct SysEventRoute {
	uint8_t device;		/* device event number, 0xff when the slot is free */
	uint8_t source;		/* event source it is routed to */
	uint8_t channel;
	uint8_t flags;
};

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
