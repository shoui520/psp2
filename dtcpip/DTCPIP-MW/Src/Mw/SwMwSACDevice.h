/*
 * The SAC device (SwMwSACDevice.cpp): the middleware's handle on the SWS secure
 * authentication channel object. It hands the channel its key material in 4-, 32- and
 * 32+32-byte blocks and closes it. The channel object is the SWS content library's
 * (vtable 0x8109cdb0, 16 bytes, created by 0x8104b208 and released by 0x8104b28a).
 */

#pragma once

#include <scetypes.h>

/* the channel object's interface, in dispatch-table order (0x8109cdb0) */
struct SwMwSacChannel {
	virtual int setKey(const void *data, int size);			/* 0x00 */
	virtual int setCertificatePair(const void *first, int size,
				       const void *second, int size2);	/* 0x04 */
	virtual int setCertificate(const void *data, int size);		/* 0x08 */
	virtual int isActive();						/* 0x0c */
	virtual int close();						/* 0x10 */
};

/* the middleware's SAC handle: the channel pointer and its area */
struct SwMwSacDevice {
	struct SwMwSacChannel *channel;	/* 0x00 */
	unsigned char work[0x44];	/* 0x04 */
};

extern "C" {
int SwMwSacDeviceCreate(struct SwMwSacDevice **device);
int SwMwSacDeviceDestroy(struct SwMwSacDevice *device);
int SwMwSacDeviceSetKey(struct SwMwSacDevice *device, const void *data);
int SwMwSacDeviceSetCertificatePair(struct SwMwSacDevice *device, const void *data);
int SwMwSacDeviceSetCertificate(struct SwMwSacDevice *device, const void *data);
}
