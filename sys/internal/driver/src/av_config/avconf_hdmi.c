/* HDMI state and configuration. */

#include "av_config_internal.h"
#include <sysmem/debug.h>
#include <hdmi/hdmi.h>
#include <display/display.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/cpu.h>
#include <string.h>
#include <regmgr/regmgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/sysroot.h>

static int avConfigHdmiLastVic = -1;
static int avConfigHdmiInitialOutput = 1;
int avConfigHdmiAudioMode = 1;
static SceKernelSpinlock avConfigHdmiMonitorLock = 0;
#if FW_VERSION >= 0x04000000
static int avConfigHdmiMonitorValid = 0;
static SceHdmiEdidInfo avConfigHdmiMonitor = {0};
#else
static SceHdmiEdidInfo avConfigHdmiMonitor = {0};
static int avConfigHdmiMonitorValid = 0;
#endif
int avConfigHdmiPowered = 0;
int avConfigHdmiAvailable = 0;
static const char avConfigHdmiToolCategory[] = "/DEVENV/TOOL";
static const char avConfigHdmiVideoModeKey[] = "hdmi_video_mode";
static const char avConfigHdmiColorRangeKey[] = "hdmi_color_range";
static const char avConfigDisplayCategory[] = "/CONFIG/DISPLAY";
static const char avConfigHdmiResolutionModeKey[] = "hdmi_resolution_mode";
static const char avConfigRgbRangeModeKey[] = "rgb_range_mode";
static const char avConfigHdmiScalingRatioKey[] = "hdmi_out_scaling_ratio";
static const char avConfigCecSystemCategory[] = "/CONFIG/SYSTEM";
static const char avConfigCecEnabledKey[] = "hdmi_use_cec";

void sceAVConfigHdmiHdcpDoneIntr(void)
{
	const void *lr = __builtin_return_address(0);
	int result;

	result = SceHdmiForDriver_99A3CABE(0);
	if (result < 0)
		SCE_DBG_ASSERT_CHECK_LR(0, 165, lr);
	result = SceHdmiForDriver_08AFFB03(0);
	if (result < 0)
		SCE_DBG_ASSERT_CHECK_LR(0, 171, lr);
	avConfigConfiguration.settings.hdcpState = 3;
	result = avConfigSetEventFlag(0x400);
	if (avConfigHdmiInitialOutput != 0)
		avConfigHdmiInitialOutput = 0;
}

int avConfigInitializeHdmiOutput(void)
{
	SceHdmiAudioMode audio;
	SceKernelIntrStatus intr;
	int videoMode, colorRange, fullRange;
	unsigned int vic, flags;
	float scale;
	int retries, result;

	retries = 5;
	while (sceRegMgrGetKeyInt(avConfigHdmiToolCategory,
			avConfigHdmiVideoModeKey, &videoMode) != 0) {
		sceKernelDelayThread(40000);
		if (--retries == 0) {
			videoMode = 32;
			break;
		}
	}
	retries = 5;
	while (sceRegMgrGetKeyInt(avConfigHdmiToolCategory,
			avConfigHdmiColorRangeKey, &colorRange) != 0) {
		sceKernelDelayThread(40000);
		if (--retries == 0) {
			colorRange = 0;
			break;
		}
	}
	fullRange = colorRange == 0;
	switch (videoMode) {
	case 8:
		vic = 0x8300;
		flags = 1;
		scale = 1.0f;
		break;
	case 0x10020:
		vic = 0x8600;
		flags = 0x80;
		scale = 0.0f;
		break;
	case 0x20020:
		vic = 0x8600;
		flags = 1;
		scale = 0.9f;
		break;
	default:
		vic = 0x8600;
		flags = videoMode == 0x40020 ? 0x40 : 1;
		scale = videoMode == 0x40020 ? 0.0f : 1.0f;
		break;
	}
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	if (avConfigConfiguration.settings.hdcpState == 1)
		avConfigConfiguration.settings.hdcpState = 2;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	SceDisplayForDriver_AF5EE5BE(1, vic, fullRange);
	SceDisplayForDriver_EB390A76(scale, 1, 0, flags);
	SceDisplayForDriver_EB390A76(scale, 1, 1, flags);
	SceDisplayForDriver_496032D6(1);
	audio.source = 1;
	audio.coding = 1;
	audio.channelCount = 2;
	audio.spdifLayout = 2;
	audio.sampleRate = 3;
	audio.reserved = 0;
	audio.speakerAllocation = 0;
	audio.downmixInhibit = 0;
	audio.levelShift = 0;
	result = SceHdmiForDriver_854EEB78(&audio);
	if (result < 0)
		SCE_DBG_ASSERT_CHECK_FUNC(0, 68, "_hdmiAudioConf");
	return 0;
}

int avConfigHdmiDisableHead(void)
{
	return SceDisplayForDriver_43347565(1);
}

int avConfigHdmiWaitHdcp(int *state)
{
	int attempts = 25;

	for (;;) {
		*state = avConfigConfiguration.settings.hdcpState;
		if (*state != 2)
			break;
		sceKernelDelayThread(40000);
		if (--attempts == 0)
			break;
	}
	if (*state != 3)
		return -1;
	*state = 4;
	avConfigConfiguration.settings.hdcpState = 4;
	return 0;
}

int avConfigHdmiUpdateMonitor(void)
{
	SceHdmiEdidInfo info;
	SceKernelIntrStatus intr;
	int result;

	result = SceHdmiForDriver_8EB0FD9C(&info);
	if (result == 0) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		if (avConfigGetHdmiAudioMode() != 0)
			avConfigConfiguration.settings.connectedDevices |= 8;
		avConfigConfiguration.settings.hdcpState = 1;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
		result = avConfigSetEventFlag(0x100);
	}
	return result;
}

int hdmiSetOutput(int vic, unsigned int rangeMode, int flags)
{
	SceHdmiEdidInfo monitor;
	SceDisplayResolutionInfo resolution;
	SceHdmiAudioMode audio;
	SceKernelIntrStatus intr;
	int range;
	int force, valid, fullRange, outputVic, result;
	const void *lr = __builtin_return_address(0);

	if (avConfigIsHdmiAvailable() == 0)
		return (int)0x803a0107;
	force = flags & 1;
	SceKernelSuspendForDriver_4DF40893(0);
	if (flags < 0 || (flags & 0x40000000) != 0) {
		switch (vic) {
		case 0x8300:
		case 0x8480:
		case 0x8500:
		case 0x8580:
		case 0x8600:
		case 0x8680:
		case 0x8710:
		case 0x8730:
		case 0x10000000:
			break;
		default:
			SceKernelSuspendForDriver_2BB92967(0);
			SCE_DBG_ASSERT_CHECK_LR(0, 384, lr);
			return (int)0x803a0101;
		}
		if (rangeMode != 3) {
			SceKernelSuspendForDriver_2BB92967(0);
			SCE_DBG_ASSERT_CHECK_LR(0, 391, lr);
			return (int)0x803a0101;
		}
		if (flags < 0) {
			if (avConfigHdmiLastVic != -1)
				vic = avConfigHdmiLastVic;
			if (avConfigHdmiInitialOutput == 0)
				force = 1;
		} else if ((flags & 0x40000000) != 0) {
			if (avConfigHdmiLastVic == vic)
				goto unlock;
			if (avConfigConfiguration.settings.hdcpState == 0) {
				avConfigHdmiLastVic = vic;
				goto unlock;
			}
		} else if ((flags & 0x20000000) != 0) {
			vic = avConfigHdmiLastVic;
			if (vic == -1)
				goto unlock;
		}
	} else if ((flags & 0x20000000) != 0) {
		if (vic != -1) {
			SceKernelSuspendForDriver_2BB92967(0);
			SCE_DBG_ASSERT_CHECK_LR(0, 400, lr);
			return (int)0x803a0101;
		}
		if (rangeMode > 2) {
			SceKernelSuspendForDriver_2BB92967(0);
			SCE_DBG_ASSERT_CHECK_LR(0, 409, lr);
			return (int)0x803a0101;
		}
		vic = avConfigHdmiLastVic;
		if (vic == -1)
			goto unlock;
	} else {
		SceKernelSuspendForDriver_2BB92967(0);
		SCE_DBG_ASSERT_CHECK_LR(0, 416, lr);
		return (int)0x803a0101;
	}
	if (avConfigGetHdmiPowerState() == 0)
		goto unlock;
	memset(&monitor, 0, sizeof(monitor));
	result = SceHdmiForDriver_8EB0FD9C(&monitor);
	if (result != 0) {
		valid = 0;
		if (result == (int)0x8037000d)
			force = 1;
	} else {
		valid = 1;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigHdmiMonitorLock);
		avConfigHdmiMonitorValid = 1;
		memcpy(&avConfigHdmiMonitor, &monitor, sizeof(monitor));
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigHdmiMonitorLock, intr);
	}
	if (flags < 0 && avConfigHdmiInitialOutput != 0) {
		if (force == 0)
			goto selectMode;
		if (valid == 0)
			goto selectRange;
		switch (vic) {
		case 0x8300:
			if (monitor.videoFormats[0].supported & 1)
				goto selectRange;
		case 0x8500:
			if (monitor.videoFormats[0].supported & 8)
				goto selectRange;
		case 0x8600:
			if (monitor.videoFormats[0].supported & 4)
				goto selectRange;
		case 0x8480:
			if (monitor.videoFormats[1].supported & 2)
				goto selectRange;
		case 0x8580:
			if (monitor.videoFormats[1].supported & 8)
				goto selectRange;
		case 0x8680:
			if (monitor.videoFormats[1].supported & 4)
				goto selectRange;
		case 0x8710:
			if (monitor.videoFormats[2].supported & 2)
				goto selectRange;
		case 0x8730:
			if (monitor.videoFormats[2].supported & 1)
				goto selectRange;
		}
		vic = 0x8600;
		sceRegMgrSetKeyInt(avConfigDisplayCategory, avConfigHdmiResolutionModeKey, 0);
	} else if (force != 0) {
		goto selectRange;
	}
selectMode:
	if (valid == 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		return (int)0x803a0107;
	}
	switch (vic) {
	case 0x8300:
		if (monitor.videoFormats[0].supported & 1)
			goto selectRange;
	case 0x8500:
		if (monitor.videoFormats[0].supported & 8)
			goto selectRange;
	case 0x8600:
		if (monitor.videoFormats[0].supported & 4)
			goto selectRange;
	case 0x8480:
		if (monitor.videoFormats[1].supported & 2)
			goto selectRange;
	case 0x8580:
		if (monitor.videoFormats[1].supported & 8)
			goto selectRange;
	case 0x8680:
		if (monitor.videoFormats[1].supported & 4)
			goto selectRange;
	case 0x8710:
		if (monitor.videoFormats[2].supported & 2)
			goto selectRange;
	case 0x8730:
		if (monitor.videoFormats[2].supported & 1)
			goto selectRange;
	}
	if ((monitor.videoFormats[0].supported & 4) != 0 ||
	    (monitor.videoFormats[2].supported & 3) != 0 ||
	    (monitor.videoFormats[1].supported & 0xc) != 0)
		vic = 0x8600;
	else
		vic = (monitor.videoFormats[1].supported & 2) != 0 ? 0x8600 : 0x8300;
selectRange:
	if (rangeMode == 3) {
		result = sceRegMgrGetKeyInt(avConfigDisplayCategory,
				avConfigRgbRangeModeKey, &range);
		if (result != 0)
			range = 0;
	} else {
		range = rangeMode;
	}
	if (range == 0)
		fullRange = valid == 0 || monitor.quantizationSelectable == 0;
	else if (range == 1)
		fullRange = 1;
	else if (range == 2)
		fullRange = 0;
	else {
		SCE_DBG_ASSERT_CHECK_LR(0, 567, lr);
		return (int)0x803a0101;
	}
	resolution.size = sizeof(resolution);
	result = SceDisplayForDriver_B3C6D647(1, &resolution);
	if (result != (int)0x80290009) {
		if (result < 0) {
			SCE_DBG_ASSERT_CHECK_LR(0, 578, lr);
			resolution.vic = 0xffff;
		}
		switch (vic) {
		case 0x8300: case 0x8480: case 0x8500: case 0x8580:
		case 0x8600: case 0x8680: case 0x8710: case 0x8730:
			outputVic = vic;
			break;
		case 0x10000000:
			outputVic = 0x8600;
			break;
		default:
			outputVic = -1;
			break;
		}
		if ((unsigned int)outputVic != resolution.vic || resolution.mode != fullRange) {
			result = SceDisplayForDriver_43347565(1);
			if (result < 0)
				goto error;
		}
	}
	switch (vic) {
	case 0x8300: case 0x8480: case 0x8500: case 0x8580:
	case 0x8600: case 0x8680: case 0x8710: case 0x8730:
		outputVic = vic;
		break;
	case 0x10000000:
		outputVic = 0x8600;
		break;
	default:
		outputVic = -1;
		break;
	}
	result = SceDisplayForDriver_AF5EE5BE(1, outputVic, fullRange);
	if (result < 0)
		goto error;
	result = SceDisplayForDriver_496032D6(1);
	if (result < 0)
		goto error;
	audio.sampleRate = 3;
	audio.source = 1;
	audio.coding = 1;
	audio.channelCount = 2;
	audio.spdifLayout = 2;
	audio.reserved = 0;
	audio.speakerAllocation = 0;
	audio.downmixInhibit = 0;
	audio.levelShift = 0;
	result = SceHdmiForDriver_854EEB78(&audio);
	if (result < 0) {
		SCE_DBG_ASSERT_CHECK_FUNC(0, 68, "_hdmiAudioConf");
		goto error;
	}
	if (avConfigConfiguration.settings.hdcpState == 4) {
		result = SceHdmiForDriver_99A3CABE(0);
		if (result < 0)
			SCE_DBG_ASSERT_CHECK_LR(0, 626, lr);
		result = SceHdmiForDriver_08AFFB03(0);
		if (result < 0)
			SCE_DBG_ASSERT_CHECK_LR(0, 632, lr);
	}
	avConfigHdmiLastVic = vic;
unlock:
	SceKernelSuspendForDriver_2BB92967(0);
	return 0;
error:
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int avConfigHdmiSetInitialOutput(void)
{
	unsigned int mode;
	int vic;
	unsigned int flags;

	if (SceSysrootForDriver_834439A7() != 0 || SceSysrootForDriver_B0E1FC67() != 0) {
		vic = 0x8300;
		flags = 0x80000001;
	} else if (sceRegMgrGetKeyInt(avConfigDisplayCategory,
				    avConfigHdmiResolutionModeKey, (int *)&mode) != 0 ||
		   mode > 3) {
		vic = 0x8600;
		flags = 0x80000000;
	} else {
		flags = 0x80000001;
		switch (mode) {
		case 2:
			vic = 0x8600;
			break;
		case 3:
			vic = 0x8300;
			break;
		case 1:
			vic = 0x8500;
			break;
		default:
			vic = 0x8600;
			flags = 0x80000000;
			break;
		}
	}
	return hdmiSetOutput(vic, 3, flags);
}

int avConfigSetHdmiScalingRatio(const AvConfigScalingRatio *ratio)
{
	float scale;
	int result;

	if (ratio == NULL)
		return (int)0x803a0102;
	if (ratio->size > sizeof(*ratio))
		return (int)0x803a0101;
	scale = (float)ratio->ratio / 1000000000.0f;
	result = SceDisplayForDriver_EB390A76(scale, 1, 0, 1);
	if (result >= 0)
		return SceDisplayForDriver_EB390A76(scale, 1, 1, 1);
	return result;
}

int avConfigGetHdmiScalingRatio(AvConfigScalingRatio *ratio)
{
	int value, result;

	if (ratio == NULL)
		return (int)0x803a0102;
	if (ratio->size > sizeof(*ratio))
		return (int)0x803a0101;
	result = sceRegMgrGetKeyInt(avConfigDisplayCategory,
			avConfigHdmiScalingRatioKey, &value);
	if (result != 0 || (SceUInt32)value - 800000000u > 400000000u)
		ratio->ratio = 900000000;
	else
		ratio->ratio = value;
	return result;
}

int avConfigGetMonitorInfo(AvConfigMonitorInfo *info)
{
	SceKernelIntrStatus intr;
	SceUInt8 blockCount;

	if (info == NULL)
		return (int)0x803a0102;
	if (info->size > sizeof(*info))
		return (int)0x803a0101;
	if (avConfigHdmiMonitorValid == 0)
		return (int)0x803a010a;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigHdmiMonitorLock);
	__builtin_memcpy(info->productId, avConfigHdmiMonitor.productId,
			sizeof(info->productId));
	blockCount = avConfigHdmiMonitor.blockCount;
	__builtin_memcpy(info->monitorName, avConfigHdmiMonitor.monitorName,
			sizeof(info->monitorName) + sizeof(info->reserved));
	__builtin_memcpy(&info->videoFormats[0], &avConfigHdmiMonitor.videoFormats[0],
			sizeof(info->videoFormats[0]));
	__builtin_memcpy(&info->videoFormats[1], &avConfigHdmiMonitor.videoFormats[1],
			sizeof(info->videoFormats[1]));
	__builtin_memcpy(&info->videoFormats[2], &avConfigHdmiMonitor.videoFormats[2],
			sizeof(info->videoFormats[2]));
	__builtin_memcpy(&info->videoFormats[3], &avConfigHdmiMonitor.videoFormats[3],
			sizeof(info->videoFormats[3]));
	info->blockCount = blockCount;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigHdmiMonitorLock, intr);
	return 0;
}

int avConfigGetCecInfo(CecStatistics *statistics)
{
	if (statistics == NULL)
		return (int)0x803a0102;
	if (statistics->size > sizeof(*statistics))
		return (int)0x803a0101;
	return SceHdmiForDriver_93AA48AC(statistics);
}

int avConfigClearCecInfo(void)
{
	return SceHdmiForDriver_47B753EB();
}

int avConfigHdmiConfigureCec(void)
{
	int enabled;
	int result;

	if (avConfigIsHdmiAvailable() == 0 || SceSblAIMgrForDriver_71608CA3() == 0)
		return 0;
	result = sceRegMgrGetKeyInt(avConfigCecSystemCategory, avConfigCecEnabledKey,
				 &enabled);
	if (result != 0)
		enabled = 1;
	else if (enabled == 0)
		return 0;
	result = sceHdmiCecEnableCec();
	if (result == (int)0x8037000e)
		return 0;
	return result;
}
