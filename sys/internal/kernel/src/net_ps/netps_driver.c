/* Shared SceNetPs driver services; original bodies retain their owners. */

#include "netps_driver.h"
#include <net_ps/netps_thread.h>
#include <net_ps/net_ps_bsd.h>
#include <string.h>
#include "net_ps_internal.h"

/* Name unknown; publish the driver's maximum transmission unit. */
int sceNetPsSetDriverMtu(struct NetDriver *driver, unsigned int mtu)
{
	if (!driver)
		return (int)0x80418101;
	driver->context->interface->if_mtu = mtu;
	return 0;
}

/* Name unknown; advance the driver's connection state and schedule reception. */
int sceNetPsUpdateConnection(struct SceNetPsDriverContext *context,
			    int error)
{
	struct ifnet *interface = context->interface;
	struct ifaddr *address;

	switch (context->connectionState) {
	case 1:
		goto updatePolicy;
	case 2:
	case 3:
		goto schedule;
	case 4:
		TAILQ_FOREACH(address, &interface->if_addrlist, ifa_list) {
			if (address->ifa_addr->sa_family == AF_INET)
				break;
		}
		if (!address ||
		    (interface->if_flags & (IFF_UP | IFF_RUNNING)) !=
		    (IFF_UP | IFF_RUNNING) ||
		    !((struct in_ifaddr *)address)->ia_addr.sin_addr.s_addr) {
			context->flags |= 2;
			goto schedule;
		}
		break;
	default:
		if (context->flags & 2)
			goto schedule;
		context->connectionError = 0;
		return 0;
	}
updatePolicy:
	if (context->autoConnectPolicy != -1) {
		context->autoConnectEnabled = context->autoConnectPolicy > 0;
		context->connectionState = 2;
	}
schedule:
	context->connectionError = error;
	context->pendingEvents |= NET_PS_EVENT_CONNECTION;
	sceNetPsScheduleReceive();
	return 1;
}

/* Name unknown; reset inactivity on request before scheduling transmission. */
int sceNetPsKickTransmit(struct NetDriver *driver, int resetIdle)
{
	struct SceNetPsDriverContext *context = driver->context;

	if (resetIdle)
		memset(&context->idle, 0, sizeof(context->idle));
	return driver->kickTx(driver, 0);
}

/* Name unknown; deliver driver events to the worker and registered callbacks. */
int sceNetPsNotifyDriver(struct NetDriver *driver, SceUInt64 events,
			 const void *data, int size)
{
	struct SceNetPsDriverContext *context = driver ? driver->context : NULL;

	if (driver && (events & 0xffffffff00000000ULL) && context) {
		context->pendingEvents |= events;
		if (data && (events == 0x800000000ULL || events == 0x1000000000ULL) &&
		    size == sizeof(context->lastError))
			context->lastError = *(const int *)data;
		sceNetPsScheduleReceive();
	}
	if ((unsigned int)events)
		sceNetPsNotifyDriverCallbacks(context, events);
	return 0;
}

/* Name unknown; convert one complete eight-counter group to network order. */
void sceNetPsSwapCounterByteOrder(unsigned int counters[8])
{
	counters[0] = __builtin_bswap32(counters[0]);
	counters[1] = __builtin_bswap32(counters[1]);
	counters[2] = __builtin_bswap32(counters[2]);
	counters[3] = __builtin_bswap32(counters[3]);
	counters[4] = __builtin_bswap32(counters[4]);
	counters[5] = __builtin_bswap32(counters[5]);
	counters[6] = __builtin_bswap32(counters[6]);
	counters[7] = __builtin_bswap32(counters[7]);
}
