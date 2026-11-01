/*
 * SceGxm: transfers (the 2D copy, fill and downscale engine).
 *
 * A transfer is a short list of control words handed to the driver with
 * the sync object and notification that order it.
 */

#include "gxm_internal.h"

#define SCE_GXM_TRANSFER_LINEAR		0x00000000
#define SCE_GXM_TRANSFER_TILED		0x00400000
#define SCE_GXM_TRANSFER_SWIZZLED	0x00800000

#define SCE_GXM_TRANSFER_COLORKEY_NONE	0
#define SCE_GXM_TRANSFER_COLORKEY_PASS	1
#define SCE_GXM_TRANSFER_COLORKEY_REJECT	2

#define SCE_GXM_TRANSFER_FORMAT_U2U10U10U10_ABGR	0x000d0000
#define SCE_GXM_TRANSFER_FORMAT_RAW64	0x00120000
#define SCE_GXM_TRANSFER_FORMAT_RAW128	0x00130000

#define SCE_GXM_TRANSFER_FRAGMENT_SYNC	0x00000001
#define SCE_GXM_TRANSFER_VERTEX_SYNC	0x00000002

/* control words */
#define GXM_TRANSFER_COLOR_KEY		0x00000002
#define GXM_TRANSFER_SOURCE		0x40000000
#define GXM_TRANSFER_SOURCE_POSITION	0x10000000
#define GXM_TRANSFER_DEST		0x50000000
#define GXM_TRANSFER_CONTROL		0x60000000
#define GXM_TRANSFER_CONTROL_FILL	0x00000010
#define GXM_TRANSFER_CONTROL_KEY	0x00000020
#define GXM_TRANSFER_CONTROL_KEY_PASS	0x00000100
#define GXM_TRANSFER_CONTROL_DOWNSCALE	0x00000080

#define GXM_TRANSFER_MAX_COORD		0x2000
#define GXM_TRANSFER_MAX_WORDS		26

static inline SceUInt32 gxmTransferPosition(SceUInt32 x, SceUInt32 y)
{
	return ((x << 13) & 0x03ffe000) | (y & 0x1fff);
}

static inline SceUInt32 gxmTransferSize(SceUInt32 width, SceUInt32 height)
{
	return ((width << 13) & 0x03ffe000) | height;
}

/* the checks every entry point makes, with Sony's messages */
#define GXM_TRANSFER_CHECK_THREAD(line, function)					\
	if (gxmDisplayQueueIsCurrentThread()) {						\
		GXM_DBG_ERROR(line, "Function " #function " cannot be called from the "	\
			      "display queue thread.");					\
		return SCE_GXM_ERROR_INVALID_THREAD;					\
	}
#define GXM_TRANSFER_CHECK_FORMAT(line, function, format)				\
	if (!gxmTransferFormatIsSupported(format)) {					\
		GXM_DBG_ERROR(line, "Parameter " #format " to function " #function	\
			      " has value %08x, which is not a valid transfer format.",	\
			      format);							\
		return SCE_GXM_ERROR_INVALID_VALUE;					\
	}
#define GXM_TRANSFER_CHECK_ADDRESS(nullLine, alignLine, function, address, format)	\
	if (address == NULL) {								\
		GXM_DBG_ERROR_NULL(nullLine, function, address);			\
		return SCE_GXM_ERROR_INVALID_POINTER;					\
	}										\
	if (((SceUInt32)address & (gxmTransferFormatGetAlignment(format) - 1)) != 0) {	\
		GXM_DBG_ERROR(alignLine, "Parameter " #address " to function " #function	\
			      " has value %p, which is not aligned to %d.", address,	\
			      gxmTransferFormatGetAlignment(format));			\
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;					\
	}
/* a stride is 16-bit signed and must hold whole pixels */
#define GXM_TRANSFER_CHECK_STRIDE(rangeLine, multipleLine, function, stride, format)	\
	if ((SceUInt32)(stride + 0x8000) >= 0x10000) {					\
		GXM_DBG_ERROR(rangeLine, "Parameter " #stride " to function " #function	\
			      " must be in the range [%d, %d].", -0x8000, 0x7fff);	\
		return SCE_GXM_ERROR_INVALID_VALUE;					\
	}										\
	if (stride % gxmTransferFormatGetBytesPerPixel(format) != 0) {			\
		GXM_DBG_ERROR(multipleLine, "Parameter " #stride " to function " #function	\
			      " has value %d, but must be a multiple of %d bytes when "	\
			      #format " is [%s].", stride,				\
			      gxmTransferFormatGetBytesPerPixel(format),		\
			      gxmTransferFormatGetName(format));			\
		return SCE_GXM_ERROR_INVALID_VALUE;					\
	}
#define GXM_TRANSFER_CHECK_SUM(line, function, a, b, sum, limit)			\
	if ((sum) >= GXM_TRANSFER_MAX_COORD) {						\
		GXM_DBG_ERROR(line, "Sum of parameters " a " and " b " to function "	\
			      #function " is %u, but must be " limit ".", sum,		\
			      GXM_TRANSFER_MAX_COORD - 1);				\
		return SCE_GXM_ERROR_INVALID_VALUE;					\
	}
#define GXM_TRANSFER_IN_RANGE	"in the range [0, %d]"
#define GXM_TRANSFER_LESS_THAN	"less than %d"

/* the notification as the GPU addresses it */
static inline int gxmTransferNotification(const SceGxmNotification *notification,
					  GxmGpuNotification *gpu, const GxmGpuNotification **out)
{
	const PVRSRV_CLIENT_MEM_INFO *block;
	int err;

	if (notification == NULL) {
		*out = NULL;
		return 0;
	}
	err = gxmNotificationCheckAddress(notification->address);
	if (err != 0)
		return err;
	block = gxmGlobals.top.notifications;
	gpu->value = notification->value;
	gpu->handle = block->hKernelMemInfo;
	gpu->gpuAddress = (SceUInt32)notification->address + block->sDevVAddr -
			  (SceUInt32)block->pvLinAddr;
	*out = gpu;
	return 0;
}

static inline __attribute__((always_inline))
int gxmTransferControlStream(const SceUInt32 *controlStream, SceUInt32 controlStreamWords,
			     void *syncObject, SceUInt32 syncFlags,
			     const SceGxmNotification *notification)
{
	const GxmGpuNotification *gpuNotification;
	GxmGpuNotification gpu;
	int err;

	if (controlStream == NULL) {
		GXM_DBG_ERROR_NULL(794, sceGxmTransferControlStream, controlStream);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (controlStreamWords - 1 >= GXM_TRANSFER_MAX_WORDS) {
		GXM_DBG_ERROR(800, "Parameter controlStreamWords to function "
			      "sceGxmTransferControlStream has value %u, which is not in the "
			      "range [1, %u]", controlStreamWords, GXM_TRANSFER_MAX_WORDS);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	err = gxmTransferNotification(notification, &gpu, &gpuNotification);
	if (err != 0)
		return err;
	err = SceGpuEs4User_5564C1BE(controlStream, controlStreamWords, &gxmGlobals.top,
				     gxmGlobals.top.transferQueue, syncObject,
				     (syncFlags & SCE_GXM_TRANSFER_VERTEX_SYNC) >> 1,
				     syncFlags & SCE_GXM_TRANSFER_FRAGMENT_SYNC, gpuNotification);
	if (err != 0) {
		GXM_DBG_ERROR(829, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	return 0;
}

int sceGxmTransferControlStream(const SceUInt32 *controlStream, SceUInt32 controlStreamWords,
				void *syncObject, SceUInt32 syncFlags,
				const SceGxmNotification *notification);

/* Sony's release build inlined the control stream call into Fill and Downscale, its debug build did not */
#if SCE_DBG_ASSERTS_ENABLED
#define gxmTransferSubmit	sceGxmTransferControlStream
#else
#define gxmTransferSubmit	gxmTransferControlStream
#endif

int sceGxmTransferDownscale(SceGxmTransferFormat srcFormat, const void *srcAddress,
			    SceUInt32 srcX, SceUInt32 srcY, SceUInt32 srcWidth, SceUInt32 srcHeight,
			    SceInt32 srcStride, SceGxmTransferFormat destFormat, void *destAddress,
			    SceUInt32 destX, SceUInt32 destY, SceInt32 destStride, void *syncObject,
			    SceUInt32 syncFlags, const SceGxmNotification *notification)
{
	SceUInt32 words[9];

	GXM_TRANSFER_CHECK_THREAD(474, sceGxmTransferDownscale);
	/* an even size, in whole 16-pixel blocks above 16 */
	if (srcWidth - 2 >= 0x3ff) {
		GXM_DBG_ERROR(480, "Parameter srcWidth to function sceGxmTransferDownscale must "
			      "be in the range [2, %d].", 1024);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if ((srcWidth & 1) != 0) {
		GXM_DBG_ERROR(486, "Parameter srcWidth to function sceGxmTransferDownscale must "
			      "be even.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (srcWidth >= 16 && (srcWidth & 0xf) != 0) {
		GXM_DBG_ERROR(492, "Parameter srcWidth to function SceGxmTransferDownscale must "
			      "be less than 16, or aligned to 16.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (srcHeight - 2 >= 0x3ff) {
		GXM_DBG_ERROR(498, "Parameter srcHeight to function sceGxmTransferDownscale must "
			      "be in the range [2, %d].", 1024);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if ((srcHeight & 1) != 0) {
		GXM_DBG_ERROR(503, "Parameter srcHeight to function sceGxmTransferDownscale must "
			      "be even.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	GXM_TRANSFER_CHECK_FORMAT(509, sceGxmTransferDownscale, srcFormat);
	if (!gxmTransferFormatIsColor(srcFormat)) {
		GXM_DBG_ERROR(515, "Parameter srcFormat [%s] to function sceGxmTransferDownscale "
			      "must be an RGB format.", gxmTransferFormatGetName(srcFormat));
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	GXM_TRANSFER_CHECK_ADDRESS(520, 526, sceGxmTransferDownscale, srcAddress, srcFormat);
	GXM_TRANSFER_CHECK_STRIDE(532, 538, sceGxmTransferDownscale, srcStride, srcFormat);
	GXM_TRANSFER_CHECK_FORMAT(544, sceGxmTransferDownscale, destFormat);
	if (!gxmTransferFormatIsColor(destFormat)) {
		GXM_DBG_ERROR(550, "Parameter destFormat [%s] to function sceGxmTransferDownscale "
			      "must be an RGB format.", gxmTransferFormatGetName(destFormat));
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	GXM_TRANSFER_CHECK_ADDRESS(555, 561, sceGxmTransferDownscale, destAddress, destFormat);
	GXM_TRANSFER_CHECK_STRIDE(567, 573, sceGxmTransferDownscale, destStride, destFormat);
	GXM_TRANSFER_CHECK_SUM(584, sceGxmTransferDownscale, "srcX", "srcWidth",
			       srcWidth + srcX, GXM_TRANSFER_IN_RANGE);
	GXM_TRANSFER_CHECK_SUM(590, sceGxmTransferDownscale, "srcY", "srcHeight",
			       srcHeight + srcY, GXM_TRANSFER_IN_RANGE);
	GXM_TRANSFER_CHECK_SUM(596, sceGxmTransferDownscale, "destX", "half of srcWidth",
			       (srcWidth >> 1) + destX, GXM_TRANSFER_IN_RANGE);
	GXM_TRANSFER_CHECK_SUM(602, sceGxmTransferDownscale, "destY", "half of srcHeight",
			       (srcHeight >> 1) + destY, GXM_TRANSFER_IN_RANGE);

	words[0] = srcFormat | (srcStride & 0xffff) | GXM_TRANSFER_SOURCE;
	words[1] = (SceUInt32)srcAddress;
	words[2] = gxmTransferPosition(srcX, srcY) | GXM_TRANSFER_SOURCE_POSITION;
	words[3] = gxmTransferSize(srcWidth, srcHeight);
	words[4] = destFormat | (destStride & 0xffff) | GXM_TRANSFER_DEST;
	words[5] = (SceUInt32)destAddress;
	words[6] = GXM_TRANSFER_CONTROL | GXM_TRANSFER_CONTROL_DOWNSCALE;
	words[7] = gxmTransferPosition(destX, destY);
	words[8] = gxmTransferSize(srcWidth >> 1, srcHeight >> 1);
	return gxmTransferSubmit(words, 9, syncObject, syncFlags, notification);
}

int sceGxmTransferFill(SceUInt32 fillColor, SceGxmTransferFormat destFormat, void *destAddress,
		       SceUInt32 destX, SceUInt32 destY, SceUInt32 destWidth, SceUInt32 destHeight,
		       SceInt32 destStride, void *syncObject, SceUInt32 syncFlags,
		       const SceGxmNotification *notification)
{
	SceUInt32 words[8], format;

	GXM_TRANSFER_CHECK_THREAD(650, sceGxmTransferFill);
	GXM_TRANSFER_CHECK_FORMAT(656, sceGxmTransferFill, destFormat);
	if (!gxmTransferFormatIsColor(destFormat) && !gxmTransferFormatIsYuv(destFormat)) {
		GXM_DBG_ERROR(661, "Parameter destFormat to function sceGxmTransferFill must be "
			      "an RGB or YUV format.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	GXM_TRANSFER_CHECK_ADDRESS(666, 672, sceGxmTransferFill, destAddress, destFormat);
	if (destWidth - 1 >= 0x400) {
		GXM_DBG_ERROR(678, "Parameter destWidth to function sceGxmTransferFill must be in "
			      "the range [1, %d].", 1024);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (destHeight - 1 >= 0x400) {
		GXM_DBG_ERROR(684, "Parameter destHeight to function sceGxmTransferFill must be in "
			      "the range [1, %d].", 1024);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	GXM_TRANSFER_CHECK_STRIDE(690, 696, sceGxmTransferFill, destStride, destFormat);
	/* YUV is filled in pixel pairs */
	if (gxmTransferFormatIsYuv(destFormat)) {
		if ((destWidth & 1) != 0) {
			GXM_DBG_ERROR(704, "Parameter destWidth to function sceGxmTransferFill has "
				      "value %u which must be even when using a YUV format.",
				      destWidth);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if ((destX & 1) != 0) {
			GXM_DBG_ERROR(710, "Parameter destX to function sceGxmTransferFill has "
				      "value %u which must be even when using YUV formats.", destX);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	}
	GXM_TRANSFER_CHECK_SUM(718, sceGxmTransferFill, "destX", "destWidth",
			       destWidth + destX, GXM_TRANSFER_IN_RANGE);
	GXM_TRANSFER_CHECK_SUM(724, sceGxmTransferFill, "destY", "destHeight",
			       destHeight + destY, GXM_TRANSFER_IN_RANGE);

	format = destFormat | (destStride & 0xffff);
	words[7] = gxmTransferSize(destWidth, destHeight);
	words[6] = gxmTransferPosition(destX, destY);
	words[0] = format | GXM_TRANSFER_SOURCE;
	words[2] = format | GXM_TRANSFER_DEST;
	words[4] = GXM_TRANSFER_CONTROL | GXM_TRANSFER_CONTROL_FILL;
	words[1] = (SceUInt32)destAddress;
	words[3] = (SceUInt32)destAddress;
	words[5] = fillColor;
	return gxmTransferSubmit(words, 8, syncObject, syncFlags, notification);
}

int sceGxmTransferFinish(void)
{
	int err;

	GXM_TRANSFER_CHECK_THREAD(756, sceGxmTransferFinish);
	gxmRazorMarker(0x19000000, gxmGlobals.frameIndex);
	err = SceGpuEs4User_7E965369(&gxmGlobals.top, gxmGlobals.top.transferQueue);
	if (err != 0) {
		GXM_DBG_ERROR(771, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	gxmRazorMarker(0x1a000000, gxmGlobals.frameIndex);
	return 0;
}

GXM_NOINLINE int sceGxmTransferControlStream(const SceUInt32 *controlStream,
					     SceUInt32 controlStreamWords, void *syncObject,
					     SceUInt32 syncFlags,
					     const SceGxmNotification *notification)
{
	return gxmTransferControlStream(controlStream, controlStreamWords, syncObject, syncFlags,
					notification);
}

static inline _Bool gxmIsPow2(SceUInt32 value)
{
	return (value & (value - 1)) == 0;
}

int sceGxmTransferCopy(SceUInt32 width, SceUInt32 height, SceUInt32 colorKeyValue,
		       SceUInt32 colorKeyMask, SceUInt32 colorKeyMode,
		       SceGxmTransferFormat srcFormat, SceUInt32 srcType, const void *srcAddress,
		       SceUInt32 srcX, SceUInt32 srcY, SceInt32 srcStride,
		       SceGxmTransferFormat destFormat, SceUInt32 destType, void *destAddress,
		       SceUInt32 destX, SceUInt32 destY, SceInt32 destStride, void *syncObject,
		       SceUInt32 syncFlags, const SceGxmNotification *notification)
{
	_Bool srcSwizzled, srcTiled, destSwizzled, destTiled;
	SceUInt32 words[12], count, size;

	GXM_TRANSFER_CHECK_THREAD(106, sceGxmTransferCopy);
	if (width - 1 >= 0x400) {
		GXM_DBG_ERROR(112, "Parameter width to function sceGxmTransferCopy must be in the "
			      "range [1, %d].", 1024);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (height - 1 >= 0x400) {
		GXM_DBG_ERROR(118, "Parameter height to function sceGxmTransferCopy must be in the "
			      "range [1, %d].", 1024);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	GXM_TRANSFER_CHECK_FORMAT(124, sceGxmTransferCopy, srcFormat);
	GXM_TRANSFER_CHECK_ADDRESS(129, 135, sceGxmTransferCopy, srcAddress, srcFormat);
	GXM_TRANSFER_CHECK_STRIDE(141, 147, sceGxmTransferCopy, srcStride, srcFormat);
	GXM_TRANSFER_CHECK_FORMAT(153, sceGxmTransferCopy, destFormat);
	GXM_TRANSFER_CHECK_ADDRESS(158, 165, sceGxmTransferCopy, destAddress, destFormat);
	GXM_TRANSFER_CHECK_STRIDE(171, 177, sceGxmTransferCopy, destStride, destFormat);
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_SURFACE_ANY)) {
		if ((srcType & ~SCE_GXM_TRANSFER_TILED) != 0 &&
		    srcType != SCE_GXM_TRANSFER_SWIZZLED) {
			GXM_DBG_ERROR(186, "Parameter srcType to function sceGxmTransferCopy is not "
				      "valid.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if ((destType & ~SCE_GXM_TRANSFER_TILED) != 0 &&
		    destType != SCE_GXM_TRANSFER_SWIZZLED) {
			GXM_DBG_ERROR(193, "Parameter destType to function sceGxmTransferCopy is not "
				      "valid.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (colorKeyMode > SCE_GXM_TRANSFER_COLORKEY_REJECT) {
			GXM_DBG_ERROR(200, "Parameter colorKeyMode to function sceGxmTransferCopy is "
				      "not valid.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	}

	/* swizzled and tiled layouts do not convert into each other */
	srcSwizzled = srcType == SCE_GXM_TRANSFER_SWIZZLED;
	destSwizzled = destType == SCE_GXM_TRANSFER_SWIZZLED;
	if (srcSwizzled && destSwizzled) {
		GXM_DBG_ERROR(207, "Parameters srcType and destType to function sceGxmTransferCopy "
			      "cannot both be SCE_GXM_TRANSFER_SWIZZLED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	srcTiled = srcType == SCE_GXM_TRANSFER_TILED;
	if (srcTiled & destSwizzled) {
		GXM_DBG_ERROR(213, "Parameters srcType to function sceGxmTransferCopy cannot be "
			      "SCE_GXM_TRANSFER_TILED if destType is SCE_GXM_TRANSFER_SWIZZLED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	destTiled = destType == SCE_GXM_TRANSFER_TILED;
	if (srcSwizzled & destTiled) {
		GXM_DBG_ERROR(218, "Parameters srcType to function sceGxmTransferCopy cannot be "
			      "SCE_GXM_TRANSFER_SWIZZLED if destType is SCE_GXM_TRANSFER_TILED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (gxmTransferFormatIsRaw(srcFormat) != gxmTransferFormatIsRaw(destFormat)) {
		GXM_DBG_ERROR(224, "Parameter srcFormat and destFormat to function "
			      "sceGxmTransferCopy must both be RAW formats, or both be non-RAW "
			      "formats.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (gxmTransferFormatIsYuv(srcFormat) != gxmTransferFormatIsYuv(destFormat)) {
		GXM_DBG_ERROR(228, "Parameter srcFormat and destFormat to function "
			      "sceGxmTransferCopy must both be YUV formats, or both be non-YUV "
			      "formats.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (gxmTransferFormatIsColor(srcFormat) != gxmTransferFormatIsColor(destFormat)) {
		GXM_DBG_ERROR(232, "Parameter srcFormat and destFormat to function "
			      "sceGxmTransferCopy must both be RGB formats, or both be non-RGB "
			      "formats.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	/* Sony's srcX and destX messages print the width */
	if (gxmTransferFormatIsYuv(srcFormat)) {
		if ((width & 1) != 0) {
			GXM_DBG_ERROR(240, "Parameter width to function sceGxmTransferCopy has value "
				      "%u which must be even when using YUV formats.", width);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if ((srcX & 1) != 0) {
			GXM_DBG_ERROR(246, "Parameter srcX to function sceGxmTransferCopy has value "
				      "%u which must be even when using YUV formats.", width);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if ((destX & 1) != 0) {
			GXM_DBG_ERROR(252, "Parameter destX to function sceGxmTransferCopy has value "
				      "%u which must be even when using YUV formats.", width);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	}
	if (gxmTransferFormatIsRaw(srcFormat)) {
		/* raw data is copied as it is, linear only, and not too much of it */
		if (srcFormat != destFormat) {
			GXM_DBG_ERROR(260, "Parameter srcFormat and destFormat to function "
				      "sceGxmTransferCopy must both be identical when using RAW "
				      "formats.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
		if (colorKeyMode != SCE_GXM_TRANSFER_COLORKEY_NONE) {
			GXM_DBG_ERROR(265, "Parameter colorKeyMode to function sceGxmTransferCopy "
				      "must be SCE_GXM_TRANSFER_COLORKEY_NONE when using RAW formats.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
		if (srcFormat == SCE_GXM_TRANSFER_FORMAT_RAW64) {
			if (width * height > 0x80000) {
				GXM_DBG_ERROR(272, "When srcFormat is SCE_GXM_TRANSFER_FORMAT_RAW64, the "
					      "total number of pixels being copied must be in the range "
					      "[1, %d].", 0x80000);
				return SCE_GXM_ERROR_INVALID_VALUE;
			}
		} else if (srcFormat == SCE_GXM_TRANSFER_FORMAT_RAW128) {
			if (width * height > 0x40000) {
				GXM_DBG_ERROR(280, "When srcFormat is SCE_GXM_TRANSFER_FORMAT_RAW128, the "
					      "total number of pixels being copied must be in the range "
					      "[1, %d].", 0x40000);
				return SCE_GXM_ERROR_INVALID_VALUE;
			}
		}
		if (srcTiled) {
			GXM_DBG_ERROR(287, "Parameter srcType to function sceGxmTransferCopy cannot be "
				      "SCE_GXM_TRANSFER_TILED when using RAW formats.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
		if (destTiled) {
			GXM_DBG_ERROR(292, "Parameter destType to function sceGxmTransferCopy cannot be "
				      "SCE_GXM_TRANSFER_TILED when using RAW formats.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	}
	if (srcSwizzled || destSwizzled) {
		if (!gxmIsPow2(width)) {
			GXM_DBG_ERROR(301, "Parameter width to function sceGxmTransferCopy has value %u "
				      "which is not a power of 2, but either srcType or destType is "
				      "SCE_GXM_TRANSFER_SWIZZLED.", width);
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;
		}
		if (!gxmIsPow2(height)) {
			GXM_DBG_ERROR(307, "Parameter height to function sceGxmTransferCopy has value %u "
				      "which is not a power of 2, but either srcType or destType is "
				      "SCE_GXM_TRANSFER_SWIZZLED.", height);
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;
		}
	}
	if (srcTiled || destTiled) {
		if ((width & 0x1f) != 0) {
			GXM_DBG_ERROR(316, "Parameter width to function sceGxmTransferCopy has value %u, "
				      "but must be aligned to %u when either srcType or destType is "
				      "SCE_GXM_TRANSFER_TILED.", width, 32);
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;
		}
		if ((height & 0x1f) != 0) {
			GXM_DBG_ERROR(322, "Parameter height to function sceGxmTransferCopy has value %u, "
				      "but must be aligned to %u when srcType or destType is "
				      "SCE_GXM_TRANSFER_TILED.", height, 32);
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;
		}
	}

	/* a swizzled or tiled side is a whole surface */
	if (srcType != SCE_GXM_TRANSFER_LINEAR) {
		if (srcX != 0) {
			GXM_DBG_ERROR(332, "Parameter srcX to function sceGxmTransferCopy has value %u, "
				      "but must be 0 when srcType is not SCE_GXM_TRANSFER_LINEAR.", srcX);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (srcY != 0) {
			GXM_DBG_ERROR(338, "Parameter srcY to function sceGxmTransferCopy has value %u, "
				      "but must be 0 when srcType is not SCE_GXM_TRANSFER_LINEAR.", srcY);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (srcStride != (width * gxmTransferFormatGetBitsPerPixel(srcFormat)) >> 3) {
			GXM_DBG_ERROR(344, "Parameter srcStride to function sceGxmTransferCopy has value "
				      "%d, but must match the width in bytes (%u) when srcType is not "
				      "SCE_GXM_TRANSFER_LINEAR.", srcStride,
				      (width * gxmTransferFormatGetBitsPerPixel(srcFormat)) >> 3);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (colorKeyMode != SCE_GXM_TRANSFER_COLORKEY_NONE) {
			GXM_DBG_ERROR(349, "Parameter colorKeyMode to function sceGxmTransferCopy must "
				      "be SCE_GXM_TRANSFER_COLORKEY_NONE when srcType is not "
				      "SCE_GXM_TRANSFER_LINEAR.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	}
	if (destType != SCE_GXM_TRANSFER_LINEAR) {
		if (destX != 0) {
			GXM_DBG_ERROR(357, "Parameter destX to function sceGxmTransferCopy has value %u, "
				      "but must be 0 when destType is not SCE_GXM_TRANSFER_LINEAR.",
				      destX);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (destY != 0) {
			GXM_DBG_ERROR(363, "Parameter destY to function sceGxmTransferCopy has value %u, "
				      "but must be 0 when destType is not SCE_GXM_TRANSFER_LINEAR.",
				      destY);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (destStride != (width * gxmTransferFormatGetBitsPerPixel(destFormat)) >> 3) {
			GXM_DBG_ERROR(369, "Parameter destStride to function sceGxmTransferCopy has "
				      "value %d, but must match the width in bytes (%u) when destType "
				      "is not SCE_GXM_TRANSFER_LINEAR.", destStride,
				      (width * gxmTransferFormatGetBitsPerPixel(destFormat)) >> 3);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (colorKeyMode != SCE_GXM_TRANSFER_COLORKEY_NONE) {
			GXM_DBG_ERROR(374, "Parameter colorKeyMode to function sceGxmTransferCopy must "
				      "be SCE_GXM_TRANSFER_COLORKEY_NONE when destType is not "
				      "SCE_GXM_TRANSFER_LINEAR.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	}
	if ((destFormat == SCE_GXM_TRANSFER_FORMAT_U2U10U10U10_ABGR ||
	     srcFormat == SCE_GXM_TRANSFER_FORMAT_U2U10U10U10_ABGR) &&
	    colorKeyMode != SCE_GXM_TRANSFER_COLORKEY_NONE && srcFormat != destFormat) {
		GXM_DBG_ERROR(382, "Parameter colorKeyMode to function sceGxmTransferCopy must be "
			      "SCE_GXM_TRANSFER_COLORKEY_NONE if either of srcFormat or destFormat "
			      "are SCE_GXM_TRANSFER_FORMAT_U2U10U10U10 and srcFormat is not equal to "
			      "destFormat.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (srcType == SCE_GXM_TRANSFER_LINEAR) {
		GXM_TRANSFER_CHECK_SUM(391, sceGxmTransferCopy, "srcX", "width", width + srcX,
				       GXM_TRANSFER_LESS_THAN);
		GXM_TRANSFER_CHECK_SUM(397, sceGxmTransferCopy, "srcY", "height", height + srcY,
				       GXM_TRANSFER_LESS_THAN);
	}
	if (destType == SCE_GXM_TRANSFER_LINEAR) {
		GXM_TRANSFER_CHECK_SUM(405, sceGxmTransferCopy, "destX", "width", width + destX,
				       GXM_TRANSFER_LESS_THAN);
		GXM_TRANSFER_CHECK_SUM(411, sceGxmTransferCopy, "destY", "height", height + destY,
				       GXM_TRANSFER_LESS_THAN);
	}

	count = 0;
	if (colorKeyMode != SCE_GXM_TRANSFER_COLORKEY_NONE) {
		words[count++] = GXM_TRANSFER_COLOR_KEY;
		words[count++] = colorKeyValue;
		words[count++] = colorKeyMask;
	}
	size = gxmTransferSize(width, height);
	words[count++] = srcType | GXM_TRANSFER_SOURCE | srcFormat | (srcStride & 0xffff);
	words[count++] = (SceUInt32)srcAddress;
	words[count++] = gxmTransferPosition(srcX, srcY) | GXM_TRANSFER_SOURCE_POSITION;
	words[count++] = size;
	words[count++] = destType | GXM_TRANSFER_DEST | destFormat | (destStride & 0xffff);
	words[count++] = (SceUInt32)destAddress;
	if (colorKeyMode == SCE_GXM_TRANSFER_COLORKEY_NONE)
		words[count++] = GXM_TRANSFER_CONTROL;
	else
		words[count++] = GXM_TRANSFER_CONTROL | GXM_TRANSFER_CONTROL_KEY |
				 (colorKeyMode == SCE_GXM_TRANSFER_COLORKEY_PASS ?
				  GXM_TRANSFER_CONTROL_KEY_PASS : 0);
	words[count++] = gxmTransferPosition(destX, destY);
	words[count++] = size;
	return sceGxmTransferControlStream(words, count, syncObject, syncFlags, notification);
}
