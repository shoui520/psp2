/*
 * SceHdmi device layer: the packets and InfoFrames the transmitter sends.
 * hdmiDeviceSmSetPacketData loads one, hdmiDeviceSmSetPacketCtrl starts the
 * enabled ones (pDevice.packetCtrl) and stops the others. Both fire the
 * DTrace probes __hdmi__hdmi_set_packet_direct and __hdmi__hdmi_set_packet.
 * Internal names are ours unless a comment says otherwise.
 */
#include <psp2kern/types.h>
#include "fw.h"
#include "sce/debug.h"
#include "sce/dtrace.h"
#include "../hdmi_internal.h"

#define HDMI_DBG_FRAM	0x00000200	/* "[hdmiFRAM] " */

#define SCE_ERROR_INVALID_TYPE	0x80000021

/* The transmitter's frame (packet) ids, HdmiDevice.packetCtrl bits */
#define HDMI_FRAME_ACR		0x0002	/* packet type 1 */
#define HDMI_FRAME_TYPE2	0x0004
#define HDMI_FRAME_GC		0x0008	/* general control */
#define HDMI_FRAME_ISRC1	0x0010
#define HDMI_FRAME_ISRC2	0x0020
#define HDMI_FRAME_TYPE6	0x0040
#define HDMI_FRAME_TYPE10	0x0400
#define HDMI_FRAME_VS		0x0800	/* vendor specific InfoFrame */
#define HDMI_FRAME_AVI		0x1000
#define HDMI_FRAME_SPD		0x2000
#define HDMI_FRAME_AUDIO	0x4000
#define HDMI_FRAME_MPEG		0x8000
#define HDMI_FRAME_ALL		0xffff

/* .bss: the DTrace hooks */
#if FW_SINCE(0x04000000)
void (*setPacketDirectProbeHook)(const SceDtraceProbe *probe, ...);
int setPacketDirectProbeEnabled;
void (*setPacketProbeHook)(const SceDtraceProbe *probe, ...);
int setPacketProbeEnabled;
#endif

#if FW_SINCE(0x04000000)
static const SceDtraceProbe setPacketDirectProbe;
static const SceDtraceProbe setPacketProbe;
#endif

int hdmiDeviceSmStopPackets(HdmiDevice *device)
{
	return HdmiDeviceHwSetFrame(device, HDMI_FRAME_ALL);
}

/* Load the packet or InfoFrame of type 'type'. */
int hdmiDeviceSmSetPacketData(HdmiDevice *device, SceUInt32 type, const void *packet,
			      SceUInt8 size)
{
	SceUInt32 frame;

	switch (type) {
	case 1:
		frame = HDMI_FRAME_ACR;
		break;
	case 2:
		frame = HDMI_FRAME_TYPE2;
		break;
	case 3:
		frame = HDMI_FRAME_GC;
		break;
	case 4:
		frame = HDMI_FRAME_ISRC1;
		break;
	case 5:
		frame = HDMI_FRAME_ISRC2;
		break;
	case 6:
		frame = HDMI_FRAME_TYPE6;
		break;
	case 10:
		frame = HDMI_FRAME_TYPE10;
		break;
	case 0x81:
		frame = HDMI_FRAME_VS;
		break;
	case 0x82:
		frame = HDMI_FRAME_AVI;
		break;
	case 0x83:
		frame = HDMI_FRAME_SPD;
		break;
	case 0x84:
		frame = HDMI_FRAME_AUDIO;
		break;
	case 0x85:
		frame = HDMI_FRAME_MPEG;
		break;
	default:
		return SCE_ERROR_INVALID_TYPE;
	}
#if FW_SINCE(0x04000000)
	if (setPacketDirectProbeHook != NULL)
		setPacketDirectProbeHook(&setPacketDirectProbe, frame);
#endif
	return HdmiDeviceHwSetFrameDirect(device, frame, packet, size);
}

/* Start the packets pDevice.packetCtrl enables and stop the others. */
int hdmiDeviceSmSetPacketCtrl(HdmiDevice *device)
{
	SceUInt32 start, stop;
	int ret;

#if FW_SINCE(0x04000000)
	if (setPacketProbeHook != NULL)
		setPacketProbeHook(&setPacketProbe, device->packetCtrl);
#endif
	hdmiDeviceSmLock();
	start = 0;
	stop = 0;
	if (device->packetCtrl & HDMI_FRAME_AVI)
		start |= HDMI_FRAME_AVI;
	else
		stop |= HDMI_FRAME_AVI;
	if (device->packetCtrl & HDMI_FRAME_AUDIO)
		start |= HDMI_FRAME_AUDIO;
	else
		stop |= HDMI_FRAME_AUDIO;
	if (device->packetCtrl & HDMI_FRAME_SPD)
		start |= HDMI_FRAME_SPD;
	else
		stop |= HDMI_FRAME_SPD;
	if (device->packetCtrl & HDMI_FRAME_MPEG)
		start |= HDMI_FRAME_MPEG;
	else
		stop |= HDMI_FRAME_MPEG;
	if (device->packetCtrl & HDMI_FRAME_ISRC1)
		start |= HDMI_FRAME_ISRC1;
	else
		stop |= HDMI_FRAME_ISRC1;
	HDMI_PRINTF(HDMI_DBG_FRAM, "[hdmiFRAM] ",
		    "ISRC1 Frame ctl[0x%08X] str_flg[0x%08X] stp_flg[0x%08X]\n",
		    device->packetCtrl, start, stop);
	if (device->packetCtrl & HDMI_FRAME_ISRC2)
		start |= HDMI_FRAME_ISRC2;
	else
		stop |= HDMI_FRAME_ISRC2;
	HDMI_PRINTF(HDMI_DBG_FRAM, "[hdmiFRAM] ",
		    "ISRC2 Frame ctl[0x%08X] str_flg[0x%08X] stp_flg[0x%08X]\n",
		    device->packetCtrl, start, stop);
	if (device->packetCtrl & HDMI_FRAME_TYPE6)
		start |= HDMI_FRAME_TYPE6;
	else
		stop |= HDMI_FRAME_TYPE6;
	if (device->packetCtrl & HDMI_FRAME_ACR)
		start |= HDMI_FRAME_ACR;
	else
		stop |= HDMI_FRAME_ACR;
	if (device->packetCtrl & HDMI_FRAME_TYPE2)
		start |= HDMI_FRAME_TYPE2;
	else
		stop |= HDMI_FRAME_TYPE2;
	if (device->packetCtrl & HDMI_FRAME_TYPE10)
		start |= HDMI_FRAME_TYPE10;
	else
		stop |= HDMI_FRAME_TYPE10;
	if (device->packetCtrl & HDMI_FRAME_GC)
		start |= HDMI_FRAME_GC;
	else
		stop |= HDMI_FRAME_GC;
	if (device->packetCtrl & HDMI_FRAME_VS)
		start |= HDMI_FRAME_VS;
	else
		stop |= HDMI_FRAME_VS;
	hdmiDeviceSmUnlock();
	HDMI_PRINTF(HDMI_DBG_FRAM, "[hdmiFRAM] ",
		    "Send Frame ctl[0x%08X] str_flg[0x%08X] stp_flg[0x%08X]\n",
		    device->packetCtrl, start, stop);
	ret = HdmiDeviceHwSetPacketCtrl(device, start);
	if (ret == 0)
		ret = HdmiDeviceHwSetFrame(device, stop);
	return ret;
}

#if FW_SINCE(0x04000000)
/* DTrace probes __hdmi__hdmi_set_packet_direct and __hdmi__hdmi_set_packet. */
static const SceDtraceProbe setPacketDirectProbe = {
	.enabled = &setPacketDirectProbeEnabled,
	.provider = "__hdmi__",
	.name = "hdmi_set_packet_direct",
	.address = hdmiDeviceSmSetPacketData,	/* PLACEHOLDER: Sony's is the probe site (0x81008052) */
	.hook = &setPacketDirectProbeHook,
};

static const SceDtraceProbe setPacketProbe = {
	.enabled = &setPacketProbeEnabled,
	.provider = "__hdmi__",
	.name = "hdmi_set_packet",
	.address = hdmiDeviceSmSetPacketCtrl,	/* PLACEHOLDER: Sony's is the probe site */
	.hook = &setPacketProbeHook,
};

static const SceDtraceProbe *const packetProbes[] __attribute__((used)) = {
	&setPacketProbe, &setPacketDirectProbe,
};
#endif

