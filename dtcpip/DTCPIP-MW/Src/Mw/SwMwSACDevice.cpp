/*
 * The SAC device (see SwMwSACDevice.h): opening and releasing the SWS channel object and
 * the three key-material hand-offs the DTCP authentication path makes.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <scetypes.h>
#include "SwMwSACDevice.h"

#define SWMW_SAC_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwSACDevice.cpp"

extern "C" {
int swMwTrace(const char *format, ...);

/* open and release the channel object (the SWS content library, /) */
int SwMwSacChannelOpen(struct SwMwSacDevice *device);
int SwMwSacChannelRelease(struct SwMwSacDevice *device);
}

/* create the middleware's SAC handle */
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

/* destroy the handle: close the channel, release it and free the handle */
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

/* hand the channel a 4-byte key */
extern "C" int SwMwSacDeviceSetKey(struct SwMwSacDevice *device, const void *data)
{
	if (device->channel->setKey(data, 4) == 0)
		return 0;
	swMwTrace("[SWMW] Error %d in %s:%d\n", 12014, SWMW_SAC_SOURCE, 80);
	return 12014;
}

/* hand the channel two 32-byte values */
extern "C" int SwMwSacDeviceSetCertificatePair(struct SwMwSacDevice *device, const void *data)
{
	const unsigned char *bytes = (const unsigned char *)data;

	if (device->channel->setCertificatePair(bytes, 32, bytes + 32, 32) == 0)
		return 0;
	swMwTrace("[SWMW] Error %d in %s:%d\n", 12014, SWMW_SAC_SOURCE, 105);
	return 12014;
}

/* hand the channel one 32-byte value */
extern "C" int SwMwSacDeviceSetCertificate(struct SwMwSacDevice *device, const void *data)
{
	if (device->channel->setCertificate(data, 32) == 0)
		return 0;
	swMwTrace("[SWMW] Error %d in %s:%d\n", 12014, SWMW_SAC_SOURCE, 128);
	return 12014;
}

/* store the sender/receiver identifiers, length and 32-byte certificate */
extern "C" int SwMwSacDeviceSetInfo(struct SwMwSacDevice *device,
				    const struct SwMwSacDeviceInfo *info)
{
	memcpy(device->info.senderId, info->senderId, sizeof(device->info.senderId));
	memcpy(device->info.receiverId, info->receiverId, sizeof(device->info.receiverId));
	device->info.length = info->length;
	memcpy(device->info.certificate, info->certificate, sizeof(device->info.certificate));
	return 0;
}
