/*
 * fw2 runtime: the frame stores, fw1's (fw1/pu1/frame.h) with another
 * extension: the runtime's accessors set a picture's SPS profile and level, part
 * of its VUI and its picture type, and the codec copies in the SEI messages and
 * the VUI as PU0 parsed them. Several accessors lost their checks and their
 * result.
 */
#pragma once

#include "../../fw1/pu1/frame.h"

#define VIP_EXTENSION_VUI_SIZE		0x238
#define VIP_EXTENSION_SEI_SIZE		0x2d0

typedef struct VipFrameExtension {
	uint16_t profileIdc;		/* the SPS's */
	uint16_t levelIdc;
	uint32_t chromaSampleLoc;	/* VIP_EXTENSION_CHROMA_*: the VUI's chroma_sample_loc_type */
	union {
		struct {
			uint32_t numUnitsInTick;	/* the VUI's timing information */
			uint32_t timeScale;
		};
		VipDisplaySize otherAspect;	/* setFrameAspect's odd kinds */
	};
	VipDisplaySize sampleAspect;	/* the VUI's sar_width, sar_height */
	uint32_t pictureType;		/* its top 3 bits: VIP_FRAME_TYPE_* */

	/* the codec's (fw2/codec): the SEI messages and VUI that came with it */
	uint32_t seiPresent;		/* VIP_SEI_* by field, VIP_EXTENSION_PTS_VALID */
	uint32_t userDataOffset;	/* its fields' user data in the context's area */
	uint8_t vui[VIP_EXTENSION_VUI_SIZE];		/* its SPS's, as PU0 parsed it */
	uint8_t sei[2][VIP_EXTENSION_SEI_SIZE];	/* each field's VipSeiMessages */
} VipFrameExtension;

_Static_assert(sizeof(VipFrameExtension) == 0x7f8, "VipFrameExtension");

/* VipFrameInfo.flags: the codec's own, besides fw1's */
#define VIP_FRAME_SKIPPED		10	/* none of its slices is decoded */
#define VIP_FRAME_UNPAIRED		9	/* a field whose pair did not come, concealed */

/* VipFrameExtension.seiPresent besides the VIP_SEI_* bits (fw2/codec) */
#define VIP_EXTENSION_PTS_VALID		0x1

#define VIP_EXTENSION_CHROMA_TOP_SHIFT		29	/* VipFrameExtension.chromaSampleLoc */
#define VIP_EXTENSION_CHROMA_BOTTOM_SHIFT	26
#define VIP_EXTENSION_TYPE_SHIFT		29	/* VipFrameExtension.pictureType */

static inline VipFrameExtension *frameExtension(VipFrameHandle *handle)
{
	VipFrameInfo *info = frameInfo(handle);

	return info ? info->extension : NULL;
}

int lockFrame(VipFrameHandle *handle);
int unlockFrame(VipFrameHandle *handle);
VipFrame *getFrameStore(VipFrameHandle *handle);
uint32_t getFrameAvailable(VipFrameHandle *handle);
void setFrameAvailable(VipFrameHandle *handle, uint32_t available);
void setFramePicture(VipFrameHandle *handle, uint32_t picture);
int getFrameRate(VipFrameHandle *handle, uint8_t *rate);
int getFrameRepeatFirstField(VipFrameHandle *handle, uint8_t *repeat);
int getFrameType(VipFrameHandle *handle, uint8_t *type);
int getFrameRandomAccess(VipFrameHandle *handle, uint8_t *randomAccess);
void setFrameWidth(VipFrameHandle *handle, uint16_t width);
void setFrameHeight(VipFrameHandle *handle, uint16_t height);
void setFrameDamaged(VipFrameHandle *handle, uint32_t damaged);
int setFrameInfo(VipFrameHandle *handle, const VipFrameInfo *info);
int setFrameProfile(VipFrameHandle *handle, uint16_t profileIdc);
int setFrameLevel(VipFrameHandle *handle, uint16_t levelIdc);
int setFramePictureType(VipFrameHandle *handle, uint32_t type);
int setFrameAspect(VipFrameHandle *handle, uint32_t kind, uint16_t width, uint16_t height);
int setFrameChromaLocTop(VipFrameHandle *handle, uint32_t type);
int setFrameChromaLocBottom(VipFrameHandle *handle, uint32_t type);
int setFrameNumUnitsInTick(VipFrameHandle *handle, uint32_t numUnitsInTick);
int setFrameTimeScale(VipFrameHandle *handle, uint32_t timeScale);
