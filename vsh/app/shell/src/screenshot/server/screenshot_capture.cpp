/*
 * screenshot_capture.cpp: SceShell screenshot allocator, surface blend/resize,
 * overlay composition, EXIF/MakerNote picture encoding, and capture job queue items.
 * 3.65-CEX: 0x81246A78 - 0x81248435, 0x8142A6E8 - 0x8142A810
 */
#include "screenshot_server.h"

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdio.h>
#include <appmgr/appmgr.h>
#include <datetime/datetime.h>
#include <dir/dir.h>
#include <file/file.h>
#include <file/localfile.h>
#include <graphics/surface/surface_display.h>
#include <image/image.h>
#include <libScePhotoExport/photo_export.h>
#include <memory/heap_allocator.h>
#include <regmgr/regmgr.h>
#include <rtc/rtc.h>
#include <thread/thread.h>
#include <widget/core/environment.h>

extern "C" {
struct ScreenShotSwVersionInfo {
	SceSize size;
	char versionString[28];
	SceUInt32 versionNumber;
	SceUInt32 unused;
};

extern int sceKernelGetSystemSwVersion(ScreenShotSwVersionInfo *info);
extern int sceAppMgrAppParamGetString(SceUID pid, int param, char *buf, SceSize size);
extern SceInt32 ScePhotoExport_F4E0F7FF(void);
}

namespace screenshot {

namespace {

static paf::memory::HeapAllocator *s_allocator = NULL;

static const SceUInt32 kAppParamTitleName = 101;
static const SceUInt32 kAppParamContentId = 100;
static const SceUInt32 kCopyChunkBytes = 0x10000;
static const SceUInt32 kIoBufferBytes = 0x10000;
static const SceUInt32 kLockSwapSleepMs = 800;

static float clampChannelByte(float value)
{
	if (value <= 0.0f)
		return 0.0f;
	if (value >= 255.0f)
		return 255.0f;
	return value;
}

} /* anonymous namespace */

/* 0x81246F78 */
void setAllocator(paf::memory::HeapAllocator *allocator)
{
	s_allocator = allocator;
}

/* 0x81246F86 */
void *allocMemory(SceSize size)
{
	if (s_allocator == NULL)
		return NULL;
	return s_allocator->sce_paf_malloc(size);
}

/* 0x81246FA2 */
void *allocAligned(SceUInt32 align, SceSize size)
{
	if (s_allocator == NULL)
		return NULL;
	return s_allocator->sce_paf_memalign(align, size);
}

/* 0x81246FC0 */
void freeMemory(void *ptr)
{
	if (s_allocator != NULL)
		s_allocator->sce_paf_free(ptr);
}

/* 0x81246FD8 */
int queryAppTitleInfo(SceInt32 appId, AppTitleInfo *outInfo)
{
	SceAppMgrAppStatus status;
	int ret = sceAppMgrGetStatusByAppId(appId, &status);
	if (ret != 0)
		return ret;

	char contentIdBuf[48];
	char titleNameBuf[128];
	sce_paf_memset(contentIdBuf, 0, sizeof(contentIdBuf));
	sce_paf_memset(titleNameBuf, 0, sizeof(titleNameBuf));

	ret = sceAppMgrAppParamGetString(status.processId, kAppParamTitleName,
					 titleNameBuf, sizeof(titleNameBuf));
	if (ret != 0)
		return ret;

	ret = sceAppMgrAppParamGetString(status.processId, kAppParamContentId,
					 contentIdBuf, sizeof(contentIdBuf));
	if (ret != 0)
		return ret;

	outInfo->titleId.assign(status.appName, sce_paf_strlen(status.appName));
	outInfo->titleName.assign(titleNameBuf, sce_paf_strlen(titleNameBuf));
	outInfo->contentId.assign(contentIdBuf, sce_paf_strlen(contentIdBuf));
	return 0;
}

/*
 * Alpha-blends src RGBA8888 surface over dst RGBA8888 surface in place:
 *   dst_c = (dst_c * (255 - src_a) + src_c * src_a) / 255
 * (exact 16-bit reciprocal (x * 0x0101 + 0x0100) >> 16 matching the NEON loop).
 * 0x81246A78
 */
int blendSurfaceRGBA(SurfaceDesc *dst, const SurfaceDesc *src)
{
	if (dst == NULL || src == NULL)
		return -1;
	if (dst->pixels == NULL || src->pixels == NULL)
		return -1;
	if (dst->width == 0 || dst->height == 0 || src->width == 0 || src->height == 0)
		return -1;
	if (dst->width != src->width || dst->height != src->height)
		return -1;
	if ((dst->width & 3U) != 0)
		return -1;

	SceUInt8 *dstRow = static_cast<SceUInt8 *>(dst->pixels);
	const SceUInt8 *srcRow = static_cast<const SceUInt8 *>(src->pixels);

	for (SceUInt32 row = 0; row < dst->height; ++row) {
		SceUInt8 *dstPx = dstRow;
		const SceUInt8 *srcPx = srcRow;
		for (SceUInt32 col = 0; col < dst->width; ++col) {
			SceUInt32 srcAlpha = srcPx[3];
			SceUInt32 invAlpha = 255U - srcAlpha;
			for (int ch = 0; ch < 4; ++ch) {
				SceUInt32 mixed = dstPx[ch] * invAlpha + srcPx[ch] * srcAlpha;
				dstPx[ch] = static_cast<SceUInt8>((mixed * 0x0101U + 0x0100U) >> 16);
			}
			dstPx += 4;
			srcPx += 4;
		}
		dstRow += dst->pitchBytes;
		srcRow += src->pitchBytes;
	}
	return 0;
}

/*
 * In-place bilinear upscale of an RGBA8888 surface from (surface->width, surface->height)
 * up to (dstWidth, dstHeight), iterating bottom-to-top and right-to-left so source pixels
 * are never overwritten before being sampled.
 * 0x81246BB4
 */
int resizeSurfaceRGBA(SurfaceDesc *surface, SceUInt32 dstWidth, SceUInt32 dstHeight)
{
	if (surface == NULL || surface->pixels == NULL)
		return -1;
	if (surface->width == 0 || surface->height == 0)
		return -1;
	if (surface->width > dstWidth || surface->height > dstHeight)
		return -1;
	if (surface->width == dstWidth && surface->height == dstHeight)
		return 0;

	const float scaleX = static_cast<float>(surface->width) / static_cast<float>(dstWidth);
	const float scaleY = static_cast<float>(surface->height) / static_cast<float>(dstHeight);
	const SceUInt32 rowStride = surface->pixelPitch * 4U;
	SceUInt8 *base = static_cast<SceUInt8 *>(surface->pixels);

	for (SceInt32 dstY = static_cast<SceInt32>(dstHeight) - 1; dstY >= 0; --dstY) {
		float sampleY = static_cast<float>(dstY) * scaleY;
		SceInt32 intY = static_cast<SceInt32>(sampleY);
		float fracY = sampleY - static_cast<float>(intY);
		float invFracY = 1.0f - fracY;

		const SceUInt8 *rowTop = base + static_cast<SceUInt32>(intY) * rowStride;
		const SceUInt8 *rowBottom = rowTop + rowStride;
		SceUInt8 *dstPixel = base + static_cast<SceUInt32>(dstY) * rowStride +
				     (dstWidth - 1U) * 4U;

		for (SceInt32 dstX = static_cast<SceInt32>(dstWidth) - 1; dstX >= 0; --dstX) {
			float sampleX = static_cast<float>(dstX) * scaleX;
			SceInt32 intX = static_cast<SceInt32>(sampleX);
			float fracX = sampleX - static_cast<float>(intX);
			float invFracX = 1.0f - fracX;

			const SceUInt8 *topLeft = rowTop + static_cast<SceUInt32>(intX) * 4U;
			const SceUInt8 *topRight = topLeft + 4;
			const SceUInt8 *bottomLeft = rowBottom + static_cast<SceUInt32>(intX) * 4U;
			const SceUInt8 *bottomRight = bottomLeft + 4;

			float weightTopLeft = invFracX * invFracY;
			float weightBottomLeft = invFracX * fracY;
			float weightTopRight = fracX * invFracY;
			float weightBottomRight = fracX * fracY;

			for (int ch = 0; ch < 4; ++ch) {
				float blended =
					static_cast<float>(topLeft[ch]) * weightTopLeft +
					static_cast<float>(bottomLeft[ch]) * weightBottomLeft +
					static_cast<float>(topRight[ch]) * weightTopRight +
					static_cast<float>(bottomRight[ch]) * weightBottomRight;
				dstPixel[ch] = static_cast<SceUInt8>(
					static_cast<SceUInt32>(clampChannelByte(blended)));
			}
			dstPixel -= 4;
		}
	}

	surface->width = dstWidth;
	surface->height = dstHeight;
	surface->pitchBytes = dstWidth * 4U;
	return 0;
}

/* LockSwapJob (0x8142A6E8 - 0x8142A747) */

LockSwapJob::LockSwapJob()
	: paf::job::JobItem("SceScreenShotLockSwapJob", NULL)
{
}

LockSwapJob::~LockSwapJob()
{
}

int LockSwapJob::Run()
{
	paf::thread::Sleep(kLockSwapSleepMs);
	return 0;
}

void LockSwapJob::Finish(int result)
{
	(void)result;
	if (paf::ui::Screen::s_root_surface != NULL)
		paf::ui::Screen::s_root_surface->m_swap = true;
}

/* ResizeImageJob (0x8142A748 - 0x8142A797) */

ResizeImageJob::ResizeImageJob(const SurfaceDesc &src, const SurfaceDesc &dst)
	: paf::job::JobItem("SceScreenShotResizeImageJob", NULL),
	  m_src(src),
	  m_dst(dst)
{
}

ResizeImageJob::~ResizeImageJob()
{
}

int ResizeImageJob::Run()
{
	return resizeSurfaceRGBA(&m_src, m_dst.width, m_dst.height);
}

void ResizeImageJob::Finish(int result)
{
	(void)result;
}

/* BlendImageJob (0x8142A798 - 0x8142A810) */

BlendImageJob::BlendImageJob(const SurfaceDesc &dst, const SurfaceDesc &src, bool lockDisplaySwap)
	: paf::job::JobItem("SceScreenShotBlendImageJob", NULL),
	  m_dst(dst),
	  m_src(src),
	  m_lockedDisplaySwap(lockDisplaySwap)
{
	if (m_lockedDisplaySwap && paf::ui::Screen::s_root_surface != NULL)
		paf::ui::Screen::s_root_surface->m_swap = true;
}

BlendImageJob::~BlendImageJob()
{
}

int BlendImageJob::Run()
{
	return blendSurfaceRGBA(&m_dst, &m_src);
}

void BlendImageJob::Finish(int result)
{
	(void)result;
	if (paf::ui::Screen::s_root_surface != NULL)
		paf::ui::Screen::s_root_surface->m_swap = false;
}

/* AppEntry */

AppEntry::AppEntry()
	: appId(-1),
	  processId(-1),
	  captureEnabled(true),
	  reservedState(false),
	  overlayEnabled(false),
	  notificationEnabled(false),
	  overlayOffsetX(0),
	  overlayOffsetY(0),
	  overlayFileFd(-1)
{
}

void AppEntry::Reset(SceInt32 newAppId, SceUID newProcessId)
{
	appId = newAppId;
	processId = newProcessId;
	captureEnabled = true;
	reservedState = false;
	overlayEnabled = false;
	notificationEnabled = false;
	overlayPath.assign("", 0);
	gameComment.assign("", 0);
	gameTitle.assign("", 0);
	overlayOffsetX = 0;
	overlayOffsetY = 0;
	photoTitle.assign("", 0);
	overlayFileFd = -1;
}

/* PendingIpcCapture */

PendingIpcCapture::PendingIpcCapture()
{
	Clear();
}

void PendingIpcCapture::Clear()
{
	appId = -1;
	captureMode = 0;
	session = NULL;
	requestId = 0;
	appExited = false;
	canceled = false;
}

/* CaptureJob (0x812470F8 - 0x81248435) */

void *CaptureJob::operator new(size_t size)
{
	return allocMemory(size);
}

void CaptureJob::operator delete(void *ptr)
{
	freeMemory(ptr);
}

CaptureJob::CaptureJob(const AppEntry &entry, SceUID processId, const FrameBufferInfo &frameBuf,
		       bool debugMode, char *exportedPathBuf, CaptureJobDoneFn onDone,
		       void *onDoneUserData)
	: paf::job::JobItem("SceScreenShotCaptureJob", NULL),
	  m_entry(entry),
	  m_processId(processId),
	  m_debugMode(debugMode),
	  m_outputFormat(0),
	  m_frameBuf(frameBuf),
	  m_onDone(onDone),
	  m_onDoneUserData(onDoneUserData),
	  m_exportedPathBuf(exportedPathBuf)
{
}

CaptureJob::~CaptureJob()
{
}

/* 0x812472D6 */
int CaptureJob::CopyFile(const char *srcPath, const char *dstPath)
{
	int err = 0;
	paf::common::SharedPtr<paf::File> srcFile =
		paf::LocalFile::Open(srcPath, 1U, 0U, &err);
	if (err == 0 && srcFile.get() != NULL) {
		paf::common::SharedPtr<paf::File> dstFile =
			paf::LocalFile::Open(dstPath, 0x202U, 6U, &err);
		if (err == 0 && dstFile.get() != NULL) {
			SceUInt8 *copyBuf = new SceUInt8[kCopyChunkBytes];
			if (copyBuf != NULL) {
				while (!srcFile->IsEof()) {
					int bytesRead = srcFile->Read(copyBuf, kCopyChunkBytes);
					if (bytesRead < 0) {
						err = bytesRead;
						break;
					}
					if (bytesRead == 0)
						break;
					int bytesWritten = dstFile->Write(copyBuf, bytesRead);
					if (bytesWritten < 0) {
						err = bytesWritten;
						break;
					}
					if (bytesWritten != bytesRead) {
						err = kScreenShotErrShortWrite;
						break;
					}
				}
			} else {
				err = kScreenShotErrNoMemory;
			}
			dstFile->Close();
			delete[] copyBuf;
			if (err != 0)
				paf::LocalFile::RemoveFile(dstPath);
		}
	}
	if (err == 0)
		err = kScreenShotErrCopyDone;
	return err;
}

/* 0x8124756C */
int CaptureJob::ExportToPhoto(const char *capturePath, paf::rtc::Tick tick)
{
	char generatedTitle[256];
	sce_paf_memset(generatedTitle, 0, sizeof(generatedTitle));

	ScreenShotSwVersionInfo swInfo;
	sce_paf_memset(&swInfo, 0, sizeof(swInfo));
	swInfo.size = sizeof(swInfo);
	sceKernelGetSystemSwVersion(&swInfo);

	ScePhotoExportParam exportParam;
	sce_paf_memset(&exportParam, 0, sizeof(exportParam));
	exportParam.version = swInfo.versionNumber;
	exportParam.gameTitle = reinterpret_cast<const SceChar8 *>(m_entry.gameTitle.c_str());
	exportParam.gameComment = reinterpret_cast<const SceChar8 *>(m_entry.gameComment.c_str());

	if (m_entry.photoTitle.size() != 0) {
		exportParam.photoTitle =
			reinterpret_cast<const SceChar8 *>(m_entry.photoTitle.c_str());
	} else {
		paf::rtc::DateTime dt;
		if (paf::rtc::ConvertTickToDateTime(&tick, &dt, NULL) != 0)
			sce_paf_memset(&dt, 0, sizeof(dt));
		sce_paf_snprintf(generatedTitle, sizeof(generatedTitle),
				 "%04d-%02d-%02d-%02d%02d%02d",
				 dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
		generatedTitle[sizeof(generatedTitle) - 1] = '\0';
		exportParam.photoTitle = reinterpret_cast<const SceChar8 *>(generatedTitle);
	}

	sce_paf_memset(m_exportedPathBuf, 0, SCE_SCREENSHOT_MAX_FS_PATH);
	return ScePhotoExport_719ACE85(
		reinterpret_cast<const SceChar8 *>(capturePath),
		&exportParam,
		2,
		NULL,
		NULL,
		NULL,
		reinterpret_cast<SceChar8 *>(m_exportedPathBuf),
		SCE_SCREENSHOT_MAX_FS_PATH);
}

/* 0x8124766C */
int CaptureJob::EncodePictureFile(const char *capturePath, paf::rtc::Tick tick)
{
	PictureMakerNote makerNote;
	PictureExifData exif;
	exif.makerNote = NULL;
	exif.gpsData = NULL;

	pictureExifInitDefaults(&exif);
	pictureExifSetDateTime(&exif, tick);

	exif.make.Set(paf::string("Sony Interactive Entertainment Inc."));
	exif.model.Set(paf::string("PlayStation(R)Vita"));

	ScreenShotSwVersionInfo swInfo;
	sce_paf_memset(&swInfo, 0, sizeof(swInfo));
	swInfo.size = sizeof(swInfo);
	if (sceKernelGetSystemSwVersion(&swInfo) == 0)
		exif.software.Set(paf::string(swInfo.versionString));

	exif.orientation.Set(1);
	exif.pixelXDimension.Set(m_frameBuf.width);
	exif.pixelYDimension.Set(m_frameBuf.height);
	exif.flash.Clear();
	exif.customRendered.Clear();
	exif.exposureMode.Clear();
	exif.whiteBalance.Clear();
	exif.sceneCaptureType.Clear();

	exif.makerNote = &makerNote;
	pictureMakerNoteInit(&makerNote);

	if (m_entry.gameTitle.size() != 0)
		makerNote.albumTitle.Set(m_entry.gameTitle);
	if (m_entry.gameComment.size() != 0)
		makerNote.gameDescription.Set(m_entry.gameComment);
	if (m_titleId.size() != 0)
		makerNote.gameProductName.Set(m_titleId);
	if (m_contentId.size() != 0)
		makerNote.gameContentId.Set(m_contentId);

	PictureSourceInfo sourceInfo;
	sce_paf_memset(&sourceInfo, 0, sizeof(sourceInfo));
	sourceInfo.pixelBuffer = m_frameBuf.pixels;
	sourceInfo.width = m_frameBuf.width;
	sourceInfo.height = m_frameBuf.height;
	sourceInfo.pitchBytes = m_frameBuf.pitchBytes;
	sourceInfo.pixelFormat = ImageMode_RGBA8888;

	PictureEncodeConfig encodeConfig;
	sce_paf_memset(&encodeConfig, 0, sizeof(encodeConfig));
	encodeConfig.source = &sourceInfo;
	encodeConfig.rotation = 0;
	encodeConfig.outputFormat = (m_outputFormat == 1) ? 1 : 0;
	encodeConfig.outputFlags = 1U;
	encodeConfig.quality = 0xFF;
	encodeConfig.jpegSubsampling = 25;
	encodeConfig.scaleX = 1.0f;
	encodeConfig.scaleY = 1.0f;

	PictureWorkSize workSize;
	workSize.outputBufferSize = 0;
	workSize.workMemorySize = 0;
	pictureFileCreatorGetWorkSize(&encodeConfig, &workSize);

	PictureCreateOutput createOutput;
	sce_paf_memset(&createOutput, 0, sizeof(createOutput));
	createOutput.outputFilePath = capturePath;
	createOutput.ioBufferSize = kIoBufferBytes;

	if (workSize.outputBufferSize != 0) {
		createOutput.outputBuffer = new SceUInt8[workSize.outputBufferSize];
		if (createOutput.outputBuffer != NULL)
			createOutput.outputBufferSize = workSize.outputBufferSize;
	}
	if (workSize.workMemorySize != 0) {
		createOutput.workMemory = new SceUInt8[workSize.workMemorySize];
		if (createOutput.workMemory != NULL)
			createOutput.workMemorySize = workSize.workMemorySize;
	}

	PictureCreateInput createInput;
	createInput.config = &encodeConfig;
	createInput.exif = &exif;
	createInput.cancelFunc = NULL;
	createInput.cancelUserData = NULL;

	int ret = pictureFileCreatorCreate(&createInput, &createOutput);

	if (createOutput.outputBuffer != NULL) {
		delete[] static_cast<SceUInt8 *>(createOutput.outputBuffer);
		createOutput.outputBuffer = NULL;
	}
	if (createOutput.workMemory != NULL) {
		delete[] static_cast<SceUInt8 *>(createOutput.workMemory);
		createOutput.workMemory = NULL;
	}

	return (ret < 0) ? ret : 0;
}

/* 0x81247E9C */
int CaptureJob::BlendOverlayImage(const char *overlayPath)
{
	if (!paf::LocalFile::Exists(overlayPath))
		return -1;

	paf::common::SharedPtr<paf::image::Image> image = paf::image::Image::Open(
		overlayPath, GetGlobalHeapAllocator(),
		paf::image::ImageFormat_Png, 0, 0);
	if (image.get() == NULL)
		return -1;

	paf::image::ImageExtent extent = image->GetExtent();
	if (extent.w > static_cast<int32_t>(kMaxOverlayWidth) ||
	    extent.h > static_cast<int32_t>(kMaxOverlayHeight))
		return -1;

	SceInt32 copyWidth = extent.w;
	SceInt32 copyHeight = extent.h;
	SceInt32 offsetX = m_entry.overlayOffsetX;
	SceInt32 offsetY = m_entry.overlayOffsetY;

	if (offsetX + copyWidth > static_cast<SceInt32>(kScreenWidth))
		copyWidth = static_cast<SceInt32>(kScreenWidth) - offsetX;
	if (offsetY + copyHeight > static_cast<SceInt32>(kScreenHeight))
		copyHeight = static_cast<SceInt32>(kScreenHeight) - offsetY;

	SceInt32 dstStartX = (offsetX < 0) ? 0 : offsetX;
	SceInt32 dstStartY = (offsetY < 0) ? 0 : offsetY;
	SceInt32 srcStride = extent.w * 4;

	if (copyWidth <= 0 || copyHeight <= 0)
		return -1;

	paf::image::ImageBuffer imgBuf = image->ToBuffer(false);
	SceUInt8 *srcPixels = static_cast<SceUInt8 *>(imgBuf.buff);
	if (srcPixels != NULL) {
		SceUInt8 *srcRow = srcPixels;
		if (offsetY < 0)
			srcRow += (-offsetY) * srcStride;
		if (offsetX < 0)
			srcRow += (-offsetX) * 4;

		SceUInt8 *dstBase = static_cast<SceUInt8 *>(m_frameBuf.pixels);
		SceUInt8 *dstRow = dstBase + dstStartY * m_frameBuf.pitchBytes + dstStartX * 4;

		for (SceInt32 row = 0; row < copyHeight; ++row) {
			const SceUInt8 *srcPx = srcRow;
			SceUInt8 *dstPx = dstRow;
			for (SceInt32 col = 0; col < copyWidth; ++col) {
				float alpha = static_cast<float>(srcPx[3]) / 255.0f;
				if (alpha != 0.0f) {
					float invAlpha = 1.0f - alpha;
					float red = static_cast<float>(srcPx[0]) * alpha +
						    static_cast<float>(dstPx[0]) * invAlpha;
					float green = static_cast<float>(srcPx[1]) * alpha +
						      static_cast<float>(dstPx[1]) * invAlpha;
					float blue = static_cast<float>(srcPx[2]) * alpha +
						     static_cast<float>(dstPx[2]) * invAlpha;
					dstPx[0] = static_cast<SceUInt8>(
						static_cast<SceUInt32>(clampChannelByte(red)));
					dstPx[1] = static_cast<SceUInt8>(
						static_cast<SceUInt32>(clampChannelByte(green)));
					dstPx[2] = static_cast<SceUInt8>(
						static_cast<SceUInt32>(clampChannelByte(blue)));
				}
				srcPx += 4;
				dstPx += 4;
			}
			srcRow += srcStride;
			dstRow += m_frameBuf.pitchBytes;
		}
	}

	return 0;
}

/* 0x812481F4 */
int CaptureJob::PopulateAppInfo()
{
	AppTitleInfo appInfo;
	int ret = queryAppTitleInfo(m_entry.appId, &appInfo);
	if (ret == 0) {
		m_titleId = appInfo.titleId;
		m_contentId = appInfo.contentId;
		if (m_entry.gameTitle.size() == 0)
			m_entry.gameTitle = appInfo.titleName;
	}
	return ret;
}

/* 0x812482A4 */
int CaptureJob::Run()
{
	sce_paf_memset(m_exportedPathBuf, 0, SCE_SCREENSHOT_MAX_FS_PATH);

	const char *capturePath = "ur0:temp/screenshot/capture.jpg";
	const char *overlayPath = m_entry.overlayPath.c_str();
	bool hasOverlay = (m_entry.overlayPath.size() != 0);

	if (m_debugMode) {
		int debugFormat = 0;
		if (sceRegMgrGetKeyInt("/CONFIG/PHOTO", "debug_screenshot_format", &debugFormat) == 0 &&
		    debugFormat == 1) {
			m_outputFormat = 1;
			capturePath = "ur0:temp/screenshot/capture.bmp";
		}
	}

	PopulateAppInfo();
	ScePhotoExport_F4E0F7FF();

	int ret = 0;
	if (!hasOverlay || (ret = BlendOverlayImage(overlayPath)) == 0) {
		paf::rtc::Tick tick = 0;
		ret = paf::rtc::GetCurrentLocalTick(&tick);
		if (ret == 0) {
			ret = EncodePictureFile(capturePath, tick);
			if (ret == 0) {
				ret = ExportToPhoto(capturePath, tick);
				if (m_debugMode &&
				    (ret == kPhotoExportErrInvalidParam ||
				     ret == kPhotoExportErrUnsupported)) {
					paf::datetime::DateTime now = paf::datetime::DateTime::Now();
					paf::Dir::Create("ux0:picture/ALL/bb", 6U);
					const char *ext = (m_outputFormat == 1) ? "bmp" : "jpg";
					char debugDstPath[256];
					sce_paf_snprintf(
						debugDstPath, sizeof(debugDstPath),
						"ux0:picture/ALL/bb/%04d-%02d-%02d-%02d%02d%02d.%s",
						now.m_year, now.m_month, now.m_day,
						now.m_hour, now.m_minute, now.m_second, ext);
					ret = CopyFile(capturePath, debugDstPath);
				}
			}
		}
	}
	return ret;
}

/* 0x81248414 */
void CaptureJob::Finish(int result)
{
	if (m_onDone != NULL)
		m_onDone(result, m_entry.appId, m_exportedPathBuf, m_onDoneUserData);
}

} /* namespace screenshot */
