/* Shared Sony driver state adapters; unchanged original bodies. */

#include "net_ps_internal.h"
#include <string.h>

extern struct NetDriver *gNetPsDriverHead;

int sceNetPsUpdateConnections(struct SceNetPsDriverContext *context,
			     int receiveArgument)
{
	struct NetDriver *driver;
	int updated = 0;

	if (context)
		return sceNetPsUpdateConnection(context, receiveArgument);
	for (driver = gNetPsDriverHead; driver; driver = driver->next)
		updated += sceNetPsUpdateConnection(driver->context,
						   receiveArgument);
	return updated;
}

int sceNetPsResetIdleTime(struct SceNetPsDriverContext *context)
{
	struct NetDriver *driver;

	if (context) {
		memset(&context->idle, 0, sizeof(context->idle));
		return 0;
	}
	for (driver = gNetPsDriverHead; driver; driver = driver->next)
		memset(&driver->context->idle, 0, sizeof(driver->context->idle));
	return 0;
}

int sceNetPsNotifyLinkState(struct SceNetPsDriverContext *context)
{
	struct NetDriver *driver = context->driver;
	int link;

	if (driver->type != NET_DRIVER_TYPE_ETHER)
		return -5;
	driver->control(driver, NET_DRIVER_CTL_GET_LINK, &link, sizeof(link));
	if (!link)
		sceNetPsNotifyDriver(driver, NULL, 8, 0, NULL, 0);
	else
		sceNetPsNotifyDriver(driver, NULL, 4, 0, NULL, 0);
	return 0;
}

int sceNetPsResetAddressIdleTime(const struct sockaddr_in *address)
{
	unsigned int ip = address->sin_addr.s_addr;
	struct in_ifaddr *entry;
	struct ifnet *interface;
	struct SceNetPsDriverContext *context;

	LIST_FOREACH(entry, &in_ifaddrhashtbl[ip % 509], ia_hash) {
		if (ip == entry->ia_addr.sin_addr.s_addr)
			break;
	}
	if (entry) {
		interface = entry->ia_ifp;
		if (interface && interface != loif) {
			context = interface->if_softc;
			memset(&context->idle, 0, sizeof(context->idle));
		}
	}
	return 0;
}

