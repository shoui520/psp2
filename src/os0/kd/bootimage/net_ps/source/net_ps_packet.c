/* SCE packet-service adapters; original source filename is unavailable. */

#include "net_ps_internal.h"
#include <string.h>

SceNetPsPacketService gNetPsPacketService;

extern int gNetPsSoftTick;
void callout_reset(struct callout *timer, int ticks,
		   void (*callback)(void *), void *argument);
void callout_stop(struct callout *timer);
void sceNetPsBandwidthTimer(void *argument);
void sceNetPsSwapEmulationParam(SceNetEmulationParam *parameters);
int sceNetPsSetEmulationPreset(SceNetEmulationParam *parameters);
int sceNetPsProcessDelayedPackets(struct ifnet *interface);

/* SCE: arm the emulated bandwidth timer for the next 100 milliseconds. */
int sceNetPsStartBandwidthTimer(struct ifnet *interface)
{
	gNetPsPacketService.flags |= 0x100000;
	callout_reset(&gNetPsPacketService.bandwidthCallout,
		      100000 / gNetPsSoftTick, sceNetPsBandwidthTimer, interface);
	return 0;
}

/* SCE: retrieve a preset or the driver's current parameters. */
int sceNetPsGetDriverEmulation(struct SceNetPsDriverContext *context,
			       SceNetEmulationParam *parameters, int networkOrder)
{
	int version;

	if (networkOrder)
		sceNetPsSwapEmulationParam(parameters);
	version = parameters->version;
	if (version) {
		int minor = (unsigned char)version;
		int major = version & 0xff00;
		int supported = (minor <= 4) & (major == 0x100);

		if (!supported)
			return -2;
	}
	if (parameters->option_number > 4)
		return -22;
	if (parameters->option_number) {
		sceNetPsSetEmulationPreset(parameters);
	} else {
		if (!context)
			return -19;
		memcpy(parameters, &context->emulation, sizeof(*parameters));
	}
	parameters->current_version = 0x104;
	if (networkOrder)
		sceNetPsSwapEmulationParam(parameters);
	return 0;
}

/* SCE: delayed traffic must drain before the interface accepts another frame. */
int sceNetPsCanTransmit(struct SceNetPsDriverContext *context,
			struct ifnet *interface)
{
	(void)context;
	return !(gNetPsPacketService.flags & 1) ||
		sceNetPsProcessDelayedPackets(interface) == 0;
}

/* SCE: cancel both timers when their owning driver is removed. */
int sceNetPsStopDriverEmulation(struct NetDriver *driver)
{
	if (gNetPsPacketService.activeDriver != driver)
		return 0;
	if (gNetPsPacketService.flags & 1) {
		callout_stop(&gNetPsPacketService.delayCallout);
		gNetPsPacketService.flags &= ~1u;
	}
	if (gNetPsPacketService.flags & 2) {
		callout_stop(&gNetPsPacketService.bandwidthCallout);
		gNetPsPacketService.flags &= ~2u;
	}
	gNetPsPacketService.activeDriver = NULL;
	return 0;
}

/* Name unknown; initialize the complete packet-service state and adapters. */
int sceNetPsInitializePacketCallbacks(struct NetDriverPacket *(*prepare)(
		SceNetPsPacketQueue *, int, int),
		int (*kick)(struct NetDriver *, int))
{
	memset(&gNetPsPacketService, 0, sizeof(gNetPsPacketService));
	gNetPsPacketService.prepareQueue = prepare;
	gNetPsPacketService.kickTx = kick;
	return 0;
}
