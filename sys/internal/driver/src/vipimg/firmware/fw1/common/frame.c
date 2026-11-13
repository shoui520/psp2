/*
 * The frame store accessors (frame.h), in the original's order. PU0 links the
 * whole library and calls none of it; PU1 uses part of it.
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

int getFramePicture(VipFrameHandle *handle, uint32_t *picture)
{
	if (!handle->frame)
		return -1;
	*picture = handle->frame->picture;
	return 0;
}

int getFrameTimestampIndex(VipFrameHandle *handle, uint32_t *index)
{
	if (!handle->frame)
		return -1;
	*index = handle->frame->timestampIndex;
	return 0;
}

int setFrameStore(VipFrameHandle *handle, VipFrame *frame)
{
	handle->frame = frame;
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

int setFrameTimestampIndex(VipFrameHandle *handle, uint32_t index)
{
	if (!handle->frame)
		return -1;
	handle->frame->timestampIndex = index;
	return 0;
}

int setFrameExtension(VipFrameHandle *handle, VipFrameExtension *extension)
{
	if (!handle->frame)
		return -1;
	handle->frame->info.extension = extension;
	return 0;
}

/*
 * Clear the display information, the colour description to unspecified, and
 * the extension's VUI fields and state. The handle must have a frame store.
 */
int clearFrameInfo(VipFrameHandle *handle)
{
	VipFrameInfo *info = frameInfo(handle);
	VipFrameExtension *extension;
	uint32_t i;

	info->width = 0;
	info->height = 0;
	info->cropLeft = 0;
	info->cropRight = 0;
	info->cropTop = 0;
	info->cropBottom = 0;
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++) {
		info->display[i].width = 0;
		info->display[i].height = 0;
	}
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++) {
		info->offset[i].x = 0;
		info->offset[i].y = 0;
	}
	info->flags = 0;
	info->colour = VIP_COLOUR_UNSPECIFIED;
	extension = info->extension;
	if (extension) {
		extension->profileIdc = 0;
		extension->levelIdc = 0;
		extension->chromaSampleLoc = 0;
		extension->numUnitsInTick = 0;
		extension->timeScale = 0;
		extension->state = 0;
	}
	return 0;
}

/* the display information's halfwords */
#define INFO_GETTER(name, type, field)					\
int name(VipFrameHandle *handle, type *value)				\
{									\
	VipFrameInfo *info = frameInfo(handle);				\
									\
	if (!info)							\
		return -1;						\
	*value = info->field;						\
	return 0;							\
}

INFO_GETTER(getFrameWidth, uint16_t, width)
INFO_GETTER(getFrameHeight, uint16_t, height)
INFO_GETTER(getFrameCropLeft, uint16_t, cropLeft)
INFO_GETTER(getFrameCropRight, uint16_t, cropRight)
INFO_GETTER(getFrameCropTop, uint16_t, cropTop)
INFO_GETTER(getFrameCropBottom, uint16_t, cropBottom)
INFO_GETTER(getFrameDisplayWidth0, uint16_t, display[0].width)
INFO_GETTER(getFrameDisplayHeight0, uint16_t, display[0].height)
INFO_GETTER(getFrameDisplayWidth1, uint16_t, display[1].width)
INFO_GETTER(getFrameDisplayHeight1, uint16_t, display[1].height)
INFO_GETTER(getFrameDisplayWidth2, uint16_t, display[2].width)
INFO_GETTER(getFrameDisplayHeight2, uint16_t, display[2].height)
INFO_GETTER(getFrameOffsetX0, int16_t, offset[0].x)
INFO_GETTER(getFrameOffsetY0, int16_t, offset[0].y)
INFO_GETTER(getFrameOffsetX1, int16_t, offset[1].x)
INFO_GETTER(getFrameOffsetY1, int16_t, offset[1].y)
INFO_GETTER(getFrameOffsetX2, int16_t, offset[2].x)
INFO_GETTER(getFrameOffsetY2, int16_t, offset[2].y)

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

static inline int setFlags(VipFrameHandle *handle, uint32_t value, uint32_t shift,
	uint32_t mask)
{
	VipFrameInfo *info = frameInfo(handle);

	if (!info)
		return -1;
	info->flags = (value & mask) << shift | (info->flags & ~(mask << shift));
	return 0;
}

/* a byte of VipFrameInfo.colour */
static inline int getColour(VipFrameHandle *handle, uint8_t *value, uint32_t shift)
{
	VipFrameInfo *info = frameInfo(handle);

	if (!info)
		return -1;
	*value = info->colour >> shift;
	return 0;
}

static inline int setColour(VipFrameHandle *handle, uint8_t value, uint32_t shift)
{
	VipFrameInfo *info = frameInfo(handle);

	if (!info)
		return -1;
	info->colour = (uint32_t)value << shift | (info->colour & ~(0xffu << shift));
	return 0;
}

int getFrameAspect(VipFrameHandle *handle, uint8_t *aspect)
{
	return getFlags(handle, aspect, VIP_FRAME_ASPECT_SHIFT, 0xf);
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

int getFrameFormat(VipFrameHandle *handle, uint8_t *format)
{
	return getFlags(handle, format, VIP_FRAME_FORMAT_SHIFT, 7);
}

int getFrameRandomAccess(VipFrameHandle *handle, uint8_t *randomAccess)
{
	return getFlags(handle, randomAccess, VIP_FRAME_RANDOM_ACCESS, 1);
}

int getFrameNalReference(VipFrameHandle *handle, uint8_t *reference)
{
	return getFlags(handle, reference, VIP_FRAME_NAL_REFERENCE, 1);
}

int getFrameDamaged(VipFrameHandle *handle, uint8_t *damaged)
{
	return getFlags(handle, damaged, VIP_FRAME_DAMAGED, 1);
}

int getFrameColourPrimaries(VipFrameHandle *handle, uint8_t *primaries)
{
	return getColour(handle, primaries, VIP_FRAME_COLOUR_PRIMARIES_SHIFT);
}

int getFrameTransferCharacteristics(VipFrameHandle *handle, uint8_t *transfer)
{
	return getColour(handle, transfer, VIP_FRAME_TRANSFER_SHIFT);
}

int getFrameMatrixCoefficients(VipFrameHandle *handle, uint8_t *matrix)
{
	return getColour(handle, matrix, VIP_FRAME_MATRIX_SHIFT);
}

#define INFO_SETTER(name, type, field)					\
int name(VipFrameHandle *handle, type value)				\
{									\
	VipFrameInfo *info = frameInfo(handle);				\
									\
	if (!info)							\
		return -1;						\
	info->field = value;						\
	return 0;							\
}

INFO_SETTER(setFrameWidth, uint16_t, width)
INFO_SETTER(setFrameHeight, uint16_t, height)
INFO_SETTER(setFrameCropLeft, uint16_t, cropLeft)
INFO_SETTER(setFrameCropRight, uint16_t, cropRight)
INFO_SETTER(setFrameCropTop, uint16_t, cropTop)
INFO_SETTER(setFrameCropBottom, uint16_t, cropBottom)
INFO_SETTER(setFrameDisplayWidth0, uint16_t, display[0].width)
INFO_SETTER(setFrameDisplayHeight0, uint16_t, display[0].height)
INFO_SETTER(setFrameDisplayWidth1, uint16_t, display[1].width)
INFO_SETTER(setFrameDisplayHeight1, uint16_t, display[1].height)
INFO_SETTER(setFrameDisplayWidth2, uint16_t, display[2].width)
INFO_SETTER(setFrameDisplayHeight2, uint16_t, display[2].height)
INFO_SETTER(setFrameOffsetX0, int16_t, offset[0].x)
INFO_SETTER(setFrameOffsetY0, int16_t, offset[0].y)
INFO_SETTER(setFrameOffsetX1, int16_t, offset[1].x)
INFO_SETTER(setFrameOffsetY1, int16_t, offset[1].y)
INFO_SETTER(setFrameOffsetX2, int16_t, offset[2].x)
INFO_SETTER(setFrameOffsetY2, int16_t, offset[2].y)

int setFrameAspect(VipFrameHandle *handle, uint32_t aspect)
{
	return setFlags(handle, aspect, VIP_FRAME_ASPECT_SHIFT, 0xf);
}

int setFrameRate(VipFrameHandle *handle, uint32_t rate)
{
	return setFlags(handle, rate, VIP_FRAME_RATE_SHIFT, 0xf);
}

int setFrameProgressiveSequence(VipFrameHandle *handle, uint32_t progressive)
{
	return setFlags(handle, progressive, VIP_FRAME_PROGRESSIVE_SEQUENCE, 1);
}

int setFrameProgressiveFrame(VipFrameHandle *handle, uint32_t progressive)
{
	return setFlags(handle, progressive, VIP_FRAME_PROGRESSIVE_FRAME, 1);
}

int setFrameTopFieldFirst(VipFrameHandle *handle, uint32_t topFieldFirst)
{
	return setFlags(handle, topFieldFirst, VIP_FRAME_TOP_FIELD_FIRST, 1);
}

int setFrameRepeatFirstField(VipFrameHandle *handle, uint32_t repeat)
{
	return setFlags(handle, repeat, VIP_FRAME_REPEAT_FIRST_FIELD, 1);
}

int setFrameType(VipFrameHandle *handle, uint32_t type)
{
	return setFlags(handle, type, VIP_FRAME_TYPE_SHIFT, 7);
}

int setFrameFormat(VipFrameHandle *handle, uint32_t format)
{
	return setFlags(handle, format, VIP_FRAME_FORMAT_SHIFT, 7);
}

int setFrameRandomAccess(VipFrameHandle *handle, uint32_t randomAccess)
{
	return setFlags(handle, randomAccess, VIP_FRAME_RANDOM_ACCESS, 1);
}

int setFrameNalReference(VipFrameHandle *handle, uint32_t reference)
{
	return setFlags(handle, reference, VIP_FRAME_NAL_REFERENCE, 1);
}

int setFrameDamaged(VipFrameHandle *handle, uint32_t damaged)
{
	return setFlags(handle, damaged, VIP_FRAME_DAMAGED, 1);
}

int setFrameColourPrimaries(VipFrameHandle *handle, uint8_t primaries)
{
	return setColour(handle, primaries, VIP_FRAME_COLOUR_PRIMARIES_SHIFT);
}

int setFrameTransferCharacteristics(VipFrameHandle *handle, uint8_t transfer)
{
	return setColour(handle, transfer, VIP_FRAME_TRANSFER_SHIFT);
}

int setFrameMatrixCoefficients(VipFrameHandle *handle, uint8_t matrix)
{
	return setColour(handle, matrix, VIP_FRAME_MATRIX_SHIFT);
}

/* the display information without the extension pointer, which reads back NULL */
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

/* the extension's chroma (or video signal) word and timing, to and from another's */
int getFrameVuiFields(VipFrameHandle *handle, VipFrameExtension *fields)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	fields->chromaSampleLoc = extension->chromaSampleLoc;
	fields->numUnitsInTick = extension->numUnitsInTick;
	fields->timeScale = extension->timeScale;
	return 0;
}

int setFrameVuiFields(VipFrameHandle *handle, const VipFrameExtension *fields)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->chromaSampleLoc = fields->chromaSampleLoc;
	extension->numUnitsInTick = fields->numUnitsInTick;
	extension->timeScale = fields->timeScale;
	return 0;
}

/* a field of the extension's word at the given place */
static inline int getExtensionBits(uint32_t word, uint8_t *value, uint32_t shift,
	uint32_t mask)
{
	*value = word >> shift & mask;
	return 0;
}

int getFrameProfile(VipFrameHandle *handle, uint16_t *profileIdc)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	*profileIdc = extension->profileIdc;
	return 0;
}

int getFrameLevel(VipFrameHandle *handle, uint16_t *levelIdc)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	*levelIdc = extension->levelIdc;
	return 0;
}

int getFrameExtensionState(VipFrameHandle *handle, uint8_t *state)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	return getExtensionBits(extension->state, state, VIP_EXTENSION_STATE_SHIFT, 7);
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

int setFrameExtensionState(VipFrameHandle *handle, uint32_t state)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->state = state << VIP_EXTENSION_STATE_SHIFT |
		(extension->state & ~(7u << VIP_EXTENSION_STATE_SHIFT));
	return 0;
}

int getFrameChromaLocTop(VipFrameHandle *handle, uint8_t *type)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	return getExtensionBits(extension->chromaSampleLoc, type,
		VIP_EXTENSION_CHROMA_TOP_SHIFT, 7);
}

int getFrameChromaLocBottom(VipFrameHandle *handle, uint8_t *type)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	return getExtensionBits(extension->chromaSampleLoc, type,
		VIP_EXTENSION_CHROMA_BOTTOM_SHIFT, 7);
}

int getFrameNumUnitsInTick(VipFrameHandle *handle, uint32_t *numUnitsInTick)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	*numUnitsInTick = extension->numUnitsInTick;
	return 0;
}

int getFrameTimeScale(VipFrameHandle *handle, uint32_t *timeScale)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	*timeScale = extension->timeScale;
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

int setFrameRateKind(VipFrameHandle *handle, uint8_t kind)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->rateKind = kind;
	return 0;
}

/* the extension's word read as a video signal type (frame.h) */
int getFrameSignalTypePresent(VipFrameHandle *handle, uint8_t *present)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	return getExtensionBits(extension->chromaSampleLoc, present,
		VIP_EXTENSION_SIGNAL_TYPE_PRESENT, 1);
}

int getFrameColourDescriptionPresent(VipFrameHandle *handle, uint8_t *present)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	return getExtensionBits(extension->chromaSampleLoc, present,
		VIP_EXTENSION_COLOUR_DESCRIPTION_PRESENT, 1);
}

int getFrameVideoFormat(VipFrameHandle *handle, uint8_t *format)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	return getExtensionBits(extension->chromaSampleLoc, format,
		VIP_EXTENSION_VIDEO_FORMAT_SHIFT, 7);
}

int getFrameFullRange(VipFrameHandle *handle, uint8_t *fullRange)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	return getExtensionBits(extension->chromaSampleLoc, fullRange,
		VIP_EXTENSION_FULL_RANGE_SHIFT, 7);
}

/* checks the display information only: the extension must be there */
int getFrameColourDescription(VipFrameHandle *handle, uint32_t *description)
{
	VipFrameInfo *info = frameInfo(handle);

	if (!info)
		return -1;
	*description = info->extension->chromaSampleLoc & VIP_EXTENSION_COLOUR_DESCRIPTION;
	return 0;
}

int setFrameSignalTypePresent(VipFrameHandle *handle, uint32_t present)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->chromaSampleLoc = present << VIP_EXTENSION_SIGNAL_TYPE_PRESENT |
		(extension->chromaSampleLoc & ~(1u << VIP_EXTENSION_SIGNAL_TYPE_PRESENT));
	return 0;
}

int setFrameColourDescriptionPresent(VipFrameHandle *handle, uint32_t present)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->chromaSampleLoc = (present & 1) << VIP_EXTENSION_COLOUR_DESCRIPTION_PRESENT |
		(extension->chromaSampleLoc & ~(1u << VIP_EXTENSION_COLOUR_DESCRIPTION_PRESENT));
	return 0;
}

int setFrameVideoFormat(VipFrameHandle *handle, uint32_t format)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->chromaSampleLoc = (format & 7) << VIP_EXTENSION_VIDEO_FORMAT_SHIFT |
		(extension->chromaSampleLoc & ~(7u << VIP_EXTENSION_VIDEO_FORMAT_SHIFT));
	return 0;
}

int setFrameFullRange(VipFrameHandle *handle, uint32_t fullRange)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->chromaSampleLoc = (fullRange & 7) << VIP_EXTENSION_FULL_RANGE_SHIFT |
		(extension->chromaSampleLoc & ~(7u << VIP_EXTENSION_FULL_RANGE_SHIFT));
	return 0;
}

int setFrameColourDescription(VipFrameHandle *handle, uint32_t description)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->chromaSampleLoc = (description & VIP_EXTENSION_COLOUR_DESCRIPTION) |
		(extension->chromaSampleLoc & ~VIP_EXTENSION_COLOUR_DESCRIPTION);
	return 0;
}

/* the flags compareFrameInfo compares: the aspect to the progressive frame flag */
#define COMPARED_FLAGS		0xffc00000u

/*
 * Whether two pictures' display information differs: 0 if not, 1 if it does,
 * -1 without both. With both extensions it also compares their profile, level,
 * state and chroma word, and unless signalOnly, their timing.
 */
int compareFrameInfo(const VipFrameInfo *info, const VipFrameInfo *other, uint32_t signalOnly)
{
	const VipFrameExtension *extension, *otherExtension;
	uint32_t i;

	if (!info || !other)
		return -1;
	if (info->width != other->width || info->height != other->height ||
	    info->cropLeft != other->cropLeft || info->cropRight != other->cropRight ||
	    info->cropTop != other->cropTop || info->cropBottom != other->cropBottom)
		return 1;
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++)
		if (info->display[i].width != other->display[i].width ||
		    info->display[i].height != other->display[i].height)
			return 1;
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++)
		if (info->offset[i].x != other->offset[i].x ||
		    info->offset[i].y != other->offset[i].y)
			return 1;
	if ((info->flags & COMPARED_FLAGS) != (other->flags & COMPARED_FLAGS) ||
	    info->colour != other->colour)
		return 1;
	extension = info->extension;
	otherExtension = other->extension;
	if (!extension || !otherExtension)
		return extension || otherExtension ? 1 : 0;
	if (extension->profileIdc != otherExtension->profileIdc ||
	    extension->levelIdc != otherExtension->levelIdc ||
	    extension->state != otherExtension->state)
		return 1;
	if (signalOnly)
		return extension->chromaSampleLoc != otherExtension->chromaSampleLoc;
	if (extension->chromaSampleLoc != otherExtension->chromaSampleLoc ||
	    extension->numUnitsInTick != otherExtension->numUnitsInTick ||
	    extension->timeScale != otherExtension->timeScale)
		return 1;
	return 0;
}
