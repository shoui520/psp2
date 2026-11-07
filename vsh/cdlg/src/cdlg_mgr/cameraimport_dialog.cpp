/*
 * The camera import dialog's API (sceCameraImportDialog*): its parameter
 * checks (one for the SDKs before 1.03, one for the later ones) and its
 * Init, GetResult and Abort on the worker thread. The dialog itself is
 * libcdlg_cameraimport's plugin. _OnInit, GetResult and Abort are Sony's
 * names (the names the calls keep in the work memory); the file's name and
 * the namespace are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/sysmem/memblock.h>
#include <common_dialog.h>
#include <libsysmodule.h>
#include <cameraimport_dialog.h>
#include <gxm/error.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define CAMERAIMPORT_DIALOG_ERROR_NO_WORK_MEMORY	-2146416862	/* 0x80104722 */
#define CAMERAIMPORT_DIALOG_ERROR_API_NAME		-2146416863	/* 0x80104721 */

/* the draw priorities of sceCameraImportDialogInit */
#define CAMERAIMPORT_DIALOG_PRIORITY		229
#define CAMERAIMPORT_DIALOG_DRAW_PRIORITY	120

/* the SDK whose parameter check is checkParam2 */
#define CAMERAIMPORT_DIALOG_SDK_VERSION_CHECK2	0x01030000

/* the GPU uses the working buffer (sceGxmCheckMemoryInternal) */
#define CAMERAIMPORT_GXM_CHECK_MEMORY		1
#define CAMERAIMPORT_GXM_CHECK_MEMORY_MAPPED	3

#define CAMERAIMPORT_MAX(a, b)	((b) < (a) ? (a) : (b))

/* the argument of the two-argument Init (SceCommonDialog_FEBF87EB) */
typedef struct CameraImportDialogInitArg {
	SceUInt32 priority;
	SceUInt8 reserved[32];
} CameraImportDialogInitArg;

typedef CommonDialogApiScope<CommonDialogCameraImportParam, COMMON_DIALOG_TYPE_CAMERAIMPORT,
			     CAMERAIMPORT_DIALOG_ERROR_NO_WORK_MEMORY,
			     CAMERAIMPORT_DIALOG_ERROR_API_NAME> CameraImportApiScope;

namespace CameraImportDialog {

int _OnInit(const SceCameraImportDialogParam *param, int priority, int drawPriority);
int GetResult(SceCameraImportDialogResult *result);
int Abort(void);

}

using namespace CameraImportDialog;

extern "C" SceInt32 sceCameraImportDialogInit(const SceCameraImportDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_CAMERAIMPORT, (void *)param);
}

extern "C" int SceCommonDialog_FEBF87EB(void *param, void *arg)
{
	return OnInit2(COMMON_DIALOG_TYPE_CAMERAIMPORT, param, arg);
}

extern "C" SceCommonDialogStatus sceCameraImportDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_CAMERAIMPORT);
}

extern "C" SceInt32 sceCameraImportDialogGetResult(SceCameraImportDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_CAMERAIMPORT, (void *)GetResult, 1, result);
}

extern "C" SceInt32 sceCameraImportDialogAbort(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_CAMERAIMPORT, (void *)Abort, 0);
}

extern "C" SceInt32 sceCameraImportDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_CAMERAIMPORT);
}

namespace CameraImportDialog {

/* whether size bytes are all zero (the reserved fields) */
static inline bool isCleared(const void *data, SceUInt32 size)
{
	const SceUInt8 *p = (const SceUInt8 *)data;
	SceUInt32 i;

	for (i = 0; i < size; i++, p++) {
		if (*p != 0)
			return false;
	}
	return true;
}

/* the overlay image both checks take */
static inline int checkOverlayImage(const SceCameraImportDialogParam *param)
{
	const SceCameraImportDialogOverlayImage *image;
	SceUInt32 stride;

	if (param->overlayMode == SCE_CAMERAIMPORT_DIALOG_OVERLAY_MODE_NOT_USE) {
		if (param->overlayImage != NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		return SCE_OK;
	}
	if (param->overlayMode == SCE_CAMERAIMPORT_DIALOG_OVERLAY_MODE_DISPLAY
	    || param->overlayMode == SCE_CAMERAIMPORT_DIALOG_OVERLAY_MODE_PRINT
	    || param->overlayMode == SCE_CAMERAIMPORT_DIALOG_OVERLAY_MODE_DISPLAY_AND_PRINT) {
		image = param->overlayImage;
		if (image == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (image->texFormat != SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (image->buffer == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (image->width == 0 || image->width > SCE_CAMERAIMPORT_DIALOG_MAX_OVERLAY_IMAGE_WIDTH)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (image->height == 0 || image->height > SCE_CAMERAIMPORT_DIALOG_MAX_OVERLAY_IMAGE_HEIGHT)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (image->texType == SCE_GXM_TEXTURE_LINEAR) {
			if (image->stride != 0)
				return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
			/* the rows of a linear texture are 8 texels aligned */
			stride = ((image->width + 7) >> 3) * 8 * 4;
		} else if (image->texType == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
			stride = image->stride;
			if (stride < image->width * 4)
				return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
			if ((stride & 3) != 0)
				return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		} else {
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		}
		if (image->bufferSize < stride * image->height)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (!isCleared(image->reserved, sizeof(image->reserved)))
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		return SCE_OK;
	}
	return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
}

/* the parameter of the SDKs before 1.03 */
int checkParam(const SceCameraImportDialogParam *param)
{
	SceKernelMemBlockInfo info;
	const SceCameraImportDialogMemoryRawParam *raw;
	const SceCameraImportDialogMemoryJpegParam *jpeg;

	if (!isCleared(param->commonParam.reserved, sizeof(param->commonParam.reserved)))
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (!isCleared(param->reserved1, sizeof(param->reserved1)))
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	/* the working buffer: 2 MiB, page aligned, in memory the GPU maps */
	if (param->workingBuffer == NULL)
		return (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER;
	if (param->workingBufferSize < SCE_CAMERAIMPORT_DIALOG_WORKING_BUFFER_SIZE)
		return (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER;
	if (((SceUIntPtr)param->workingBuffer & 0xfff) != 0)
		return (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER;
	sceClibMemset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	if (sceKernelGetMemBlockInfoByRange(param->workingBuffer, SCE_CAMERAIMPORT_DIALOG_WORKING_BUFFER_SIZE,
					    &info) != SCE_OK)
		return (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER;
	if (info.type != SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW
	    && info.type != SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW)
		return (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER;
	if (sceGxmCheckMemoryInternal(param->workingBuffer, SCE_CAMERAIMPORT_DIALOG_WORKING_BUFFER_SIZE,
				      CAMERAIMPORT_GXM_CHECK_MEMORY) != SCE_OK)
		return (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER;
	if (param->availableResolution == 0
	    || (param->availableResolution & 7) != param->availableResolution)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	/* one resolution at most (Sony's test of the two is always false here) */
	if ((param->initialResolution & -param->initialResolution) != param->initialResolution
	    || (param->initialResolution | param->availableResolution) == 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->availableDevice == 0 || (param->availableDevice & 3) != param->availableDevice)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if ((param->initialDevice & -param->initialDevice) != param->initialDevice
	    || (param->initialDevice | param->availableDevice) == 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->rotationMode != SCE_CAMERAIMPORT_DIALOG_ROTATION_MODE_DISABLE
	    && param->rotationMode != SCE_CAMERAIMPORT_DIALOG_ROTATION_MODE_ENABLE)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (!isCleared(param->reserved2, sizeof(param->reserved2)))
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->outputMode == SCE_CAMERAIMPORT_DIALOG_OUTPUT_MODE_MEMORY_RAW) {
		raw = param->memRawParam;
		if (raw == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (raw->buffer == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (raw->bufferSize < SCE_CAMERAIMPORT_DIALOG_RAW_U8U8U8U8_SIZE_640X480)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (raw->texType != SCE_GXM_TEXTURE_LINEAR)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (raw->texFormat != SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (!isCleared(param->memRawParam->reserved, sizeof(param->memRawParam->reserved)))
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (param->memJpegParam != NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	} else if (param->outputMode == SCE_CAMERAIMPORT_DIALOG_OUTPUT_MODE_MEMORY_JPEG) {
		jpeg = param->memJpegParam;
		if (jpeg == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (jpeg->buffer == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (jpeg->bufferSize < SCE_CAMERAIMPORT_DIALOG_JPEG_MAX_SIZE_640X480)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (!isCleared(param->memJpegParam->reserved, sizeof(param->memJpegParam->reserved)))
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (param->memRawParam != NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	} else {
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	return checkOverlayImage(param);
}

/* the parameter of SDK 1.03 and later: the resolutions on offer size the
   output buffer, and the GPU check also takes mapped memory */
int checkParam2(const SceCameraImportDialogParam *param)
{
	SceKernelMemBlockInfo info;
	const SceCameraImportDialogMemoryRawParam *raw;
	const SceCameraImportDialogMemoryJpegParam *jpeg;
	bool has640x480;
	bool has640x360;
	bool has480x480;
	int size640x480;
	int size640x360;
	int size480x480;
	int ret;

	if (!isCleared(param->commonParam.reserved, sizeof(param->commonParam.reserved)))
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (!isCleared(param->reserved1, sizeof(param->reserved1)))
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	/* the working buffer: 2 MiB, page aligned, in memory the GPU maps */
	if (param->workingBuffer == NULL)
		return (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER;
	if (param->workingBufferSize < SCE_CAMERAIMPORT_DIALOG_WORKING_BUFFER_SIZE)
		return (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER;
	if (((SceUIntPtr)param->workingBuffer & 0xfff) != 0)
		return (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER;
	sceClibMemset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	if (sceKernelGetMemBlockInfoByRange(param->workingBuffer, SCE_CAMERAIMPORT_DIALOG_WORKING_BUFFER_SIZE,
					    &info) != SCE_OK)
		return (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER;
	if (info.type != SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW
	    && info.type != SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW)
		return (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER;
	ret = sceGxmCheckMemoryInternal(param->workingBuffer, SCE_CAMERAIMPORT_DIALOG_WORKING_BUFFER_SIZE,
					CAMERAIMPORT_GXM_CHECK_MEMORY);
	if (ret == SCE_GXM_ERROR_INVALID_MAPPING)
		ret = sceGxmCheckMemoryInternal(param->workingBuffer, SCE_CAMERAIMPORT_DIALOG_WORKING_BUFFER_SIZE,
						CAMERAIMPORT_GXM_CHECK_MEMORY_MAPPED);
	if (ret != SCE_OK)
		return (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER;
	if (param->availableResolution == 0
	    || (param->availableResolution & 7) != param->availableResolution)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	/* one resolution, one of those on offer */
	if ((param->initialResolution & -param->initialResolution) != param->initialResolution
	    || (param->initialResolution & param->availableResolution) == 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->availableDevice == 0 || (param->availableDevice & 3) != param->availableDevice)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if ((param->initialDevice & -param->initialDevice) != param->initialDevice
	    || (param->initialDevice & param->availableDevice) == 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->rotationMode != SCE_CAMERAIMPORT_DIALOG_ROTATION_MODE_DISABLE
	    && param->rotationMode != SCE_CAMERAIMPORT_DIALOG_ROTATION_MODE_ENABLE)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (!isCleared(param->reserved2, sizeof(param->reserved2)))
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->outputMode == SCE_CAMERAIMPORT_DIALOG_OUTPUT_MODE_MEMORY_RAW) {
		raw = param->memRawParam;
		if (raw == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (raw->buffer == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		has640x480 = (param->availableResolution & SCE_CAMERAIMPORT_DIALOG_CAMERA_RESOLUTION_640X480) != 0;
		has640x360 = (param->availableResolution & SCE_CAMERAIMPORT_DIALOG_CAMERA_RESOLUTION_640X360) != 0;
		has480x480 = (param->availableResolution & SCE_CAMERAIMPORT_DIALOG_CAMERA_RESOLUTION_480X480) != 0;
		size640x480 = has640x480 ? SCE_CAMERAIMPORT_DIALOG_RAW_U8U8U8U8_SIZE_640X480 : 0;
		size640x360 = has640x360 ? SCE_CAMERAIMPORT_DIALOG_RAW_U8U8U8U8_SIZE_640X360 : 0;
		size480x480 = has480x480 ? SCE_CAMERAIMPORT_DIALOG_RAW_U8U8U8U8_SIZE_480X480 : 0;
		if (raw->bufferSize < (SceSize)CAMERAIMPORT_MAX(CAMERAIMPORT_MAX(size640x480, size640x360),
								size480x480))
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (raw->texType != SCE_GXM_TEXTURE_LINEAR)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (raw->texFormat != SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (!isCleared(param->memRawParam->reserved, sizeof(param->memRawParam->reserved)))
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (param->memJpegParam != NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	} else if (param->outputMode == SCE_CAMERAIMPORT_DIALOG_OUTPUT_MODE_MEMORY_JPEG) {
		jpeg = param->memJpegParam;
		if (jpeg == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (jpeg->buffer == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		has640x480 = (param->availableResolution & SCE_CAMERAIMPORT_DIALOG_CAMERA_RESOLUTION_640X480) != 0;
		has640x360 = (param->availableResolution & SCE_CAMERAIMPORT_DIALOG_CAMERA_RESOLUTION_640X360) != 0;
		has480x480 = (param->availableResolution & SCE_CAMERAIMPORT_DIALOG_CAMERA_RESOLUTION_480X480) != 0;
		size640x480 = has640x480 ? SCE_CAMERAIMPORT_DIALOG_JPEG_MAX_SIZE_640X480 : 0;
		size640x360 = has640x360 ? SCE_CAMERAIMPORT_DIALOG_JPEG_MAX_SIZE_640X360 : 0;
		size480x480 = has480x480 ? SCE_CAMERAIMPORT_DIALOG_JPEG_MAX_SIZE_480X480 : 0;
		if (jpeg->bufferSize < (SceSize)CAMERAIMPORT_MAX(CAMERAIMPORT_MAX(size640x480, size640x360),
								 size480x480))
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (!isCleared(param->memJpegParam->reserved, sizeof(param->memJpegParam->reserved)))
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (param->memRawParam != NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	} else {
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	return checkOverlayImage(param);
}

/* what a call returns: the dialog's working buffer and module errors as
   they are, else as any dialog's call */
static inline int convertResult(int ret)
{
	switch (ret) {
	case (int)SCE_CAMERAIMPORT_DIALOG_ERROR_INVALID_WORKING_BUFFER:
	case (int)SCE_CAMERAIMPORT_DIALOG_ERROR_REQUIRED_PRX_IS_NOT_LOADED:
		return ret;
	}
	return toCommonDialogResult(ret);
}

int _OnInit(const SceCameraImportDialogParam *param, int priority, int drawPriority)
{
	CommonDialogParamOption option;
	CommonDialogCameraImportParam *work;
	SceCommonDialogColor defaultBgColor;
	const SceCommonDialogColor *bgColor;
	bool infobarShown;
	int ret;

	infobarShown = false;
	defaultBgColor.r = 38;
	defaultBgColor.g = 40;
	defaultBgColor.b = 46;
	defaultBgColor.a = 255;
	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else if (sceSysmoduleIsLoaded(SCE_SYSMODULE_SHUTTER_SOUND) != SCE_OK) {
		ret = (int)SCE_CAMERAIMPORT_DIALOG_ERROR_REQUIRED_PRX_IS_NOT_LOADED;
	} else {
		if (param->sdkVersion < CAMERAIMPORT_DIALOG_SDK_VERSION_CHECK2)
			ret = checkParam(param);
		else
			ret = checkParam2(param);
		if (ret == SCE_OK) {
			option.keepInfobar = false;
			option.hideInfobar = true;
			option.drawPriority = priority;
			ret = checkCommonParam(&option, &param->commonParam, &infobarShown);
			if (ret == SCE_OK) {
				ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_CAMERAIMPORT,
							 sizeof(CommonDialogCameraImportParam));
				if (ret == SCE_OK) {
					CameraImportApiScope scope(__FUNCTION__, ret);

					if (ret == SCE_OK) {
						work = scope.work();
						if (work == NULL) {
							ret = SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
						} else {
							sceClibMemset(work, 0,
								      __builtin_offsetof(CommonDialogCameraImportParam,
											 apiName));
							sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
							bgColor = &defaultBgColor;
							if (param->commonParam.bgColor != NULL)
								bgColor = param->commonParam.bgColor;
							sceClibMemcpy_safe(&work->bgColor, bgColor, sizeof(work->bgColor));
							work->param.commonParam.bgColor = &work->bgColor;
							work->drawPriority = drawPriority;
							if (param->memRawParam != NULL) {
								sceClibMemcpy_safe(&work->memRawParam, param->memRawParam,
										   sizeof(work->memRawParam));
								work->param.memRawParam = &work->memRawParam;
							}
							if (param->memJpegParam != NULL) {
								sceClibMemcpy_safe(&work->memJpegParam, param->memJpegParam,
										   sizeof(work->memJpegParam));
								work->param.memJpegParam = &work->memJpegParam;
							}
							if (param->overlayImage != NULL) {
								sceClibMemcpy_safe(&work->overlayImage, param->overlayImage,
										   sizeof(work->overlayImage));
								work->param.overlayImage = &work->overlayImage;
							}
						}
					}
				}
			}
		}
	}
	return convertResult(ret);
}

int onInit(void *param)
{
	return _OnInit((const SceCameraImportDialogParam *)param, CAMERAIMPORT_DIALOG_PRIORITY,
		       CAMERAIMPORT_DIALOG_DRAW_PRIORITY);
}

int onInit2(void *param, void *arg)
{
	const CameraImportDialogInitArg *initArg = (const CameraImportDialogInitArg *)arg;
	SceUInt32 i;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (initArg->priority == 0 || initArg->priority > 255)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < sizeof(initArg->reserved); i++) {
		if (initArg->reserved[i] != 0)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	return _OnInit((const SceCameraImportDialogParam *)param, initArg->priority, initArg->priority - 1);
}

int GetResult(SceCameraImportDialogResult *result)
{
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_CAMERAIMPORT) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	/* the caller passes a cleared result */
	if (!isCleared(result, sizeof(*result)))
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	{
		CameraImportApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			if (scope.work() == NULL)
				ret = SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
			else
				sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
		}
	}
	return convertResult(ret);
}

int Abort(void)
{
	int ret;

	{
		CameraImportApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			ret = SetParamReady(COMMON_DIALOG_TYPE_CAMERAIMPORT);
	}
	return convertResult(ret);
}

}
