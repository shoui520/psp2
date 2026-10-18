/*
 * Link 0: DECI4P over the DevKit SDIO channel of SceDbgSdio (Sony's sdio.c).
 */

#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include <dbgsdio/dbgsdio.h>
#include "dfmgr_internal.h"

#define SDIO_CHANNEL	0

static const DfMgrLinkInfo s_sdioLinkInfo = { "SDIO", 0 };

static DfMgrLinkCB *s_sdioLink;
static DfMgrLinkCallback s_sdioSendCallback;
static DfMgrLinkCallback s_sdioRecvCallback;
static SceUID s_sdioSendThread;
static SceUID s_sdioRecvThread;
static SceUID s_sdioSendEvf;
static SceUID s_sdioRecvEvf;		/* bit 0: data arrived (set by SceDbgSdio) */

static int sdioRecv(void *buf, SceSize size)
{
	SceSize done = 0;
	int ret;

	while (done < size) {
		ret = receiveData(SDIO_CHANNEL, (SceUInt8 *)buf + done, size - done);
		done += ret;
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(15, 263, "0x%08X\n", ret);
			return ret;
		}
	}
	return size;
}

static int sdioSend(void *buf, SceSize size)
{
	SceSize done = 0;
	int ret;

	while (done < size) {
		ret = sceDbgSdioWrite(SDIO_CHANNEL, (SceUInt8 *)buf + done, size - done);
		done += ret;
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(15, 245, "0x%08X\n", ret);
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
		ret = sceKernelWaitEventFlag(s_sdioRecvEvf, 1,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					      &bits, NULL);
		if (ret < 0)
			break;
		if (bits & 1)
			s_sdioRecvCallback(s_sdioLink);
		else
			SCE_PRINTF_WITH_INFO(15, 355, "0x%08X\n", bits);
	}
	SCE_PRINTF_WITH_INFO(15, 345, "0x%08X\n", ret);
	SCE_DBG_ASSERT_FAIL(346);
	return ret;
}

static int _sendThread(SceSize args, void *argp)
{
	SceUInt32 bits;
	int ret;

	for (;;) {
		bits = 0;
		ret = sceKernelWaitEventFlag(s_sdioSendEvf, 1,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					      &bits, NULL);
		if (ret < 0)
			break;
		if (bits & 1)
			s_sdioSendCallback(s_sdioLink);
		else
			SCE_PRINTF_WITH_INFO(15, 315, "0x%08X\n", bits);
	}
	SCE_PRINTF_WITH_INFO(15, 305, "0x%08X\n", ret);
	SCE_DBG_ASSERT_FAIL(306);
	return ret;
}

int sdioInit(DfMgrLinkCB *link, DfMgrLinkCallback sendCallback, DfMgrLinkCallback recvCallback,
	     SceUID *pSendEvf)
{
	SceKernelThreadOptParam opt;
	int ret;

	s_sdioRecvCallback = recvCallback;
	link->info = &s_sdioLinkInfo;
	link->send = sdioSend;
	link->recv = sdioRecv;
	opt.kStackMemType = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW;
	opt.size = sizeof(opt);
	opt.attr = 1;
	s_sdioSendCallback = sendCallback;
	s_sdioLink = link;

	ret = sceKernelCreateEventFlag("SceDeci4pDfMgrSdioSend", SCE_KERNEL_EVF_ATTR_MULTI, 0,
					NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 89);
		return ret;
	}
	*pSendEvf = ret;
	s_sdioSendEvf = ret;
	ret = sceKernelCreateEventFlag("SceDeci4pDfMgrSdioRecv", SCE_KERNEL_EVF_ATTR_MULTI, 0,
					NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 103);
		return ret;
	}
	s_sdioRecvEvf = ret;
	ret = SceDbgSdioForDriver_3D3FAF3A(SDIO_CHANNEL, s_sdioRecvEvf);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 111, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(112);
		return ret;
	}

	ret = sceKernelCreateThread("SceDeci4pDfMgrSdioSend", _sendThread, 0x1e, 0x4000, 0, 0xf,
				     &opt);
	if (ret < 0)
		return ret;
	s_sdioSendThread = ret;
	ret = sceKernelCreateThread("SceDeci4pDfMgrSdioRecv", _recvThread, 0x1e, 0x4000, 0, 0xf,
				     &opt);
	if (ret < 0)
		return ret;
	s_sdioRecvThread = ret;
	ret = sceKernelStartThread(s_sdioSendThread, 0, NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelStartThread(s_sdioRecvThread, 0, NULL);
	if (ret < 0)
		return ret;

	ret = dfmgrSetLinkStatus(link, DFMGR_LINK_READY, 1);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 159, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(160);
		return ret;
	}
	return 0;
}

int sdioTerm(DfMgrLinkCB *link)
{
	int ret;

	ret = dfmgrSetLinkStatus(link, DFMGR_LINK_READY, 0);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 181, "0x%08X\n", ret);
	ret = sceKernelDeleteThread(s_sdioRecvThread);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 188, "0x%08X\n", ret);
	ret = sceKernelDeleteThread(s_sdioSendThread);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 195, "0x%08X\n", ret);
	ret = SceDbgSdioForDriver_361E1BB6(SDIO_CHANNEL);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 202, "0x%08X\n", ret);
	ret = sceKernelDeleteEventFlag(s_sdioRecvEvf);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 209, "0x%08X\n", ret);
	ret = sceKernelDeleteEventFlag(s_sdioSendEvf);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 216, "0x%08X\n", ret);
	return 0;
}
