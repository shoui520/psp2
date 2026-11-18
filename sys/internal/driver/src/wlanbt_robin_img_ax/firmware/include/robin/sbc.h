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
#include "robin/threadx.h"

#define SBC_DRIVERS		2

/* mdevIoctl() operations of the codec and PCM devices (meanings inferred). */
#define SBC_IOCTL_SET_CALLBACK	0x20
#define SBC_IOCTL_SET_MODE	0x2b
#define SBC_IOCTL_SET_BUFFER	0x25
#define SBC_IOCTL_CONFIGURE	0x26
#define SBC_IOCTL_STATUS	0x27	/* bit 0: busy */
#define SBC_IOCTL_QUEUE		0x2c
#define SBC_IOCTL_CONSUMED	0x2d	/* the bytes the decoder took */
#define SBC_IOCTL_STOP		0x28	/* the PCM port */
#define SBC_IOCTL_START		0x29	/* the PCM port */
#define SBC_IOCTL_RESTART	0x2a	/* a codec */

/* The SCO stream's requests (btSbcSyncState()). */
#define SBC_SCO_OPEN		0x001
#define SBC_SCO_CLOSE		0x006
#define SBC_SCO_MSBC		0x008
#define SBC_SCO_HOLD		0x040
#define SBC_SCO_RESUME		0x080
#define SBC_SCO_FRAME		0x200

/*
 * A clock register of the Bluetooth audio block: bits 3..0 and 15..8 are
 * dividers, which btSbcTrackState() halves once the path presets change the
 * rate; bit 6 asks for that to be skipped once. Inferred from that use alone.
 */
#define SBC_AUDIO_CLOCK		REG32(0x9000c018)
#define SBC_AUDIO_CLOCK_KEEP	0x40

/* The SCO line the controller's audio interrupt arrives on. */
#define SBC_SCO_IRQ_LINE	21

/* SbcFrame.active of a stream without a codec (CVSD: the PCM samples go to the host as they are). */
#define SBC_FRAME_PCM		2
#define SBC_PCM_SCO_OFFSET	39	/* where the samples go in a SCO packet: after its 3-byte header */

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

/*
 * The PCM stream's format words, given to sbcPcmStreamHook: one word of bit
 * fields (Sony writes them one at a time, a store each), a word the FM radio
 * clears, and the sample rate's code.
 */
struct SbcPcmStream {
	uint32_t enable : 1;		/* cleared by the FM radio */
	uint32_t mono : 1;		/* the FM policy's rate mode is 4 */
	uint32_t reserved2 : 5;
	uint32_t format : 4;		/* the codec's interface format: 0 (0x10) or 1 (8) */
	uint32_t frameSync : 1;		/* register 4 gets 0x86 */
	uint32_t master : 1;		/* register 4 gets 0x100 */
	uint32_t width : 5;		/* the sample width's code: 23 and 31 take 8 bits of clock */
	uint32_t clocking : 2;		/* SBC_PCM_CODEC_CLOCKS: the codec clocks the port */
	uint32_t level : 8;		/* 0x80 and the FM policy's bit 18: register 6 bit 0 from its bit 0 */
	uint32_t invert : 1;		/* register 4 bit 0 */
	uint32_t reserved29 : 3;
	uint32_t reserved1;		/* cleared by the FM radio */
	uint32_t rate;			/* the sample rate's code (0..6) */
};

_Static_assert(sizeof(struct SbcPcmStream) == 12, "SBC PCM stream format");

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

/*
 * The codec's settings, as the A2DP configuration gives them, or the mSBC
 * ones of wideband speech (btSbcBuildState()). Only the bitpool, the channel
 * mode and the sampling frequency are read here; the roles of the others are
 * inferred from the mSBC values (1, 0 and 0x8003).
 */
struct SbcEncoderConfig {
	uint16_t bitpool;
	uint16_t wideband;
	uint16_t allocation;
	uint16_t channelMode;		/* 0: mono */
	uint16_t subbandFlags;
	uint16_t samplingFrequency;	/* 0: 16 kHz, 1: 32 kHz, 2: 44.1 kHz, 3: 48 kHz */
};

/* mSBC (wideband speech): its settings and frames. */
#define SBC_MSBC_BITPOOL	26
#define SBC_MSBC_SUBBAND_FLAGS	0x8003
#define SBC_MSBC_FRAME		58	/* bytes of an encoded frame */
#define SBC_MSBC_SAMPLES	240
#define SBC_MSBC_SAMPLE_BYTES	480
#define SBC_MSBC_SCO_HEADER	41	/* where the frames start in a SCO packet */
#define SBC_MSBC_SCO_SIZE	0x5c	/* of a SCO packet's buffer */
#define SBC_SCO_TAG		3	/* OsaBuffer.transportTag and the event of the HCI device */

/* The mSBC H2 synchronization header: 0x01, then 0x08 and the sequence number's two bits, each doubled. */
#define SBC_H2_SYNC		0x01
#define SBC_H2_SIGNATURE	0x08

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
/* An 11-bit count of the descriptor: Sony writes it byte by byte, its own field packed. */
struct __attribute__((packed)) SbcPcmCount {
	uint16_t value : 11;
	uint16_t reserved : 5;
};

struct SbcPcmDescriptor {
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
	struct SbcPcmCount encoderSamples;
	struct SbcPcmCount encoderBytes;
	uint16_t decoderOffset;
	struct SbcPcmCount decoderSamples;
	struct SbcPcmCount decoderBytes;
};

_Static_assert(sizeof(struct SbcPcmDescriptor) == 16, "SBC PCM descriptor");

/*
 * The mSBC receive path's state (btSbcResetState()): the H2 sequence numbers
 * sent and received, whether the last frame was good, the silent frame put in
 * a lost one's place, and a test that drops frames on purpose
 * (btSbcPrepareState(): every frame below a threshold of a 128-frame cycle,
 * at random, or once every threshold frames), set by a host command
 * (btSbcConfigureState()). The roles are inferred.
 */
struct SbcMsbcState {
	uint8_t plcEnable;		/* 1: lost frames are concealed */
	uint8_t dropMode;		/* SBC_DROP_* */
	uint8_t h2Sequence;		/* the next H2 sequence number sent, 0-3 */
	uint8_t rxSequence;		/* the next H2 sequence number expected */
	uint8_t rxSynced;		/* rxSequence is known */
	uint8_t reserved5;
	uint16_t frameGood;		/* the last frame was received whole; 1 */
	uint16_t threshold;		/* of the drop test; 20 */
	uint8_t reserved10[2];
	OsaBuffer *silentFrame;		/* the silent frame decoded in a lost one's place */
	uint32_t goodFrames;
	uint32_t frameCalls;
	uint32_t count;			/* the frames since the last drop */
	uint32_t seed;			/* of the random drops */
};

_Static_assert(sizeof(struct SbcMsbcState) == 32, "mSBC receive state");

#define SBC_DROP_CYCLE		1
#define SBC_DROP_RANDOM		2
#define SBC_DROP_EVERY		3

#define SBC_MSBC_FRAME_LENGTH	57	/* an mSBC frame without its H2 header */
#define SBC_SCO_MAXIMUM		0x130	/* what the receive buffer holds */

extern struct SbcMsbcState sbcMsbc;			/* 0x55430 */
extern const uint8_t sbcMsbcSilentFrame[SBC_MSBC_FRAME_LENGTH];	/* 0x545bc */

/*
 * The packet loss concealment of mSBC frames (bt/plc.c, bt/plcdsp.c): the
 * last samples are kept; a lost frame repeats the stretch that follows the
 * best match of the newest 64 samples, overlapped and faded out.
 */
#define SBC_PLC_HISTORY		374
#define SBC_PLC_FRAME		120
#define SBC_PLC_TEMPLATE	64	/* the samples matched */
#define SBC_PLC_LAGS		256	/* the lags tried */
#define SBC_PLC_OVERLAP		16
#define SBC_PLC_FADE		54	/* the attenuation's step per sample, frames 4 to 8 */

struct SbcPlcState {
	int16_t history[SBC_PLC_HISTORY];
	int16_t lag;			/* 748: where the repeated stretch starts */
	int16_t length;			/* 750: the match's lag, then the stretch's length */
	int16_t position;		/* 752: in the stretch */
	int16_t attenuation;		/* 754: 0x7fff after a good frame */
	int32_t badFrames;		/* 756: lost frames in a row */
};
_Static_assert(sizeof(struct SbcPlcState) == 760, "PLC state");

extern struct SbcPlcState sbcPlc;			/* 0x5a998 */
extern const int16_t sbcPlcOverlap[SBC_PLC_OVERLAP];	/* 0x5459c: the fade-out of the overlap */

int16_t robinClz16(uint32_t value);			/* 0x53c64 (runtime/rtlibarm.c) */

void btSbcReleaseBuffer(struct SbcPlcState *state);					/* 0x51790 */
int16_t btSbcSetupBuffer(const struct SbcPlcState *state);				/* 0x517a8 */
int btSbcClearBuffer(const struct SbcPlcState *state);					/* 0x5188e */
void btSbcConvertBuffer(const int16_t *first, const int16_t *second, int16_t *output);	/* 0x518de */
int btSbcVerifyBuffer(struct SbcPlcState *state, int16_t *output, int count);		/* 0x51966 */
void btSbcSelectBuffer(struct SbcPlcState *state, int16_t *output, int scale);		/* 0x519c6 */
void btSbcAdjustBuffer(struct SbcPlcState *state, int16_t *input, int16_t *output);	/* 0x51ae2: a lost frame */
int btSbcRecordBuffer(struct SbcPlcState *state, int16_t *input, int16_t *output);	/* 0x51bae: a good frame */
int16_t btSbcFilterBuffer(int value);							/* 0x51c1c */
void btSbcHandlePacket(struct SbcPlcState *state, const int16_t *input, int16_t *output);	/* 0x51c30 */

/* bt/plcdsp.c (ARM). */
void btSbcBuildPacket(const int16_t *signal, const int16_t *pattern, int32_t correlations[4], int count);	/* 0x51e10 */
void btSbcComputePacket(const int16_t *input, int16_t *output, int shift, int count);	/* 0x51f98 */
void btSbcQueryPacket(const int16_t *input, int16_t *output, int count);		/* 0x52078 */
uint32_t btSbcResetPacket(const int16_t *signal);					/* 0x520d0 */
uint32_t btSbcSyncPacket(const int16_t *signal);					/* 0x5211c */
void btSbcApplyPacket(const int16_t *first, const int16_t *second, int count, int32_t *firstSum,
		      int32_t *secondSum);						/* 0x5214c */
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
extern void (*sbcPcmStreamHook)(const struct SbcPcmStream *config);	/* DTCM 0x400ef30 */
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
void btSbcRomSyncContext(uint32_t value);
void btSbcRomConfigureState(uint32_t first, uint32_t second, uint32_t third);
/* Takes the encoded frames into a SCO packet; what it does with the last two arguments is not known. */
void btSbcRomResetContext(uint8_t *frames, unsigned int sequence, unsigned int next, unsigned int header);
void mdevRomUpdateState(void *device, OsaBuffer *buffer, uintptr_t argument, unsigned int event);
void btSbcRomComputeContext(uint8_t *frame);		/* decodes the H2 header's frame */

/* bt/codec.c: the PCM port's codec, behind the serial bus at device address 0x34. */
#define SBC_CODEC_ADDRESS	0x34
#define SBC_CODEC_SETTINGS	26
#define SBC_CODEC_POWER		REG32(0x8000216c)	/* inferred: bit 14 is held while the codec is written */
#define SBC_CODEC_POWER_HOLD	0x4000
#define SBC_PCM_CODEC_CLOCKS	1	/* SbcPcmStream.clocking: the codec clocks the port */

struct SbcCodecSetting {
	uint8_t reg;
	uint8_t reserved;
	uint16_t value;			/* 9 bits */
};

extern const struct SbcCodecSetting sbcCodecSettings[2][SBC_CODEC_SETTINGS];	/* 0x544cc */
extern TX_EVENT_FLAGS_GROUP sbcPcmEvents;		/* SQRAM 0xc0033164: the PCM port's DMA events */

void btSbcTrackBuffer(unsigned int reg, unsigned int value);	/* 0x51598 */
void btSbcPcmStreamHandler(const struct SbcPcmStream *config);	/* 0x515bc */
int btSbcPcmDmaCallback(uintptr_t device, uintptr_t buffer, uintptr_t argument, unsigned int status);	/* 0x51764 */

/* bt/sbcinit.c */
int btSbcInit(void);					/* 0x4241c */

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
int btSbcBuildState(void);				/* 0x4d7ec */
int btSbcComputeState(void);				/* 0x4d8da */
int btSbcQueryState(void);				/* 0x4d92e */
int btSbcResetState(void);				/* 0x4da5e */
int btSbcSyncState(uint32_t requests);			/* 0x4dd60 */
int btSbcApplyState(void);				/* 0x4dda4 */
int btSbcResolveState(void);				/* 0x4ddde */
int btSbcScheduleState(void);				/* 0x4de16 */
int btSbcNotifyState(void);				/* 0x4ded4 */
int btSbcFormatState(void);				/* 0x4df68 */
int btSbcInspectState(uint32_t requests, uintptr_t argument);	/* 0x4dfe8 */
int btSbcTrackState(void);				/* 0x4e054 */
int (*btSbcManageState(void))(void);			/* 0x4e0aa */
int btSbcAcquireState(int line, uint32_t pending);	/* 0x4e0c0 */
void btSbcReleaseState(void);				/* 0x4e0fe */

/*
 * The CVSD stream's ROM side (btSbcInspectState()). The original calls the
 * last three with what its flag tests left in r0 and r1, so they are taken
 * to have no arguments.
 */
int btSbcRomCheckContext(uintptr_t argument);
int btSbcRomPrepareContext(void);
int btSbcRomDispatchContext(void);
int btSbcRomValidateContext(void);
void btSbcRomUpdateState(void);

/* The SQRAM code the audio interrupt runs, not converted yet; provisional names. */

/* A hook the ROM calls when the audio clock changes; set to btSbcTrackState(). */
extern int (*sbcClockHook)(void);			/* DTCM 0x400edf4 */

/* An interrupt handler the SCO code registers (btSbcReleaseState()). */
struct SbcIrqHandler {
	SysIrqHandler handler;
	uint32_t line;
};

extern struct SbcIrqHandler sbcIrqHandlers[];	/* 0x55450 */

void btSbcSetupState(void);				/* 0x4e11a: nothing */
void btSbcClearState(void);				/* 0x4e11c */
void btSbcConvertState(void);				/* 0x4e13c */

/* The ROM's hooks btSbcConvertState() sets (ROM data); provisional names. */
extern void (*romLmpPrepareHook)(void);			/* DTCM 0x400ec18 */
extern int (*romLmpDispatchHook)(void);			/* DTCM 0x400ec10 */
extern uint8_t *(*romLmpParseHook)(void);		/* DTCM 0x400ec14 */
extern uint8_t *(*romLmpAcquireHook)(void);		/* DTCM 0x400eb30 */
extern int (*romSbcRecordHook)(uint8_t *message);	/* DTCM 0x400ef34 */
extern int (*romSbcFilterHook)(const uint8_t *message);	/* DTCM 0x400ef38 */
extern void (*romSbcProcessHook)(void);			/* DTCM 0x400f19c */
extern void (*romSbcUpdateHook)(const uint8_t *message);	/* DTCM 0x400f2c0 */
void btBdhRomComputeState(void);

/* The code they point at is in bt/hcihook.c (robin/vendor.h). */

#endif /* ROBIN_SBC_H */
