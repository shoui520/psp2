/*
 * PU1: the frame store accessors.
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

int setFrameStore(VipFrameHandle *handle, VipFrame *frame)
{
	handle->frame = frame;
	return 0;
}

int getFrameStore(VipFrameHandle *handle, VipFrame **frame)
{
	if (!handle->frame)
		return -1;
	*frame = handle->frame;
	return 0;
}

int getFrameAvailable(VipFrameHandle *handle, uint32_t *available)
{
	if (!handle->frame)
		return -1;
	*available = handle->frame->available;
	return 0;
}

int setFrameAvailable(VipFrameHandle *handle, uint32_t available)
{
	if (!handle->frame)
		return -1;
	handle->frame->available = available;
	return 0;
}

int setFramePicture(VipFrameHandle *handle, uint32_t picture)
{
	if (!handle->frame)
		return -1;
	handle->frame->picture = picture;
	return 0;
}

int setFrameExtension(VipFrameHandle *handle, VipFrameExtension *extension)
{
	if (!handle->frame)
		return -1;
	handle->frame->info.extension = extension;
	return 0;
}

/* a field of VipFrameInfo.flags */
static int getFlags(VipFrameHandle *handle, uint8_t *value, uint32_t shift, uint32_t mask)
{
	VipFrameInfo *info = frameInfo(handle);

	if (!info)
		return -1;
	*value = info->flags >> shift & mask;
	return 0;
}

static int setFlags(VipFrameHandle *handle, uint32_t value, uint32_t shift, uint32_t mask)
{
	VipFrameInfo *info = frameInfo(handle);

	if (!info)
		return -1;
	info->flags = (value & mask) << shift | (info->flags & ~(mask << shift));
	return 0;
}

int getFrameRate(VipFrameHandle *handle, uint8_t *rate)
{
	return getFlags(handle, rate, VIP_FRAME_RATE_SHIFT, 0xf);
}

int getFrameProgressiveSequence(VipFrameHandle *handle, uint8_t *progressive)
{
	return getFlags(handle, progressive, VIP_FRAME_PROGRESSIVE_SEQUENCE, 1);
}

int getFrameProgressiveFrame(VipFrameHandle *handle, uint8_t *progressive)
{
	return getFlags(handle, progressive, VIP_FRAME_PROGRESSIVE_FRAME, 1);
}

int getFrameTopFieldFirst(VipFrameHandle *handle, uint8_t *topFieldFirst)
{
	return getFlags(handle, topFieldFirst, VIP_FRAME_TOP_FIELD_FIRST, 1);
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

int getFrameNalReference(VipFrameHandle *handle, uint8_t *reference)
{
	return getFlags(handle, reference, VIP_FRAME_NAL_REFERENCE, 1);
}

int setFrameWidth(VipFrameHandle *handle, uint16_t width)
{
	VipFrameInfo *info = frameInfo(handle);

	if (!info)
		return -1;
	info->width = width;
	return 0;
}

int setFrameHeight(VipFrameHandle *handle, uint16_t height)
{
	VipFrameInfo *info = frameInfo(handle);

	if (!info)
		return -1;
	info->height = height;
	return 0;
}

int setFrameRate(VipFrameHandle *handle, uint32_t rate)
{
	return setFlags(handle, rate, VIP_FRAME_RATE_SHIFT, 0xf);
}

int setFrameProgressiveSequence(VipFrameHandle *handle, uint32_t progressive)
{
	return setFlags(handle, progressive, VIP_FRAME_PROGRESSIVE_SEQUENCE, 1);
}

int setFrameTopFieldFirst(VipFrameHandle *handle, uint32_t topFieldFirst)
{
	return setFlags(handle, topFieldFirst, VIP_FRAME_TOP_FIELD_FIRST, 1);
}

int setFrameFormat(VipFrameHandle *handle, uint32_t format)
{
	return setFlags(handle, format, VIP_FRAME_FORMAT_SHIFT, 7);
}

int setFrameDamaged(VipFrameHandle *handle, uint32_t damaged)
{
	return setFlags(handle, damaged, VIP_FRAME_DAMAGED, 1);
}

/* everything but the extension, which the copy has none of */
int getFrameInfo(VipFrameHandle *handle, VipFrameInfo *info)
{
	VipFrameInfo *source = frameInfo(handle);
	uint32_t i;

	if (!source)
		return -1;
	info->width = source->width;
	info->height = source->height;
	info->cropLeft = source->cropLeft;
	info->cropRight = source->cropRight;
	info->cropTop = source->cropTop;
	info->cropBottom = source->cropBottom;
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++)
		info->display[i] = source->display[i];
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++)
		info->offset[i] = source->offset[i];
	info->flags = source->flags;
	info->extension = NULL;
	info->colour = source->colour;
	return 0;
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

int setFrameMatrix(VipFrameHandle *handle, uint8_t matrix)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->matrix = matrix;
	return 0;
}

int setFrameRateKind(VipFrameHandle *handle, uint8_t kind)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->rateKind = kind;
	return 0;
}
