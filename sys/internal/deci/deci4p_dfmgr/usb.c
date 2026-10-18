/*
 * Link 1: DECI4P over the DevKit USB port of SceDbgUsb (Sony's usb.c).
 */

#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/sysmem.h>
#include "dfmgr_internal.h"

#define USB_ERROR_DISCONNECTED	0x8024fff2

/* s_usbRecvEvf bits, set by SceDbgUsb */
#define USB_EVF_DATA		0x2
#define USB_EVF_CONNECTED	0x4
#define USB_EVF_DISCONNECTED	0x8

static const DfMgrLinkInfo s_usbLinkInfo = { "USB", 1 };

static DfMgrLinkCallback s_usbSendCallback;
static DfMgrLinkCallback s_usbRecvCallback;
static SceUID s_usbSendThread;
static SceUID s_usbRecvThread;
static SceUID s_usbSendEvf;
static SceUID s_usbRecvEvf;
static DfMgrLinkCB *s_usbLink;

static int usbRecv(void *buf, SceSize size)
{
	SceSize done = 0;
	int ret;

	while (done < size) {
		ret = _sceDbgUsbPullData((SceUInt8 *)buf + done, size - done);
		done += ret;
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(15, 270, "0x%08X\n", ret);
			return ret;
		}
	}
	return size;
}

static int usbSend(void *buf, SceSize size)
{
	SceSize done = 0;
	int ret;

	while (done < size) {
		ret = SceDbgUsbForDriver_40C65DB2((SceUInt8 *)buf + done, size - done);
		done += ret;
		if (ret < 0) {
			if (ret == (int)USB_ERROR_DISCONNECTED)
				return ret;
			SCE_PRINTF_WITH_INFO(15, 250, "0x%08X\n", ret);
			return ret;
		}
	}
	return size;
}

static int _recvThread(SceSize args, void *argp)
{
	SceUInt32 bits;
	int ret;

	for (;;) {
		bits = 0;
		ret = ksceKernelWaitEventFlag(s_usbRecvEvf,
					      USB_EVF_DATA | USB_EVF_CONNECTED | USB_EVF_DISCONNECTED,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
		if (ret < 0)
			break;
		if (bits & USB_EVF_DATA)
			s_usbRecvCallback(s_usbLink);
		if (bits & USB_EVF_CONNECTED) {
			SCE_PRINTF_WITH_INFO(0, 367, "[DECI] PS Vita DevKit (target) USB: "
					     "connecting\n");
			dfmgrSetLinkStatus(s_usbLink, DFMGR_LINK_ATTACHED, 1);
			dcmpSendStatusPower(1, 0xf, 1, 0);
		}
		if (bits & USB_EVF_DISCONNECTED) {
			SCE_PRINTF_WITH_INFO(0, 379, "[DECI] PS Vita DevKit (target) USB: "
					     "disconnected\n");
			dfmgrSetLinkStatus(s_usbLink, DFMGR_LINK_ATTACHED, 0);
			dfmgrDisconnectLink(s_usbLink, 1);
		}
	}
	SCE_PRINTF_WITH_INFO(15, 354, "0x%08X\n", ret);
	SCE_DBG_ASSERT_FAIL(355);
	return ret;
}

static int _sendThread(SceSize args, void *argp)
{
	SceUInt32 bits;
	int ret;

	for (;;) {
		bits = 0;
		ret = ksceKernelWaitEventFlag(s_usbSendEvf, 1,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					      &bits, NULL);
		if (ret < 0)
			break;
		if (bits & 1)
			s_usbSendCallback(s_usbLink);
		else
			SCE_PRINTF_WITH_INFO(15, 322, "0x%08X\n", bits);
	}
	SCE_PRINTF_WITH_INFO(15, 312, "0x%08X\n", ret);
	SCE_DBG_ASSERT_FAIL(313);
	return ret;
}

int usbInit(DfMgrLinkCB *link, DfMgrLinkCallback sendCallback, DfMgrLinkCallback recvCallback,
	    SceUID *pSendEvf)
{
	SceKernelThreadOptParam opt;
	int ret;

	s_usbRecvCallback = recvCallback;
	link->info = &s_usbLinkInfo;
	link->send = usbSend;
	link->recv = usbRecv;
	opt.kStackMemType = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW;
	opt.size = sizeof(opt);
	opt.attr = 1;
	s_usbSendCallback = sendCallback;
	s_usbLink = link;

	ret = ksceKernelCreateEventFlag("SceDeci4pDfMgrUsbSend", SCE_KERNEL_EVF_ATTR_MULTI, 0,
					NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 90);
		return ret;
	}
	*pSendEvf = ret;
	s_usbSendEvf = ret;
	ret = ksceKernelCreateEventFlag("SceDeci4pDfMgrUsbRecv", SCE_KERNEL_EVF_ATTR_MULTI, 0,
					NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 104);
		return ret;
	}
	s_usbRecvEvf = ret;
	ret = SceDbgUsbForDriver_A6BFCC0A(s_usbRecvEvf);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 112, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(113);
		return ret;
	}

	ret = ksceKernelCreateThread("SceDeci4pDfMgrUsbSend", _sendThread, 0x1e, 0x4000, 0, 0xf,
				     &opt);
	if (ret < 0)
		return ret;
	s_usbSendThread = ret;
	ret = ksceKernelCreateThread("SceDeci4pDfMgrUsbRecv", _recvThread, 0x1e, 0x4000, 0, 8,
				     &opt);
	if (ret < 0)
		return ret;
	s_usbRecvThread = ret;
	ret = ksceKernelStartThread(s_usbSendThread, 0, NULL);
	if (ret < 0)
		return ret;
	ret = ksceKernelStartThread(s_usbRecvThread, 0, NULL);
	if (ret < 0)
		return ret;

	ret = dfmgrSetLinkStatus(link, DFMGR_LINK_READY, 1);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 160, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(161);
		return ret;
	}
	return 0;
}

int usbTerm(DfMgrLinkCB *link)
{
	int ret;

	ret = dfmgrSetLinkStatus(link, DFMGR_LINK_READY, 0);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 182, "0x%08X\n", ret);
	ret = ksceKernelDeleteThread(s_usbRecvThread);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 189, "0x%08X\n", ret);
	ret = ksceKernelDeleteThread(s_usbSendThread);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 196, "0x%08X\n", ret);
	ret = SceDbgUsbForDriver_08CF16D1();
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 203, "0x%08X\n", ret);
	ret = ksceKernelDeleteEventFlag(s_usbRecvEvf);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 210, "0x%08X\n", ret);
	ret = ksceKernelDeleteEventFlag(s_usbSendEvf);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 217, "0x%08X\n", ret);
	return 0;
}
