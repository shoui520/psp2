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
#include "robin/btrom.h"
#include "robin/buffer.h"
#include "robin/mdev.h"
#include "robin/sys.h"

#define SBC_DRIVERS		2

/* mdevIoctl() operations of the codec and PCM devices (meanings inferred). */
#define SBC_IOCTL_SET_CALLBACK	0x20
#define SBC_IOCTL_SET_MODE	0x2b
#define SBC_IOCTL_SET_BUFFER	0x25
#define SBC_IOCTL_CONFIGURE	0x26
#define SBC_IOCTL_STATUS	0x27	/* bit 0: busy */
#define SBC_IOCTL_QUEUE		0x2c
#define SBC_IOCTL_CONSUMED	0x2d	/* the bytes the decoder took */

/* The devices the encoder opens (btSbcOpenCodecDevices(), 0x4d770). */
struct SbcDevices {
	uint32_t reserved0;
	uint32_t state;			/* 0: the encoder is set up on start, 2: set up again */
	uint32_t reserved8;
	void *pcm;			/* "mdev_bt_pcm" */
	void *encoder;			/* "mdev_sbcEnc" */
	void *decoder;			/* "mdev_sbcDec" */
	void *sinkPcm;			/* the PCM port the decoder plays into */
};

_Static_assert(sizeof(struct SbcDevices) == 0x1c, "SBC devices");

/*
 * The frames of the encoder (sbcFrame, set up by fmCoexProcessPacket()) or of
 * the decoder (sbcDecoderFrame, btSbcHandleState()). For the decoder, active
 * counts the frames left in the received packet, headerLength is where its
 * first frame starts and length what the decoder has consumed of it.
 */
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
	union {
		struct {
			uint8_t enable;		/* 1 */
			uint8_t mono;		/* the encoder's channel mode is 0 (mono) */
			uint8_t cleared[4];	/* 0 */
			uint8_t sampleBits;	/* 8 */
			uint8_t reserved7;
			uint8_t rate;		/* the port's code for the sampling frequency */
			uint8_t reserved9[3];
		};
		uint32_t control;		/* bits 13-17: the decoder's sample format */
	};
};

#define SBC_PCM_DECODER_FORMAT_MASK	(0x1fu << 13)
#define SBC_PCM_DECODER_FORMAT		(0xfu << 13)

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

/*
 * The A2DP stream the packets carry. header is what precedes the frames in
 * each packet: the HCI ACL header (its length in bytes 2-3), the L2CAP header
 * (its length in bytes 4-5), the RTP header (12 bytes: the sequence number in
 * bytes 2-3, the timestamp in 4-7, big-endian) and the SBC payload header (its
 * last byte, the frame count in bits 0-3).
 */
struct SbcStream {
	uint8_t reserved[2];
	uint8_t headerLength;
	uint8_t framesPerPacket;
	uint8_t header[64];
};

#define SBC_RTP_HEADER		12
#define SBC_MEDIA_OFFSET	36	/* the packet's data offset: room for the transport's header */
#define SBC_MEDIA_TAG		0x11	/* OsaBuffer.transportTag and the event of the HCI device */

_Static_assert(sizeof(struct SbcStream) == 0x44, "SBC stream");

/*
 * The PCM port's descriptor for a stream (btSbcValidateState()): the format
 * bits are copied from the host's PCM settings, the counts from the encoder's
 * and the decoder's frames. Which setting bit is which is not known; the
 * fields are named by the setting they come from.
 */
struct __attribute__((packed)) SbcPcmDescriptor {
	uint8_t portBit1 : 1;		/* sbcPcmPortConfig bit 1 */
	uint8_t portBit2 : 1;		/* sbcPcmPortConfig bit 2 */
	uint8_t enable : 1;		/* 1 */
	uint8_t cleared3 : 1;		/* 0 */
	uint8_t cleared4 : 1;		/* 0 */
	uint8_t slotMode : 2;		/* sbcPcmSlotConfig bits 0-1 */
	uint8_t valid : 1;		/* 1 */
	uint8_t portBit0 : 1;		/* sbcPcmPortConfig bit 0 */
	uint8_t linkMode : 2;		/* sbcPcmLinkConfig bits 0-1 */
	uint8_t linkBit4 : 1;		/* sbcPcmLinkConfig bit 4 */
	uint8_t longFrame : 1;		/* sbcPcmLinkConfig bit 9: frameSync 0x7f, else 0x7c */
	uint8_t set5 : 1;		/* 1 */
	uint8_t linkBit8 : 1;		/* sbcPcmLinkConfig bit 8 */
	uint8_t reserved1 : 1;
	uint8_t frameSync;
	uint8_t slots;			/* sbcPcmSlotConfig bits 2-7, at least 1 */
	uint16_t encoderOffset;		/* the encoder's sample bytes and the caller's offset */
	uint16_t encoderSamples : 11;
	uint16_t reserved6 : 5;
	uint16_t encoderBytes : 11;
	uint16_t reserved8 : 5;
	uint16_t decoderOffset;
	uint16_t decoderSamples : 11;
	uint16_t reserved12 : 5;
	uint16_t decoderBytes : 11;
	uint16_t reserved14 : 5;
};

_Static_assert(sizeof(struct SbcPcmDescriptor) == 16, "SBC PCM descriptor");

/*
 * A test of the stream that drops frames (btSbcPrepareState()): every frame
 * below a threshold of a 128-frame cycle, at random, or once every threshold
 * frames. The settings come from a host command (btSbcConfigureState()).
 * The bytes in pending are read by btSbcResetState() (0x4da5e) and are named
 * when it is converted. The roles are inferred.
 */
struct SbcFrameDrop {
	uint8_t enable;			/* 1 */
	uint8_t mode;			/* SBC_DROP_* */
	uint8_t pending2[6];
	uint16_t threshold;		/* 20 */
	uint8_t pending10[14];
	uint32_t count;			/* the frames since the last drop */
	uint32_t seed;			/* of the random drops */
};

_Static_assert(sizeof(struct SbcFrameDrop) == 32, "SBC frame drop test");

#define SBC_DROP_CYCLE		1
#define SBC_DROP_RANDOM		2
#define SBC_DROP_EVERY		3

extern struct SbcFrameDrop sbcFrameDrop;		/* 0x55430 */

/* The ROM's variables (boot/rom_syms.ld). */
extern uint8_t sbcPcmPortConfig;			/* DTCM 0x400f8d3 */
extern uint8_t sbcPcmSlotConfig;			/* DTCM 0x400f8d5 */
extern uint16_t sbcPcmLinkConfig;			/* DTCM 0x400f8da */
extern struct MdevDriver sbcCodecDrivers[SBC_DRIVERS];	/* SQRAM 0xc0033234 */
extern struct MdevDriver sbcPcmDrivers[SBC_DRIVERS];	/* SQRAM 0xc00331a4 */
extern const char *sbcCodecDriverNames[SBC_DRIVERS];	/* DTCM 0x400fa94 */
extern uint32_t sbcCodecDriverStatus[SBC_DRIVERS];	/* DTCM 0x400fa84 */
extern const char *sbcPcmDriverNames[SBC_DRIVERS];	/* DTCM 0x400fa6c */
extern uint32_t sbcPcmDriverStatus[SBC_DRIVERS];	/* DTCM 0x400fa64 */
extern int (*sbcPcmIrqRegister)(uint32_t line, SysIrqHandler handler);	/* DTCM 0x400f2e4 */
extern int (*sbcPcmIrqUnregister)(uint32_t line);	/* DTCM 0x400f2e8 */
extern uint16_t sbcPacketCapacity;			/* DTCM 0x400fa1a: the payload a packet holds */
extern OsaBuffer *sbcPacket;				/* DTCM 0x400fa20: the PCM samples' buffer */
extern OsaBuffer *sbcMediaPacket;			/* DTCM 0x400fa2c: the A2DP packet being filled */
extern OsaBuffer *sbcDecoderPacket;			/* DTCM 0x400fa24: the PCM samples the decoder writes */
extern OsaBuffer *sbcReceivedPacket;			/* DTCM 0x400fa28: the A2DP packet being decoded */
extern uint8_t sbcDevicesOpen;				/* DTCM 0x400fa18 */
extern uint8_t sbcHeld;					/* DTCM 0x400fa34: the stream is not started */
extern uint32_t sbcReceiveEmpty;			/* DTCM 0x400fa3c: decoder frames without a packet */
extern int (*sbcPcmStreamHook)(void);			/* DTCM 0x400ef30 */
extern struct OsaMsgList sbcReceiveQueue;		/* SQRAM 0xc003303c: the received A2DP packets */
extern struct SbcFrame sbcDecoderFrame;			/* SQRAM 0xc0033194 */
extern struct MdevDriver sbcAudioDriver;		/* SQRAM 0xc0033204 */
extern uint32_t sbcAudioDriverStatus;			/* DTCM 0x400fa78 */
extern const char *sbcAudioDriverName;			/* DTCM 0x400fa7c */
extern uint32_t sbcMediaDropped;			/* DTCM 0x400fa38: frames without a packet */
extern uint32_t sbcTimestamp;				/* 0x5542c: the RTP timestamp, in frames */
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
int btSbcRomAdjustState(uint32_t argument);
int btSbcRomVerifyState(uint32_t value);
int btSbcRomSelectState(void);
int btSbcRomRecordState(void);
void btSbcRomConvertState(void);
void btSbcRomFilterState(void);
void btSbcRomHandleContext(uint32_t argument);
int btSbcRomUpdateContext(void);
int btSbcRomConfigureContext(void);
int btSbcRomProcessContext(unsigned int mode);
void btSbcRomQueryState(void);
void btSbcRomResetState(void);
void btSbcRomSyncState(void);
void btSbcRomApplyState(void);
void btSbcRomResolveState(void);
void btSbcRomNotifyState(void);
void btSbcRomScheduleState(void);
int btSbcRomParseContext(OsaBuffer *event);
int btSbcRomBuildContext(const uint8_t *data);
int btSbcRomQueryContext(const uint8_t *data);

/* Not converted yet; provisional prototype. */
int btSbcPcmStreamHandler(void);			/* 0x515bc */
void btSbcPcmDmaCallback(void);				/* 0x51764 */

/* bt/sbc.c */
void fmCoexFilterBuffer(void);				/* 0x4cee0 */
void fmCoexHandlePacket(void);				/* 0x4cf4c */
int fmCoexProcessPacket(void);				/* 0x4cfd4 */
int fmCoexUpdatePacket(void);				/* 0x4d05e */
int fmCoexConfigurePacket(void);			/* 0x4d134 */
int btSbcEncoderProcess(uint32_t events, uint32_t argument);	/* 0x4d286 */
int btSbcHandleState(void);				/* 0x4d344 */
int btSbcProcessState(void);				/* 0x4d3dc */
int btSbcEncoderConfigure(uint32_t events, uint32_t argument);	/* 0x4d4ae */
int btSbcDecoderProcess(void);				/* 0x4d544 */
int btSbcUpdateState(void);				/* 0x4d548 */
void btSbcConfigureState(const uint8_t *settings);	/* 0x4d5a4 */
unsigned int btSbcCheckState(uint8_t value);		/* 0x4d5be */
void btSbcValidateState(struct SbcPcmDescriptor *descriptor, uint16_t encoderOffset,
			uint16_t decoderOffset);		/* 0x4d5dc */
int btSbcPrepareState(void);				/* 0x4d716 */
int btSbcCloseCodecDevices(OsaBuffer *event);		/* 0x4d758 */
int btSbcDispatchState(const uint8_t *data);		/* 0x4d760 */
int btSbcParseState(const uint8_t *data);		/* 0x4d768 */
int btSbcOpenCodecDevices(void);			/* 0x4d770 */

#endif /* ROBIN_SBC_H */
