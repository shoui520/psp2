/*
 * SceHdmi: the video and audio modes of the transmitter (output and audio
 * configuration, mutes). Internal names are unknown unless a comment says otherwise.
 */
#include <scetypes.h>
#include <stddef.h>
#include "hdmi_internal.h"

HdmiVideoMode videoMode;
static HdmiDeviceVideoConfig videoConfig;
static HdmiDeviceAudioConfig audioConfig;
int hdmiSetVideoMute(int mute)
{
	if (sceHdmiDeviceSetVideoMute(mute) < 0)
		return 2;
	return 0;
}

int hdmiSetAvMute(int mute)
{
	if (sceHdmiDeviceSetAvMute(mute) < 0)
		return 2;
	return 0;
}

/* SceDisplay output mode to CEA-861 video identification code. */
static int vicFromMode(SceUInt16 mode) /* name unknown */
{
	switch (mode) {
	case 0x8b00: return 1;
	case 0x8370: return 2;
	case 0x8100: return 3;
	case 0x8600: return 4;
	case 0x8500: return 5;
	case 0x8300: return 6;
	case 0x8170: return 7;
	case 0x84f0: return 17;
	case 0x8280: return 18;
	case 0x8680: return 19;
	case 0x8580: return 20;
	case 0x8480: return 21;
	case 0x82f0: return 23;
	case 0x8730: return 32;
	case 0x8790: return 33;
	case 0x8710: return 34;
	case 0x8c00: return 80;
	case 0x8d00: return 81;
	case 0x8e00: return 82;
	case 0x8f00: return 83;
	default: return 0;
	}
}

static inline int colorFormat(SceUInt16 format)
{
	switch (format) {
	case 1: return 1;
	case 2: return 2;
	case 3: return 3;
	default: return 0;
	}
}

static inline int scanInfo(SceUInt16 info)
{
	switch (info) {
	case 1: return 1;
	case 2: return 2;
	case 3: return 3;
	default: return 0;
	}
}

int hdmiSetVideoMode(const SceHdmiOutputMode *mode)
{
	int vic, csc;

	sceHdmiDeviceSetTmdsCtrl(0);
	if (mode == NULL)
		return 1;
	videoMode.vic = vicFromMode(mode->vic);
	videoMode.inputFormat = colorFormat(mode->inputFormat);
	videoMode.outputFormat = colorFormat(mode->outputFormat);
	switch (mode->colorDepth) {
	case 1:
		videoMode.colorDepth = 1;
		break;
	case 2:
		videoMode.colorDepth = 2;
		break;
	default:
		videoMode.colorDepth = 0;
		break;
	}
	videoMode.inputFullRange = mode->inputFullRange == 1 ? 1 : 0;
	videoMode.outputFullRange = mode->outputFullRange == 1 ? 1 : 0;
	videoMode.scanInfo = scanInfo(mode->scanInfo);
	controlHdmiSetPacketCtrl(0);
	hdmiSendAviInfoFrame(&videoMode);
	hdmiSendSpdInfoFrame();
	if (hdmiSetVicTiming(videoMode.vic) == 0) {
		vic = videoMode.vic;
		videoConfig.vic = vic;
		switch (videoMode.inputFormat) {
		case 0:
			csc = videoMode.inputFullRange != 1;
			break;
		case 1:
			csc = vicIsSd(vic) ? 4 : 8;
			break;
		case 2:
			csc = vicIsSd(vic) ? 3 : 7;
			break;
		case 3:
			csc = vicIsSd(vic) ? 4 : 9;
			break;
		default:
			csc = 0;
			break;
		}
		videoConfig.inputCsc = csc;
		switch (videoMode.outputFormat) {
		case 0:
			csc = videoMode.inputFullRange != 1;
			break;
		case 2:
			csc = vicIsSd(vic) ? 2 : 6;
			break;
		case 3:
			csc = vicIsSd(vic) ? 5 : 9;
			break;
		case 1:
			csc = vicIsSd(vic) ? 4 : 8;
			break;
		default:
			csc = 4;
			break;
		}
		videoConfig.outputCsc = csc;
		videoConfig.colorDepth = videoMode.colorDepth;
		videoConfig.aspectRatio = vicAspectRatio(vic);
		videoConfig.scanInfo = videoMode.scanInfo;
		sceHdmiDeviceSetVideoConfig(&videoConfig);
	}
	controlHdmiSetPacketCtrl(mode->outputFormat == 3 ? 0x3408 : 0x3008);
	return 0;
}

int hdmiSetContentType(SceUInt32 type)
{
	if (type > 4)
		return 1;
	videoMode.contentType = type;
	return 0;
}

int hdmiSetItContent(int itContent)
{
	videoMode.itContent = itContent;
	return 0;
}

/* Audio clock regeneration N of a sample rate at a pixel clock, -1: none. */
static inline int audioN(int sampleRate, int clock)
{
	switch (sampleRate) {
	case 1:
		switch (clock) {
		case 2: case 3: case 4: case 5: case 6: case 8: case 10: case 11:
			return 0x1000;
		case 7: case 9:
			return 0x2d80;
		case 1:
			return 0x11e0;
		default:
			return -1;
		}
	case 2:
		switch (clock) {
		case 1: return 0x1b5f;
		case 7: return 0x45ac;
		case 9: return 0x22d6;
		case 2: case 3: case 4: case 5: case 6: case 8: case 10: case 11:
			return 0x1880;
		default: return -1;
		}
	case 3:
		switch (clock) {
		case 1: return 0x1ad0;
		case 7: return 0x2d80;
		case 9: return 0x16c0;
		case 2: case 3: case 4: case 5: case 6: case 8: case 10: case 11:
			return 0x1800;
		default: return -1;
		}
	case 4:
		switch (clock) {
		case 1: return 0x36be;
		case 7: return 0x8b58;
		case 9: return 0x45ac;
		case 2: case 3: case 4: case 5: case 6: case 8: case 10: case 11:
			return 0x3100;
		default: return -1;
		}
	case 5:
		switch (clock) {
		case 1: return 0x35a0;
		case 7: return 0x5b00;
		case 9: return 0x2d80;
		case 2: case 3: case 4: case 5: case 6: case 8: case 10: case 11:
			return 0x3000;
		default: return -1;
		}
	case 6:
		switch (clock) {
		case 1: return 0x6d7c;
		case 7: return 0x116b0;
		case 9: return 0x8b58;
		case 2: case 3: case 4: case 5: case 6: case 8: case 10: case 11:
			return 0x6200;
		default: return -1;
		}
	case 7:
		switch (clock) {
		case 1: return 0x6b40;
		case 7: return 0xb600;
		case 9: return 0x5b00;
		case 2: case 3: case 4: case 5: case 6: case 8: case 10: case 11:
			return 0x6000;
		default: return -1;
		}
	default:
		return -1;
	}
}

int hdmiSetAudioMode(const SceHdmiAudioMode *mode)
{
	HdmiPacketCtrl ctrl;
	int vic, n, ret;

	vic = hdmiGetVic();
	if (vic == 0)
		return SCE_HDMI_ERROR_NOT_READY;
	n = audioN(mode->sampleRate, hdmiGetVicClockClass(vic));
	if (n == -1)
		return SCE_HDMI_ERROR_INVALID_ARGUMENT;
	audioConfig.n[0] = n;
	audioConfig.n[1] = n >> 8;
	audioConfig.n[2] = n >> 16;
	audioConfig.channelCount = mode->channelCount;
	if (mode->source == 1) {
		audioConfig.source = 0;
		audioConfig.spdifMode = 0;
		ret = sceHdmiDeviceSetAudioConfig(&audioConfig);
	} else if (mode->source == 0) {
		audioConfig.spdifLayout = mode->spdifLayout;
		audioConfig.source = 2;
		audioConfig.spdifMode = 0;
		audioConfig.spdifFlags = 0;
		ret = sceHdmiDeviceSetAudioConfig(&audioConfig);
	} else {
		return SCE_HDMI_ERROR_INVALID_ARGUMENT;
	}
	if (ret != 0)
		return 2;
	ret = hdmiSendAudioInfoFrame(mode);
	if (ret != 0)
		return 2;
	ctrl.set = ctrl.mask = 0x4000;
	controlHdmiUpdatePacketCtrl(ctrl);
	ctrl.set = ctrl.mask = 2;
	controlHdmiUpdatePacketCtrl(ctrl);
	ctrl.set = ctrl.mask = 4;
	controlHdmiUpdatePacketCtrl(ctrl);
	return ret;
}

int hdmiSetAudioMute(int mute)
{
	return sceHdmiDeviceSetAudioMute(mute);
}
