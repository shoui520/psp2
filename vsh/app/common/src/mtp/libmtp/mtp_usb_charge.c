/*
 * USB charging ("USB CHARGE:"): while the responder is stopped and a cable is in, a
 * thread keeps the MTP port open and answers a host with the DeviceInfo dataset only,
 * so the host keeps the port's charging current up.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <udcd/udcd_user.h>
#include <mtpif/mtpif.h>
#include "mtp_internal.h"

#define MTP_TAG_USB_CHARGE	"USB CHARGE:"

#define USB_CHARGE_THREAD_PRIORITY	89
#define USB_CHARGE_THREAD_STACK		0x8000

#define USB_CHARGE_SEND_BUFFER_SIZE	0x2000
#define USB_CHARGE_WAIT_TIMEOUT		1000000		/* us */
#define USB_CHARGE_RETRY_DELAY		1000000

/* the MTP operations and responses it handles */
#define MTP_OP_GET_DEVICE_INFO		0x1001
#define MTP_OP_OPEN_SESSION		0x1002
#define MTP_OP_CLOSE_SESSION		0x1003
#define MTP_RESPONSE_OPERATION_NOT_SUPPORTED	0x2005

/* SceMtpIf's phases */
#define MTP_IF_PHASE_COMMAND		0
#define MTP_IF_PHASE_DATA		1
#define MTP_IF_PHASE_RESPONSE		8

#define MTP_IF_PORT_USB			1

static SceUID s_chargeThread = -1;
static int s_started;
static int s_stopRequested;
static const void *s_port;

static int usbChargeThread(SceSize args, void *argp);

void usbChargeInit(void)
{
	SceKernelThreadOptParam option;
	SceUID thread;

	mtpUtilMemset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	thread = sceKernelCreateThread("SceMtpUsbCharge", usbChargeThread,
				       USB_CHARGE_THREAD_PRIORITY, USB_CHARGE_THREAD_STACK, 0, 0,
				       &option);
	if (thread < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 344, "unable to create usb charge thread 0x%x\n",
			thread);
		return;
	}
	s_chargeThread = thread;
}

void usbChargeEnd(void)
{
	int ret = sceKernelDeleteThread(s_chargeThread);

	if (ret < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 364, "unable to delete usb charge thread 0x%x\n", ret);
		return;
	}
	s_chargeThread = -1;
}

void usbChargeStart(void)
{
	int ret;

	if (s_started || s_chargeThread < 0)
		return;
	s_port = sceMtpIfGetPort(MTP_IF_PORT_USB);
	sceMtpIfStartPort(s_port);
	s_stopRequested = 0;
	ret = sceKernelStartThread(s_chargeThread, 0, NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 396, "unable to start usb charge thread 0x%x\n", ret);
		sceMtpIfStopPort(s_port);
		return;
	}
	s_started = 1;
}

void usbChargeStop(void)
{
	if (!s_started)
		return;
	s_stopRequested = 1;
	sceMtpIfStopPort(s_port);
	SceUdcd_F1A3690B();
	sceKernelWaitThreadEnd(s_chargeThread, NULL, NULL);
	s_started = 0;
}

static int sendDeviceInfo(SceUInt32 transactionId)
{
	const MtpDeviceInfo *info;
	MtpIfTransfer transfer;
	MtpString string;
	SceUInt16 functionalMode = 1;
	SceUInt8 *buf;
	int size, ret, length;

	buf = mtpUtilMalloc(USB_CHARGE_SEND_BUFFER_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 71, "unable to alloc memory for send data\n");
		return -1;
	}

	info = mtpSpecGetDeviceInfo();
	if (info == NULL) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 78, "No device info");
		ret = -2;
		goto out;
	}
	mtpUtilMemcpy(buf, &info->standardVersion, 2);
	mtpUtilMemcpy(buf + 2, &info->vendorExtensionId, 4);
	mtpUtilMemcpy(buf + 6, &info->vendorExtensionVersion, 2);

	/* no MTP extensions while charging */
	string.numChars = 0xFF;
	string.chars = buf + 9;
	length = mtpUtilPackMtpString(&string, NULL);
	if (length < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 97, "unable to pack MTP Extensions=0x%x", length);
		ret = -2;
		goto out;
	}
	buf[8] = string.numChars;
	mtpUtilMemcpy(buf + 9 + length, &functionalMode, 2);
	size = 9 + length + 2;

	length = mtpUtilPackCodeList(buf + size, &info->operations);
	if (length < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 110, "unable to pack Operations Supported=0x%x", length);
		ret = -2;
		goto out;
	}
	size += length;
	length = mtpUtilPackCodeList(buf + size, &info->events);
	if (length < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 118, "unable to pack Events Supported=0x%x", length);
		ret = -2;
		goto out;
	}
	size += length;
	length = mtpUtilPackCodeList(buf + size, &info->deviceProperties);
	if (length < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 126,
			"unable to pack Device Properties Supported=0x%x", length);
		ret = -2;
		goto out;
	}
	size += length;
	length = mtpUtilPackCodeList(buf + size, &info->captureFormats);
	if (length < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 134, "unable to pack Capture Formats=0x%x", length);
		ret = -2;
		goto out;
	}
	size += length;
	length = mtpUtilPackCodeList(buf + size, &info->playbackFormats);
	if (length < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 142, "unable to pack Playback Formats=0x%x", length);
		ret = -2;
		goto out;
	}
	size += length;

	string.numChars = 0xFF;
	string.chars = buf + size + 1;
	length = mtpUtilPackMtpString(&string, info->manufacturer);
	if (length < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 152, "unable to pack Manufacturer=0x%x", length);
		ret = -2;
		goto out;
	}
	buf[size] = string.numChars;
	size += length + 1;
	string.numChars = 0xFF;
	string.chars = buf + size + 1;
	length = mtpUtilPackMtpString(&string, info->model);
	if (length < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 164, "unable to pack Model=0x%x", length);
		ret = -2;
		goto out;
	}
	buf[size] = string.numChars;
	size += length + 1;
	string.numChars = 0xFF;
	string.chars = buf + size + 1;
	length = mtpUtilPackMtpString(&string, info->deviceVersion);
	if (length < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 176, "unable to pack Device Version=0x%x", length);
		ret = -2;
		goto out;
	}
	buf[size] = string.numChars;
	size += length + 1;
	string.numChars = 0xFF;
	string.chars = buf + size + 1;
	length = mtpUtilPackMtpString(&string, info->serialNumber);
	if (length < 0) {
		MTP_ERR(MTP_TAG_USB_CHARGE, 188, "unable to pack Serial Number=0x%x", length);
		ret = -2;
		goto out;
	}
	buf[size] = string.numChars;
	size += length + 1;

	transfer.transactionId = transactionId;
	transfer.buffer = buf;
	transfer.size = size;
	transfer.totalSize = size;
	transfer.transferred = 0;
	ret = sceMtpIfSendDataWithParam(s_port, &transfer);
	if (ret >= 0)
		ret = (int)transfer.transferred;
	if (ret < 0)
		MTP_ERR(MTP_TAG_USB_CHARGE, 199, "unable to send data=0x%x\n", ret);
out:
	mtpUtilFree(buf);
	return ret;
}

static int usbChargeThread(SceSize args, void *argp)
{
	SceUdcdWaitParam waitParam;
	UdcdUserDeviceState state;
	MtpOperation command;
	MtpOperation response;
	int ret;

	mtpUtilMemset(&waitParam, 0, sizeof(waitParam));
	waitParam.status = SCE_UDCD_STATUS_ACTIVATED;
	waitParam.driverName = "USB_MTP_Driver";
	mtpSpecLoadIniData(NULL);

	while (!s_stopRequested) {
		waitParam.status = SCE_UDCD_STATUS_ACTIVATED;
		ret = sceUdcdWaitState(&waitParam, USB_CHARGE_WAIT_TIMEOUT);
		if (ret == (int)UDCD_ERROR_INTERRUPTED)
			break;
		if (ret == (int)UDCD_ERROR_TIMEOUT)
			continue;
		if (ret < 0) {
			MTP_ERR(MTP_TAG_USB_CHARGE, 233, "sceUdcdWaitState failed 0x%x\n", ret);
			sceKernelDelayThread(USB_CHARGE_RETRY_DELAY);
			continue;
		}
		ret = sceMtpIfWaitConnect(s_port, USB_CHARGE_WAIT_TIMEOUT);
		if (ret == MTP_ERROR_CANCELLED)
			break;
		if (ret == MTP_ERROR_TIMEOUT)
			continue;
		if (ret < 0) {
			MTP_ERR(MTP_TAG_USB_CHARGE, 245, "sceMtpIfWaitConnect failed 0x%x\n", ret);
			sceKernelDelayThread(USB_CHARGE_RETRY_DELAY);
			continue;
		}

		sceMtpIfChangePhase(s_port, MTP_IF_PHASE_COMMAND);
		for (;;) {
			ret = sceMtpIfRecvCommand(s_port, &command);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_USB_CHARGE, 257, "MtpIfRecvCommand failed: 0x%x\n",
					ret);
				sceKernelDelayThread(USB_CHARGE_RETRY_DELAY);
				break;
			}
			sceMtpIfChangePhase(s_port, MTP_IF_PHASE_DATA);
			switch (command.code) {
			case MTP_OP_GET_DEVICE_INFO:
				if (sendDeviceInfo(command.transactionId) < 0) {
					mtpUtilSetMtpResponse(MTP_RESPONSE_GENERAL_ERROR, NULL, 0,
							      &command, &response);
					break;
				}
				mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, &command, &response);
				break;
			case MTP_OP_OPEN_SESSION:
				mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, &command, &response);
				break;
			case MTP_OP_CLOSE_SESSION:
				mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, &command, &response);
				break;
			default:
				mtpUtilSetMtpResponse(MTP_RESPONSE_OPERATION_NOT_SUPPORTED, NULL, 0,
						      &command, &response);
				break;
			}
			sceMtpIfChangePhase(s_port, MTP_IF_PHASE_RESPONSE);
			ret = sceMtpIfSendResponse(s_port, &response);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_USB_CHARGE, 296,
					"failed in sending response=0x%x\n", ret);
				sceKernelDelayThread(USB_CHARGE_RETRY_DELAY);
				break;
			}
		}
		if (ret == MTP_ERROR_DISCONNECTED)
			continue;

		/* the host went away: wait for the cable to be pulled */
		mtpUtilMemset(&state, 0, sizeof(state));
		ret = sceUdcdGetDeviceState(&state);
		if (ret == (int)UDCD_ERROR_INTERRUPTED)
			break;
		if (ret < 0) {
			MTP_ERR(MTP_TAG_USB_CHARGE, 313, "sceUdcdGetDeviceState failed 0x%x\n", ret);
			sceKernelDelayThread(USB_CHARGE_RETRY_DELAY);
		} else if (state.activation != SCE_UDCD_STATUS_DEACTIVATED) {
			sceKernelDelayThread(USB_CHARGE_RETRY_DELAY);
		}
	}
	mtpSpecTerminate();
	return 0;
}
