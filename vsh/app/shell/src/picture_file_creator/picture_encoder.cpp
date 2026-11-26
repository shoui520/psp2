/*
 * picture_encoder.cpp: ScePictureFileCreator public API, pixel/overlay sampler,
 * JPEG/EXIF/thumbnail encoder (SceJpegEncArm), BMP encoder, and RAW encoder.
 * 3.65-CEX: 0x81365BFC - 0x81366677, 0x813669A8 - 0x8136731B, 0x81368D10 - 0x8136C28B
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <scejpegenc.h>
#include <kernel/processmgr.h>
#include <std/string.h>
#include <memory/heap_allocator.h>

#include "picture_file_creator_internal.h"

using namespace paf::memory;

extern "C" {
int sceJpegArmEncoderGetContextSize(void);
int sceJpegArmEncoderInit(void *ctx, int width, int height, int pixelFormat, void *outBuf,
			  SceSize outBufSize);
int sceJpegArmEncoderSetValidRegion(void *ctx, uint16_t width, uint16_t height);
int sceJpegArmEncoderSetCompressionRatio(void *ctx, uint8_t ratio);
int SceJpegEncArm_4EE1BAED(void *ctx, const void *yPlane, const void *cbPlane,
			   const void *crPlane, int encodeMode, void *outBuf, SceSize outBufSize,
			   int *outSize);
int sceJpegArmEncoderEnd(void *ctx);
}

#define PIC_THUMB_WIDTH			160u
#define PIC_THUMB_HEIGHT		120u
#define PIC_THUMB_MEM_BUFFER_SIZE	39424u
#define PIC_FILE_IO_BUFFER_SIZE		0x2000u
#define PIC_THUMB_IO_BUFFER_SIZE	1024u

static SceUInt8 s_creatorInitialized = 0; /* 0x815C6698 */

static inline uint8_t clampChannel(int value)
{
	if (value < 0)
		return 0;
	if (value > 255)
		return 255;
	return static_cast<uint8_t>(value);
}

static inline uint32_t sourceEffectiveWidth(const PictureSourceInfo *source)
{
	if (source == NULL)
		return 0;
	if (source->cropWidth != 0 && source->cropHeight != 0)
		return source->cropWidth;
	return source->width;
}

static inline uint32_t sourceEffectiveHeight(const PictureSourceInfo *source)
{
	if (source == NULL)
		return 0;
	if (source->cropWidth != 0 && source->cropHeight != 0)
		return source->cropHeight;
	return source->height;
}

static bool validateSourceInfo(const PictureSourceInfo *source)
{
	if (source == NULL || source->pixelBuffer == NULL || source->width == 0 ||
	    source->height == 0)
		return false;

	if ((source->cropX + source->cropY + source->cropWidth + source->cropHeight) != 0) {
		if (source->cropWidth == 0 || source->cropHeight == 0 ||
		    source->cropX + source->cropWidth > source->width ||
		    source->cropY + source->cropHeight > source->height)
			return false;
	}

	uint32_t bytesPerPixel = 0;
	switch (source->pixelFormat) {
	case 3:
	case 4:
	case 5:
	case 6:
		bytesPerPixel = 4;
		break;
	case 0x19:
	case 0x1A:
		bytesPerPixel = 3;
		break;
	case 0x1C:
		if ((source->height & 1u) != 0 || source->reserved >= 2u)
			return false;
		bytesPerPixel = 1;
		break;
	default:
		return false;
	}

	return (bytesPerPixel * source->width <= source->pitchBytes);
}

static uint32_t readSourcePixelRgba(const PictureSourceInfo &img, uint32_t posX, uint32_t posY)
{
	uint32_t sampleX = img.cropX + posX;
	uint32_t sampleY = img.cropY + posY;
	if (sampleX >= img.width)
		sampleX = img.width - 1;
	if (sampleY >= img.height)
		sampleY = img.height - 1;

	const uint8_t *basePixels = static_cast<const uint8_t *>(img.pixelBuffer);
	switch (img.pixelFormat) {
	case 3:
	case 4: { /* ABGR8888 / RGBA8888 */
		const uint8_t *px = basePixels + sampleY * img.pitchBytes + sampleX * 4;
		return static_cast<uint32_t>(px[0]) | (static_cast<uint32_t>(px[1]) << 8) |
		       (static_cast<uint32_t>(px[2]) << 16) | (static_cast<uint32_t>(px[3]) << 24);
	}
	case 5:
	case 6: { /* ARGB8888 / BGRA8888 */
		const uint8_t *px = basePixels + sampleY * img.pitchBytes + sampleX * 4;
		return static_cast<uint32_t>(px[2]) | (static_cast<uint32_t>(px[1]) << 8) |
		       (static_cast<uint32_t>(px[0]) << 16) | (static_cast<uint32_t>(px[3]) << 24);
	}
	case 0x19: { /* BGR888 */
		const uint8_t *px = basePixels + sampleY * img.pitchBytes + sampleX * 3;
		return static_cast<uint32_t>(px[0]) | (static_cast<uint32_t>(px[1]) << 8) |
		       (static_cast<uint32_t>(px[2]) << 16) | 0xFF000000u;
	}
	case 0x1A: { /* RGB888 */
		const uint8_t *px = basePixels + sampleY * img.pitchBytes + sampleX * 3;
		return static_cast<uint32_t>(px[2]) | (static_cast<uint32_t>(px[1]) << 8) |
		       (static_cast<uint32_t>(px[0]) << 16) | 0xFF000000u;
	}
	case 0x1C: { /* YUV420 planar */
		uint32_t yPlaneSize = img.pitchBytes * img.height;
		uint32_t uvPitch = img.pitchBytes >> 1;
		uint32_t uvPlaneSize = uvPitch * (img.height >> 1);
		int luma = static_cast<int>(basePixels[sampleY * img.pitchBytes + sampleX]);
		int chromaU = static_cast<int>(
				      basePixels[yPlaneSize + (sampleY >> 1) * uvPitch +
						 (sampleX >> 1)]) -
			      128;
		int chromaV = static_cast<int>(
				      basePixels[yPlaneSize + uvPlaneSize +
						 (sampleY >> 1) * uvPitch + (sampleX >> 1)]) -
			      128;
		int red = luma + ((359 * chromaV) >> 8);
		int green = luma - ((88 * chromaU + 183 * chromaV) >> 8);
		int blue = luma + ((454 * chromaU) >> 8);
		return static_cast<uint32_t>(clampChannel(red)) |
		       (static_cast<uint32_t>(clampChannel(green)) << 8) |
		       (static_cast<uint32_t>(clampChannel(blue)) << 16) | 0xFF000000u;
	}
	default:
		return 0xFF000000u;
	}
}

static uint32_t sampleCompositedRgba(const PictureSourceInfo &base,
				     const PictureSourceInfo *overlay, uint8_t overlayAlpha,
				     uint32_t posX, uint32_t posY)
{
	uint32_t basePx = readSourcePixelRgba(base, posX, posY);
	if (overlay == NULL || overlay->pixelBuffer == NULL || overlayAlpha == 0)
		return basePx;

	uint32_t ovW = sourceEffectiveWidth(overlay);
	uint32_t ovH = sourceEffectiveHeight(overlay);
	uint32_t baseW = sourceEffectiveWidth(&base);
	uint32_t baseH = sourceEffectiveHeight(&base);

	int offX = (static_cast<int>(baseW) - static_cast<int>(ovW)) / 2;
	int offY = (static_cast<int>(baseH) - static_cast<int>(ovH)) / 2;
	int ovX = static_cast<int>(posX) - offX;
	int ovY = static_cast<int>(posY) - offY;
	if (ovX < 0 || ovY < 0 || static_cast<uint32_t>(ovX) >= ovW ||
	    static_cast<uint32_t>(ovY) >= ovH)
		return basePx;

	uint32_t ovPx = readSourcePixelRgba(*overlay, static_cast<uint32_t>(ovX),
					    static_cast<uint32_t>(ovY));
	uint32_t alpha = (((ovPx >> 24) & 0xFFu) * overlayAlpha) / 255u;
	uint32_t invAlpha = 255u - alpha;
	uint32_t red = (((ovPx & 0xFFu) * alpha) + ((basePx & 0xFFu) * invAlpha)) / 255u;
	uint32_t green =
		((((ovPx >> 8) & 0xFFu) * alpha) + (((basePx >> 8) & 0xFFu) * invAlpha)) / 255u;
	uint32_t blue =
		((((ovPx >> 16) & 0xFFu) * alpha) + (((basePx >> 16) & 0xFFu) * invAlpha)) / 255u;

	return (red & 0xFFu) | ((green & 0xFFu) << 8) | ((blue & 0xFFu) << 16) | 0xFF000000u;
}

static inline void convertRgbaToYCbCr(uint32_t rgba, uint8_t &outY, uint8_t &outCb,
				      uint8_t &outCr)
{
	int red = static_cast<int>(rgba & 0xFFu);
	int green = static_cast<int>((rgba >> 8) & 0xFFu);
	int blue = static_cast<int>((rgba >> 16) & 0xFFu);
	int luma = ((77 * red + 150 * green + 29 * blue + 128) >> 8);
	int chromaB = ((-43 * red - 85 * green + 128 * blue + 128) >> 8) + 128;
	int chromaR = ((128 * red - 107 * green - 21 * blue + 128) >> 8) + 128;

	outY = clampChannel(luma);
	outCb = clampChannel(chromaB);
	outCr = clampChannel(chromaR);
}

static int findJpegDqtMarker(const uint8_t *buffer, int length)
{
	if (buffer == NULL || length < 2 || buffer[0] != 0xFF || buffer[1] != 0xD8)
		return -1;
	int pos = 2;
	while (pos + 4 <= length) {
		if (buffer[pos] == 0xFF) {
			uint8_t marker = buffer[pos + 1];
			if (marker == 0xDB)
				return pos;
			if (marker != 0x00 && marker != 0xFF) {
				int segLen = (static_cast<int>(buffer[pos + 2]) << 8) |
					     buffer[pos + 3];
				pos += segLen + 2;
				continue;
			}
		}
		++pos;
	}
	return -1;
}

/* 0x8136BDA8 */
static int encodeJpegMcuStream(const PictureSourceInfo &base, const PictureSourceInfo *overlay,
			       uint8_t overlayAlpha, uint32_t outW, uint32_t outH, int mcuMode,
			       uint8_t compRatio, bool omitSoi, PictureOutputStream *outStream,
			       const PictureCancelHook *cancelHook)
{
	if (outStream == NULL || !outStream->IsOpen())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	HeapAllocator *workHeap = pictureWorkHeapGet();
	uint32_t mcuH = (mcuMode == 2) ? 8u : 16u;
	uint32_t uvDivY = (mcuMode == 2) ? 1u : 2u;
	uint32_t uvH = (mcuH + uvDivY - 1u) / uvDivY;
	uint32_t ySize = 16u * mcuH;
	uint32_t uvSize = 8u * uvH;

	uint8_t *mcuBuf =
		static_cast<uint8_t *>(ScePafStdc_3CF19248(ySize + 2u * uvSize, workHeap));
	if (mcuBuf == NULL)
		return PIC_CREATOR_ERROR_OUT_OF_MEMORY;

	uint8_t *jpegChunk = static_cast<uint8_t *>(ScePafStdc_3CF19248(1024, workHeap));
	if (jpegChunk == NULL) {
		operator delete(mcuBuf);
		return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
	}

	int ctxSize = sceJpegArmEncoderGetContextSize();
	void *encCtx = (ctxSize > 0) ? ScePafStdc_3CF19248(static_cast<size_t>(ctxSize), workHeap)
				     : NULL;
	if (encCtx == NULL) {
		operator delete(jpegChunk);
		operator delete(mcuBuf);
		return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
	}

	uint32_t alignedW = (outW + 15u) & ~15u;
	uint32_t alignedH = (outH + mcuH - 1u) & ~(mcuH - 1u);
	int pixelFmt = (mcuMode == 3) ? SCE_JPEGENC_PIXEL_YCBCR420 : SCE_JPEGENC_PIXEL_YCBCR422;

	int ret = sceJpegArmEncoderInit(encCtx, static_cast<int>(alignedW),
					static_cast<int>(alignedH), pixelFmt, NULL, 0);
	if (ret == 0) {
		if (alignedW != outW || alignedH != outH)
			ret = sceJpegArmEncoderSetValidRegion(encCtx, static_cast<uint16_t>(outW),
							      static_cast<uint16_t>(outH));
		if (ret == 0)
			ret = sceJpegArmEncoderSetCompressionRatio(encCtx, compRatio);
		if (ret == 0 && !omitSoi) {
			static const uint8_t soiMarker[2] = { 0xFF, 0xD8 };
			ret = outStream->Write(soiMarker, 2);
		}
		if (ret == 0) {
			uint8_t *yPlane = mcuBuf;
			uint8_t *cbPlane = mcuBuf + ySize;
			uint8_t *crPlane = cbPlane + uvSize;
			int headerSize = 0;

			sce_paf_memset(mcuBuf, 0, ySize + 2u * uvSize);
			ret = SceJpegEncArm_4EE1BAED(encCtx, yPlane, cbPlane, crPlane, 2,
						     jpegChunk, 1024, &headerSize);
			if (ret >= 0) {
				int dqtOffset = findJpegDqtMarker(jpegChunk, headerSize);
				if (dqtOffset < 0)
					ret = PIC_CREATOR_ERROR_GENERIC;
				else
					ret = outStream->Write(
						jpegChunk + dqtOffset,
						static_cast<SceSize>(headerSize - dqtOffset));
			}

			uint32_t srcW = sourceEffectiveWidth(&base);
			uint32_t srcH = sourceEffectiveHeight(&base);

			for (uint32_t mcuY = 0; ret == 0 && mcuY < alignedH; mcuY += mcuH) {
				for (uint32_t mcuX = 0; ret == 0 && mcuX < alignedW; mcuX += 16u) {
					if (pictureIsCanceled(cancelHook)) {
						ret = PIC_CREATOR_ERROR_CANCELED;
						break;
					}
					for (uint32_t row = 0; row < mcuH; ++row) {
						uint32_t destY = (mcuY + row < outH) ? (mcuY + row)
										     : (outH - 1u);
						uint32_t sampleY = (destY * srcH) / outH;

						for (uint32_t col = 0; col < 16u; ++col) {
							uint32_t destX = (mcuX + col < outW)
										 ? (mcuX + col)
										 : (outW - 1u);
							uint32_t sampleX = (destX * srcW) / outW;
							uint32_t rgba = sampleCompositedRgba(
								base, overlay, overlayAlpha,
								sampleX, sampleY);
							uint8_t ySample, cbSample, crSample;
							convertRgbaToYCbCr(rgba, ySample, cbSample,
									   crSample);
							yPlane[row * 16u + col] = ySample;
							if ((col & 1u) == 0 &&
							    (uvDivY == 1u || (row & 1u) == 0)) {
								uint32_t uvOffset =
									(row / uvDivY) * 8u +
									(col >> 1);
								cbPlane[uvOffset] = cbSample;
								crPlane[uvOffset] = crSample;
							}
						}
					}

					int chunkLen = 0;
					int encStatus = SceJpegEncArm_4EE1BAED(
						encCtx, yPlane, cbPlane, crPlane, 3, jpegChunk,
						1024, &chunkLen);
					if (encStatus < 0) {
						ret = encStatus;
						break;
					}
					ret = outStream->Write(jpegChunk,
							       static_cast<SceSize>(chunkLen));
					if (ret != 0 || encStatus > 0)
						break;
				}
			}
			if (ret == 0)
				ret = static_cast<int>(outStream->GetWrittenSize());
		}
		sceJpegArmEncoderEnd(encCtx);
	}

	operator delete(encCtx);
	operator delete(jpegChunk);
	operator delete(mcuBuf);
	return ret;
}

/* 0x81366AF8 */
static int encodeJpegPicture(const PictureSourceInfo &base, const PictureSourceInfo *overlay,
			     uint8_t overlayAlpha, const PictureExifData *exif, uint8_t compRatio,
			     PictureOutputStream *outStream, const char *thumbTempPath,
			     uint32_t thumbIoFlags, const PictureCancelHook *cancelHook)
{
	if (outStream == NULL || !outStream->IsOpen())
		return PIC_CREATOR_ERROR_GENERIC;

	HeapAllocator *workHeap = pictureWorkHeapGet();
	PictureOutputStream *thumbStream = NULL;
	PictureFileStream *thumbFileStream = NULL;
	PictureMemoryStream *thumbMemStream = NULL;
	uint8_t *thumbMemBuf = NULL;
	int ret = 0;

	if (thumbTempPath != NULL) {
		thumbFileStream = new PictureFileStream(PIC_THUMB_IO_BUFFER_SIZE);
		if (thumbFileStream == NULL)
			return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
		ret = thumbFileStream->Open(thumbTempPath, thumbIoFlags);
		thumbStream = thumbFileStream;
	} else {
		thumbMemBuf = static_cast<uint8_t *>(
			ScePafStdc_3CF19248(PIC_THUMB_MEM_BUFFER_SIZE, workHeap));
		if (thumbMemBuf == NULL)
			return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
		thumbMemStream = new PictureMemoryStream();
		if (thumbMemStream == NULL) {
			operator delete(thumbMemBuf);
			return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
		}
		ret = thumbMemStream->Open(thumbMemBuf, PIC_THUMB_MEM_BUFFER_SIZE);
		thumbStream = thumbMemStream;
	}

	if (ret == 0) {
		static const uint8_t thumbRatios[6] = { 8, 32, 64, 128, 192, 255 };
		int thumbResult = PIC_CREATOR_ERROR_GENERIC;

		for (int i = 0; i < 6; ++i) {
			ret = thumbStream->Rewind();
			if (ret != 0)
				break;
			thumbResult = encodeJpegMcuStream(
				base, overlay, overlayAlpha, PIC_THUMB_WIDTH, PIC_THUMB_HEIGHT, 2,
				thumbRatios[i], false, thumbStream, cancelHook);
			if (thumbResult > 0)
				break;
		}

		if (ret == 0 && thumbResult <= 0)
			ret = (thumbResult < 0) ? thumbResult : PIC_CREATOR_ERROR_GENERIC;
		if (ret == 0)
			ret = thumbStream->Flush();
		if (ret == 0 && pictureIsCanceled(cancelHook))
			ret = PIC_CREATOR_ERROR_CANCELED;
		if (ret == 0) {
			static const uint8_t soiMarker[2] = { 0xFF, 0xD8 };
			ret = outStream->Write(soiMarker, 2);
		}
		if (ret == 0 && exif != NULL) {
			PictureMakerNoteWriter makerWriter;
			PictureExifApp1Writer exifWriter;
			void *makerPtr = NULL;
			uint32_t makerLen = 0;

			if (exif->makerNote != NULL) {
				int mnRet = makerWriter.Build(exif->makerNote, &makerPtr,
							      &makerLen);
				if (mnRet < 0)
					ret = (mnRet == PIC_EXIF_ERROR_OUT_OF_MEMORY)
						      ? PIC_CREATOR_ERROR_OUT_OF_MEMORY
						      : PIC_CREATOR_ERROR_GENERIC;
			}
			if (ret == 0) {
				void *app1Ptr = NULL;
				uint32_t app1Len = 0;
				int app1Ret = pictureBuildExifApp1(
					exifWriter, exif, thumbStream->GetWrittenSize(), makerPtr,
					makerLen, &app1Ptr, &app1Len);
				if (app1Ret < 0) {
					ret = (app1Ret == PIC_EXIF_ERROR_OUT_OF_MEMORY)
						      ? PIC_CREATOR_ERROR_OUT_OF_MEMORY
						      : PIC_CREATOR_ERROR_GENERIC;
				} else {
					ret = outStream->Write(app1Ptr, app1Len);
					if (ret == 0)
						ret = thumbStream->CopyTo(outStream);
				}
			}
		}
		if (ret == 0 && pictureIsCanceled(cancelHook))
			ret = PIC_CREATOR_ERROR_CANCELED;
		if (ret == 0) {
			uint32_t outW = sourceEffectiveWidth(&base);
			uint32_t outH = sourceEffectiveHeight(&base);
			int mainResult = encodeJpegMcuStream(base, overlay, overlayAlpha, outW,
							     outH, 3, compRatio, true, outStream,
							     cancelHook);
			ret = (mainResult > 0)
				      ? 0
				      : ((mainResult < 0) ? mainResult : PIC_CREATOR_ERROR_GENERIC);
		}
	}

	if (thumbFileStream != NULL)
		delete thumbFileStream;
	if (thumbMemStream != NULL)
		delete thumbMemStream;
	if (thumbMemBuf != NULL)
		operator delete(thumbMemBuf);
	if (thumbTempPath != NULL)
		PictureFileStream::RemoveFile(thumbTempPath);

	return ret;
}

/* 0x813669A8 */
static int encodeBmpPicture(const PictureSourceInfo &base, const PictureSourceInfo *overlay,
			    uint8_t overlayAlpha, PictureOutputStream *outStream,
			    const PictureCancelHook *cancelHook)
{
	if (outStream == NULL || !outStream->IsOpen())
		return PIC_CREATOR_ERROR_GENERIC;

	uint32_t outW = sourceEffectiveWidth(&base);
	uint32_t outH = sourceEffectiveHeight(&base);
	uint32_t rowStride = (3u * outW + 3u) & ~3u;
	uint32_t rowPadLen = rowStride - 3u * outW;
	uint32_t imageBytes = rowStride * outH;
	uint32_t totalSize = 54u + imageBytes;

	uint8_t fileHeader[14];
	sce_paf_memset(fileHeader, 0, sizeof(fileHeader));
	fileHeader[0] = 'B';
	fileHeader[1] = 'M';
	fileHeader[2] = static_cast<uint8_t>(totalSize & 0xFFu);
	fileHeader[3] = static_cast<uint8_t>((totalSize >> 8) & 0xFFu);
	fileHeader[4] = static_cast<uint8_t>((totalSize >> 16) & 0xFFu);
	fileHeader[5] = static_cast<uint8_t>((totalSize >> 24) & 0xFFu);
	fileHeader[10] = 54;

	struct BmpInfoHeader {
		uint32_t headerSize;
		int32_t width;
		int32_t height;
		uint16_t planes;
		uint16_t bitsPerPixel;
		uint32_t compression;
		uint32_t imageSize;
		int32_t xPixelsPerMeter;
		int32_t yPixelsPerMeter;
		uint32_t colorsUsed;
		uint32_t colorsImportant;
	};
	BmpInfoHeader dibHeader;
	sce_paf_memset(&dibHeader, 0, sizeof(dibHeader));
	dibHeader.headerSize = 40;
	dibHeader.width = static_cast<int32_t>(outW);
	dibHeader.height = -static_cast<int32_t>(outH);
	dibHeader.planes = 1;
	dibHeader.bitsPerPixel = 24;
	dibHeader.imageSize = imageBytes;

	int ret = outStream->Write(fileHeader, sizeof(fileHeader));
	if (ret == 0)
		ret = outStream->Write(&dibHeader, sizeof(dibHeader));

	static const uint8_t zeroPad[4] = { 0, 0, 0, 0 };
	for (uint32_t posY = 0; ret == 0 && posY < outH; ++posY) {
		if (pictureIsCanceled(cancelHook))
			return PIC_CREATOR_ERROR_CANCELED;
		for (uint32_t posX = 0; posX < outW; ++posX) {
			uint32_t rgba =
				sampleCompositedRgba(base, overlay, overlayAlpha, posX, posY);
			uint8_t bgr[3] = {
				static_cast<uint8_t>((rgba >> 16) & 0xFFu),
				static_cast<uint8_t>((rgba >> 8) & 0xFFu),
				static_cast<uint8_t>(rgba & 0xFFu)
			};
			ret = outStream->Write(bgr, sizeof(bgr));
			if (ret != 0)
				break;
		}
		if (ret == 0 && rowPadLen != 0)
			ret = outStream->Write(zeroPad, rowPadLen);
	}
	return ret;
}

/* 0x81367220 */
static int encodeRawPicture(const PictureSourceInfo &base, const PictureSourceInfo *overlay,
			    uint8_t overlayAlpha, PictureOutputStream *outStream,
			    const PictureCancelHook *cancelHook)
{
	if (outStream == NULL || !outStream->IsOpen())
		return PIC_CREATOR_ERROR_GENERIC;

	uint32_t outW = sourceEffectiveWidth(&base);
	uint32_t outH = sourceEffectiveHeight(&base);
	uint32_t alignedW = (outW + 7u) & ~7u;
	uint32_t rowPadLen = (alignedW - outW) * 4u;
	static const uint8_t zeroRowPad[32] = { 0 };

	for (uint32_t posY = 0; posY < outH; ++posY) {
		if (pictureIsCanceled(cancelHook))
			return PIC_CREATOR_ERROR_CANCELED;
		for (uint32_t posX = 0; posX < outW; ++posX) {
			uint32_t rgba =
				sampleCompositedRgba(base, overlay, overlayAlpha, posX, posY);
			uint8_t pixel[4] = {
				static_cast<uint8_t>(rgba & 0xFFu),
				static_cast<uint8_t>((rgba >> 8) & 0xFFu),
				static_cast<uint8_t>((rgba >> 16) & 0xFFu),
				static_cast<uint8_t>((rgba >> 24) & 0xFFu)
			};
			int ret = outStream->Write(pixel, sizeof(pixel));
			if (ret != 0)
				return ret;
		}
		if (rowPadLen != 0) {
			int ret = outStream->Write(zeroRowPad, rowPadLen);
			if (ret != 0)
				return ret;
		}
	}
	return 0;
}

/* --- Public ScePictureFileCreator API (0x81365BFC - 0x81366677) --- */

/* 0x81365BFC */
int pictureFileCreatorInit(void)
{
	if (s_creatorInitialized != 0)
		return PIC_CREATOR_ERROR_INVALID_STATE;
	s_creatorInitialized = 1;
	return 0;
}

/* 0x81366060 */
int pictureFileCreatorGetWorkSize(const PictureEncodeConfig *config, PictureWorkSize *outSize)
{
	if (config == NULL || config->source == NULL || outSize == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	const PictureSourceInfo *source = config->source;
	if (source->width == 0 || source->height == 0 || source->pitchBytes < source->width)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	if (source->cropWidth * source->cropHeight != 0) {
		if (source->cropWidth == 0 || source->cropHeight == 0 ||
		    source->cropX + source->cropWidth > source->width ||
		    source->cropY + source->cropHeight > source->height)
			return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	}

	uint32_t effW = sourceEffectiveWidth(source);
	uint32_t effH = sourceEffectiveHeight(source);
	uint32_t minOutputSize = 0;
	uint32_t workBytes = 0;

	if (config->outputFormat == PIC_FORMAT_JPEG) {
		if ((config->outputFlags & PIC_OUTPUT_FLAG_FILE) == 0)
			minOutputSize =
				((((effW + 15u) >> 4) * ((effH + 15u) >> 4)) << 10) + 98304u;
		workBytes = ((config->outputFlags & PIC_OUTPUT_FLAG_THUMB_TEMP_FILE) != 0)
				    ? 11136u
				    : 48512u;
	} else if (config->outputFormat == PIC_FORMAT_BMP) {
		if ((config->outputFlags & PIC_OUTPUT_FLAG_FILE) == 0)
			minOutputSize = 54u + effH * ((3u * effW + 3u) & ~3u);
		workBytes = 512u;
	} else if (config->outputFormat == PIC_FORMAT_RAW) {
		if ((config->outputFlags & PIC_OUTPUT_FLAG_FILE) == 0)
			minOutputSize = 32u * ((effW + 7u) >> 3) * effH;
		workBytes = 512u;
	} else {
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	}

	workBytes += ((config->outputFlags & PIC_OUTPUT_FLAG_FILE) != 0) ? 9344u : 1152u;
	outSize->outputBufferSize = minOutputSize;
	outSize->workMemorySize = (workBytes + 1535u) & ~1023u;
	return 0;
}

/* 0x81366198 */
static bool validateCreateRequest(const PictureCreateInput *input,
				  const PictureCreateOutput *output)
{
	if (input == NULL || output == NULL || input->config == NULL)
		return false;

	const PictureEncodeConfig *config = input->config;
	if (!validateSourceInfo(config->source))
		return false;
	if (config->overlay != NULL && !validateSourceInfo(config->overlay))
		return false;
	if (config->outputFormat < PIC_FORMAT_JPEG || config->outputFormat > PIC_FORMAT_RAW)
		return false;

	PictureWorkSize requiredSize;
	requiredSize.outputBufferSize = 0;
	requiredSize.workMemorySize = 0;
	if (pictureFileCreatorGetWorkSize(config, &requiredSize) != 0)
		return false;

	if ((config->outputFlags & PIC_OUTPUT_FLAG_FILE) != 0) {
		if (output->outputFilePath == NULL || output->outputFilePath[0] == '\0')
			return false;
	}
	if ((config->outputFlags & PIC_OUTPUT_FLAG_THUMB_TEMP_FILE) != 0) {
		if (output->thumbTempPath == NULL || output->thumbTempPath[0] == '\0')
			return false;
	}
	if (requiredSize.outputBufferSize != 0 &&
	    (output->outputBuffer == NULL ||
	     output->outputBufferSize < requiredSize.outputBufferSize))
		return false;
	if (requiredSize.workMemorySize != 0 &&
	    (output->workMemory == NULL ||
	     output->workMemorySize < requiredSize.workMemorySize))
		return false;

	return true;
}

/* 0x8136638A */
int pictureFileCreatorCreate(const PictureCreateInput *input, PictureCreateOutput *output)
{
	sceKernelGetProcessTimeWide();

	if (s_creatorInitialized == 0) {
		sceKernelGetProcessTimeWide();
		return PIC_CREATOR_ERROR_INVALID_STATE;
	}
	if (!validateCreateRequest(input, output)) {
		sceKernelGetProcessTimeWide();
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	}

	const PictureEncodeConfig *config = input->config;
	PictureCancelHook cancelHook;
	cancelHook.isCanceled = input->cancelFunc;
	cancelHook.userData = input->cancelUserData;

	uintptr_t alignedWork =
		(reinterpret_cast<uintptr_t>(output->workMemory) + 15u) & ~static_cast<uintptr_t>(15u);
	HeapAllocator workHeap(reinterpret_cast<void *>(alignedWork), output->workMemorySize - 16u,
			       "ScePictureFileCreatorWorkMemory", NULL);
	if (pictureWorkHeapSet(&workHeap) != 0) {
		sceKernelGetProcessTimeWide();
		return PIC_CREATOR_ERROR_GENERIC;
	}

	PictureOutputStream *stream = NULL;
	PictureFileStream *fileStream = NULL;
	PictureMemoryStream *memStream = NULL;
	int ret = 0;

	if ((config->outputFlags & PIC_OUTPUT_FLAG_FILE) != 0) {
		fileStream = new PictureFileStream(PIC_FILE_IO_BUFFER_SIZE);
		if (fileStream == NULL) {
			pictureWorkHeapClear();
			sceKernelGetProcessTimeWide();
			return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
		}
		stream = fileStream;
		ret = fileStream->Open(output->outputFilePath, output->ioBufferSize);
	} else {
		memStream = new PictureMemoryStream();
		if (memStream == NULL) {
			pictureWorkHeapClear();
			sceKernelGetProcessTimeWide();
			return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
		}
		stream = memStream;
		ret = memStream->Open(output->outputBuffer,
				      static_cast<uint32_t>(output->outputBufferSize));
	}

	if (ret == 0) {
		const char *thumbPath = NULL;
		uint32_t thumbIoFlags = 0;
		if ((config->outputFlags & PIC_OUTPUT_FLAG_THUMB_TEMP_FILE) != 0) {
			thumbPath = output->thumbTempPath;
			thumbIoFlags = output->thumbIoBufferSize;
		}

		if (config->outputFormat == PIC_FORMAT_JPEG) {
			ret = encodeJpegPicture(*config->source, config->overlay,
						config->overlayAlpha, input->exif,
						config->jpegCompressionRatio, stream, thumbPath,
						thumbIoFlags, &cancelHook);
		} else if (config->outputFormat == PIC_FORMAT_BMP) {
			ret = encodeBmpPicture(*config->source, config->overlay,
					       config->overlayAlpha, stream, &cancelHook);
		} else if (config->outputFormat == PIC_FORMAT_RAW) {
			ret = encodeRawPicture(*config->source, config->overlay,
					       config->overlayAlpha, stream, &cancelHook);
		} else {
			ret = PIC_CREATOR_ERROR_INVALID_ARGUMENT;
		}
		if (ret == PIC_CREATOR_ERROR_INVALID_ARGUMENT)
			ret = PIC_CREATOR_ERROR_GENERIC;
	}

	uint32_t writtenBytes = 0;
	if (stream != NULL && stream->IsOpen()) {
		writtenBytes = stream->GetWrittenSize();
		int closeRet = stream->Close();
		if (ret == 0)
			ret = closeRet;
		if (ret != 0 && (config->outputFlags & PIC_OUTPUT_FLAG_FILE) != 0) {
			PictureFileStream::RemoveFile(output->outputFilePath);
			writtenBytes = 0;
		}
	}

	if (fileStream != NULL)
		delete fileStream;
	if (memStream != NULL)
		delete memStream;

	pictureWorkHeapClear();
	sceKernelGetProcessTimeWide();
	return (ret == 0) ? static_cast<int>(writtenBytes) : ret;
}
