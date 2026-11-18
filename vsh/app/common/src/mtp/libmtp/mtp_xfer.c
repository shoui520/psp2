/*
 * The transfer thread ("XFER:"): sends or receives one data phase while the command layer
 * prepares the next buffer. mtpXferStart() hands it a buffer and the request, mtpXferWait()
 * waits for the request's completion and returns its result.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include "mtp_internal.h"

#define MTP_TAG_XFER	"XFER:"

#define MTP_XFER_THREAD_STACK	0x400

typedef struct MtpXfer {
	void *data;
	SceSize size;
	SceUInt64 totalSize;
	int result;
	/* the requests started and not waited for, MTP_XFER_EVF_SEND or _RECV */
	SceUInt32 requests;
	SceUID eventFlag;
	SceUID thread;
	SceUInt32 reserved[2];
} MtpXfer;

static MtpXfer *s_xfer;

static int _transferThread(SceSize args, void *argp);

int mtpXferInitialize(int priority, int cpuAffinityMask)
{
	SceKernelThreadOptParam option;
	int ret;

	s_xfer = (MtpXfer *)mtpUtilMalloc(sizeof(*s_xfer));
	if (s_xfer == NULL) {
		MTP_ERR(MTP_TAG_XFER, 149, "unable to alloc memory for Xfer function\n");
		ret = SCE_MTP_ERROR_NO_MEMORY;
		goto error;
	}
	s_xfer->eventFlag = sceKernelCreateEventFlag("SceMtpXfer", SCE_KERNEL_EVF_ATTR_MULTI, 0,
						     NULL);
	if (s_xfer->eventFlag <= 0) {
		MTP_ERR(MTP_TAG_XFER, 160,
			"unable to create event flag for Xfer function=0x%x\n", s_xfer->eventFlag);
		goto free;
	}
	mtpUtilMemset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	s_xfer->thread = sceKernelCreateThread("SceMtpXfer", _transferThread, priority,
					       MTP_XFER_THREAD_STACK, 0, cpuAffinityMask, &option);
	if (s_xfer->thread < 0) {
		MTP_ERR(MTP_TAG_XFER, 176, "unable to create thread for Xfer function=0x%x\n",
			s_xfer->thread);
		goto delete_event_flag;
	}
	if (sceKernelStartThread(s_xfer->thread, sizeof(s_xfer), &s_xfer) >= 0)
		return 0;
	MTP_ERR(MTP_TAG_XFER, 183, "unable to start Xfer thread\n");
	sceKernelDeleteThread(s_xfer->thread);
delete_event_flag:
	sceKernelDeleteEventFlag(s_xfer->eventFlag);
free:
	ret = SCE_MTP_ERROR_NOT_INITIALIZED;
	mtpUtilFree(s_xfer);
error:
	s_xfer = NULL;
	return ret;
}

int mtpXferUninitialize(void)
{
	int ret = 0;

	if (s_xfer != NULL) {
		sceKernelSetEventFlag(s_xfer->eventFlag, MTP_XFER_EVF_EXIT);
		ret = sceKernelWaitThreadEnd(s_xfer->thread, NULL, NULL);
		if (ret < 0)
			MTP_ERR(MTP_TAG_XFER, 218, "unable to wait for thread finish=0x%x\n", ret);
		if (sceKernelDeleteEventFlag(s_xfer->eventFlag) < 0)
			MTP_ERR(MTP_TAG_XFER, 223,
				"unable to delete event flag for Xfer function\n");
		if (sceKernelDeleteThread(s_xfer->thread) < 0)
			MTP_ERR(MTP_TAG_XFER, 228, "unable to delete thread for Xfer function\n");
		mtpUtilFree(s_xfer);
		s_xfer = NULL;
	}
	return ret;
}

int mtpXferStart(SceUInt32 request, void *data, SceSize size, SceUInt64 totalSize)
{
	int ret;

	if (s_xfer == NULL || data == NULL) {
		MTP_ERR(MTP_TAG_XFER, 256, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	s_xfer->data = data;
	s_xfer->size = size;
	s_xfer->totalSize = totalSize;
	s_xfer->result = SCE_MTP_ERROR_NOT_INITIALIZED;
	s_xfer->requests |= request;
	ret = sceKernelSetEventFlag(s_xfer->eventFlag, request);
	if (ret < 0)
		MTP_ERR(MTP_TAG_XFER, 270, "unable to set event for Xfer function=0x%x\n", ret);
	return ret;
}

int mtpXferWait(SceUInt32 done)
{
	int ret = 0;

	if ((s_xfer->requests & (MTP_XFER_EVF_SEND | MTP_XFER_EVF_RECV)) != 0) {
		s_xfer->requests &= ~(MTP_XFER_EVF_SEND | MTP_XFER_EVF_RECV);
		ret = sceKernelWaitEventFlag(s_xfer->eventFlag, done,
					     SCE_KERNEL_EVF_WAITMODE_OR |
					     SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
		if (ret >= 0)
			return s_xfer->result;
		MTP_ERR(MTP_TAG_XFER, 291, "failed in waiting for Xfer event=0x%x\n", ret);
	}
	return ret;
}

/* nothing calls it */
void mtpXferUnused(void)
{
}

static int _transferThread(SceSize args, void *argp)
{
	MtpXfer *xfer = *(MtpXfer **)argp;
	SceUInt32 outBits;

	if (xfer == NULL) {
		MTP_ERR(MTP_TAG_XFER, 65, "Xfer function is not initialized(null)\n");
		goto end;
	}
	while (sceKernelWaitEventFlag(xfer->eventFlag,
				      MTP_XFER_EVF_SEND | MTP_XFER_EVF_RECV | MTP_XFER_EVF_EXIT,
				      SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				      &outBits, NULL) >= 0) {
		if ((outBits & MTP_XFER_EVF_EXIT) != 0)
			goto end;
		if ((outBits & MTP_XFER_EVF_SEND) != 0) {
			if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
				NetMtpExpandSocketSendTimeout();
			xfer->result = mtpCoreDriverSendData(xfer->data, xfer->size,
							     xfer->totalSize);
			if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
				NetMtpResetSocketSendTimeout();
			sceKernelSetEventFlag(xfer->eventFlag, MTP_XFER_EVF_SEND_DONE);
		} else if ((outBits & MTP_XFER_EVF_RECV) != 0) {
			if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
				NetMtpExpandSocketReceiveTimeout();
			xfer->result = mtpCoreDriverRecvData(xfer->data, xfer->size, NULL);
			if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
				NetMtpResetSocketReceiveTimeout();
			sceKernelSetEventFlag(xfer->eventFlag, MTP_XFER_EVF_RECV_DONE);
		}
	}
	MTP_ERR(MTP_TAG_XFER, 77, "failed in waiting for Xfer event\n");
end:
	return -1;
}
