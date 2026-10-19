/* Robin initialization work; filename inferred, no source-name debug record. */
#include "../../wlanbt/wlanbt_internal.h"
#include <sysmem/sblaimgr.h>
#include <sdif/sdif.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <post_ss_mgr/sblfwloader.h>
int robinResetWlanForResume(void);
int robinDeauthenticateForResume(void);

SceWlanBtRobinSdioStatus gBootstrapStatus;
SceWlanBtRobinBootstrapMessage gBootstrapMessage;
SceWlanBtRobin gRobin;

int robinInitializeWork(void *argument)
{
	if (robinInitializeWlanContext() >= 0)
		robinCompleteInitialization();
	return 1;
}

int robinInitializeSdio(void)
{
	SdifSdioFunctionIdentifier identifier;
	uint64_t value;
	unsigned int low, middle;
	int result;

	result = SceSdifForDriver_AABAA0F0(2, &gRobin.sdioContext);
	if (result < 0)
		return result;
	identifier.manufacturerId = 0x2df;
	identifier.productId = 0x911a;
	identifier.interfaceCode = UINT32_MAX;
	gRobin.bluetoothFunction = SceSdifForDriver_0C66E36F(gRobin.sdioContext,
						      &identifier);
	if (!gRobin.bluetoothFunction)
		return 0x80418007;
	identifier.productId = 0x9119;
	gRobin.wlanFunction = SceSdifForDriver_0C66E36F(gRobin.sdioContext,
							   &identifier);
	if (!gRobin.wlanFunction)
		return 0x80418007;
	result = SceSdifForDriver_BC45C83D(gRobin.sdioContext, 2);
	if (result < 0)
		return result;
	result = SceSdifForDriver_733BC373(gRobin.wlanFunction, 256);
	if (result < 0)
		return result;
	result = SceSdifForDriver_3428884D(gRobin.bluetoothFunction);
	if (result < 0)
		return result;
	result = SceSdifForDriver_3428884D(gRobin.wlanFunction);
	if (result < 0)
		return result;
	result = SceSdifForDriver_D0F78D9B(gRobin.wlanFunction, 92, 1,
					 &gRobin.revision, 1);
	if (result < 0)
		return result;
	if ((unsigned int)gRobin.revision - 48 <= 1)
		gRobin.sdioContext->masterInterruptEnabled = 1;
	else
		gRobin.sdioContext->masterInterruptEnabled = 0;
	result = SceSdifForDriver_D0F78D9B(gRobin.wlanFunction, 120, 1,
					 (uint8_t *)&value, 1);
	if (result < 0)
		return result;
	low = ((uint8_t *)&value)[0];
	result = SceSdifForDriver_D0F78D9B(gRobin.wlanFunction, 121, 1,
					 (uint8_t *)&value, 1);
	if (result < 0)
		return result;
	middle = ((uint8_t *)&value)[0] << 8;
	result = SceSdifForDriver_D0F78D9B(gRobin.wlanFunction, 122, 1,
					 (uint8_t *)&value, 1);
	if (result >= 0)
		gRobin.fifoAddress = low | middle | (((uint8_t *)&value)[0] << 16);
	return result;
}

int robinResetWlanForResume(void)
{
	uint8_t request[4];
	unsigned int retries;
	int result = 0;

	request[0] = 1;
	if (SceSdifForDriver_3C4CDC8B(gRobin.wlanFunction, 2, 1, request, 1) < 0)
		return 0;
	gBootstrapMessage.header.messageType = 1;
	gBootstrapMessage.command.sequence.encoded = 1;
	gBootstrapMessage.command.commandId = 0xaa;
	gBootstrapMessage.command.commandSize = 8;
	gBootstrapMessage.header.messageSize = 256;
	gBootstrapMessage.command.result = 0;
	if (SceSdifForDriver_3C4CDC8B(gRobin.wlanFunction, gRobin.fifoAddress,
				     sizeof(gBootstrapMessage),
				     (uint8_t *)&gBootstrapMessage, 2) < 0)
		return 0;
	retries = 100;
	while (SceSdifForDriver_D0F78D9B(gRobin.wlanFunction, 0,
				     sizeof(gBootstrapStatus),
				     (uint8_t *)&gBootstrapStatus, 2) >= 0) {
		if (gBootstrapStatus.interruptStatus & 1) {
			if (!(gBootstrapStatus.readBitmap[0] & 1))
				break;
			if (gBootstrapStatus.readLengths[0][0] |
			    (gBootstrapStatus.readLengths[0][1] << 8)) {
				if (SceSdifForDriver_D0F78D9B(gRobin.wlanFunction,
						     gRobin.fifoAddress,
						     sizeof(gBootstrapMessage),
						     (uint8_t *)&gBootstrapMessage, 2) < 0)
					break;
				if (gBootstrapMessage.header.messageType == 1 &&
				    gBootstrapMessage.command.commandId == 0x80aa)
					result = 1;
			}
		} else
			sceKernelDelayThread(10000);
		if (!--retries)
			break;
	}
	return result;
}

int robinDeauthenticateForResume(void)
{
	uint8_t request[4];
	unsigned int retries;
	int result = 0;

	request[0] = 1;
	if (SceSdifForDriver_3C4CDC8B(gRobin.wlanFunction, 2, 1, request, 1) < 0)
		return 0;
	gBootstrapMessage.header.messageType = 1;
	gBootstrapMessage.command.sequence.encoded = 1;
	gBootstrapMessage.command.commandId = 0x24;
	gBootstrapMessage.command.commandSize = 16;
	gBootstrapMessage.header.messageSize = 256;
	gBootstrapMessage.command.result = 0;
	robinWlanGetBssid(gBootstrapMessage.payload.deauthentication.bssid);
	gBootstrapMessage.payload.deauthentication.reason = 3;
	if (SceSdifForDriver_3C4CDC8B(gRobin.wlanFunction, gRobin.fifoAddress,
				     sizeof(gBootstrapMessage),
				     (uint8_t *)&gBootstrapMessage, 2) < 0)
		return 0;
	retries = 100;
	while (SceSdifForDriver_D0F78D9B(gRobin.wlanFunction, 0,
				     sizeof(gBootstrapStatus),
				     (uint8_t *)&gBootstrapStatus, 2) >= 0) {
		if (gBootstrapStatus.interruptStatus & 1) {
			if ((gBootstrapStatus.readBitmap[0] |
			     (gBootstrapStatus.readBitmap[1] << 8)) & ~1u)
				return 0;
			if (!(gBootstrapStatus.readBitmap[0] & 1))
				break;
			if (gBootstrapStatus.readLengths[0][0] |
			    (gBootstrapStatus.readLengths[0][1] << 8)) {
				if (SceSdifForDriver_D0F78D9B(gRobin.wlanFunction,
						     gRobin.fifoAddress,
						     sizeof(gBootstrapMessage),
						     (uint8_t *)&gBootstrapMessage, 2) < 0)
					break;
				if (gBootstrapMessage.header.messageType == 1 &&
				    gBootstrapMessage.command.commandId == 0x8024)
					result = 1;
			}
		} else
			sceKernelDelayThread(10000);
		if (!--retries)
			break;
	}
	return result;
}

int robinTransferFirmware(const void *buffer, unsigned int size)
{
	unsigned int retries, repeated = 0, transferSize = 0, written = 0;
	unsigned int available, chunk;
	struct SdifSdioFunction *function;
	uint8_t status[8], requestSize[4];
	int result;

	for (;;) {
		function = gRobin.wlanFunction;
		retries = 100;
		for (;;) {
			result = SceSdifForDriver_D0F78D9B(function, 48, 1,
						 status, 1);
			if (result < 0)
				return 0x80418004;
			if ((status[0] & 9) == 9)
				break;
			sceKernelDelayThread(10);
			if (!--retries)
				return 0x80418004;
		}
		if (written >= size)
			return 0;
		retries = 100;
		for (;;) {
			result = SceSdifForDriver_D0F78D9B(gRobin.wlanFunction,
						 64, 2, requestSize, 1);
			if (result < 0)
				return result;
			available = requestSize[0] | (requestSize[1] << 8);
			if (available)
				break;
			sceKernelDelayThread(10);
			if (!--retries)
				goto nextChunk;
		}
		if (available > 2048)
			return 0x80418004;
		if (available & 1) {
			if (++repeated > 2)
				return 0x80418004;
			chunk = 0;
		} else {
nextChunk:
			repeated = 0;
			chunk = size - written;
			if (available < chunk)
				chunk = available;
			transferSize = (chunk + 255) & ~255u;
		}
		result = SceSdifForDriver_3C4CDC8B(gRobin.wlanFunction,
						gRobin.fifoAddress, transferSize,
						(uint8_t *)buffer + written, 2);
		if (result < 0)
			return result;
		written += chunk;
	}
}

int robinInitialize(void)
{
	SceKernelAllocMemBlockKernelOpt options;
	int result;

	gRobin = (SceWlanBtRobin){0};
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	result = sceKernelAllocMemBlockForKernel("SceWlanBtRobin",
					SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW,
					0x80000, &options);
	if (result >= 0) {
		gRobin.firmwareMemBlock = result;
		result = sceKernelGetMemBlockBase(result, &gRobin.firmwareBuffer);
		if (result >= 0) {
			sceKernelScheduleWorkTask(0x10023, "SceWlanBtRobinInit",
						 robinInitializeWork, NULL);
			return 0;
		}
	}
	if (gRobin.firmwareMemBlock > 0)
		SceSysmemForDriver_009E1C61(gRobin.firmwareMemBlock);
	return result;
}

int robinGetCardRevision(void)
{
	return gRobin.revision;
}

int wlanBtStartRobin(void)
{
	unsigned int attempts = 0, retries;
	SdifSdioFunction *function;
	uint8_t status[2];
	int result;

	wlanBtPowerOnDevice();
	for (;;) {
		gRobin.sdioContext = NULL;
		gRobin.revision = 0;
		gRobin.bluetoothFunction = NULL;
		gRobin.wlanFunction = NULL;
		gRobin.fifoAddress = 0;
		result = robinInitializeSdio();
		if (result < 0)
			goto releaseFunctions;
		if (SceSdifForDriver_D0F78D9B(gRobin.wlanFunction, 96, 2,
					     status, 1) >= 0 &&
		    (int16_t)(status[0] | (status[1] << 8)) == -292) {
			result = 0;
			goto releaseFunctions;
		}
		switch (gRobin.revision) {
		case '0':
		case '1':
			result = 0x80418007;
			SceDebugForDriver_391B74B7("[SceWlanBt]: Robin W0/W1 : NOT SUPPORTED.\n");
			goto releaseFunctions;
		case '@':
		case 'P':
			break;
		default:
			SceDebugForDriver_391B74B7("Unknown card rev : 0x%02x\n", gRobin.revision);
			break;
		}
		if (!gRobin.firmwareLoaded) {
			result = sceSblFwLoaderSetPath("os0:kd/wlanbt_robin_img_ax.skprx", 0);
			if (result < 0)
				goto releaseFunctions;
			result = sceSblFwLoaderLoad(1, gRobin.firmwareBuffer, 0x80000,
						     &gRobin.firmwareSize);
			if (result < 0) {
				sceSblFwLoaderClose();
				goto releaseFunctions;
			}
			result = sceSblFwLoaderClose();
			if (result < 0)
				goto releaseFunctions;
			gRobin.firmwareLoaded = 1;
			result = SceSysmemForDriver_DFE2C8CB(gRobin.firmwareMemBlock,
							SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_R);
			if (result < 0)
				goto releaseFunctions;
		}
		result = robinTransferFirmware(gRobin.firmwareBuffer, gRobin.firmwareSize);
		if (result < 0)
			goto releaseFunctions;
		function = gRobin.wlanFunction;
		retries = 100;
		while (SceSdifForDriver_D0F78D9B(function, 96, 2, status, 1) < 0 ||
		       (int16_t)(status[0] | (status[1] << 8)) != -292) {
			sceKernelDelayThread(10000);
			if (!--retries)
				goto timedOut;
		}
		function = gRobin.bluetoothFunction;
		retries = 100;
		while (SceSdifForDriver_D0F78D9B(function, 96, 2, status, 1) < 0 ||
		       (int16_t)(status[0] | (status[1] << 8)) != -292) {
			sceKernelDelayThread(10000);
			if (!--retries)
				goto timedOut;
		}
		result = 0;
		goto releaseFunctions;
timedOut:
		result = 0x80418004;
releaseFunctions:
		if (gRobin.wlanFunction) {
			SceSdifForDriver_E80293EF(gRobin.wlanFunction);
			gRobin.wlanFunction = NULL;
		}
		if (gRobin.bluetoothFunction) {
			SceSdifForDriver_E80293EF(gRobin.bluetoothFunction);
			gRobin.bluetoothFunction = NULL;
		}
		if (!result)
			break;
		++attempts;
		wlanBtPowerDownDevice();
		wlanBtPowerOnDevice();
		if (attempts == 20) {
			SceDebugForDriver_391B74B7("[SceWlanBt]: Reset retry (%d) : NG\n", attempts);
			wlanBtPowerDownDevice();
			goto finished;
		}
	}
	if (attempts)
		SceDebugForDriver_391B74B7("[SceWlanBt]: Reset retry (%d) : OK\n", attempts);
finished:
	return result;
}

int wlanBtStopRobin(void)
{
	wlanBtPowerDownDevice();
	return 0;
}

int wlanBtResumeRobin(unsigned int *wakeSources)
{
	SdifSdioFunction *function;
	uint8_t wakeStatus, status[2];
	unsigned int retries;
	int result;

	gRobin.sdioContext = NULL;
	gRobin.revision = 0;
	gRobin.bluetoothFunction = NULL;
	gRobin.wlanFunction = NULL;
	gRobin.fifoAddress = 0;
	result = robinInitializeSdio();
	if (result < 0)
		goto checkWakeSources;
	if (SceSdifForDriver_D0F78D9B(gRobin.wlanFunction, 96, 2, status, 1) >= 0 &&
	    (int16_t)(status[0] | (status[1] << 8)) == -292) {
		result = 0;
		goto checkWakeSources;
	}
	switch (gRobin.revision) {
	case '0':
	case '1':
		result = 0x80418007;
		SceDebugForDriver_391B74B7("[SceWlanBt]: Robin W0/W1 : NOT SUPPORTED.\n");
		goto checkWakeSources;
	case '@':
	case 'P':
		break;
	default:
		SceDebugForDriver_391B74B7("Unknown card rev : 0x%02x\n", gRobin.revision);
		break;
	}
	if (!gRobin.firmwareLoaded) {
		result = sceSblFwLoaderSetPath("os0:kd/wlanbt_robin_img_ax.skprx", 0);
		if (result < 0)
			goto checkWakeSources;
		result = sceSblFwLoaderLoad(1, gRobin.firmwareBuffer, 0x80000,
					     &gRobin.firmwareSize);
		if (result < 0) {
			sceSblFwLoaderClose();
			goto checkWakeSources;
		}
		result = sceSblFwLoaderClose();
		if (result < 0)
			goto checkWakeSources;
		gRobin.firmwareLoaded = 1;
		result = SceSysmemForDriver_DFE2C8CB(gRobin.firmwareMemBlock,
						SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_R);
		if (result < 0)
			goto checkWakeSources;
	}
	result = robinTransferFirmware(gRobin.firmwareBuffer, gRobin.firmwareSize);
	if (result < 0)
		goto checkWakeSources;
	function = gRobin.wlanFunction;
	retries = 100;
	while (SceSdifForDriver_D0F78D9B(function, 96, 2, status, 1) < 0 ||
	       (int16_t)(status[0] | (status[1] << 8)) != -292) {
		sceKernelDelayThread(10000);
		if (!--retries)
			goto timedOut;
	}
	function = gRobin.bluetoothFunction;
	retries = 100;
	while (SceSdifForDriver_D0F78D9B(function, 96, 2, status, 1) < 0 ||
	       (int16_t)(status[0] | (status[1] << 8)) != -292) {
		sceKernelDelayThread(10000);
		if (!--retries)
			goto timedOut;
	}
	result = 0;
	goto checkWakeSources;
timedOut:
	result = 0x80418004;
checkWakeSources:
	if (wakeSources) {
		if (*wakeSources == 3) {
			*wakeSources = 0;
			if (SceSdifForDriver_D0F78D9B(gRobin.wlanFunction, 106, 1,
						     &wakeStatus, 1) >= 0 && wakeStatus == 1)
				*wakeSources |= 1;
			if (SceSdifForDriver_D0F78D9B(gRobin.bluetoothFunction, 106, 1,
						     &wakeStatus, 1) >= 0 && wakeStatus == 1)
				*wakeSources |= 2;
		}
		if (SceSblAIMgrForDriver_71608CA3()) {
			if (robinWlanNeedsDeauthentication()) {
				if (!robinDeauthenticateForResume()) {
					SceDebugForDriver_391B74B7("[SceWlanBt]: Reset to continue resume. (deauth failure)\n");
					*wakeSources = 0;
				}
			} else if (*wakeSources == 2 && !robinResetWlanForResume()) {
				SceDebugForDriver_391B74B7("[SceWlanBt]: Reset to continue resume.\n");
				*wakeSources = 0;
			}
		}
	}
	if (gRobin.wlanFunction) {
		SceSdifForDriver_E80293EF(gRobin.wlanFunction);
		gRobin.wlanFunction = NULL;
	}
	if (gRobin.bluetoothFunction) {
		SceSdifForDriver_E80293EF(gRobin.bluetoothFunction);
		gRobin.bluetoothFunction = NULL;
	}
	return result;
}
