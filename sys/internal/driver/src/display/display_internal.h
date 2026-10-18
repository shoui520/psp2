/*
 * SceDisplay internal state.
 *
 * No debug record names a structure or field of this module; the names below
 * describe what the code does with each one.
 */
#pragma once

#include <psp2kern/types.h>
#include <sysmem/cpu.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <lowio/iftu.h>
#include <psp2common/display.h>
#include <dtrace/sce/dtrace.h>
#include <display/display.h>

#define DISPLAY_HEAD_COUNT	3	/* 0: main panel (OLED/LCD), 1: HDMI, 2: unused */
#define DISPLAY_PLANE_COUNT	2

/* What one process shows on one IFTU plane of one head. */
typedef struct DisplayPlane {
	SceUID owner;			/* 0x00: process shown on the plane, -1: none */
	SceUIntPtr paddr;		/* 0x04: frame buffer last sent to the IFTU */
	SceUInt32 scaleFlags;		/* 0x08: SceDisplayForDriver_EB390A76 flags */
	float scale;			/* 0x0c: SceDisplayForDriver_EB390A76 scale */
	SceUInt32 viewport[4];		/* 0x10: x, y, width, height (16.16 of the frame) */
} DisplayPlane;

typedef struct DisplayHead {
	SceKernelSpinlock lock;		/* 0x00 */
	int dsiEnabled;			/* 0x04: DSI and IFTU output running */
	int enabled;			/* 0x08: SceDisplayForDriver_496032D6 */
	SceUInt32 vcount;		/* 0x0c: vblanks counted */
	SceUID vblankEvent;		/* 0x10: SceUIDVblankEventClass object of the head */
	void *vblankEventObject;	/* 0x14 */
	SceUInt32 eventBits;		/* 0x18: pattern pulsed on a vblank or a new frame buffer */
	int dsiBus;			/* 0x1c: DSI controller driving the head */
	int iftuPlane[DISPLAY_PLANE_COUNT];	/* 0x20: IFTU plane, -1: none */
	SceUInt32 vic;			/* 0x28: output mode (SceDsiForDriver_97BFEA76) */
	SceUInt32 width;		/* 0x2c: output resolution */
	SceUInt32 height;		/* 0x30 */
	SceUInt32 outputFlags;		/* 0x34: SceDisplayForDriver_AF5EE5BE flags */
	SceUInt32 outputFormat;		/* 0x38: IFTU output format */
	SceUInt32 pixelSize;		/* 0x3c: DSI bits per pixel */
	float refreshRate;		/* 0x40 */
	int interlaced;			/* 0x44 */
	SceUInt32 gain;			/* 0x48: 16.16 colour gain (brightness) */
	int invertColors;		/* 0x4c */
	DisplayPlane plane[DISPLAY_PLANE_COUNT];	/* 0x50 */
	SceUID frameBufCallback;	/* 0x90 */
	SceUInt32 mergeSetting;		/* 0x94 */
} DisplayHead;

#define DISPLAY_FRAMEBUF_SIZE		sizeof(SceDisplayFrameBuf)
#define DISPLAY_FRAMEBUF_PARAM_SIZE	sizeof(DisplayFrameBufParam)

/* The frame buffer a process set for one plane of one head. */
typedef struct DisplayFrameBufRecord {
	SceDisplayFrameBuf frameBuf;	/* 0x00: as passed (size 0x18 or 0x1c) */
	SceUInt32 resolution;		/* 0x18: DisplayFrameBufParam.resolution */
	SceUIntPtr paddr;		/* 0x1c: 0: no frame buffer */
	SceUInt32 iftuPixelformat;	/* 0x20 */
	SceUInt32 flags;		/* 0x24: resolution flags */
	SceUInt32 vcountShown;		/* 0x28: head vcount when it is shown */
	SceUInt32 vcountChanged;	/* 0x2c: head vcount of the last format change */
	int notified;			/* 0x30: frame buffer callback notified */
	SceUInt16 cropWidth;		/* 0x34: PSP emulation crop - 1, 0: none */
	SceUInt16 cropHeight;		/* 0x36 */
	SceUInt16 cropX;		/* 0x38 */
	SceUInt16 cropY;		/* 0x3a */
	SceUInt32 reserved[2];		/* 0x3c */
} DisplayFrameBufRecord;

/* Process local storage of SceDisplay (0x1d8 bytes). */
typedef struct DisplayProcessLocal {
	DisplayFrameBufRecord frameBuf[2][DISPLAY_PLANE_COUNT];	/* 0x000: [head][plane] */
	SceUInt32 reserved[0x22];	/* 0x110 */
	SceUInt32 vcountBase[3];	/* 0x198: vcount offsets while suspended */
	SceUInt32 vcountStopped[3];	/* 0x1a4: vcounts when the process stopped */
	SceUID frameEvent;		/* 0x1b0: simple event pulsed on a frame change */
	SceUInt32 resolutionFlags;	/* 0x1b4: from SceProcessmgrForDriver_E1A67C86 */
	SceUInt32 reserved2[8];		/* 0x1b8 */
} DisplayProcessLocal;

/* SceDisplayForDriver_EE5EB52D / SceDisplayForDriver_40ACFE51 (0x14 bytes). */
typedef struct SceDisplayViewportConf {
	SceSize size;			/* 0x00: sizeof(SceDisplayViewportConf) */
	SceUInt32 x;			/* 0x04: in a 960x544 frame */
	SceUInt32 y;			/* 0x08 */
	SceUInt32 width;		/* 0x0c */
	SceUInt32 height;		/* 0x10 */
} SceDisplayViewportConf;

/* Interrupt state of a vblank SGI (SceDisplayVbStartSGI): subinterrupts enabled. */
typedef struct DisplaySgiState {
	SceKernelSpinlock lock;		/* 0x00 */
	SceUInt32 enabled;		/* 0x04: bit n: subinterrupt n */
} DisplaySgiState;

/* Thread local: vcounts of the vblank waits the thread is nested in. */
typedef struct DisplayVblankWait {
	SceUInt32 depth;		/* 0x00 */
	SceUInt32 vcount[7][3];		/* 0x04: [depth][head] */
} DisplayVblankWait;

/* Main OLED panel: what was asked, and what the panel was last set to at vblank. */
typedef struct DisplayOledState {
	int present;			/* 0x00 */
	int suspended;			/* 0x04 */
	int wantOn;			/* 0x08 */
	int isOn;			/* 0x0c: -1: unknown */
	SceUInt32 brightness;		/* 0x10 */
	SceUInt32 curBrightness;	/* 0x14 */
	int colorSpace;			/* 0x18 */
	int curColorSpace;		/* 0x1c */
} DisplayOledState;

/* Main LCD panel. */
typedef struct DisplayLcdState {
	int present;			/* 0x00 */
	int suspended;			/* 0x04 */
	int wantOn;			/* 0x08 */
	int isOn;			/* 0x0c: -1: unknown */
	SceUInt32 brightness;		/* 0x10 */
	SceUInt32 curBrightness;	/* 0x14 */
	int mode;			/* 0x18: SceLcdForDriver_1D73D7F3 */
	int colorSpace;			/* 0x1c */
	int curColorSpace;		/* 0x20 */
} DisplayLcdState;

/* HDMI transmitter. */
typedef struct DisplayHdmiState {
	int present;			/* 0x00 */
	int probed;			/* 0x04 */
} DisplayHdmiState;

/* SceHdmiForDriver_5A212BB3: output mode. */
typedef struct DisplayHdmiMode {
	SceUInt16 vic;			/* 0x00 */
	SceUInt16 format[4];		/* 0x02: from the pixel format; [2]: colour depth */
	SceUInt16 reserved[3];		/* 0x0a */
} DisplayHdmiMode;

/* display.c */
extern DisplayHead heads[DISPLAY_HEAD_COUNT];
extern int hdmiOnly;
extern SceUID devOwnerPid;
extern SceUInt32 *devStatus;
extern int primaryHead;
extern int startupHead;
extern int bootLogoShown;
extern int plsKey;
int setFrameBufForCompat(int x, int y, int cropWidth, int cropHeight,
			 const SceDisplayFrameBuf *pParam, const SceDisplayFrameBuf *convDst);

/* vblank.c */
int vblankInit(void);
int vblankTimerStop(void);
int vblankTimerStart(void);
int vblankResume(void);

/* bootlogo.c */
int showBootLogo(SceUInt32 head);
void freeBootLogo(SceUInt32 head);

/* oled.c */
int oledInit(void);
int oledSuspend(void);
void oledVblank(void);
int oledStart(void);
int _sceDisplayOledSetEnable(int on);
int oledBusy(void);
int oledSetBrightness(SceUInt32 brightness);
int oledSetColorSpaceMode(int mode);

/* lcd.c */
int lcdInit(void);
int lcdSuspend(void);
void lcdVblank(void);
int lcdStart(void);
int lcdSetPower(int on);
int lcdBusy(void);
int lcdSetBrightness(SceUInt32 brightness);
int lcdSetMode(int mode);
int lcdSetColorSpaceMode(int mode);

/* hdmi.c */
int hdmiInit(void);
int _sceDisplaySetupHdmiTx(SceUInt32 vic, SceUInt32 pixelformat, SceUInt32 pixelSize);
int _sceDisplaySetHdmiTxMute(int mute);
