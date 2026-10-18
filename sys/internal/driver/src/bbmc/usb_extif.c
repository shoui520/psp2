/* USB external-interface transfers and completion. */
#include <threadmgr/threadmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <stdint.h>
#include <udcd/udcd.h>
#include <usbserv/usbserv.h>
#include "usb_serial.h"
#include "usb_extif.h"

int SceBbmcForDriver_D98DB926(void);
int SceBbmcForDriver_D16D0845(void);

static const SceUInt32 g_usbExtIfBus = 2;
static BbmcUsbExtIfControl g_usbExtIfStorage = {
	.mode = -1,
};
static BbmcUsbExtIfControl *g_usbExtIf;

/* name unknown: publish the transfer result before waking its waiting caller. */
int bbmcUsbExtIfTransferCompleted(int forwardedValue, BbmcUsbSerialRequest *request,
	int resultOrCount)
{
	BbmcUsbExtIfCompletion *completion = request->context;

	completion->result = resultOrCount;
	sceKernelSignalSema(completion->semaphore, 1);
	return 0;
}

/* name unknown: initialize the serial backend before publishing the frontend. */
int bbmcInitializeUsbExtIf(void)
{
	int ret;

	g_usbExtIfStorage.lock = 0;
	ret = bbmcInitializeUsbSerial(sizeof(g_usbExtIfBus), &g_usbExtIfBus);
	if (ret >= 0)
		g_usbExtIf = &g_usbExtIfStorage;
	return ret;
}

/* name unknown: tear down the serial backend and unpublish the frontend. */
int bbmcFinalizeUsbExtIf(void)
{
	bbmcFinalizeUsbSerial();
	g_usbExtIf = NULL;
	return 0;
}

/* name unknown: close bridge channels before releasing the GPIO user. */
int bbmcUsbExtIfReleaseGpio(void)
{
	int ret;

	if (g_usbExtIf == NULL)
		return (int)0x80330003;
	SceCpuForDriver_BF82DEB2(&g_usbExtIf->lock);
	if (g_usbExtIf->mode == 0)
		bbmcCloseUsbSerialChannels();
	ret = SceBbmcForDriver_D16D0845();
	sceKernelSpinlockLowUnlock(&g_usbExtIf->lock);
	return ret;
}

/* name unknown: acquire the GPIO user and wake the selected serial backend. */
int bbmcUsbExtIfAcquireGpio(void)
{
	int ret;

	if (g_usbExtIf == NULL)
		return (int)0x80330003;
	SceCpuForDriver_BF82DEB2(&g_usbExtIf->lock);
	ret = SceBbmcForDriver_D98DB926();
	if (ret >= 0) {
		if (g_usbExtIf->mode == 1)
			goto out;
		if (g_usbExtIf->mode != 0) {
			ret = (int)0x80330002;
			goto out;
		}
		bbmcWakeUsbSerial();
	}
out:
	sceKernelSpinlockLowUnlock(&g_usbExtIf->lock);
	return ret;
}

/* name unknown: return the frontend's active transport mode. */
int bbmcUsbExtIfGetMode(int *mode)
{
	if (g_usbExtIf == NULL)
		return (int)0x80330003;
	if (mode == NULL)
		return (int)0x80330002;
	*mode = g_usbExtIf->mode;
	return 0;
}

int sceBbmcUsbExtIfStart(int mode)
{
	int ret, controllerStarted;
	unsigned int failed;

	if (g_usbExtIf == NULL)
		return (int)0x80330003;
	SceCpuForDriver_BF82DEB2(&g_usbExtIf->lock);
	ret = SceBbmcForDriver_D98DB926();
	if (ret < 0)
		goto fail;
	if (mode == 0) {
		ret = bbmcWaitUsbSerialHost(1, 1, NULL);
		if (ret < 0)
			goto fail;
		ret = bbmcWaitUsbSerialHost(4, 1, NULL);
		if (ret < 0)
			goto fail;
		ret = bbmcOpenUsbSerialQcom(&g_usbExtIf->qcomChannel);
		if (ret < 0)
			goto fail;
		ret = bbmcOpenUsbSerialAt(&g_usbExtIf->atChannel);
		if (ret < 0)
			goto fail;
		g_usbExtIf->mode = 1;
		goto out;
	}
	if (mode != 1) {
		ret = (int)0x80330002;
		goto fail;
	}
	if (SceUdcdForDriver_FDC10F93(2) < 0) {
		g_usbExtIf->restoreMtp = 0;
	} else {
		g_usbExtIf->restoreMtp = 1;
		ret = SceUdcdForDriver_150FD3BB("USB_MTP_Driver", 0, NULL, 2);
		if (ret < 0)
			goto fail;
	}
	ret = SceUsbServForDriver_7AD36284(2, 1);
	failed = (unsigned int)ret >> 31;
	if (ret == (int)0x80248001)
		failed = 0;
	if (failed != 0)
		goto fail;
	ret = SceUdcdForDriver_90F8BCAE("USBDeviceControllerDriver", 0, NULL, 2);
	if (ret < 0) {
		if (ret != (int)0x80243002)
			goto fail;
		controllerStarted = 0;
		g_usbExtIf->controllerStarted = 0;
	} else {
		controllerStarted = 1;
		g_usbExtIf->controllerStarted = 1;
	}
	ret = SceUdcdForDriver_90F8BCAE("SceUsbQcomSer", 0, NULL, 2);
	if (ret >= 0) {
		ret = SceUdcdForDriver_9119247B(1430, 0, 2);
		if (ret >= 0) {
			g_usbExtIf->mode = 0;
			goto out;
		}
		SceUdcdForDriver_150FD3BB("SceUsbQcomSer", 0, NULL, 2);
	}
	if (controllerStarted != 0)
		SceUdcdForDriver_150FD3BB("SceUsbQcomSer", 0, NULL, 2);
	goto fail;
out:
	sceKernelSpinlockLowUnlock(&g_usbExtIf->lock);
	return ret;
fail:
	if (g_usbExtIf->qcomChannel > 0) {
		bbmcCloseUsbSerialQcom(g_usbExtIf->qcomChannel);
		g_usbExtIf->qcomChannel = 0;
	}
	if (g_usbExtIf->atChannel > 0) {
		bbmcCloseUsbSerialAt(g_usbExtIf->atChannel);
		g_usbExtIf->atChannel = 0;
	}
	sceKernelSpinlockLowUnlock(&g_usbExtIf->lock);
	return ret;
}

int sceBbmcUsbExtIfStop(void)
{
	int ret;

	if (g_usbExtIf == NULL)
		return (int)0x80330003;
	SceCpuForDriver_BF82DEB2(&g_usbExtIf->lock);
	if (g_usbExtIf->mode != 0) {
		if (g_usbExtIf->mode == 1) {
			bbmcCloseUsbSerialQcom(g_usbExtIf->qcomChannel);
			g_usbExtIf->qcomChannel = 0;
			bbmcCloseUsbSerialAt(g_usbExtIf->atChannel);
			g_usbExtIf->atChannel = 0;
		}
	} else {
		SceUdcdForDriver_FDC10F93(2);
		SceUdcdForDriver_150FD3BB("SceUsbQcomSer", 0, NULL, 2);
		if (g_usbExtIf->controllerStarted != 0) {
			SceUdcdForDriver_150FD3BB("USBDeviceControllerDriver", 0, NULL, 2);
			g_usbExtIf->controllerStarted = 0;
		}
		if (g_usbExtIf->restoreMtp != 0) {
			SceUdcdForDriver_90F8BCAE("USB_MTP_Driver", 0, NULL, 2);
			sceUdcdActivate(1252);
			g_usbExtIf->restoreMtp = 0;
		}
	}
	ret = SceBbmcForDriver_D16D0845();
	g_usbExtIf->mode = -1;
	sceKernelSpinlockLowUnlock(&g_usbExtIf->lock);
	return ret;
}

int sceBbmcUsbExtIfRead(unsigned int channel, void *data, SceSize length)
{
	BbmcUsbExtIfCompletion completion;
	BbmcUsbSerialRequest request;
	int ret;
	void *allocation, *buffer;

	if (data == NULL || length > 1024)
		return (int)0x80330002;
	const char *names[2] = {
		"SceBbmcUsbExtIfQcomRead", "SceBbmcUsbExtIfATRead",
	};
	allocation = __builtin_alloca(length + 64);
	buffer = (void *)(((uintptr_t)allocation + 64) & ~(uintptr_t)63);
	if (g_usbExtIf == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	completion.semaphore = 0;
	completion.result = -1;
	ret = sceKernelCreateSema(names[channel], 0, 0, 1, NULL);
	if (ret < 0)
		return ret;
	completion.semaphore = ret;
	if (channel != 0) {
		if (g_usbExtIf->atChannel < 0)
			goto wait;
		request.transferType = 1;
		request.arguments.bulk.length = length;
		request.arguments.bulk.data = buffer;
		request.completed = bbmcUsbExtIfTransferCompleted;
		request.context = &completion;
		ret = bbmcReadUsbSerial(g_usbExtIf->atChannel, &request);
	} else {
		if (g_usbExtIf->qcomChannel < 0)
			goto wait;
		request.completed = bbmcUsbExtIfTransferCompleted;
		request.arguments.bulk.length = length;
		request.arguments.bulk.data = buffer;
		request.transferType = 1;
		request.context = &completion;
		ret = bbmcReadUsbSerial(g_usbExtIf->qcomChannel, &request);
	}
	if (ret < 0)
		goto finished;
wait:
	ret = sceKernelWaitSema(completion.semaphore, 1, NULL);
	if (ret >= 0)
		ret = completion.result;
finished:
	if (completion.semaphore > 0)
		sceKernelDeleteSema(completion.semaphore);
	if (ret > 0) {
		int copied = SceSysmemForDriver_6D88EF8A(data, buffer, ret);

		if (copied < 0)
			return copied;
	}
	return ret;
}

int sceBbmcUsbExtIfWrite(unsigned int channel, const void *data, SceSize length)
{
	BbmcUsbExtIfCompletion completion;
	BbmcUsbSerialRequest request;
	void *allocation, *buffer;
	int ret;

	if (data == NULL || length > 1024)
		return (int)0x80330002;
	allocation = __builtin_alloca(length + 64);
	buffer = (void *)(((uintptr_t)allocation + 64) & ~(uintptr_t)63);
	ret = SceSysmemForDriver_BC996A7A(buffer, data, length);
	if (ret < 0)
		return ret;
	const char *names[2] = {
		"SceBbmcUsbExtIfQcomWrite", "SceBbmcUsbExtIfATWrite",
	};
	if (g_usbExtIf == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	completion.semaphore = 0;
	completion.result = -1;
	ret = sceKernelCreateSema(names[channel], 0, 0, 1, NULL);
	if (ret < 0)
		return ret;
	completion.semaphore = ret;
	if (channel != 0) {
		if (g_usbExtIf->atChannel < 0)
			goto wait;
		request.transferType = 1;
		request.arguments.bulk.length = length;
		request.arguments.bulk.data = buffer;
		request.completed = bbmcUsbExtIfTransferCompleted;
		request.context = &completion;
		ret = bbmcWriteUsbSerial(g_usbExtIf->atChannel, &request);
	} else {
		if (g_usbExtIf->qcomChannel < 0)
			goto wait;
		request.completed = bbmcUsbExtIfTransferCompleted;
		request.arguments.bulk.length = length;
		request.arguments.bulk.data = buffer;
		request.transferType = 1;
		request.context = &completion;
		ret = bbmcWriteUsbSerial(g_usbExtIf->qcomChannel, &request);
	}
	if (ret < 0)
		goto finished;
wait:
	ret = sceKernelWaitSema(completion.semaphore, 1, NULL);
	if (ret >= 0)
		ret = completion.result;
finished:
	if (completion.semaphore > 0)
		sceKernelDeleteSema(completion.semaphore);
	return ret;
}
