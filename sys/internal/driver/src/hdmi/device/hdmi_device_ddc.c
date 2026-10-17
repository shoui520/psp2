/*
 * SceHdmi device layer: reads over the DDC (the EDID and the HDCP receiver's
 * KSV FIFO), serialized by the "SceHdmiDeviceDdc" mutex, and the check of
 * each EDID block read. Internal names are ours unless a comment says otherwise.
 */
#include <psp2kern/types.h>
#include <psp2kern/kernel/threadmgr.h>
#include <fw.h>
#include <sysmem/debug.h>
#include "../hdmi_internal.h"
#include <threadmgr/threadmgr.h>

/* The DDC event flag's bits */
#define HDMI_DDC_EVENT_ABORT		0x01	/* hdmiDeviceDdcAbort */
#define HDMI_DDC_EVENT_EDID_READY	0x02	/* the transmitter read an EDID segment */
#define HDMI_DDC_EVENT_ERROR		0x10	/* the transmitter's DDC controller failed */
#define HDMI_DDC_EVENT_EDID		(HDMI_DDC_EVENT_ABORT | HDMI_DDC_EVENT_EDID_READY | \
					 HDMI_DDC_EVENT_ERROR)

/* The transmitter's EDID / DDC read map */
#define HDMI_DDC_SLAVE			0x7e

/* HdmiDdcRequest.mode */
#define HDMI_DDC_MODE_EDID		0x00
#define HDMI_DDC_MODE_KSV_FIFO		0x21

#define HDMI_EDID_BLOCK_SIZE		128
#define HDMI_EDID_SEGMENT_TIMEOUT	500000	/* us */
#define HDMI_EDID_RETRY_DELAY		100000	/* us */

typedef struct HdmiDdcWork {
	SceUID eventFlag;				/* 0x00 */
	SceUID mutex;					/* 0x04 */
	SceKernelEventFlagOptParam eventFlagOpt;	/* 0x08 */
} HdmiDdcWork;

static HdmiDdcWork ddcWork;

int hdmiDeviceDdcInitialize(HdmiDevice *device)
{
	ddcWork = (HdmiDdcWork){ 0 };
	ddcWork.eventFlag = ksceKernelCreateEventFlag("SceHdmiDeviceDdc", SCE_EVENT_WAITMULTIPLE, 0,
						      &ddcWork.eventFlagOpt);
	ddcWork.mutex = ksceKernelCreateMutex("SceHdmiDeviceDdc", 0x2002, 0, NULL);
	if (ddcWork.mutex < 0)
		return SCE_HDMI_ERROR_DEVICE;
	return 0;
}

int hdmiDeviceDdcClearAbort(HdmiDevice *device)
{
	return ksceKernelClearEventFlag(ddcWork.eventFlag, ~HDMI_DDC_EVENT_ABORT);
}

int hdmiDeviceDdcAbort(void)
{
	return ksceKernelSetEventFlag(ddcWork.eventFlag, HDMI_DDC_EVENT_ABORT);
}

/* Wait for one of the events in pattern and clear those that came. */
inline int hdmiDeviceDdcWaitEvent(SceUInt32 *events, SceUInt32 pattern, SceUInt32 *timeout)
{
	SceUInt32 cleared;
	int ret;

	HDMI_PRINTF(HDMI_DBG_DDC, "[hdmiDDC ] ", "WaitSystemEvent Ptn:0x%08X\n", pattern);
	ret = ksceKernelWaitEventFlag(ddcWork.eventFlag, pattern, SCE_EVENT_WAITOR, events, timeout);
	cleared = pattern & *events;
	ksceKernelClearEventFlag(ddcWork.eventFlag, ~cleared);
	HDMI_PRINTF(HDMI_DBG_DDC, "[hdmiDDC ] ", "Event Occure :0x%08X\n", *events);
	HDMI_PRINTF(HDMI_DBG_DDC, "[hdmiDDC ] ", "Event Clear  :0x%08X\n", cleared);
	return ret;
}

int hdmiDeviceDdcSetEvent(SceUInt32 bits)
{
	return ksceKernelSetEventFlag(ddcWork.eventFlag, bits);
}

int hdmiDeviceDdcClearEvent(SceUInt32 bits)
{
	return ksceKernelClearEventFlag(ddcWork.eventFlag, ~bits);
}

/*
 * Sony's name. Read request->size bytes from the transmitter's DDC buffer.
 * The transmitter reads the EDID a segment (two blocks) at a time: the
 * header and the first block of each segment start the segment's read.
 */
int sceHdmiDeviceDdcRead(HdmiDevice *device, const HdmiDdcRequest *request, SceUInt32 timeout,
			 SceUInt8 block)
{
	SceUInt32 events;
	SceUInt32 waitTimeout;
	int ret;

	HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", "sceHdmiDeviceDdcRead(block%d)\n", block);
	waitTimeout = 0xffffffff;
	ksceKernelLockMutex(ddcWork.mutex, 1, &waitTimeout);
	if (request->mode == HDMI_DDC_MODE_EDID &&
	    (block == 0 ? request->size == 8 : !(block & 1))) {
		HdmiDeviceHwSetEdidSegment(device, block >> 1);
		waitTimeout = HDMI_EDID_SEGMENT_TIMEOUT;
		ret = hdmiDeviceDdcWaitEvent(&events, HDMI_DDC_EVENT_EDID, &waitTimeout);
		if (ret != 0) {
			HDMI_ERR_PRINTF("[hdmiERR ] WaitEdidDone Event Error:0x%08X\n", ret);
			goto out;
		}
		if (events & HDMI_DDC_EVENT_ERROR) {
			HDMI_ERR_PRINTF("[hdmiERR ] WaitEdidDone Receive DDC-ERROR :0x%08X\n", events);
			ret = -1;
			sceKernelDelayThread(HDMI_EDID_RETRY_DELAY);
			goto out;
		}
		if (events & HDMI_DDC_EVENT_ABORT) {
			HDMI_ERR_PRINTF("[hdmiERR ] WaitEdidDone Receive DDC-ERROR :0x%08X\n", events);
			ret = -1;
			goto out;
		}
	}
	ret = sceHdmiDeviceI2cRead(1, HDMI_DDC_SLAVE,
				   request->mode == HDMI_DDC_MODE_EDID ? request->offset : 0,
				   request->buffer, request->size);
	if (ret >= 0 && request->mode == HDMI_DDC_MODE_KSV_FIFO)
		ret = HdmiDeviceAD80244ClearBksvFlag(1);
out:
	ksceKernelUnlockMutex(ddcWork.mutex, 1);
	return ret;
}

/* The EDID checksum (the 8-bit sum of a block is 0) and a CRC of the block. */
static inline SceUInt8 edidBlockSum(const SceUInt8 *data, SceUInt16 *crc)
{
	SceUInt8 sum = 0;
	SceUInt8 value;
	int i, bit;

	*crc = 0;
	for (i = 0; i < HDMI_EDID_BLOCK_SIZE; i++) {
		value = data[i];
		sum += value;
		for (bit = 0; bit < 8; bit++) {
			if ((*crc ^ (value >> bit)) & 1)
				*crc = (*crc >> 1) ^ 0x1021;
			else
				*crc = *crc >> 1;
		}
	}
	return sum;
}

/*
 * Read EDID block 'block' into data. A block whose checksum fails is read a
 * second time and taken when both reads are the same.
 */
int hdmiDeviceEdidReadBlock(HdmiDevice *device, int block, SceUInt8 *data)
{
	HdmiDdcRequest request;
	SceUInt16 crc, crc2;
	int ret;

	request.address = block / 2;
	request.offset = (block & 1) ? HDMI_EDID_BLOCK_SIZE : 0;
	request.size = HDMI_EDID_BLOCK_SIZE;
	request.mode = HDMI_DDC_MODE_EDID;
	request.buffer = data;
	ret = sceHdmiDeviceDdcRead(device, &request, 200000, block);
	if (ret != 0) {
		HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ",
			    " Error Read EDID body %d block in 1st time:0x%08X\n", block, ret);
		return ret;
	}
	if (edidBlockSum(data, &crc) == 0)
		return 0;
	HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ",
		    " Error Check Sum EDID body %d block in 1st time\n", block);
	HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", " Try 2nd time..\n");
	sceKernelDelayThread(HDMI_EDID_RETRY_DELAY);
	HdmiDeviceHwRestartEdid(device);
	ret = sceHdmiDeviceDdcRead(device, &request, 200000, block);
	if (ret != 0) {
		HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ",
			    " Error Read EDID body %d block in 2nd time\n", block);
		return ret;
	}
	edidBlockSum(data, &crc2);
	if (crc2 != crc) {
		HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ",
			    " Error CRC Check bitween 1st time and 2nd time in %d block\n", block);
		return 1;
	}
	HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ",
		    " Check Sum is error but the same data is read twice. So I decide Success..\n");
	return 0;
}

