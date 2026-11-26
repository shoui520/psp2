/*
 * The SAC device (see SwMwSACDevice.h): opening and releasing the SWS channel object and
 * the three key-material hand-offs the DTCP authentication path makes.
 */

#include <stddef.h>
#include <stdlib.h>
#include <scetypes.h>
#include "SwMwSACDevice.h"

#define SWMW_SAC_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwSACDevice.cpp"

extern "C" {
int swMwTrace(const char *format, ...);

/* open and release the channel object (the SWS content library, 0x8104b208/0x8104b28a) */
int SwMwSacChannelOpen(struct SwMwSacDevice *device);
int SwMwSacChannelRelease(struct SwMwSacDevice *device);
}

/* create the middleware's SAC handle (0x8100c5e4) */
extern "C" int SwMwSacDeviceCreate(struct SwMwSacDevice **out)
{
	struct SwMwSacDevice *device;

	device = (struct SwMwSacDevice *)malloc(sizeof(struct SwMwSacDevice));
	if (device != NULL) {
		if (SwMwSacChannelOpen(device) == 0) {
			*out = device;
			return 0;
		}
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12014, SWMW_SAC_SOURCE, 36);
	} else {
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12014, SWMW_SAC_SOURCE, 29);
	}
	if (device != NULL)
		free(device);
	return 12014;
}

/* destroy the handle: close the channel, release it and free the handle (0x8100c67a) */
extern "C" int SwMwSacDeviceDestroy(struct SwMwSacDevice *device)
{
	if (device->channel != NULL) {
		device->channel->close();
		SwMwSacChannelRelease(device);
		device->channel = NULL;
	}
	if (device != NULL)
		free(device);
	return 0;
}

/* hand the channel a 4-byte key (0x8100c6b8) */
extern "C" int SwMwSacDeviceSetKey(struct SwMwSacDevice *device, const void *data)
{
	if (device->channel->setKey(data, 4) == 0)
		return 0;
	swMwTrace("[SWMW] Error %d in %s:%d\n", 12014, SWMW_SAC_SOURCE, 80);
	return 12014;
}

/* hand the channel two 32-byte values (0x8100c712) */
extern "C" int SwMwSacDeviceSetCertificatePair(struct SwMwSacDevice *device, const void *data)
{
	const unsigned char *bytes = (const unsigned char *)data;

	if (device->channel->setCertificatePair(bytes, 32, bytes + 32, 32) == 0)
		return 0;
	swMwTrace("[SWMW] Error %d in %s:%d\n", 12014, SWMW_SAC_SOURCE, 105);
	return 12014;
}

/* hand the channel one 32-byte value (0x8100c780) */
extern "C" int SwMwSacDeviceSetCertificate(struct SwMwSacDevice *device, const void *data)
{
	if (device->channel->setCertificate(data, 32) == 0)
		return 0;
	swMwTrace("[SWMW] Error %d in %s:%d\n", 12014, SWMW_SAC_SOURCE, 128);
	return 12014;
}
