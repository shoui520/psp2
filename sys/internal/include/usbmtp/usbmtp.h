/*
 * SceUsbMtp (os0:kd/bootimage/usbmtp): SceUsbMtpForDriver, the USB transport
 * SceMtpIfDriver drives.
 */
#pragma once

#include <scetypes.h>

/*
 * SceUsbMtp's errors, which SceMtpIfDriver returns too (names unknown: the SDK's error
 * list does not name the range)
 */
#define MTP_ERROR_PROTOCOL		((int)0x80543601)
#define MTP_ERROR_BUSY			((int)0x80543602)	/* also: the port is another process's */
#define MTP_ERROR_INVALID		((int)0x80543603)
#define MTP_ERROR_RUNNING		((int)0x80543604)
#define MTP_ERROR_TIMEOUT		((int)0x80543605)
#define MTP_ERROR_CANCELLED		((int)0x80543606)
#define MTP_ERROR_DISCONNECTED		((int)0x80543607)
#define MTP_ERROR_MEMORY		((int)0x80543608)
#define MTP_ERROR_UNSUPPORTED		((int)0x80543609)
#define MTP_ERROR_RETRY_LIMIT		((int)0x8054360a)
#define MTP_ERROR_UNAVAILABLE		((int)0x8054360c)
#define MTP_ERROR_WAIT			((int)0x8054360d)

/* USB MTP data phase: a buffer and its progress */
typedef struct MtpIfTransfer {
	SceUInt32 reserved;
	void *buffer;
	SceUInt64 size;
	SceUInt64 totalSize;		/* of the data phase, -1 when unknown */
	SceUInt64 transferred;
} MtpIfTransfer;

int SceUsbMtpForDriver_B958E5DE(void);
int sceUsbMtpSendData(MtpIfTransfer *transfer);
int sceUsbMtpRecvData(MtpIfTransfer *transfer);
int SceUsbMtpForDriver_576A266F(void *command, MtpIfTransfer *transfer);
int SceUsbMtpForDriver_4463009B(const void *response);
int SceUsbMtpForDriver_5FA86BA4(void *event);
int SceUsbMtpForDriver_151947D2(void);
int SceUsbMtpForDriver_76D707B1(SceUInt32 timeout);
int SceUsbMtpForDriver_6F7DF4B8(void);
int SceUsbMtpForDriver_E64513A9(int phase);
int sceUsbMtpStart(void);
int sceUsbMtpStop(void);
