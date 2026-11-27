/*
 * The SAC device (SwMwSACDevice.cpp): the middleware's handle on the SWS secure
 * authentication channel object. It hands the channel its key material in 4-, 32- and
 * 32+32-byte blocks and closes it. The channel object is the SWS content library's
 * (vtable , 16 bytes, created by and released by ).
 */

#pragma once

#include <scetypes.h>

/* the channel object's interface, in dispatch-table order */
struct SwMwSacChannel {
	virtual int setKey(const void *data, int size);			/* 0x00 */
	virtual int setCertificatePair(const void *first, int size,
				       const void *second, int size2);	/* 0x04 */
	virtual int setCertificate(const void *data, int size);		/* 0x08 */
	virtual int isActive();						/* 0x0c */
	virtual int close();						/* 0x10 */
};

/* the SAC device certificate/key block copied by 0x2108 */
struct SwMwSacDeviceInfo {
	unsigned char senderId[16];		/* 0x00 */
	unsigned char receiverId[16];		/* 0x10 */
	int length;				/* 0x20 */
	unsigned char certificate[32];		/* 0x24 */
};

/* the middleware's SAC handle: the channel pointer and its area */
struct SwMwSacDevice {
	struct SwMwSacChannel *channel;		/* 0x00 */
	struct SwMwSacDeviceInfo info;		/* 0x04 */
};

extern "C" {
int SwMwSacDeviceCreate(struct SwMwSacDevice **device);
int SwMwSacDeviceDestroy(struct SwMwSacDevice *device);
int SwMwSacDeviceSetKey(struct SwMwSacDevice *device, const void *data);
int SwMwSacDeviceSetCertificatePair(struct SwMwSacDevice *device, const void *data);
int SwMwSacDeviceSetCertificate(struct SwMwSacDevice *device, const void *data);
int SwMwSacDeviceSetInfo(struct SwMwSacDevice *device, const struct SwMwSacDeviceInfo *info);
}
