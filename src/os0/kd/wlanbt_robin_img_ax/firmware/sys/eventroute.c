/*
 * Four-entry table that routes device events to event sources, with the
 * lookups in each direction. The filename is inferred.
 */
#include "robin/eventroute.h"

struct SysEventRoute sysEventRoutes[SYS_EVENT_ROUTE_COUNT] __attribute__((section(".itcm.rw"))) = {
	{ 0xff, 0xff, 0xff, 0 },
	{ 0xff, 0xff, 0xff, 0 },
	{ 0xff, 0xff, 0xff, 0 },
	{ 0xff, 0xff, 0xff, 0 },
};

/* Device event of a route, by route number. */
unsigned int sysEventRouteDevice(unsigned int index)
{
	if (index >= SYS_EVENT_ROUTE_COUNT)
		return SYS_EVENT_ROUTE_NONE;
	return sysEventRoutes[index].device;
}

/* Route number of a device event. */
unsigned int sysEventRouteFind(unsigned int device)
{
	unsigned int index;

	if (device == SYS_EVENT_ROUTE_NONE)
		return SYS_EVENT_ROUTE_NONE;
	for (index = 0; index < SYS_EVENT_ROUTE_COUNT; ++index) {
		if (sysEventRoutes[index].device == device)
			return (uint8_t)index;
	}
	return SYS_EVENT_ROUTE_NONE;
}

/* Event source of a route, by route number. */
unsigned int sysEventRouteSource(unsigned int index)
{
	if (index >= SYS_EVENT_ROUTE_COUNT)
		return SYS_EVENT_ROUTE_NONE;
	return sysEventRoutes[index].source;
}

/* Event source a device event is routed to. */
unsigned int sysEventRouteSourceOf(unsigned int device)
{
	unsigned int index;

	if (device == SYS_EVENT_ROUTE_NONE)
		return SYS_EVENT_ROUTE_NONE;
	for (index = 0; index < SYS_EVENT_ROUTE_COUNT; ++index) {
		if (sysEventRoutes[index].device == device)
			return sysEventRoutes[index].source;
	}
	return SYS_EVENT_ROUTE_NONE;
}

unsigned int sysEventRouteChannelOf(unsigned int device)
{
	unsigned int index;

	if (device == SYS_EVENT_ROUTE_NONE)
		return SYS_EVENT_ROUTE_NONE;
	for (index = 0; index < SYS_EVENT_ROUTE_COUNT; ++index) {
		if (sysEventRoutes[index].device == device)
			return sysEventRoutes[index].channel;
	}
	return SYS_EVENT_ROUTE_NONE;
}

/* Device event routed from an event source. */
unsigned int sysGetEventSourceMapping(unsigned int source)
{
	unsigned int index;

	if (source == SYS_EVENT_ROUTE_NONE)
		return SYS_EVENT_ROUTE_NONE;
	for (index = 0; index < SYS_EVENT_ROUTE_COUNT; ++index) {
		if (sysEventRoutes[index].source == source)
			return sysEventRoutes[index].device;
	}
	return SYS_EVENT_ROUTE_NONE;
}

/* Masks or, with enable set, unmasks every route of a device event. */
int sysEventRouteSetMasked(int device, int enable)
{
	unsigned int index;

	for (index = 0; index < SYS_EVENT_ROUTE_COUNT; ++index) {
		struct SysEventRoute *route = &sysEventRoutes[index];

		if (route->device != device)
			continue;
		if (enable)
			route->flags &= ~SYS_EVENT_ROUTE_MASKED;
		else
			route->flags |= SYS_EVENT_ROUTE_MASKED;
	}
	return enable;
}

/* 0 when the device event has no route, 1 when it is routed, 2 when masked. */
int sysEventRouteState(int device)
{
	unsigned int index;

	for (index = 0; index < SYS_EVENT_ROUTE_COUNT; ++index) {
		if (sysEventRoutes[index].device == device)
			return ((sysEventRoutes[index].flags & SYS_EVENT_ROUTE_MASKED) != 0) + 1;
	}
	return 0;
}
