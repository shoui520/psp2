/*
 * fw2 runtime: the frame store accessors (see frame.h).
 */
#include "frame.h"

/* -2 if it already is */
int lockFrame(VipFrameHandle *handle)
{
	if (handle->locked)
		return -2;
	handle->locked = 1;
	return 0;
}

int unlockFrame(VipFrameHandle *handle)
{
	if (!handle->locked)
		return -2;
	handle->locked = 0;
	return 0;
}

VipFrame *getFrameStore(VipFrameHandle *handle)
{
	return handle->frame;
}

uint32_t getFrameAvailable(VipFrameHandle *handle)
{
	return handle->frame->available;
}

void setFrameAvailable(VipFrameHandle *handle, uint32_t available)
{
	handle->frame->available = available;
}

void setFramePicture(VipFrameHandle *handle, uint32_t picture)
{
	handle->frame->picture = picture;
}

/* a field of VipFrameInfo.flags */
static inline int getFlags(VipFrameHandle *handle, uint8_t *value, uint32_t shift,
	uint32_t mask)
{
	VipFrameInfo *info = frameInfo(handle);

	if (!info)
		return -1;
	*value = info->flags >> shift & mask;
	return 0;
}

int getFrameRate(VipFrameHandle *handle, uint8_t *rate)
{
	return getFlags(handle, rate, VIP_FRAME_RATE_SHIFT, 0xf);
}

int getFrameRepeatFirstField(VipFrameHandle *handle, uint8_t *repeat)
{
	return getFlags(handle, repeat, VIP_FRAME_REPEAT_FIRST_FIELD, 1);
}

int getFrameType(VipFrameHandle *handle, uint8_t *type)
{
	return getFlags(handle, type, VIP_FRAME_TYPE_SHIFT, 7);
}

int getFrameRandomAccess(VipFrameHandle *handle, uint8_t *randomAccess)
{
	return getFlags(handle, randomAccess, VIP_FRAME_RANDOM_ACCESS, 1);
}

void setFrameWidth(VipFrameHandle *handle, uint16_t width)
{
	frameInfo(handle)->width = width;
}

void setFrameHeight(VipFrameHandle *handle, uint16_t height)
{
	frameInfo(handle)->height = height;
}

void setFrameDamaged(VipFrameHandle *handle, uint32_t damaged)
{
	VipFrameInfo *info = frameInfo(handle);

	info->flags = (damaged & 1) << VIP_FRAME_DAMAGED |
		(info->flags & ~(1u << VIP_FRAME_DAMAGED));
}

int setFrameInfo(VipFrameHandle *handle, const VipFrameInfo *info)
{
	VipFrameInfo *target = frameInfo(handle);
	uint32_t i;

	if (!target)
		return -1;
	target->width = info->width;
	target->height = info->height;
	target->cropLeft = info->cropLeft;
	target->cropRight = info->cropRight;
	target->cropTop = info->cropTop;
	target->cropBottom = info->cropBottom;
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++)
		target->display[i] = info->display[i];
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++)
		target->offset[i] = info->offset[i];
	target->flags = info->flags;
	target->colour = info->colour;
	return 0;
}

int setFrameProfile(VipFrameHandle *handle, uint16_t profileIdc)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->profileIdc = profileIdc;
	return 0;
}

int setFrameLevel(VipFrameHandle *handle, uint16_t levelIdc)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->levelIdc = levelIdc;
	return 0;
}

int setFramePictureType(VipFrameHandle *handle, uint32_t type)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->pictureType = type << VIP_EXTENSION_TYPE_SHIFT |
		(extension->pictureType & ~(7u << VIP_EXTENSION_TYPE_SHIFT));
	return 0;
}

/*
 * Record an aspect ratio of the given kind: the decoder gives the VUI's sample
 * aspect ratio as kind 16. Kinds outside 16 to 19 and 24 to 27 are ignored.
 */
int setFrameAspect(VipFrameHandle *handle, uint32_t kind, uint16_t width, uint16_t height)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	switch (kind) {
	case 16:
	case 18:
	case 24:
	case 26:
		extension->sampleAspect.width = width;
		extension->sampleAspect.height = height;
		break;
	case 17:
	case 19:
	case 25:
	case 27:
		extension->otherAspect.width = width;
		extension->otherAspect.height = height;
		break;
	}
	return 0;
}

int setFrameChromaLocTop(VipFrameHandle *handle, uint32_t type)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->chromaSampleLoc = type << VIP_EXTENSION_CHROMA_TOP_SHIFT |
		(extension->chromaSampleLoc & ~(7u << VIP_EXTENSION_CHROMA_TOP_SHIFT));
	return 0;
}

int setFrameChromaLocBottom(VipFrameHandle *handle, uint32_t type)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->chromaSampleLoc = (type & 7) << VIP_EXTENSION_CHROMA_BOTTOM_SHIFT |
		(extension->chromaSampleLoc & ~(7u << VIP_EXTENSION_CHROMA_BOTTOM_SHIFT));
	return 0;
}

int setFrameNumUnitsInTick(VipFrameHandle *handle, uint32_t numUnitsInTick)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->numUnitsInTick = numUnitsInTick;
	return 0;
}

int setFrameTimeScale(VipFrameHandle *handle, uint32_t timeScale)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->timeScale = timeScale;
	return 0;
}
