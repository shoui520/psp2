/*
 * PU1: the frame stores. Each decoded picture lives in a frame store, a VipFrame
 * in DRAM the ARM side shares, with its display information (VipFrameInfo) and an
 * extension (VipFrameExtension) for what the decoder keeps of it as a reference.
 * The decoder reaches a frame store through a VipFrameHandle and the accessors
 * below; each returns -1 if the handle has no frame store.
 */
#pragma once

#include "../common/runtime.h"
#include "../common/parsed.h"

/* a pan-scan rectangle (pan-scan SEI) */
typedef struct VipDisplaySize {
	uint16_t width;
	uint16_t height;
} VipDisplaySize;

typedef struct VipDisplayOffset {
	int16_t x;
	int16_t y;
} VipDisplayOffset;

#define VIP_DISPLAY_RECTANGLES	3

/* the display information of a picture: what the ARM side reports of it */
typedef struct VipFrameInfo {
	uint16_t width;			/* the picture, in pixels */
	uint16_t height;
	uint16_t cropLeft;		/* the SPS's frame cropping, in pixels */
	uint16_t cropRight;
	uint16_t cropTop;
	uint16_t cropBottom;
	VipDisplaySize display[VIP_DISPLAY_RECTANGLES];	/* the pan-scan rectangles; the cropped picture without them */
	VipDisplayOffset offset[VIP_DISPLAY_RECTANGLES];	/* where their centres are, in 1/16 pixels */
	uint32_t flags;			/* VIP_FRAME_* */
	uint32_t colour;		/* VIP_FRAME_COLOUR_* bytes */
	struct VipFrameExtension *extension;	/* not copied by get/setFrameInfo */
} VipFrameInfo;

/* VipFrameInfo.flags */
#define VIP_FRAME_ASPECT_SHIFT		28	/* 4 bits */
#define VIP_FRAME_RATE_SHIFT		24	/* 4 bits: an MPEG frame_rate_code */
#define VIP_FRAME_PROGRESSIVE_SEQUENCE	23	/* as MPEG-2 has them */
#define VIP_FRAME_PROGRESSIVE_FRAME	22
#define VIP_FRAME_TOP_FIELD_FIRST	21
#define VIP_FRAME_REPEAT_FIRST_FIELD	20
#define VIP_FRAME_TYPE_SHIFT		17	/* 3 bits: VIP_FRAME_TYPE_* */
#define VIP_FRAME_FORMAT_SHIFT		14	/* 3 bits */
#define VIP_FRAME_RANDOM_ACCESS		13	/* IDR, or a recovery point */
#define VIP_FRAME_NAL_REFERENCE		12	/* nal_ref_idc != 0 */
#define VIP_FRAME_DAMAGED		11

/* VIP_FRAME_ASPECT_SHIFT: the nearest MPEG-2 aspect_ratio_information */
#define VIP_FRAME_ASPECT_1_1		1
#define VIP_FRAME_ASPECT_4_3		2
#define VIP_FRAME_ASPECT_16_9		3
#define VIP_FRAME_ASPECT_221_100	4

/* VIP_FRAME_TYPE_SHIFT: an MPEG picture_coding_type, by slice_type */
#define VIP_FRAME_TYPE_I		1
#define VIP_FRAME_TYPE_P		2
#define VIP_FRAME_TYPE_B		3
#define VIP_FRAME_TYPE_IDR		5

/* VipFrameInfo.colour: a byte each */
#define VIP_FRAME_COLOUR_PRIMARIES_SHIFT	24
#define VIP_FRAME_TRANSFER_SHIFT		16
#define VIP_FRAME_MATRIX_SHIFT			8

/* VipFrameInfo.colour without colour_description */
#define VIP_COLOUR_UNSPECIFIED	(0xffu << VIP_FRAME_COLOUR_PRIMARIES_SHIFT | \
	0xffu << VIP_FRAME_TRANSFER_SHIFT | 0xffu << VIP_FRAME_MATRIX_SHIFT)

#ifndef VIP_FW2	/* FW2 has its own extension (fw2/common/frame.h) */
/*
 * What the decoder keeps of a picture beyond its display information: its place
 * in the picture order and reference marking, and the SEI messages and VUI that
 * came with it, as PU0 parsed them.
 */
#define VIP_EXTENSION_COPIES		2	/* of the SEI messages a frame's fields may each have */

typedef struct VipFrameExtension {
	uint16_t profileIdc;		/* the SPS's, from VipOutputPicture.key */
	uint16_t levelIdc;
	uint32_t chromaSampleLoc;	/* VIP_EXTENSION_CHROMA_*: the VUI's chroma_sample_loc_type */
	uint32_t reserved2[2];
	uint8_t rateKind;		/* VIP_RATE_*, from VipOutputPicture.aspectRate */
	uint8_t matrix;			/* VipOutputPicture.matrix */
	uint8_t reserved0[2];
	uint8_t bufferingPeriodCount;	/* the SEI messages kept, each count | */
	uint8_t pictureTimingCount;	/* VIP_EXTENSION_TAKEN once the picture has one */
	uint8_t recoveryPointCount;
	uint8_t panScanCount;
	int8_t vuiPresent;
	uint8_t fieldState;		/* VIP_EXTENSION_FRAME, _FIRST_FIELD, _FIELD_PAIR */
	uint8_t reserved1[2];
	uint32_t bufferingPeriod[VIP_BUFFERING_PERIOD_WORDS];
	uint32_t pictureTiming[VIP_EXTENSION_COPIES][VIP_PICTURE_TIMING_WORDS];
	uint32_t recoveryPoint[VIP_RECOVERY_POINT_WORDS];
	uint32_t panScan[VIP_EXTENSION_COPIES][VIP_PAN_SCAN_WORDS];
	uint32_t vui[VIP_VUI_WORDS];
	uint32_t nalHrd[VIP_HRD_WORDS];
	uint32_t vclHrd[VIP_HRD_WORDS];
	uint32_t state;			/* its top 3 bits: VIP_EXTENSION_STATE_SHIFT */
} VipFrameExtension;

#define VIP_EXTENSION_SIZE		0x548
_Static_assert(sizeof(VipFrameExtension) == VIP_EXTENSION_SIZE, "VipFrameExtension");
#define VIP_EXTENSION_CHROMA_TOP_SHIFT		29	/* VipFrameExtension.chromaSampleLoc */
#define VIP_EXTENSION_CHROMA_BOTTOM_SHIFT	26
#define VIP_EXTENSION_STATE_SHIFT	29	/* VipFrameExtension.state */

/* VipFrameExtension's SEI counts */
#define VIP_EXTENSION_TAKEN		0x80

/* VipFrameExtension.fieldState */
#define VIP_EXTENSION_COMPLETE		0x80	/* a frame, or a field pair */
#define VIP_EXTENSION_FIRST_FIELD	0x01
#define VIP_EXTENSION_FRAME		(VIP_EXTENSION_COMPLETE | 1)
#define VIP_EXTENSION_FIELD_PAIR	(VIP_EXTENSION_COMPLETE | 2)
#endif /* !VIP_FW2 */

/* a frame store */
typedef struct VipFrame {
	uint32_t available;		/* 1: free to decode into; 0: decoded, not yet given back */
	uint32_t reserved;
	uint32_t picture;		/* DRAM: the decoded picture */
	uint32_t timestampIndex;
	VipFrameInfo info;
} VipFrame;

#define VIP_FRAME_SIZE		0x40
_Static_assert(sizeof(VipFrame) == VIP_FRAME_SIZE, "VipFrame");

typedef struct VipFrameHandle {
	VipFrame *frame;
	uint32_t locked;
} VipFrameHandle;

static inline VipFrameInfo *frameInfo(VipFrameHandle *handle)
{
	return &handle->frame->info;
}

#ifndef VIP_FW2
static inline VipFrameExtension *frameExtension(VipFrameHandle *handle)
{
	VipFrameInfo *info = frameInfo(handle);

	return info ? info->extension : NULL;
}

int lockFrame(VipFrameHandle *handle);
int unlockFrame(VipFrameHandle *handle);
int setFrameStore(VipFrameHandle *handle, VipFrame *frame);
int getFrameStore(VipFrameHandle *handle, VipFrame **frame);
int getFrameAvailable(VipFrameHandle *handle, uint32_t *available);
int setFrameAvailable(VipFrameHandle *handle, uint32_t available);
int setFramePicture(VipFrameHandle *handle, uint32_t picture);
int setFrameExtension(VipFrameHandle *handle, VipFrameExtension *extension);
int getFrameRate(VipFrameHandle *handle, uint8_t *rate);
int getFrameProgressiveSequence(VipFrameHandle *handle, uint8_t *progressive);
int getFrameProgressiveFrame(VipFrameHandle *handle, uint8_t *progressive);
int getFrameTopFieldFirst(VipFrameHandle *handle, uint8_t *topFieldFirst);
int getFrameRepeatFirstField(VipFrameHandle *handle, uint8_t *repeat);
int getFrameType(VipFrameHandle *handle, uint8_t *type);
int getFrameRandomAccess(VipFrameHandle *handle, uint8_t *randomAccess);
int getFrameNalReference(VipFrameHandle *handle, uint8_t *reference);
int setFrameWidth(VipFrameHandle *handle, uint16_t width);
int setFrameHeight(VipFrameHandle *handle, uint16_t height);
int setFrameRate(VipFrameHandle *handle, uint32_t rate);
int setFrameProgressiveSequence(VipFrameHandle *handle, uint32_t progressive);
int setFrameTopFieldFirst(VipFrameHandle *handle, uint32_t topFieldFirst);
int setFrameFormat(VipFrameHandle *handle, uint32_t format);
int setFrameDamaged(VipFrameHandle *handle, uint32_t damaged);
int getFrameInfo(VipFrameHandle *handle, VipFrameInfo *info);
int setFrameInfo(VipFrameHandle *handle, const VipFrameInfo *info);
int setFrameProfile(VipFrameHandle *handle, uint16_t profileIdc);
int setFrameLevel(VipFrameHandle *handle, uint16_t levelIdc);
int setFrameChromaLocTop(VipFrameHandle *handle, uint32_t type);
int setFrameChromaLocBottom(VipFrameHandle *handle, uint32_t type);
int setFrameMatrix(VipFrameHandle *handle, uint8_t matrix);
int setFrameRateKind(VipFrameHandle *handle, uint8_t kind);
#endif /* !VIP_FW2 */
