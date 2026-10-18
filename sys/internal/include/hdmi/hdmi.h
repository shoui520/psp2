/*
 * SceHdmi (os0:kd/hdmi.skprx) structures shared with its callers (SceDisplay).
 * No Sony name is known for these types or fields; the names describe how
 * SceHdmi uses each one.
 */
#pragma once

int SceHdmiForDriver_99A3CABE(int mute);
int SceHdmiForDriver_08AFFB03(int mute);

#include <psp2kern/types.h>

int SceHdmiForDriver_B04F5B6A(void);
int SceHdmiForDriver_D929E4E5(void);
int SceHdmiForDriver_EA486A37(int start);
int sceHdmiCecCmdOneTouchPlay(int mode);

/* SceHdmiForDriver_5A212BB3: the output mode (16 bytes). */
typedef struct SceHdmiOutputMode {
	SceUInt16 vic;			/* 0x00: SceDisplay output mode (0x8100, ...), 0xffff: none */
	SceUInt16 inputFormat;		/* 0x02: 0: RGB, 1..3: YCbCr colorimetry */
	SceUInt16 outputFormat;		/* 0x04: 0: RGB, 1: YCbCr 4:4:4, 2: YCbCr 4:2:2, 3 */
	SceUInt16 colorDepth;		/* 0x06: 0: 24 bit, 1: 30 bit, 2: 36 bit */
	SceUInt16 inputFullRange;	/* 0x08: 1: full range input */
	SceUInt16 outputFullRange;	/* 0x0a: 1: full range output */
	SceUInt16 reserved;		/* 0x0c */
	SceUInt16 scanInfo;		/* 0x0e: AVI InfoFrame scan information + 1 */
} SceHdmiOutputMode;

/* SceHdmiForDriver_854EEB78: the audio mode (18 bytes). */
typedef struct SceHdmiAudioMode {
	SceUInt16 source;		/* 0x00: 0: S/PDIF, 1: I2S */
	SceUInt16 coding;		/* 0x02: 1: PCM */
	SceUInt16 channelCount;		/* 0x04: 2..8 */
	SceUInt16 sampleRate;		/* 0x06: 1: 32 kHz, 2: 44.1 kHz, 3: 48 kHz, ... */
	SceUInt16 spdifLayout;		/* 0x08 */
	SceUInt16 reserved;		/* 0x0a */
	SceUInt8 speakerAllocation;	/* 0x0c: audio InfoFrame CA */
	SceUInt8 reserved2;		/* 0x0d */
	SceUInt16 downmixInhibit;	/* 0x0e */
	SceUInt16 levelShift;		/* 0x10 */
} SceHdmiAudioMode;

/*
 * SceHdmiForDriver_8EB0FD9C: what SceHdmi learnt from the sink's EDID
 * (0xf4 bytes). videoFormats[] are SceHdmi's format bits: [0] CEA codes 2..10
 * (bits 0..8) and 11..16 (bits 12..17), [1] codes 17..31, [2] codes 32 and 34,
 * [3] code 1 and the established VGA/SVGA timings.
 */
typedef struct SceHdmiEdidInfo {
	SceUInt8 productId[10];		/* 0x00: EDID bytes 8..17: manufacturer, product, serial, week, year */
	SceUInt8 blockCount;		/* 0x0a: 1, or 2 with extensions */
	char monitorName[13];		/* 0x0b: monitor name descriptor */
	SceUInt8 reserved[3];		/* 0x18 */
	SceUInt8 quantizationSelectable;	/* 0x1b: video capability data block QS */
	struct {
		SceUInt32 supported;
		SceUInt32 reserved;
	} videoFormats[4];		/* 0x1c */
	SceUInt8 reserved2[0x1c];	/* 0x3c */
	SceUInt16 chromaticity[8];	/* 0x58: red x, y, green x, y, blue x, y, white x, y (low 8 bits) */
	SceUInt16 reserved3;		/* 0x68 */
	SceUInt8 maxImageWidth;		/* 0x6a: cm */
	SceUInt8 maxImageHeight;	/* 0x6b: cm */
	SceUInt16 contentTypes;		/* 0x6c: HDMI vendor block content types (bit 3: game) */
	SceUInt8 reserved4[0x86];	/* 0x6e */
} SceHdmiEdidInfo;

typedef void (*HdmiEventCallback)(void);

/* SceHdmiForDriver_93AA48AC */
typedef struct CecStatistics {
	SceSize size;			/* 0x00 */
	SceUInt32 reserved;		/* 0x04 */
	SceUInt64 time;			/* 0x08: first event counted */
	SceUInt32 oneTouchPlay;		/* 0x10 */
	SceUInt32 standby;		/* 0x14 */
	SceUInt32 userControl;		/* 0x18 */
	SceUInt32 reserved2;		/* 0x1c */
} CecStatistics;

int SceHdmiForDriver_0F12364A(SceUInt32 event, HdmiEventCallback callback);
int SceHdmiForDriver_291F000B(void);
int sceHdmiCecRegisterEventCallback(int event, void (*callback)(int event));
int SceHdmiForDriver_47894820(int mode);
int SceHdmiForDriver_47B753EB(void);
int SceHdmiForDriver_6D2D10D8(void);	/* CEC forced polling message */
int SceHdmiForDriver_854EEB78(const SceHdmiAudioMode *mode);
int SceHdmiForDriver_8EB0FD9C(SceHdmiEdidInfo *info);
int SceHdmiForDriver_93AA48AC(CecStatistics *stats);
int sceHdmiCecEnableCec(void);

