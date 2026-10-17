/*
 * SceHdmi internal interfaces.
 *
 * Sony's source files (debug records): event_handle_manager.c, edid_manager.c,
 * control_hdmi_mode.c, cec.c, device/hdmi_device.c, device/hdmi_device_i2c.c,
 * device/adi/hdmi_ad80244.c, device/hdmi_device_sm.c, device/hdmi_device_cec.c.
 * The device layer's API names follow its "[hdmiCmd ] Set ..." messages.
 */
#pragma once

#include <psp2kern/types.h>
#include <hdmi/hdmi.h>

#define SCE_HDMI_ERROR_INVALID_ARGUMENT	0x80370001
#define SCE_HDMI_ERROR_BUSY		0x80370002
#define SCE_HDMI_ERROR_SET_MODE		0x80370004
#define SCE_HDMI_ERROR_NOT_READY	0x80370006
#define SCE_HDMI_ERROR_DEVICE		0x8037000b
#define SCE_HDMI_ERROR_CEC_FAILED	0x8037000c
#define SCE_HDMI_ERROR_CEC_UNSUPPORTED	0x8037000e
#define SCE_HDMI_ERROR_CEC_DISABLED	0x8037000f

/* Video mode the transmitter is set to (hdmi.c). */
typedef struct HdmiVideoMode {
	int vic;			/* 0x00: CEA-861 video identification code */
	int inputFormat;		/* 0x04: SceHdmiOutputMode.inputFormat */
	int outputFormat;		/* 0x08: SceHdmiOutputMode.outputFormat */
	int colorDepth;			/* 0x0c: 0: 24 bit, 1: 30 bit, 2: 36 bit */
	int inputFullRange;		/* 0x10 */
	int outputFullRange;		/* 0x14 */
	int contentType;		/* 0x18: SceHdmiForDriver_8EB0FD9C's sibling setter */
	int scanInfo;			/* 0x1c */
	int itContent;			/* 0x20 */
} HdmiVideoMode;

/* What sceHdmiDeviceSetVideoConfig programs ("Set Video Config"). */
typedef struct HdmiDeviceVideoConfig {
	int vic;			/* 0x00 */
	int inputCsc;			/* 0x04 */
	int outputCsc;			/* 0x08 */
	int colorDepth;			/* 0x0c */
	int aspectRatio;		/* 0x10: 0: 4:3, 1: 16:9, 2: none */
	int scanInfo;			/* 0x14 */
} HdmiDeviceVideoConfig;

/* What sceHdmiDeviceSetAudioConfig programs. */
typedef struct HdmiDeviceAudioConfig {
	int source;			/* 0x00: 0: I2S, 2: S/PDIF */
	int spdifMode;			/* 0x04 */
	SceUInt8 n[3];			/* 0x08: audio clock regeneration N, little endian */
	SceUInt8 padding;
	int channelCount;		/* 0x0c */
	int reserved[3];		/* 0x10 */
	int spdifFlags;			/* 0x1c */
	int spdifLayout;		/* 0x20 */
} HdmiDeviceAudioConfig;

/* Transmitter packet enable bits (sceHdmiDeviceSetPacketCtrl). */
typedef struct HdmiPacketCtrl {
	SceUInt32 set;
	SceUInt32 mask;
} HdmiPacketCtrl;

/* An InfoFrame to send (sceHdmiDeviceSetInfoFrame). */
typedef struct HdmiInfoFrame {
	SceUInt8 type;			/* 0x00 */
	SceUInt8 size;			/* 0x01 */
	SceUInt8 padding[2];
	void *packet;			/* 0x04 */
} HdmiInfoFrame;

/* A standard definition code (the BT.601 colour space). */
static inline int vicIsSd(int vic) /* name unknown */
{
	switch (vic) {
	case 1: case 2: case 3: case 6: case 7: case 8: case 9: case 10: case 11:
	case 12: case 13: case 14: case 15: case 17: case 18: case 21: case 22:
	case 23: case 24: case 25: case 26: case 27: case 28: case 29: case 30:
		return 1;
	default:
		return 0;
	}
}

/* Picture aspect ratio of a code. 0: 4:3, 1: 16:9, 2: unknown. */
static inline int vicAspectRatio(int vic) /* name unknown */
{
	switch (vic) {
	case 1: case 2: case 6: case 8: case 10: case 12: case 17: case 21: case 23:
	case 25: case 27: case 29:
		return 0;
	case 3: case 4: case 5: case 7: case 9: case 11: case 13: case 14: case 15:
	case 16: case 18: case 19: case 20: case 22: case 24: case 26: case 28:
	case 30: case 31: case 32:
		return 1;
	default:
		return 2;
	}
}

/* hdmi_mode.c */
extern HdmiVideoMode videoMode;

/* hdmi_vic.c, hdmi_mode.c */
int hdmiSetVicTiming(int vic);
void hdmiResetVicTiming(void);
int hdmiGetVic(void);
int hdmiIsSdVic(int vic);
int hdmiGetVicAspectRatio(int vic);
int hdmiGetVicClockClass(int vic);
int hdmiSetVideoMute(int mute);
int hdmiSetAvMute(int mute);
int hdmiSetVideoMode(const SceHdmiOutputMode *mode);
int hdmiSetContentType(SceUInt32 type);
int hdmiSetItContent(int quantizationSelectable);
int hdmiSetAudioMode(const SceHdmiAudioMode *mode);
int hdmiSetAudioMute(int mute);

/* event_handle_manager.c */
int eventHandleManagerInitialize(void);
int _sceHdmiRegisterEventCallback(SceUInt32 event, HdmiEventCallback callback);
int _sceHdmiUnRegisterEventCallback(SceUInt32 event);

/* infoframe.c */
int hdmiInfoFrameInitialize(void);
int hdmiSendSpdInfoFrame(void);
int hdmiSendAviInfoFrame(const HdmiVideoMode *mode);
int hdmiSendAudioInfoFrame(const SceHdmiAudioMode *mode);

/* The sink's EDID and what was learnt from it (edid_manager.c's _getEdidManager). */
typedef struct EdidManager {
	SceUInt8 edid[0x200];		/* 0x000: up to four 128-byte blocks */
	int edidBlockCount;		/* 0x200: blocks the device read */
	int blockCount;			/* 0x204: 1, or 2 with extensions */
	int valid;			/* 0x208 */
	int isSonyD702;			/* 0x20c: the sink is Sony product 0xd702 */
	SceHdmiEdidInfo info;		/* 0x210 */
} EdidManager;

/* edid_manager.c */
int edidManagerRead(void);
int edidManagerClear(void);

/* edid_info.c */
extern EdidManager edidManager;
int edidManagerIsValid(void);
int edidManagerIsSonyD702(void);
SceHdmiEdidInfo *edidManagerGetInfo(void);
int edidManagerGetInfoCopy(SceHdmiEdidInfo *info);
int edidManagerClearInfo(void);

/* control_hdmi_mode.c */
int controlHdmiSetPacketCtrl(SceUInt32 ctrl);
int controlHdmiUpdatePacketCtrl(HdmiPacketCtrl ctrl);
int controlHdmiModuleStart(void);
int controlHdmiSetHdcpMode(int on);
int controlHdmiSetEdidMode(int on);
int controlHdmiSetHdcpModeAgain(int on);
int controlHdmiGetChipId(void *id, SceSize size, SceUInt8 *len);
int controlHdmiGetHdcpAksv(void);
int controlHdmiCecPowerOn(void);
int _sceHdmiLockMutex(void);	/* Sony's name ("err _sceHdmiLockMutex()") */
int _sceHdmiUnlockMutex(void);	/* Sony's name */

/* device/hdmi_device.c */
int _sceHdmiDeviceModuleStart(void);
int sceHdmiDeviceSuspend(void);
int sceHdmiDeviceSuspendDone(void);
int sceHdmiDeviceResume(void);
int sceHdmiDeviceGetChipId(SceUInt8 *id, SceUInt8 *length);
int sceHdmiDeviceGetEdid(void *edid, int *blocks);
int sceHdmiDeviceCheckStatus(void);
int sceHdmiDeviceSetEdidMode(int on);
int sceHdmiDeviceSetHdcpMode(int on);
int sceHdmiDeviceGetHdcpAksv(SceUInt8 *aksv, int fromChip);
int sceHdmiDeviceSetVideoConfig(const HdmiDeviceVideoConfig *config);
int sceHdmiDeviceSetAudioConfig(const HdmiDeviceAudioConfig *config);
int sceHdmiDeviceSetInfoFrame(const HdmiInfoFrame *frame);
int sceHdmiDeviceSetPacketCtrl(SceUInt32 ctrl);
int sceHdmiDeviceSetTmdsCtrl(int on);
int sceHdmiDeviceSetAvMute(int mute);
int sceHdmiDeviceSetVideoMute(int mute);
int sceHdmiDeviceSetAudioMute(int mute);
int sceHdmiDeviceRegisterEventCallback(SceUInt32 event, HdmiEventCallback callback);

/* device/: the transmitter (an AD80244) and its state */

/* hdmiDebugFlags: which messages the device layer prints */
#define HDMI_DBG_CMD	0x00000002	/* "[hdmiCmd ] " */
#define HDMI_DBG_INTR	0x00000008	/* "[hdmiINTR] " */
#define HDMI_DBG_ITRS	0x00000010	/* "[hdmiITRS] " */
#define HDMI_DBG_EDID	0x00000080	/* "[hdmiEDID] " */
#define HDMI_DBG_DDC	0x00000100	/* "[hdmiDDC ] " */
#define HDMI_DBG_HW	0x00000400	/* "[hdmiHW  ] " */
#define HDMI_DBG_SEVT	0x00004000	/* "[hdmiSEVT] " */
#define HDMI_DBG_ERR	0x80000000	/* "[hdmiERR ] " */

extern int hdmiChipVersion;		/* 5 once HdmiDeviceAD80244GetChipId knew the chip */
extern SceUInt32 hdmiDebugFlags;

/* The device layer's messages: debug firmwares only. */
#if FW_DEBUG
/* A message with its tag printed first. */
#define HDMI_PRINTF(_flag, _tag, _fmt, ...) do {			\
	if (hdmiDebugFlags & (_flag)) {			\
		SceDebugForDriver_391B74B7(_tag);					\
		SceDebugForDriver_391B74B7(_fmt, ##__VA_ARGS__);			\
	}								\
} while (0)

/* A message whose format starts with its tag. */
#define HDMI_CMD_PRINTF(_fmt, ...) do {					\
	if (hdmiDebugFlags & HDMI_DBG_CMD)			\
		SceDebugForDriver_391B74B7(_fmt, ##__VA_ARGS__);			\
} while (0)

#define HDMI_ITRS_PRINTF(_fmt, ...) do {				\
	if (hdmiDebugFlags & HDMI_DBG_ITRS)			\
		SceDebugForDriver_391B74B7(_fmt, ##__VA_ARGS__);			\
} while (0)

#define HDMI_ERR_PRINTF(_fmt, ...) do {					\
	if ((int)hdmiDebugFlags < 0)				\
		SceDebugForDriver_391B74B7(_fmt, ##__VA_ARGS__);			\
} while (0)

/* A message whose format starts with its tag, printed when hdmiDebugFlags has _flag. */
#define HDMI_FLAG_PRINTF(_flag, _fmt, ...) do {				\
	if (hdmiDebugFlags & (_flag))					\
		SceDebugForDriver_391B74B7(_fmt, ##__VA_ARGS__);			\
} while (0)
#else
#define HDMI_PRINTF(_flag, _tag, _fmt, ...)	do { } while (0)
#define HDMI_CMD_PRINTF(_fmt, ...)		do { } while (0)
#define HDMI_ITRS_PRINTF(_fmt, ...)		do { } while (0)
#define HDMI_ERR_PRINTF(_fmt, ...)		do { } while (0)
#define HDMI_FLAG_PRINTF(_flag, _fmt, ...)	do { } while (0)
#endif

/* The transmitter's interrupt registers 0x92..0x97, read and written together. */
typedef struct HdmiIntrRegs {
	SceUInt8 reg[6];
	SceUInt8 padding[2];
} __attribute__((aligned(4))) HdmiIntrRegs;

/* HdmiIntrRegs.reg[]: register - 0x92 */
#define HDMI_INTR_CEC		1	/* 0x93: CEC interrupts */
#define HDMI_INTR_ENABLE1	2	/* 0x94 */
#define HDMI_INTR_ENABLE2	3	/* 0x95 */
#define HDMI_INTR_STATUS1	4	/* 0x96 */
#define HDMI_INTR_STATUS2	5	/* 0x97 */

/* What ksceKernelCreateThread is given for the interrupt thread (size 8). */
typedef struct HdmiThreadOptParam {
	SceSize size;
	SceUInt32 attr;
	SceUInt32 reserved[2];
} HdmiThreadOptParam;

/* pDevice (Sony's name, "... to fix pDevice situation"): the transmitter's state. */
typedef struct HdmiDevice {
	int *chipVersion;		/* 0x00: hdmiChipVersion */
	int state;			/* 0x04: hdmi_device_sm.c's state */
	int sinkType;			/* 0x08: 1: DVI, 2: HDMI (from the EDID) */
	SceUInt32 reserved;		/* 0x0c */
	int authState;			/* 0x10: HDCP authentication step */
	int repeater;			/* 0x14: the sink is an HDCP repeater (Bcaps) */
	int aksvValid;			/* 0x18 */
	SceUInt8 *aksv;			/* 0x1c: 5 bytes */
	int ksvCount;			/* 0x20 */
	SceUInt8 *ksvList;		/* 0x24: 5 bytes each */
	int firstConnect;		/* 0x28: no sink connected since the last unplug */
	SceUInt32 reserved2;		/* 0x2c */
	SceUInt8 *edid;			/* 0x30: 128-byte blocks */
	int edidBlockCount;		/* 0x34 */
	SceUInt8 *physicalAddress;	/* 0x38: the CEC physical address (EDID vendor block) */
	SceUInt32 reserved3;		/* 0x3c */
	int request;			/* 0x40: what the state machine is to do next */
	int audioConfigured;		/* 0x44 */
	int pendingStatus;		/* 0x48 */
	SceUInt16 packetCtrl;		/* 0x4c: HDMI_PACKET_* enables */
	SceUInt16 padding;
	SceUInt32 reserved4[4];		/* 0x50: 1, 1, 1, 0, never read */
} HdmiDevice;

extern HdmiDevice pDevice;

/* sceHdmiDeviceDdcRead: a read over the DDC. */
typedef struct HdmiDdcRequest {
	SceUInt8 address;		/* 0x00: the slave (0x74: the HDCP receiver) */
	SceUInt8 offset;		/* 0x01 */
	SceUInt8 size;			/* 0x02 */
	SceUInt8 mode;			/* 0x03: 0x21: KSV FIFO */
	void *buffer;			/* 0x04 */
} HdmiDdcRequest;

/* HdmiDeviceAD80244SetVideoConfig: what the code needs besides the configuration. */
typedef struct HdmiVideoTiming {
	int syncPolarity;		/* 0x00: 0, 2, 3; 0xff: unknown */
	int pixelRepetition;		/* 0x04: 0xff: unknown */
} HdmiVideoTiming;

/* device/hdmi_device.c */
extern SceUInt8 edidMode;
extern SceUInt8 hdcpMode;
extern int smWaitTime;
int _sceHdmiDeviceSuspend(void);
int _sceHdmiDeviceSuspendDone(void);
int _sceHdmiDeviceResume(void);		/* Sony's name */
int hdmiDeviceSetHdcpMode(SceUInt8 mode);
int hdmiDeviceSetEdidMode(SceUInt8 mode);
int hdmiDeviceIsHdcpOff(void);
int hdmiDeviceIsEdidOn(void);
int hdmiDeviceIsEdidForced(void);
int hdmiDeviceIsEdidNotForced(void);
int hdmiDeviceEnableIntr(SceUInt32 type);
int hdmiDeviceDisableIntr(SceUInt32 type);
int hdmiDeviceGetPowered(int *powered);
int hdmiDeviceSetPowered(int powered);
int HdmiDeviceHwReset(HdmiDevice *device);
int HdmiDeviceHwGetChipId(HdmiDevice *device, SceUInt8 *id, SceUInt8 *length);
int HdmiDeviceHwGetPlgStat(HdmiDevice *device);
int HdmiDeviceHwGetPllStat(HdmiDevice *device);
int HdmiDeviceHwSetFrameDirect(HdmiDevice *device, SceUInt32 frame, const SceUInt8 *data,
			       SceUInt32 size);
int HdmiDeviceHwSetFrame(HdmiDevice *device, SceUInt32 frames);
int HdmiDeviceHwSetPacketCtrl(HdmiDevice *device, SceUInt32 frames);
int HdmiDeviceHwSetAvMute(HdmiDevice *device, int mute);
int HdmiDeviceHwSetVideoMute(HdmiDevice *device, int mute);
int HdmiDeviceHwSetAudioMute(HdmiDevice *device, int mute);
int HdmiDeviceHwSetHDMIMode(HdmiDevice *device, int hdmi);
int HdmiDeviceHwCheckRepeater(HdmiDevice *device);
int HdmiDeviceHwAuthStart(HdmiDevice *device);
int HdmiDeviceHwAuthStop(HdmiDevice *device);
int HdmiDeviceHwSetCypher(HdmiDevice *device, int on);
int HdmiDeviceHwReadAksv(HdmiDevice *device, SceUInt8 *aksv);
int HdmiDeviceHwReadBksv(HdmiDevice *device, SceUInt8 *bksv);
int HdmiDeviceHwReadBcaps(HdmiDevice *device, SceUInt8 *bcaps);
int HdmiDeviceHwReadBstatus(HdmiDevice *device, SceUInt8 *bstatus);
int HdmiDeviceHwReadBksvCount(HdmiDevice *device, SceUInt8 *count);
int HdmiDeviceHwReadKSVListFromFIFO(HdmiDevice *device, SceUInt8 size, void *buffer);
int HdmiDeviceHwSetTMDSPower(HdmiDevice *device, int on);
int HdmiDeviceHwSetVideoConfig(HdmiDevice *device, const HdmiDeviceVideoConfig *config);
int HdmiDeviceHwSetAudioConfig(HdmiDevice *device, const HdmiDeviceAudioConfig *config);
int HdmiDeviceHwPowerOn(HdmiDevice *device);
int HdmiDeviceHwRestartEdid(HdmiDevice *device);
int HdmiDeviceHwSetEdidSegment(HdmiDevice *device, SceUInt32 segment);

/* device/hdmi_device_i2c.c */
int sceHdmiDeviceI2cRead(int bus, SceUInt8 slave, SceUInt8 reg, void *data, SceSize size);
int sceHdmiDeviceI2cWrite(int bus, SceUInt8 slave, SceUInt8 reg, const void *data, SceSize size);

/* device/adi/hdmi_ad80244.c */
int HdmiDeviceAD80244SetCecAddress(HdmiDevice *device);
int HdmiDeviceAD80244SetFixedRegisters(HdmiDevice *device);
int HdmiDeviceAD80244Reset(HdmiDevice *device);
int HdmiDeviceAD80244GetChipId(HdmiDevice *device, SceUInt8 *id, SceUInt8 *length);
int HdmiDeviceAD80244PowerOn(HdmiDevice *device);
int HdmiDeviceAD80244GetIntrStatus(HdmiIntrRegs *regs);
int HdmiDeviceAD80244ClearIntrStatus(const HdmiIntrRegs *regs);
int HdmiDeviceAD80244ClearBksvFlag(int unused);
int sceHdmiDeviceAD80244GetIntrMask(HdmiIntrRegs *regs);		/* Sony's name */
int sceHdmiDeviceAD80244SetIntrMask(HdmiIntrRegs *regs);		/* Sony's name */
int HdmiDeviceAD80244ReadPlgStat(SceUInt8 *status);
int HdmiDeviceAD80244EdidReread(HdmiDevice *device);
int HdmiDeviceAD80244SetEdidSegment(HdmiDevice *device, SceUInt32 segment);
int HdmiDeviceAD80244SetFrameDirect(HdmiDevice *device, SceUInt32 frame, const SceUInt8 *data,
				    SceUInt32 size);
int HdmiDeviceAD80244SetFrame(HdmiDevice *device, SceUInt32 frames);
int HdmiDeviceAD80244SetPacketCtrl(HdmiDevice *device, SceUInt32 frames);
int sceHdmiDeviceAD80244GetPlgStat(HdmiDevice *device);		/* Sony's name */
int sceHdmiDeviceAD80244GetPllStat(HdmiDevice *device);		/* Sony's name */
int HdmiDeviceAD80244SetTMDSPower(HdmiDevice *device, int on);	/* Sony's name */
int HdmiDeviceAD80244SetVideoConfig(HdmiDevice *device, const HdmiDeviceVideoConfig *config,
				    const HdmiVideoTiming *timing);
int HdmiDeviceAD80244SetAudioConfig(HdmiDevice *device, const HdmiDeviceAudioConfig *config);
int HdmiDeviceAD80244SetAvMute(HdmiDevice *device, int mute);
int HdmiDeviceAD80244SetAudioMute(HdmiDevice *device, int mute);
int HdmiDeviceAD80244SetVideoMute(HdmiDevice *device, int mute);
int HdmiDeviceAD80244SetHDMIMode(HdmiDevice *device, int hdmi);	/* Sony's name */
int HdmiDeviceAD80244AuthCtrl(HdmiDevice *device, int on);		/* Sony's name */
int HdmiDeviceAD80244SetCypher(HdmiDevice *device, int on);		/* Sony's name */
int HdmiDeviceAD80244CheckRepeater(HdmiDevice *device);
int HdmiDeviceAD80244ReadAksv(HdmiDevice *device, SceUInt8 *aksv);
int HdmiDeviceAD80244ReadBksv(HdmiDevice *device, SceUInt8 *bksv);
int HdmiDeviceAD80244ReadBcaps(HdmiDevice *device, SceUInt8 *bcaps);
int HdmiDeviceAD80244ReadBstatus(HdmiDevice *device, SceUInt8 *bstatus);
int HdmiDeviceAD80244ReadBksvCount(HdmiDevice *device, SceUInt8 *count);

/* device/hdmi_device_ddc.c */
int hdmiDeviceDdcInitialize(HdmiDevice *device);
int hdmiDeviceDdcClearAbort(HdmiDevice *device);
int hdmiDeviceDdcAbort(void);
int hdmiDeviceDdcSetEvent(SceUInt32 bits);
int hdmiDeviceDdcClearEvent(SceUInt32 bits);
int hdmiDeviceDdcWaitEvent(SceUInt32 *events, SceUInt32 pattern, SceUInt32 *timeout);
int sceHdmiDeviceDdcRead(HdmiDevice *device, const HdmiDdcRequest *request, SceUInt32 timeout,
			 SceUInt8 block);					/* Sony's name */
int hdmiDeviceEdidReadBlock(HdmiDevice *device, int block, SceUInt8 *data);

/* device/hdmi_device_edid.c */
int hdmiDeviceEdidInitialize(HdmiDevice *device);
void hdmiDeviceEdidClear(HdmiDevice *device);
int hdmiDeviceEdidSetDviMode(HdmiDevice *device);
int hdmiDeviceEdidCheckSinkType(HdmiDevice *device);
int hdmiDeviceEdidRead(HdmiDevice *device);
int hdmiDeviceEdidGetPhysicalAddress(HdmiDevice *device, SceUInt8 *address);

/* device/hdmi_device_packet.c */
int hdmiDeviceSmStopPackets(HdmiDevice *device);
int hdmiDeviceSmSetPacketData(HdmiDevice *device, SceUInt32 type, const void *packet,
			      SceUInt8 size);
int hdmiDeviceSmSetPacketCtrl(HdmiDevice *device);

/* device/hdmi_device_sm.c */
int hdmiDeviceSmInitialize(void);
int hdmiDeviceSmPollEvent(SceUInt32 *events, SceUInt32 pattern);
int hdmiDeviceSmWaitEvent(SceUInt32 *events, SceUInt32 pattern);
int hdmiDeviceSmSetEvent(SceUInt32 bits);
int hdmiDeviceSmClearAllEvents(void);
int hdmiDeviceSmClearEvent(SceUInt32 bits);
void hdmiDeviceSmAuthReset(HdmiDevice *device);
int hdmiDeviceSmAuthenticate(HdmiDevice *device);
int _sceHdmiDeviceSmRestart(HdmiDevice *device);
int _sceHdmiDeviceSmStopService(HdmiDevice *device, int wait);	/* Sony's name */
int _sceHdmiDeviceRegisterEventCallback(SceUInt32 event, HdmiEventCallback callback); /* Sony's name */
int _sceHdmiDeviceSmStartService(HdmiDevice *device);

/* device/hdmi_device_status.c */
int hdmiDeviceStatusInitialize(HdmiDevice *device);
int hdmiDeviceSmLock(void);
int hdmiDeviceSmUnlock(void);
int hdmiDeviceStatusIsStopRequested(HdmiDevice *device);
int hdmiDeviceStatusIsUnplugged(HdmiDevice *device);
int hdmiDeviceStatusHasRequest(HdmiDevice *device);
int hdmiDeviceStatusIsResetRequested(HdmiDevice *device);
int hdmiDeviceStatusIsModeRequested(HdmiDevice *device);
int hdmiDeviceStatusIsReady(HdmiDevice *device);
int hdmiDeviceStatusNeedsReauth(HdmiDevice *device);
int hdmiDeviceStatusIsChanged(HdmiDevice *device);
int hdmiDeviceStatusGetPllStat(HdmiDevice *device);
int hdmiDeviceStatusGetPlgStat(HdmiDevice *device);

/* What cec.c gives cecDevInitialize: the CEC interrupt events. */
typedef struct CecDevCallbacks {
	int (*txDone)(void);		/* 0x00 */
	int (*rx)(void);		/* 0x04 */
	int (*reserved)(void);		/* 0x08: never set */
	int (*nop1)(void);		/* 0x0c */
	int (*disconnected)(void);	/* 0x10 */
	int (*nop2)(void);		/* 0x14 */
	int (*nop3)(void);		/* 0x18 */
	int (*connected)(void);		/* 0x1c */
	int (*reserved2)(void);		/* 0x20: never set */
	int (*standby)(void);		/* 0x24 */
	int (*isEnabled)(void);		/* 0x28 */
	int (*standbyDone)(void);	/* 0x2c */
} CecDevCallbacks;

/* cec.c */
int cecInitialize(void);

/* device/hdmi_device_cec.c */
int cecDevInitialize(const CecDevCallbacks *callbacks);	/* Sony's name */
int cecDevEnable(void);
int cecDevDisable(void);
int cecDevSetLogicalAddress(SceUInt8 address);
int cecDevGetPhysicalAddress(SceUInt8 *physical);
int cecDevGetLogicalAddress(SceUInt8 *address);
int cecDevTx(const SceUInt8 *frame, SceUInt32 length);	/* Sony's name */
int cecDevGetRxFrame(SceUInt8 id, SceUInt8 *frame, SceUInt32 length);
int cecDevGetRxLength(SceUInt8 id, SceUInt32 *length);
int cecDevRxDone(SceUInt8 id);				/* Sony's name */
int cecDevRxGetPending(SceUInt8 *ids, SceUInt32 *count);
int cecDevSetMode(int mode);
int cecDevSuspend(void);
int cecDevSuspendDone(void);
int cecDevResume(void);
int cecDevIntrHandler(SceUInt32 status);
void cecDevNotifyHpd(int on);
void cecDevNotifyRxSense(int on);
void cecDevPoll(void);
void cecDevSetPhysicalAddress(const SceUInt8 *physical);
