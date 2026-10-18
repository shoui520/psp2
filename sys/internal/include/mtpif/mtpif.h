/* SceMtpIfDriver transport interface additions. Names not in the NID
 * database remain NID-named; signatures follow the module's call sites.
 */
#pragma once

#include <scetypes.h>
#include <threadmgr/threadmgr.h>
#include <acmgr/acmgr.h>
#include <sysmem/procevent.h>
#include <sysmem/sysroot.h>

/* USB MTP data phase: a buffer and its progress */
typedef struct MtpIfTransfer {
	SceUInt32 reserved;
	void *buffer;
	SceUInt64 size;
	SceUInt64 totalSize;		/* of the data phase, -1 when unknown */
	SceUInt64 transferred;
} MtpIfTransfer;

int SceUsbMtpForDriver_B958E5DE(void);
int SceUsbMtpForDriver_576A266F(void *command, MtpIfTransfer *transfer);
int SceUsbMtpForDriver_4463009B(const void *response);
int sceUsbMtpSendData(MtpIfTransfer *transfer);
int sceUsbMtpRecvData(MtpIfTransfer *transfer);
int SceUsbMtpForDriver_5FA86BA4(void *event);
int SceUsbMtpForDriver_151947D2(void);
int SceUsbMtpForDriver_76D707B1(SceUInt32 timeout);
int sceUsbMtpStart(void);
int SceUsbMtpForDriver_08CD359F(void);
int SceUsbMtpForDriver_6F7DF4B8(void);
int SceUsbMtpForDriver_E64513A9(int phase);

