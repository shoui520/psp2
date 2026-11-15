/*
 * The Bluetooth audio source (bt/sbc.c, 0x4cee0..): the ROM's SBC codec and
 * PCM port drivers ("mdev_sbcEnc", "mdev_sbcDec", "mdev_bt_pcm"), registered
 * by the image, and the encoder that fills the packets of the A2DP stream.
 *
 * Marvell published no source; names are inferred from use. Which of the two
 * driver pairs the ROM keeps is the codec and which the PCM port is inferred:
 * the second pair's code is the one given the interrupt hooks.
 */
#ifndef ROBIN_SBC_H
#define ROBIN_SBC_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/mdev.h"
#include "robin/sys.h"

#define SBC_DRIVERS		2

/* mdevIoctl() operations of the codec and PCM devices (meanings inferred). */
#define SBC_IOCTL_SET_BUFFER	0x25
#define SBC_IOCTL_CONFIGURE	0x26
#define SBC_IOCTL_QUEUE		0x2c

/* The devices the encoder opens (btSbcOpenCodecDevices(), 0x4d770). */
struct SbcDevices {
	uint32_t reserved[3];
	void *pcm;			/* "mdev_bt_pcm" */
	void *encoder;			/* "mdev_sbcEnc" */
	void *decoder;			/* "mdev_sbcDec" */
	uint32_t primaryEntry;
};

_Static_assert(sizeof(struct SbcDevices) == 0x1c, "SBC devices");

/* The encoder's frames and the packet they go into, set up by fmCoexProcessPacket(). */
struct SbcFrame {
	uint16_t active;		/* 1 once set up */
	uint16_t frames;		/* the frames in the packet being filled */
	uint16_t headerLength;		/* where the first frame goes in the packet */
	uint16_t length;		/* an encoded frame, as the encoder's configuration answers */
	uint16_t dataOffset;		/* the PCM samples' place in their buffer */
	uint16_t samples;		/* the PCM samples per frame (btSbcRomClearState()) */
	uint16_t sampleBytes;		/* of 16 bits each */
	uint16_t event;			/* 4: the samples follow the previous frame's */
};

/* The PCM port's format (SBC_IOCTL_SET_BUFFER, SBC_IOCTL_CONFIGURE). */
struct SbcPcmConfig {
	uint8_t enable;			/* 1 */
	uint8_t mono;			/* the encoder's channel mode is 0 (mono) */
	uint8_t cleared[4];		/* 0 */
	uint8_t sampleBits;		/* 8 */
	uint8_t reserved7;
	uint8_t rate;			/* the port's code for the sampling frequency */
	uint8_t reserved9[3];
};

_Static_assert(sizeof(struct SbcPcmConfig) == 12, "SBC PCM configuration");

/* The encoder's settings, as the A2DP configuration gives them. */
struct SbcEncoderConfig {
	uint16_t reserved0[3];
	uint16_t channelMode;		/* 0: mono */
	uint16_t reserved8;
	uint16_t samplingFrequency;	/* 0: 16 kHz, 1: 32 kHz, 2: 44.1 kHz, 3: 48 kHz */
};

_Static_assert(sizeof(struct SbcEncoderConfig) == 12, "SBC encoder configuration");

/* The PCM port's codes for the SBC sampling frequencies. */
#define SBC_PCM_RATE_16K	3
#define SBC_PCM_RATE_32K	6
#define SBC_PCM_RATE_44K	7
#define SBC_PCM_RATE_48K	8

/* What the PCM port is asked to fill (SBC_IOCTL_QUEUE). */
struct SbcPcmRequest {
	uint32_t length;
	uint32_t mode;			/* 2 on a reconfiguration; fmCoexProcessPacket() leaves it unset, as the original does */
	uint8_t *data;
};

/* The A2DP stream the packets carry; byte 3 the frames per packet. */
struct SbcStream {
	uint8_t reserved[3];
	uint8_t framesPerPacket;
	uint8_t reserved4[64];
};

_Static_assert(sizeof(struct SbcStream) == 0x44, "SBC stream");

/* The ROM's variables (boot/rom_syms.ld). */
extern struct MdevDriver sbcCodecDrivers[SBC_DRIVERS];	/* SQRAM 0xc0033234 */
extern struct MdevDriver sbcPcmDrivers[SBC_DRIVERS];	/* SQRAM 0xc00331a4 */
extern const char *sbcCodecDriverNames[SBC_DRIVERS];	/* DTCM 0x400fa94 */
extern uint32_t sbcCodecDriverStatus[SBC_DRIVERS];	/* DTCM 0x400fa84 */
extern const char *sbcPcmDriverNames[SBC_DRIVERS];	/* DTCM 0x400fa6c */
extern uint32_t sbcPcmDriverStatus[SBC_DRIVERS];	/* DTCM 0x400fa64 */
extern int (*sbcPcmIrqRegister)(uint32_t line, SysIrqHandler handler);	/* DTCM 0x400f2e4 */
extern int (*sbcPcmIrqUnregister)(uint32_t line);	/* DTCM 0x400f2e8 */
extern uint16_t sbcPacketCapacity;			/* DTCM 0x400fa1a: the payload a packet holds */
extern OsaBuffer *sbcPacket;				/* DTCM 0x400fa20: the packet being filled */
extern struct SbcDevices sbcDevices;			/* SQRAM 0xc00330ac */
extern struct SbcPcmConfig sbcPcmConfig;			/* SQRAM 0xc0033050 */
extern struct SbcEncoderConfig sbcEncoderConfig;			/* SQRAM 0xc003305c */
extern struct SbcStream sbcStream;			/* SQRAM 0xc0033068 */
extern struct SbcFrame sbcFrame;			/* SQRAM 0xc0033184 */

/* The ROM's driver code; the arguments are the mdev framework's. */
void btSbcRomFormatState(void);
void btSbcRomInspectState(void);
void btSbcRomTrackState(void);
void btSbcRomManageState(void);
void btSbcRomAcquireState(void);
void btSbcRomSetupState(void);
void btSbcRomReleaseState(void);
void btSbcRomCheckState(void);
void btSbcRomValidateState(void);
void btSbcRomPrepareState(void);
void btSbcRomDispatchState(void);
void btSbcRomParseState(void);
void btSbcRomComputeState(void);
void btSbcRomBuildState(void);
unsigned int btSbcRomClearState(void);			/* the PCM samples per SBC frame */

/* bt/sbc.c */
void fmCoexFilterBuffer(void);				/* 0x4cee0 */
void fmCoexHandlePacket(void);				/* 0x4cf4c */
int fmCoexProcessPacket(void);				/* 0x4cfd4 */
int fmCoexUpdatePacket(void);				/* 0x4d05e */

#endif /* ROBIN_SBC_H */
