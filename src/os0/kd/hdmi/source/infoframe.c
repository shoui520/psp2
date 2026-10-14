/*
 * SceHdmi InfoFrames: the source product description (SPD), auxiliary video
 * information (AVI) and audio InfoFrames of CEA-861, built in one packet buffer
 * and handed to the device layer. No debug record names this file; its name
 * and the names below are ours (agent/namecheck/os0/kd/hdmi.tsv).
 */
#include <psp2kern/types.h>
#include "hdmi_internal.h"

int ksceSblAimgrIsDolce(void);

#define INFOFRAME_TYPE_AVI	0x82
#define INFOFRAME_TYPE_SPD	0x83
#define INFOFRAME_TYPE_AUDIO	0x84

/* An InfoFrame packet: header, checksum and data bytes (CEA-861). */
typedef union HdmiInfoFramePacket {
	SceUInt32 words[8];
	struct {
		SceUInt8 type;
		SceUInt8 version;
		SceUInt8 length;
		SceUInt8 checksum;
		SceUInt8 data[28];
	};
	struct {
		SceUInt8 header[4];
		char vendorName[8];
		char productDescription[16];
		SceUInt8 sourceInformation;
	} spd;
	struct {
		SceUInt8 header[4];
		/* PB1 */
		SceUInt8 scanInformation:2;
		SceUInt8 barInformation:2;
		SceUInt8 activeFormatPresent:1;
		SceUInt8 colorFormat:2;
		SceUInt8 :1;
		/* PB2 */
		SceUInt8 activeFormatAspectRatio:4;
		SceUInt8 pictureAspectRatio:2;
		SceUInt8 colorimetry:2;
		/* PB3 */
		SceUInt8 scaling:2;
		SceUInt8 quantizationRange:2;
		SceUInt8 extendedColorimetry:3;
		SceUInt8 itContent:1;
		/* PB4 */
		SceUInt8 vic:7;
		SceUInt8 :1;
		/* PB5 */
		SceUInt8 pixelRepetition:4;
		SceUInt8 contentType:2;
		SceUInt8 ycQuantizationRange:2;
	} avi;
	struct {
		SceUInt8 header[4];
		/* PB1 */
		SceUInt8 channelCount:3;
		SceUInt8 :1;
		SceUInt8 codingType:4;
		SceUInt8 sampleFormat;
		SceUInt8 codingExtension;
		SceUInt8 speakerAllocation;
		/* PB5 */
		SceUInt8 :3;
		SceUInt8 levelShift:4;
		SceUInt8 downmixInhibit:1;
	} audio;
} HdmiInfoFramePacket;

static HdmiInfoFramePacket infoFramePacket;
static HdmiInfoFrame infoFrame;

static inline void infoFrameClear(void)
{
	int i;

	for (i = 0; i < 8; i++)
		infoFramePacket.words[i] = 0;
}

static inline SceUInt8 infoFrameChecksum(SceSize size)
{
	const SceUInt8 *p = (const SceUInt8 *)&infoFramePacket;
	unsigned int sum = 0;
	SceSize i;

	for (i = 0; i < size; i++)
		sum += p[i];
	return -sum;
}

int hdmiInfoFrameInitialize(void)
{
	infoFrameClear();
	return 0;
}

int hdmiSendSpdInfoFrame(void)
{
	infoFrameClear();
	infoFramePacket.type = INFOFRAME_TYPE_SPD;
	infoFramePacket.version = 1;
	infoFramePacket.length = 25;
	infoFramePacket.spd.vendorName[0] = 'S';
	infoFramePacket.spd.vendorName[1] = 'C';
	infoFramePacket.spd.vendorName[2] = 'E';
	infoFramePacket.spd.vendorName[3] = 'I';
	if (ksceSblAimgrIsDolce()) {
		infoFramePacket.spd.productDescription[0] = 'P';
		infoFramePacket.spd.productDescription[1] = 'S';
		infoFramePacket.spd.productDescription[2] = ' ';
		infoFramePacket.spd.productDescription[3] = 'T';
		infoFramePacket.spd.productDescription[4] = 'V';
	} else {
		infoFramePacket.spd.productDescription[0] = 'P';
		infoFramePacket.spd.productDescription[1] = 'S';
		infoFramePacket.spd.productDescription[2] = 'P';
		infoFramePacket.spd.productDescription[3] = '2';
	}
	infoFramePacket.spd.sourceInformation = 8;	/* game */
	infoFrame.type = INFOFRAME_TYPE_SPD;
	infoFrame.size = sizeof(infoFramePacket);
	infoFrame.packet = &infoFramePacket;
	return sceHdmiDeviceSetInfoFrame(&infoFrame);
}

int hdmiSendAviInfoFrame(const HdmiVideoMode *mode)
{
	int format, colorimetry;

	if (mode == NULL)
		return 1;
	infoFrameClear();
	infoFramePacket.type = INFOFRAME_TYPE_AVI;
	infoFramePacket.version = 2;
	infoFramePacket.length = 13;

	if (mode->outputFormat == 0)
		format = 0;	/* RGB */
	else if (mode->outputFormat == 2)
		format = 1;	/* YCbCr 4:2:2 */
	else if (mode->outputFormat == 1 || mode->outputFormat == 3)
		format = 2;	/* YCbCr 4:4:4 */
	else
		format = 3;
	infoFramePacket.avi.colorFormat = format;

	if (hdmiIsSdVic(mode->vic)) {
		if (mode->outputFormat == 3)
			colorimetry = 3;
		else if (mode->outputFormat == 1 || mode->outputFormat == 2)
			colorimetry = 1;	/* BT.601 */
		else
			colorimetry = 0;
		infoFramePacket.avi.activeFormatPresent = 0;
		infoFramePacket.avi.colorimetry = colorimetry;
		infoFramePacket.avi.activeFormatAspectRatio = 8;
		if (hdmiGetVicAspectRatio(mode->vic) == 0)
			infoFramePacket.avi.pictureAspectRatio = 1;	/* 4:3 */
		else if (hdmiGetVicAspectRatio(mode->vic) == 1)
			infoFramePacket.avi.pictureAspectRatio = 2;	/* 16:9 */
		else
			infoFramePacket.avi.pictureAspectRatio = 0;
		infoFramePacket.avi.activeFormatPresent = 0;
	} else {
		infoFramePacket.avi.activeFormatPresent = 0;
		if (mode->outputFormat == 3)
			colorimetry = 3;
		else if (mode->outputFormat == 1 || mode->outputFormat == 2)
			colorimetry = 2;	/* BT.709 */
		else
			colorimetry = 0;
		infoFramePacket.avi.colorimetry = colorimetry;
		infoFramePacket.avi.pictureAspectRatio = 2;
	}
	infoFramePacket.avi.activeFormatAspectRatio = 8;	/* as the picture */
	infoFramePacket.avi.vic = mode->vic;
	infoFramePacket.avi.itContent = mode->contentType == 1;
	infoFramePacket.avi.contentType = mode->contentType != 0 ? (mode->contentType - 1) : 0;

	if (mode->outputFormat == 0) {
		if (mode->itContent != 0) {
			if (mode->inputFullRange != 0)
				infoFramePacket.avi.quantizationRange = 2;	/* full */
			else
				infoFramePacket.avi.quantizationRange = 1;	/* limited */
		} else {
			if (mode->inputFullRange != 0)
				infoFramePacket.avi.quantizationRange = 2;
			else
				infoFramePacket.avi.quantizationRange = 0;
		}
	} else if (mode->inputFormat == 3) {
		if (!hdmiIsSdVic(mode->vic))
			infoFramePacket.avi.extendedColorimetry = 1;	/* xvYCC709 */
		else
			infoFramePacket.avi.extendedColorimetry = 0;	/* xvYCC601 */
	}

	infoFramePacket.checksum = infoFrameChecksum(19);
	infoFrame.type = INFOFRAME_TYPE_AVI;
	infoFrame.size = 19;
	infoFrame.packet = &infoFramePacket;
	sceHdmiDeviceSetInfoFrame(&infoFrame);
	return 0;
}

int hdmiSendAudioInfoFrame(const SceHdmiAudioMode *mode)
{
	int count;

	if (mode == NULL)
		return 1;
	infoFrameClear();
	infoFramePacket.type = INFOFRAME_TYPE_AUDIO;
	infoFramePacket.version = 1;
	infoFramePacket.length = 10;
	if (mode->coding == 1) {
		if (mode->channelCount == 2)
			count = 1;
		else if (mode->channelCount == 3)
			count = 2;
		else if (mode->channelCount == 4)
			count = 3;
		else if (mode->channelCount == 5)
			count = 4;
		else if (mode->channelCount == 6)
			count = 5;
		else if (mode->channelCount == 7)
			count = 6;
		else if (mode->channelCount == 8)
			count = 7;
		else
			count = 0;
		infoFramePacket.audio.channelCount = count;
		infoFramePacket.audio.speakerAllocation = mode->speakerAllocation;
	}
	infoFramePacket.audio.downmixInhibit = mode->downmixInhibit != 0;
	infoFramePacket.audio.levelShift = mode->levelShift;
	infoFramePacket.checksum = infoFrameChecksum(13);
	infoFrame.type = INFOFRAME_TYPE_AUDIO;
	infoFrame.size = 13;
	infoFrame.packet = &infoFramePacket;
	sceHdmiDeviceSetInfoFrame(&infoFrame);
	return 0;
}
