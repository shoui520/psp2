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

/* a stride is 16-bit signed and must hold whole pixels */
static inline _Bool gxmTransferStrideValid(SceInt32 stride, SceGxmTransferFormat format)
{
	return (SceUInt32)(stride + 0x8000) < 0x10000 &&
	       stride % gxmTransferFormatGetBytesPerPixel(format) == 0;
}

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

static inline int gxmTransferSend(const SceUInt32 *words, SceUInt32 count, void *syncObject,
				  SceUInt32 syncFlags, const GxmGpuNotification *notification)
{
	if (SceGpuEs4User_5564C1BE(words, count, &gxmGlobals.top, gxmGlobals.top.transferQueue,
				   syncObject, (syncFlags & SCE_GXM_TRANSFER_VERTEX_SYNC) >> 1,
				   syncFlags & SCE_GXM_TRANSFER_FRAGMENT_SYNC, notification) != 0)
		return SCE_GXM_ERROR_DRIVER;
	return 0;
}

int sceGxmTransferDownscale(SceGxmTransferFormat srcFormat, const void *srcAddress,
			    SceUInt32 srcX, SceUInt32 srcY, SceUInt32 srcWidth, SceUInt32 srcHeight,
			    SceInt32 srcStride, SceGxmTransferFormat destFormat, void *destAddress,
			    SceUInt32 destX, SceUInt32 destY, SceInt32 destStride, void *syncObject,
			    SceUInt32 syncFlags, const SceGxmNotification *notification)
{
	const GxmGpuNotification *gpuNotification;
	GxmGpuNotification gpu;
	SceUInt32 words[9];
	int err;

	if (gxmDisplayQueueIsCurrentThread())
		return SCE_GXM_ERROR_INVALID_THREAD;
	/* an even size, in whole 16-pixel blocks above 16 */
	if (srcWidth - 2 >= 0x3ff || (srcWidth & 1) != 0 ||
	    (srcWidth >= 16 && (srcWidth & 0xf) != 0) ||
	    srcHeight - 2 >= 0x3ff || (srcHeight & 1) != 0 ||
	    !gxmTransferFormatIsSupported(srcFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (!gxmTransferFormatIsColor(srcFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (srcAddress == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (((SceUInt32)srcAddress & (gxmTransferFormatGetAlignment(srcFormat) - 1)) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (!gxmTransferStrideValid(srcStride, srcFormat) ||
	    !gxmTransferFormatIsSupported(destFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (!gxmTransferFormatIsColor(destFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (destAddress == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (((SceUInt32)destAddress & (gxmTransferFormatGetAlignment(destFormat) - 1)) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (!gxmTransferStrideValid(destStride, destFormat) ||
	    srcWidth + srcX >= GXM_TRANSFER_MAX_COORD || srcHeight + srcY >= GXM_TRANSFER_MAX_COORD ||
	    (srcWidth >> 1) + destX >= GXM_TRANSFER_MAX_COORD ||
	    (srcHeight >> 1) + destY >= GXM_TRANSFER_MAX_COORD)
		return SCE_GXM_ERROR_INVALID_VALUE;

	words[0] = srcFormat | (srcStride & 0xffff) | GXM_TRANSFER_SOURCE;
	words[1] = (SceUInt32)srcAddress;
	words[2] = gxmTransferPosition(srcX, srcY) | GXM_TRANSFER_SOURCE_POSITION;
	words[3] = gxmTransferSize(srcWidth, srcHeight);
	words[4] = destFormat | (destStride & 0xffff) | GXM_TRANSFER_DEST;
	words[5] = (SceUInt32)destAddress;
	words[6] = GXM_TRANSFER_CONTROL | GXM_TRANSFER_CONTROL_DOWNSCALE;
	words[7] = gxmTransferPosition(destX, destY);
	words[8] = gxmTransferSize(srcWidth >> 1, srcHeight >> 1);
	err = gxmTransferNotification(notification, &gpu, &gpuNotification);
	if (err != 0)
		return err;
	return gxmTransferSend(words, 9, syncObject, syncFlags, gpuNotification);
}

int sceGxmTransferFill(SceUInt32 fillColor, SceGxmTransferFormat destFormat, void *destAddress,
		       SceUInt32 destX, SceUInt32 destY, SceUInt32 destWidth, SceUInt32 destHeight,
		       SceInt32 destStride, void *syncObject, SceUInt32 syncFlags,
		       const SceGxmNotification *notification)
{
	const GxmGpuNotification *gpuNotification;
	GxmGpuNotification gpu;
	SceUInt32 words[8], format;
	int err;

	if (gxmDisplayQueueIsCurrentThread())
		return SCE_GXM_ERROR_INVALID_THREAD;
	if (!gxmTransferFormatIsSupported(destFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (!gxmTransferFormatIsColor(destFormat) && !gxmTransferFormatIsYuv(destFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (destAddress == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (((SceUInt32)destAddress & (gxmTransferFormatGetAlignment(destFormat) - 1)) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (destWidth - 1 >= 0x400 || destHeight - 1 >= 0x400 ||
	    !gxmTransferStrideValid(destStride, destFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	/* YUV is filled in pixel pairs */
	if ((gxmTransferFormatIsYuv(destFormat) && ((destWidth | destX) & 1) != 0) ||
	    destWidth + destX >= GXM_TRANSFER_MAX_COORD ||
	    destHeight + destY >= GXM_TRANSFER_MAX_COORD)
		return SCE_GXM_ERROR_INVALID_VALUE;

	format = destFormat | (destStride & 0xffff);
	words[7] = gxmTransferSize(destWidth, destHeight);
	words[6] = gxmTransferPosition(destX, destY);
	words[0] = format | GXM_TRANSFER_SOURCE;
	words[2] = format | GXM_TRANSFER_DEST;
	words[4] = GXM_TRANSFER_CONTROL | GXM_TRANSFER_CONTROL_FILL;
	words[1] = (SceUInt32)destAddress;
	words[3] = (SceUInt32)destAddress;
	words[5] = fillColor;
	err = gxmTransferNotification(notification, &gpu, &gpuNotification);
	if (err != 0)
		return err;
	return gxmTransferSend(words, 8, syncObject, syncFlags, gpuNotification);
}

int sceGxmTransferFinish(void)
{
	if (gxmDisplayQueueIsCurrentThread())
		return SCE_GXM_ERROR_INVALID_THREAD;
	gxmRazorMarker(0x19000000, gxmGlobals.frameIndex);
	if (SceGpuEs4User_7E965369(&gxmGlobals.top, gxmGlobals.top.transferQueue) != 0)
		return SCE_GXM_ERROR_DRIVER;
	gxmRazorMarker(0x1a000000, gxmGlobals.frameIndex);
	return 0;
}

GXM_NOINLINE int gxmTransferSubmit(const SceUInt32 *words, SceUInt32 count, void *syncObject,
		      SceUInt32 syncFlags, const SceGxmNotification *notification)
{
	const GxmGpuNotification *gpuNotification;
	GxmGpuNotification gpu;
	int err;

	if (words == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (count - 1 >= GXM_TRANSFER_MAX_WORDS)
		return SCE_GXM_ERROR_INVALID_VALUE;
	err = gxmTransferNotification(notification, &gpu, &gpuNotification);
	if (err != 0)
		return err;
	return gxmTransferSend(words, count, syncObject, syncFlags, gpuNotification);
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

	if (gxmDisplayQueueIsCurrentThread())
		return SCE_GXM_ERROR_INVALID_THREAD;
	if (width - 1 >= 0x400 || height - 1 >= 0x400 || !gxmTransferFormatIsSupported(srcFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (srcAddress == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (((SceUInt32)srcAddress & (gxmTransferFormatGetAlignment(srcFormat) - 1)) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (!gxmTransferStrideValid(srcStride, srcFormat) ||
	    !gxmTransferFormatIsSupported(destFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (destAddress == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (((SceUInt32)destAddress & (gxmTransferFormatGetAlignment(destFormat) - 1)) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (!gxmTransferStrideValid(destStride, destFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_SURFACE_ANY) &&
	    (((srcType & ~SCE_GXM_TRANSFER_TILED) != 0 && srcType != SCE_GXM_TRANSFER_SWIZZLED) ||
	     ((destType & ~SCE_GXM_TRANSFER_TILED) != 0 && destType != SCE_GXM_TRANSFER_SWIZZLED) ||
	     colorKeyMode > SCE_GXM_TRANSFER_COLORKEY_REJECT))
		return SCE_GXM_ERROR_INVALID_VALUE;

	/* swizzled and tiled layouts do not convert into each other */
	srcSwizzled = srcType == SCE_GXM_TRANSFER_SWIZZLED;
	srcTiled = srcType == SCE_GXM_TRANSFER_TILED;
	destSwizzled = destType == SCE_GXM_TRANSFER_SWIZZLED;
	if (destSwizzled & (srcTiled | srcSwizzled))
		return SCE_GXM_ERROR_UNSUPPORTED;
	destTiled = destType == SCE_GXM_TRANSFER_TILED;
	if (srcSwizzled & destTiled)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (gxmTransferFormatIsRaw(srcFormat) != gxmTransferFormatIsRaw(destFormat) ||
	    gxmTransferFormatIsYuv(srcFormat) != gxmTransferFormatIsYuv(destFormat) ||
	    gxmTransferFormatIsColor(srcFormat) != gxmTransferFormatIsColor(destFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (gxmTransferFormatIsYuv(srcFormat) && ((width & 1) != 0 || ((srcX | destX) & 1) != 0))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmTransferFormatIsRaw(srcFormat)) {
		/* raw data is copied as it is, linear only, and not too much of it */
		if (srcFormat != destFormat || colorKeyMode != SCE_GXM_TRANSFER_COLORKEY_NONE)
			return SCE_GXM_ERROR_UNSUPPORTED;
		if (srcFormat == SCE_GXM_TRANSFER_FORMAT_RAW64) {
			if (width * height > 0x80000)
				return SCE_GXM_ERROR_INVALID_VALUE;
		} else if (srcFormat == SCE_GXM_TRANSFER_FORMAT_RAW128) {
			if (width * height > 0x40000)
				return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (srcTiled || destTiled)
			return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if ((srcSwizzled | destSwizzled) && (!gxmIsPow2(width) || !gxmIsPow2(height)))
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if ((srcTiled | destTiled) && ((width | height) & 0x1f) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;

	/* a swizzled or tiled side is a whole surface */
	if (srcType != SCE_GXM_TRANSFER_LINEAR) {
		if (srcX != 0 || srcY != 0 ||
		    srcStride != (width * gxmTransferFormatGetBitsPerPixel(srcFormat)) >> 3)
			return SCE_GXM_ERROR_INVALID_VALUE;
		if (colorKeyMode != SCE_GXM_TRANSFER_COLORKEY_NONE)
			return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (destType != SCE_GXM_TRANSFER_LINEAR) {
		if (destX != 0 || destY != 0 ||
		    destStride != (width * gxmTransferFormatGetBitsPerPixel(destFormat)) >> 3)
			return SCE_GXM_ERROR_INVALID_VALUE;
		if (colorKeyMode != SCE_GXM_TRANSFER_COLORKEY_NONE)
			return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if ((destFormat == SCE_GXM_TRANSFER_FORMAT_U2U10U10U10_ABGR ||
	     srcFormat == SCE_GXM_TRANSFER_FORMAT_U2U10U10U10_ABGR) &&
	    colorKeyMode != SCE_GXM_TRANSFER_COLORKEY_NONE && srcFormat != destFormat)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if ((srcType == SCE_GXM_TRANSFER_LINEAR &&
	     (width + srcX >= GXM_TRANSFER_MAX_COORD || height + srcY >= GXM_TRANSFER_MAX_COORD)) ||
	    (destType == SCE_GXM_TRANSFER_LINEAR &&
	     (width + destX >= GXM_TRANSFER_MAX_COORD || height + destY >= GXM_TRANSFER_MAX_COORD)))
		return SCE_GXM_ERROR_INVALID_VALUE;

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
	return gxmTransferSubmit(words, count, syncObject, syncFlags, notification);
}
