/*
 * PU1: the picture output. Each decoded picture goes out at its presentation time:
 * the output compares the picture's PTS with the system time clock (the STC, plus
 * the offset the ARM side sets) and tells the decoder whether to show it, hold the
 * previous one, skip a field or drop it. Trick-play commands from the ARM side
 * (VipDecoderArea.command) change the speed of the clock; how far it has moved from
 * the STC is kept as an adjustment, which the ARM side reads in VipOutputClock.
 *
 * Times are 33-bit PTS values (VIP_PTS_MASK) in 90 kHz units. VipOutputState.nextPts
 * counts in 180 kHz units, frame durations in 720 kHz units. Adjustments are in
 * 1/16 units with a sign bit, VIP_ADJUSTMENT_NEGATIVE.
 */
#pragma once

#include "frame.h"

#define VIP_PTS_SIGN		0x100000000ull		/* a difference of two PTS */
#define VIP_PTS_EXTEND		0xffffffff00000000ull	/* sign-extends one */
#define VIP_ADJUSTMENT_MASK	0x1fffffffffull
#define VIP_ADJUSTMENT_SHIFT	4
#define VIP_ADJUSTMENT_NEGATIVE	(1ull << 37)
#define VIP_DIFFERENCE_NEGATIVE	(VIP_ADJUSTMENT_NEGATIVE >> VIP_ADJUSTMENT_SHIFT)

/* how long the display takes to scan a picture out, subtracted from the STC */
#define VIP_OUTPUT_LATENCY	5400

/* the AV timers of the ARM side: each value is its high word << 24 | its low word */
#define VIP_TIMER_BASE		0xe3110000
#define VIP_TIMER(offset)	(*(volatile uint32_t *)(VIP_TIMER_BASE + (offset)))
#define VIP_TIMER_STC_LOW	0x820	/* the system time clock */
#define VIP_TIMER_STC_HIGH	0x824
#define VIP_TIMER_BASE_LOW	0x920	/* added to it */
#define VIP_TIMER_BASE_HIGH	0x924
#define VIP_TIMER_START_LOW	0x930	/* subtracted from it */
#define VIP_TIMER_START_HIGH	0x934

/* the decoder's field counter: which field the display scans out */
#define VIP_CB_DISPLAY_FIELD	0x4030

/* the clock the output shares with the ARM side */
typedef struct VipOutputClock {
	volatile uint32_t ptsHigh;		/* the last picture's time: bits 32-24 */
	volatile uint32_t ptsLow;		/* bits 23-0 */
	uint32_t reserved0;
	volatile uint32_t driftHigh;		/* VIP_DRIFT_NEGATIVE, drift bits 36-28 */
	volatile uint32_t driftMiddle;		/* bits 27-4 */
	volatile uint32_t driftLow;		/* bits 3-0 */
	volatile uint32_t offsetHigh;		/* the ARM side's offset: bits 32-24 */
	volatile uint32_t offsetLow;		/* bits 23-0 */
	uint32_t reserved1;
	volatile uint32_t stcHigh;		/* the STC when the picture was scheduled */
	volatile uint32_t stcLow;
	uint32_t reserved2;
	volatile uint32_t command;		/* VipDecoderArea.command then */
	volatile uint32_t *semaphore;		/* guards the clock */
	volatile uint32_t *request;		/* VIP_CLOCK_* from the ARM side */
	volatile uint32_t *acknowledge;		/* VIP_CLOCK_* done */
} VipOutputClock;

#define VIP_OUTPUT_CLOCK	((VipOutputClock *)(VIP_IMAGE + 0xc3500))

/* VipOutputClock.driftHigh: the sign is in bits 9-16 */
#define VIP_DRIFT_SIGN_SHIFT	9

/* *VipOutputClock.request and .acknowledge */
#define VIP_CLOCK_RUNNING	0x00000001
#define VIP_CLOCK_HOLD		0x00000010	/* show pictures without the clock */
#define VIP_CLOCK_OFFSET	0x40000000	/* offsetHigh and offsetLow are new */
#define VIP_CLOCK_RESET		0x80000000	/* restart from the STC */
#define VIP_CLOCK_STOPPED	0x00000001	/* acknowledge: no picture to resync to */

/* what the output reports of a picture to the ARM side */
typedef struct VipOutputPicture {
	uint32_t size;			/* width << 16 | height */
	uint32_t cropVertical;		/* VipFrameInfo.cropTop << 16 | cropBottom */
	uint32_t cropHorizontal;	/* VipFrameInfo.cropLeft << 16 | cropRight */
	uint32_t aspectRate;		/* VipFrameInfo.flags bits 31-24, the rate kind << 8 */
	uint32_t progressive;		/* 0x100, and VIP_PICTURE_CHROMA_* (decode) */
	uint32_t scan;			/* VIP_SCAN_* */
	uint32_t panScan0;		/* VipFrameInfo.offset[0], x << 16 | y */
	uint32_t key;
	uint32_t displaySize;		/* VipFrameInfo.display[0], width << 16 | height */
	uint32_t matrix;		/* the colour matrix */
	uint32_t panScan1;
	uint32_t panScan2;
} VipOutputPicture;

/* VipOutputPicture.progressive: the VUI's chroma_sample_loc_type, as decode adds them */
#define VIP_PICTURE_CHROMA_TOP_SHIFT	20
#define VIP_PICTURE_CHROMA_BOTTOM_SHIFT	16

/* VipOutputPicture.scan */
#define VIP_SCAN_PROGRESSIVE_FRAME	0x001
#define VIP_SCAN_FIELD_FLAGS_SHIFT	1	/* top_field_first, repeat_first_field */
#define VIP_SCAN_TYPE_SHIFT		8	/* VIP_FRAME_TYPE_* */
#define VIP_SCAN_INTERLACED		0x020
#define VIP_SCAN_PROGRESSIVE		0x030

/* VipOutputPicture.aspectRate bits 9-8: where the frame rate came from */
#define VIP_RATE_KIND_SHIFT	8
#define VIP_RATE_STREAM		0	/* the stream's */
#define VIP_RATE_DEFAULT	2	/* VipDecoderArea.control's, the stream has none */
#define VIP_RATE_OVERRIDE	3	/* VipDecoderArea.control's */

/*
 * The decoder's area of the debugger (getDebugArea of its debug point): what the
 * ARM side sets before it opens the decoder, the output's commands, and what the
 * output reports of the pictures.
 */
typedef struct VipDecoderArea {
	volatile uint32_t flags;		/* VIP_AREA_* */
	volatile uint32_t status;		/* cleared when the decoder closes */
	volatile uint32_t endFlags;		/* VIP_END_*: how the stream ended */
	volatile uint32_t streamSettings;	/* VIP_STREAM_KIND_*; taken at open */
	volatile uint32_t outputMode;		/* VIP_OUTPUT_*; taken at open */
	volatile uint32_t command;		/* VIP_COMMAND_* << 16 | a count */
	volatile uint32_t userData;		/* VIP_USER_DATA_*; taken each decode */
	uint32_t reserved1;
	volatile uint32_t ptsHigh;		/* the picture's PTS: bits 32-24, bit 31 negative */
	volatile uint32_t ptsLow;
	volatile uint32_t version;		/* VIP_DECODER_VERSION */
	uint32_t reserved2;
	volatile uint32_t resync;		/* the clock may resync to the pictures */
	volatile uint32_t stepPtsHigh;		/* the PTS a command stepped to */
	volatile uint32_t stepPtsLow;
	volatile uint32_t control;		/* VIP_CONTROL_* */
	volatile uint32_t dtsHigh;		/* as ptsHigh */
	volatile uint32_t dtsLow;
	volatile uint32_t concealColour;	/* taken at open: for missing macroblocks */
	volatile uint32_t matrix;		/* when the stream gives none */
	uint32_t reserved3;
	volatile uint32_t options;		/* VIP_OPTION_* */
	volatile uint32_t recoverySettings;	/* VIP_RECOVERY_*; taken at open */
	uint32_t reserved4[41];
	volatile uint32_t width;		/* the picture */
	volatile uint32_t height;
	volatile uint32_t rate;
	uint32_t reserved5[2];
	volatile VipOutputPicture picture;
	uint32_t reserved6[15];
	volatile VipOutputPicture secondPicture;
} VipDecoderArea;

/* VipDecoderArea.userData: the SEI user data the decoder passes on */
#define VIP_USER_DATA_UNREGISTERED	0x1
#define VIP_USER_DATA_REGISTERED	0x2

/* VipDecoderArea.version, set when the decoder task starts */
#define VIP_DECODER_VERSION	0x3138

/* VipDecoderArea.streamSettings */
#define VIP_STREAM_KIND_SHIFT	4
#define VIP_STREAM_KIND_MASK	7

/* VipDecoderArea.options: the low byte is the ARM side's; the decoder puts bit 0 as
   it took it at open into bits 16- */
#define VIP_OPTION_MASK		0xff
#define VIP_OPTION_TAKEN_SHIFT	16
#define VIP_OPTION_TIMING_FIRST	0x1	/* pic_timing() before the PTS (provisional) */
#define VIP_OPTION_SWITCH_TIMING	0x2	/* the other way when no buffer comes free */

/* VipDecoderArea.concealColour: Y << 16 | Cb << 8 | Cr, or this to leave them */
#define VIP_CONCEAL_NONE	0x80000000

/* VipDecoderArea.recoverySettings (provisional names) */
#define VIP_RECOVERY_IGNORE_BROKEN_LINK	0x1	/* takeRecoveryPoint's */
#define VIP_RECOVERY_AT_I		0x2	/* an I picture may start the decoding */

/* VipDecoderArea.endFlags, by the codec's decode result */
#define VIP_END_ERROR		0x01	/* VIP_DECODE_ERROR */
#define VIP_END_LAST_PICTURE	0x04	/* VIP_DECODE_LAST_PICTURE */
#define VIP_END_STOPPED		0x40	/* VIP_DECODE_STOPPED */

/* VipDecoderArea.flags */
#define VIP_AREA_OPEN		0xc0	/* the decoder is open */
#define VIP_AREA_WAITING	0x40	/* part of it until the decode finds a picture */
#define VIP_AREA_PARAMETERS	0x10	/* the decode takes a parameter set */
#define VIP_AREA_PICTURE_HEADER	0x08	/* and a picture header */
#define VIP_AREA_NO_CLOCK	0x02	/* the ARM side has not started the clock */
#define VIP_AREA_COMMAND	0x04	/* a trick-play command runs */
#define VIP_AREA_REPEAT		0x20	/* the picture is repeated */
#define VIP_AREA_FLAGS		0xff	/* the bits the output keeps */

/* VipDecoderArea.command, its top 16 bits; the low 4 bits are a count, n */
#define VIP_COMMAND_REPEAT		0x1000
#define VIP_COMMAND_REPEAT_FIELD	0x4100
#define VIP_COMMAND_REPEAT_PICTURE	0x4300
#define VIP_COMMAND_STOP		0x5100	/* after this picture */
#define VIP_COMMAND_PAUSE		0x8000
#define VIP_COMMAND_SEEK		0x9000	/* the clock jumps to the picture */
#define VIP_COMMAND_SLOW		0xa000	/* 1/n of the speed */
#define VIP_COMMAND_SLOW_POWER		0xa800	/* 1/2^n */
#define VIP_COMMAND_SLOW_FRACTION	0xac00	/* 1/(n + 1) */
#define VIP_COMMAND_FAST		0xb000	/* n times the speed */
#define VIP_COMMAND_FAST_EVEN		0xb400	/* 2n times */
#define VIP_COMMAND_FAST_FRACTION	0xb800	/* 1 + 1/n times */
#define VIP_COMMAND_STEP_I		0xc100	/* show only I pictures */
#define VIP_COMMAND_STEP_IP		0xc300	/* I and P pictures */
#define VIP_COMMAND_STEP_I_COUNT	0xc900	/* n of them */
#define VIP_COMMAND_STEP_IP_COUNT	0xcb00
#define VIP_COMMAND_COUNT		0x000f

/* VipDecoderArea.control: frame rates for pictures without one, or instead of theirs */
#define VIP_CONTROL_RATE_MASK		0x007	/* for interlaced pictures, rate code - 2 */
#define VIP_CONTROL_RATE		0x008	/* override it */
#define VIP_CONTROL_PROGRESSIVE_SHIFT	4	/* for progressive ones, rate code - 5 */
#define VIP_CONTROL_PROGRESSIVE_RATE	0x080
#define VIP_CONTROL_PROGRESSIVE		0x100	/* the scan to force */
#define VIP_CONTROL_FORCE_SCAN		0x200

/* the debugger's events from the output */
#define VIP_EVENT_OUTPUT		0x00000080	/* a picture went to the output ring */
#define VIP_EVENT_FRAME_LATE		0x00000800
#define VIP_EVENT_WIDE_PICTURE		0x00002000	/* VIP_STREAM_REPORT_WIDE */
#define VIP_EVENT_PICTURE		0x04000000	/* VipDecoderArea.picture changed */
#define VIP_EVENT_SECOND_PICTURE	0x30000000
#define VIP_EVENT_STEP			0x40000000	/* a command stepped a picture */
#define VIP_EVENT_CLOCK			0x80000000	/* the adjustment changed */

/* the output's modes, from the decoder */
#define VIP_OUTPUT_CLOCKED	0
#define VIP_OUTPUT_STILL	1	/* show without the clock */
#define VIP_OUTPUT_STARTING	2	/* until the clock is in sync */
#define VIP_OUTPUT_FREE		0x11	/* show every picture */

/* scheduleFrame's flags */
#define VIP_SCHEDULE_RESYNC	0x400	/* the clock may resync to the picture */

/* the colour standard of the pictures, for the matrix */
#define VIP_STANDARD_MASK	0x300
#define VIP_STANDARD_HD		0x000	/* ITU-R BT.709 */
#define VIP_STANDARD_SD		0x100	/* ITU-R BT.601 */
#define VIP_MATRIX_UNSPECIFIED	0xff
#define VIP_MATRIX_BT709	1
#define VIP_MATRIX_BT601	5

/* what to do with a picture */
#define VIP_OUTPUT_SHOW		0
#define VIP_OUTPUT_HOLD		1	/* early: show the previous picture again */
#define VIP_OUTPUT_SKIP		2	/* a field the command does not show */
#define VIP_OUTPUT_DROP		(-1)	/* late, or the wrong field */
#define VIP_OUTPUT_CADENCE	(-2)	/* the field cadence changed */

/* the output's state, one per stream */
typedef struct VipOutputState {
	uint64_t nextPts;		/* the next picture's PTS, in 180 kHz units */
	uint64_t adjustment;		/* the clock's, VIP_ADJUSTMENT_* */
	uint64_t publishedAdjustment;	/* the one in VipOutputClock */
	uint64_t offset;		/* the ARM side's, added to the STC */
	uint64_t presentTime;		/* the last picture's */
	uint64_t lastTarget;		/* the clock then */
	uint64_t commandTime;		/* the clock when the command came */
	uint64_t commandAdjustment;	/* the adjustment then */
	uint64_t reserved0;
	uint32_t lastCommand;
	uint32_t command;		/* VipDecoderArea.command */
	uint32_t clockRequest;		/* *VipOutputClock.request */
	int8_t inSync;			/* a picture was on time */
	int8_t started;			/* a picture was shown */
	int8_t unclocked;		/* show the next picture without the clock */
	int8_t restart;			/* the next picture restarts the clock */
	int8_t rebase;			/* take a new commandTime */
	int8_t stopped;			/* VIP_COMMAND_STOP stopped the output */
	int8_t scheduled;		/* scheduleFrame placed a picture */
	uint8_t repeatCount;
	int8_t reserved1;
	uint8_t cadence;		/* the last display field, counted in bits 4-7 */
	uint8_t rate;			/* the frame rate code */
	int8_t topFieldFirst;		/* the last picture's */
	VipFrameInfo info;		/* the last reported */
	uint32_t infoKey;
	VipFrameInfo secondInfo;
	int8_t intervalStarted;
	uint32_t intervalFrame;		/* VipStatus.frameCount at the last picture */
	int8_t waitRandomAccess;	/* skip P and B pictures until a random access point */
	uint32_t remainder;		/* of nextPts, in 720 kHz units */
} VipOutputState;

void reportFrameInterval(VipOutputState *state);
int publishPictureInfo(VipOutputState *state, VipFrameHandle *frame, VipDecoderArea *area,
	uint32_t standard, uint32_t key);
int publishSecondPictureInfo(VipOutputState *state, VipFrameHandle *frame,
	VipDecoderArea *area, uint32_t standard);
void applyRateControl(VipOutputState *state, VipFrameHandle *frame, VipDecoderArea *area,
	uint32_t unused, const uint64_t *dts, uint32_t ptsValid, const uint64_t *pts);
void resetOutputState(VipOutputState *state);
void closeOutputState(VipOutputState *state);
int beginOutput(VipOutputState *state, uint32_t areaKind, uint32_t mode, uint32_t reset);
int scheduleFrame(VipOutputState *state, uint32_t ptsValid, uint64_t *pts,
	const uint64_t *dts, uint32_t areaKind, uint32_t flags, uint32_t mode,
	VipFrameHandle *frame, VipFrameHandle *field);
void advancePts(VipOutputState *state, VipFrameHandle *frame, VipFrameHandle *field);
int checkFrame(VipOutputState *state, uint32_t ptsValid, uint64_t *pts, uint32_t unused,
	uint32_t mode, VipFrameHandle *frame, uint32_t hold);

