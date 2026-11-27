/*
 * picture_encoder.cpp: ScePictureFileCreator public API, pixel source/scaler/compositor
 * pipeline, MCU feeders, JPEG/EXIF/thumbnail encoder (SceJpegEncArm), BMP encoder,
 * and RAW encoder.
 * 3.65-CEX: 0x81365BFC - 0x81366677, 0x813669A8 - 0x8136731B, 0x81368D10 - 0x8136C28B,
 *           0x8136CCCC - 0x8136D04F
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <scejpegenc.h>
#include <kernel/processmgr.h>
#include <std/new>
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
#define PIC_BILINEAR_CACHE_SLOTS	8

struct PictureBilinearCacheEntry {
	uint32_t pixel;
	int32_t posX;
	int32_t posY;
};

struct PictureDimensionQuery {
	uint32_t width;
	uint32_t height;
};

struct PictureJpegSizeQuery {
	uint32_t width;
	uint32_t height;
	bool useThumbTempFile;
};

struct PictureBmpHeaderParam {
	uint32_t width;
	uint32_t height;
	PictureOutputStream *stream;
};

struct PicturePixelWriteParam {
	PicturePixelSource *source;
	PictureOutputStream *stream;
	const PictureCancelHook *cancelHook;
};

struct PictureJpegMcuParam {
	PictureMcuFeederBase *feeder;
	PictureOutputStream *stream;
	int subsampling;
	uint8_t compressionRatio;
	bool omitSoi;
	const PictureCancelHook *cancelHook;
};

struct PictureSimpleEncodeParam {
	const PictureSourceInfo *source;
	const PictureSourceInfo *overlay;
	uint8_t overlayAlpha;
	PictureOutputStream *stream;
	const PictureCancelHook *cancelHook;
};

struct PictureJpegEncodeParam {
	const PictureSourceInfo *source;
	const PictureSourceInfo *overlay;
	uint8_t overlayAlpha;
	const PictureExifData *exif;
	uint8_t compressionRatio;
	PictureOutputStream *stream;
	const char *thumbTempPath;
	uint32_t thumbIoFlags;
	const PictureCancelHook *cancelHook;
	float scaleX;
	float scaleY;
};

static SceUInt8 s_creatorInitialized = 0;			/* 0x815C6698 */
static int s_bilinearCacheIndex = 0;				/* 0x815C669C */
static PictureBilinearCacheEntry s_bilinearCache[PIC_BILINEAR_CACHE_SLOTS]; /* 0x815C66A0 */

/* 0x813696EE */
static uint32_t picturePackPixel(uint8_t redOrY, uint8_t greenOrCb, uint8_t blueOrCr,
				 uint8_t alpha)
{
	return static_cast<uint32_t>(redOrY) |
	       (static_cast<uint32_t>(greenOrCb) << 8) |
	       (static_cast<uint32_t>(blueOrCr) << 16) |
	       (static_cast<uint32_t>(alpha) << 24);
}

static inline uint8_t clampByteInt(int value)
{
	if (value <= 0)
		return 0;
	if (value >= 255)
		return 255;
	return static_cast<uint8_t>(value);
}

static inline uint8_t clampByteFloat(float value)
{
	if (value <= 0.0f)
		return 0;
	if (value >= 255.0f)
		return 255;
	return static_cast<uint8_t>(static_cast<unsigned int>(value));
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

static void getYuvChromaDivisors(int subsampling, uint32_t *outDivX, uint32_t *outDivY)
{
	switch (subsampling) {
	case PIC_YUV_444:
		*outDivX = 1;
		*outDivY = 1;
		break;
	case PIC_YUV_422:
		*outDivX = 2;
		*outDivY = 1;
		break;
	case PIC_YUV_420:
		*outDivX = 2;
		*outDivY = 2;
		break;
	case PIC_YUV_411:
		*outDivX = 4;
		*outDivY = 1;
		break;
	default:
		*outDivX = 1;
		*outDivY = 1;
		break;
	}
}

/* ============================================================================
 * PicturePixelSource (0x813695F0 - 0x813696ED)
 * ============================================================================ */

/* 0x813695F0 */
PicturePixelSource::PicturePixelSource()
	: m_width(0), m_height(0), m_initialized(false)
{
}

/* 0x81369638 / 0x81369618 */
PicturePixelSource::~PicturePixelSource()
{
	Term(false);
}

/* 0x81369608 */
void PicturePixelSource::Term(bool deleteChild)
{
	(void)deleteChild;
	if (m_initialized) {
		m_initialized = false;
		m_width = 0;
		m_height = 0;
	}
}

bool PicturePixelSource::IsInitialized() const
{
	return m_initialized;
}

/* 0x81369690 */
PictureSize PicturePixelSource::GetSize() const
{
	PictureSize size;
	size.width = m_width;
	size.height = m_height;
	return size;
}

/* 0x81369652 */
int PicturePixelSource::InitBase(uint32_t width, uint32_t height)
{
	if (m_initialized)
		return PIC_CREATOR_ERROR_INVALID_STATE;
	if (width == 0 || height == 0)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	m_width = width;
	m_height = height;
	m_initialized = true;
	return 0;
}

uint32_t PicturePixelSource::GetDefaultPixel() const
{
	return 0;
}

/* 0x8136969A */
uint32_t PicturePixelSource::GetPixel(uint32_t posX, uint32_t posY)
{
	if (IsInitialized() && posX < m_width && posY < m_height)
		return GetPixelAt(posX, posY);
	return GetDefaultPixel();
}

/* ============================================================================
 * PictureCanvasSource (0x813693D0 - 0x813695EF)
 * ============================================================================ */

/* 0x813693D0 */
PictureCanvasSource::PictureCanvasSource()
	: m_source(NULL),
	  m_clipLeft(0),
	  m_clipRight(0),
	  m_clipTop(0),
	  m_clipBottom(0),
	  m_offsetX(0),
	  m_offsetY(0)
{
}

/* 0x813693F8 / 0x81369414 */
PictureCanvasSource::~PictureCanvasSource()
{
	Term(false);
}

/* 0x81369434 */
int PictureCanvasSource::Init(PicturePixelSource *source, uint32_t width, uint32_t height)
{
	if (source == NULL || !source->IsInitialized())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	int ret = InitBase(width, height);
	if (ret == 0) {
		m_source = source;
		PictureSize srcSize = source->GetSize();
		int32_t offsetX = (static_cast<int32_t>(width) - static_cast<int32_t>(srcSize.width)) / 2;
		int32_t offsetY = (static_cast<int32_t>(height) - static_cast<int32_t>(srcSize.height)) / 2;

		m_clipLeft = (offsetX >= 0) ? static_cast<uint32_t>(offsetX) : 0u;
		m_clipRight = (offsetX >= 0) ? (static_cast<uint32_t>(offsetX) + srcSize.width) : width;
		m_clipTop = (offsetY >= 0) ? static_cast<uint32_t>(offsetY) : 0u;
		m_clipBottom = (offsetY >= 0) ? (static_cast<uint32_t>(offsetY) + srcSize.height) : height;
		m_offsetX = offsetX;
		m_offsetY = offsetY;
	}
	return ret;
}

/* 0x813694FA */
void PictureCanvasSource::Term(bool deleteChild)
{
	if (deleteChild && m_source != NULL)
		delete m_source;
	m_source = NULL;
	PicturePixelSource::Term(deleteChild);
}

/* 0x8136951E */
int PictureCanvasSource::GetColorSpace() const
{
	if (IsInitialized() && m_source != NULL && m_source->IsInitialized())
		return m_source->GetColorSpace();
	return PIC_COLOR_SPACE_NONE;
}

/* 0x81369548 */
uint32_t PictureCanvasSource::GetDefaultPixel() const
{
	if (IsInitialized() && m_source != NULL && m_source->IsInitialized())
		return m_source->GetDefaultPixel();
	return 0;
}

/* 0x81369594 */
uint32_t PictureCanvasSource::GetPixelAt(uint32_t posX, uint32_t posY)
{
	if (m_source != NULL && posX >= m_clipLeft && posX < m_clipRight &&
	    posY >= m_clipTop && posY < m_clipBottom) {
		return m_source->GetPixel(
			static_cast<uint32_t>(static_cast<int32_t>(posX) - m_offsetX),
			static_cast<uint32_t>(static_cast<int32_t>(posY) - m_offsetY));
	}
	return GetDefaultPixel();
}

/* ============================================================================
 * PictureScalerSource (0x81369700 - 0x8136990F)
 * ============================================================================ */

/* 0x81369700 */
PictureScalerSource::PictureScalerSource()
	: m_scaleX(1.0f),
	  m_invScaleX(1.0f),
	  m_scaleY(1.0f),
	  m_invScaleY(1.0f),
	  m_source(NULL)
{
}

/* 0x81369728 / 0x81369744 */
PictureScalerSource::~PictureScalerSource()
{
	Term(false);
}

/* 0x81369764 */
int PictureScalerSource::InitScaler(PicturePixelSource *source, float scaleX, float scaleY)
{
	if (source == NULL || !source->IsInitialized())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	PictureSize srcSize = source->GetSize();
	int ret = InitBase(static_cast<uint32_t>(static_cast<float>(srcSize.width) * scaleX),
			   static_cast<uint32_t>(static_cast<float>(srcSize.height) * scaleY));
	if (ret == 0) {
		m_scaleX = scaleX;
		m_scaleY = scaleY;
		m_source = source;
		m_invScaleX = 1.0f / scaleX;
		m_invScaleY = 1.0f / scaleY;
	}
	return ret;
}

/* 0x81369812 */
int PictureScalerSource::InitUniformScaler(PicturePixelSource *source, float scale)
{
	if (source == NULL || !source->IsInitialized())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	PictureSize srcSize = source->GetSize();
	int ret = InitBase(static_cast<uint32_t>(static_cast<float>(srcSize.width) * scale),
			   static_cast<uint32_t>(static_cast<float>(srcSize.height) * scale));
	if (ret == 0) {
		m_scaleX = scale;
		m_scaleY = scale;
		m_source = source;
		m_invScaleX = 1.0f / scale;
		m_invScaleY = 1.0f / scale;
	}
	return ret;
}

/* 0x813698BA */
void PictureScalerSource::Term(bool deleteChild)
{
	if (deleteChild && m_source != NULL)
		delete m_source;
	m_source = NULL;
	m_scaleX = 1.0f;
	m_scaleY = 1.0f;
	PicturePixelSource::Term(deleteChild);
}

/* 0x813698E6 */
int PictureScalerSource::GetColorSpace() const
{
	if (IsInitialized() && m_source != NULL && m_source->IsInitialized())
		return m_source->GetColorSpace();
	return PIC_COLOR_SPACE_NONE;
}

/* ============================================================================
 * PictureBilinearScaler (0x81368D10 - 0x813693CF)
 * ============================================================================ */

/* 0x81368D10 */
PictureBilinearScaler::PictureBilinearScaler()
	: m_halfStepX(0), m_halfStepY(0)
{
}

/* 0x81368D30 / 0x81368D4C */
PictureBilinearScaler::~PictureBilinearScaler()
{
	Term(false);
}

/* 0x81368D6E */
int PictureBilinearScaler::Init(PicturePixelSource *source, float scaleX, float scaleY)
{
	int ret = InitScaler(source, scaleX, scaleY);
	if (ret == 0) {
		float halfX = m_invScaleX * 0.5f;
		int truncX = static_cast<int>(halfX);
		m_halfStepX = truncX - ((halfX - static_cast<float>(truncX)) < 0.5f ? 1 : 0);

		float halfY = m_invScaleY * 0.5f;
		int truncY = static_cast<int>(halfY);
		m_halfStepY = truncY - ((halfY - static_cast<float>(truncY)) < 0.5f ? 1 : 0);
	}
	for (int idx = 0; idx < PIC_BILINEAR_CACHE_SLOTS; ++idx) {
		s_bilinearCache[idx].posX = -1;
		s_bilinearCache[idx].posY = -1;
	}
	s_bilinearCacheIndex = 0;
	return ret;
}

/* 0x81369E84 */
void PictureBilinearScaler::Term(bool deleteChild)
{
	PictureScalerSource::Term(deleteChild);
}

/* 0x81368E3C */
uint32_t PictureBilinearScaler::GetDefaultPixel() const
{
	if (IsInitialized() && m_source != NULL && m_source->IsInitialized())
		return m_source->GetDefaultPixel();
	return 0;
}

/* 0x81368E88 */
uint32_t PictureBilinearScaler::GetPixelAt(uint32_t posX, uint32_t posY)
{
	if (m_source == NULL)
		return GetDefaultPixel();

	float srcX = static_cast<float>(posX) * m_invScaleX;
	float srcY = static_cast<float>(posY) * m_invScaleY;
	int32_t baseX = static_cast<int32_t>(srcX);
	int32_t baseY = static_cast<int32_t>(srcY);

	uint32_t topLeft = SampleCachedPixel(baseX, baseY);
	uint32_t topRight = SampleCachedPixel(baseX + 1, baseY);
	uint32_t botLeft = SampleCachedPixel(baseX, baseY + 1);
	uint32_t botRight = SampleCachedPixel(baseX + 1, baseY + 1);

	float fracX = srcX - static_cast<float>(baseX);
	float fracY = srcY - static_cast<float>(baseY);
	float invX = 1.0f - fracX;
	float invY = 1.0f - fracY;

	float weightTL = invX * invY;
	float weightBL = invX * fracY;
	float weightTR = fracX * invY;
	float weightBR = fracX * fracY;

	float red = static_cast<float>(topLeft & 0xFFu) * weightTL +
		    static_cast<float>(botLeft & 0xFFu) * weightBL +
		    static_cast<float>(topRight & 0xFFu) * weightTR +
		    static_cast<float>(botRight & 0xFFu) * weightBR;
	float green = static_cast<float>((topLeft >> 8) & 0xFFu) * weightTL +
		      static_cast<float>((botLeft >> 8) & 0xFFu) * weightBL +
		      static_cast<float>((topRight >> 8) & 0xFFu) * weightTR +
		      static_cast<float>((botRight >> 8) & 0xFFu) * weightBR;
	float blue = static_cast<float>((topLeft >> 16) & 0xFFu) * weightTL +
		     static_cast<float>((botLeft >> 16) & 0xFFu) * weightBL +
		     static_cast<float>((topRight >> 16) & 0xFFu) * weightTR +
		     static_cast<float>((botRight >> 16) & 0xFFu) * weightBR;
	float alpha = static_cast<float>((topLeft >> 24) & 0xFFu) * weightTL +
		      static_cast<float>((botLeft >> 24) & 0xFFu) * weightBL +
		      static_cast<float>((topRight >> 24) & 0xFFu) * weightTR +
		      static_cast<float>((botRight >> 24) & 0xFFu) * weightBR;

	return picturePackPixel(clampByteFloat(red), clampByteFloat(green),
				clampByteFloat(blue), clampByteFloat(alpha));
}

/* 0x81369228 */
uint32_t PictureBilinearScaler::SampleClampedPixel(int32_t posX, int32_t posY)
{
	PictureSize srcSize = m_source->GetSize();
	uint32_t clampedX = (posX > 0) ? static_cast<uint32_t>(posX) : 0u;
	if (clampedX >= srcSize.width - 1u)
		clampedX = srcSize.width - 1u;

	uint32_t clampedY = (posY > 0) ? static_cast<uint32_t>(posY) : 0u;
	if (clampedY >= srcSize.height - 1u)
		clampedY = srcSize.height - 1u;

	return m_source->GetPixel(clampedX, clampedY);
}

/* 0x813692AE */
uint32_t PictureBilinearScaler::SampleCachedPixel(int32_t posX, int32_t posY)
{
	PictureSize srcSize = m_source->GetSize();
	int32_t clampedX = (posX > 0) ? posX : 0;
	if (static_cast<uint32_t>(clampedX) >= srcSize.width - 1u)
		clampedX = static_cast<int32_t>(srcSize.width - 1u);

	int32_t clampedY = (posY > 0) ? posY : 0;
	if (static_cast<uint32_t>(clampedY) >= srcSize.height - 1u)
		clampedY = static_cast<int32_t>(srcSize.height - 1u);

	for (int idx = 0; idx < PIC_BILINEAR_CACHE_SLOTS; ++idx) {
		if (s_bilinearCache[idx].posX == clampedX &&
		    s_bilinearCache[idx].posY == clampedY)
			return s_bilinearCache[idx].pixel;
	}

	if (s_bilinearCacheIndex >= PIC_BILINEAR_CACHE_SLOTS)
		s_bilinearCacheIndex = 0;

	uint32_t pixel = m_source->GetPixel(static_cast<uint32_t>(clampedX),
					    static_cast<uint32_t>(clampedY));
	int slotIndex = s_bilinearCacheIndex;
	s_bilinearCache[slotIndex].pixel = pixel;
	s_bilinearCache[slotIndex].posX = clampedX;
	s_bilinearCache[slotIndex].posY = clampedY;
	s_bilinearCacheIndex = slotIndex + 1;
	return s_bilinearCache[slotIndex].pixel;
}

/* ============================================================================
 * PictureNearestScaler (0x81369E00 - 0x81369F07)
 * ============================================================================ */

/* 0x81369E00 */
PictureNearestScaler::PictureNearestScaler()
	: m_offsetX(0), m_offsetY(0)
{
}

/* 0x81369E20 / 0x81369E3C */
PictureNearestScaler::~PictureNearestScaler()
{
	Term(false);
}

/* 0x81369E5C */
int PictureNearestScaler::Init(PicturePixelSource *source, float scale)
{
	int ret = InitUniformScaler(source, scale);
	if (ret == 0) {
		uint32_t halfStep = static_cast<uint32_t>(m_invScaleX * 0.5f);
		m_offsetX = halfStep;
		m_offsetY = halfStep;
	}
	return ret;
}

/* 0x81369E84 */
void PictureNearestScaler::Term(bool deleteChild)
{
	PictureScalerSource::Term(deleteChild);
}

/* 0x81368E3C */
uint32_t PictureNearestScaler::GetDefaultPixel() const
{
	if (IsInitialized() && m_source != NULL && m_source->IsInitialized())
		return m_source->GetDefaultPixel();
	return 0;
}

/* 0x81369E8E */
uint32_t PictureNearestScaler::GetPixelAt(uint32_t posX, uint32_t posY)
{
	if (m_source == NULL)
		return GetDefaultPixel();

	uint32_t sampleX = static_cast<uint32_t>(
		static_cast<float>(m_offsetX) + static_cast<float>(posX) * m_invScaleX);
	uint32_t sampleY = static_cast<uint32_t>(
		static_cast<float>(m_offsetY) + static_cast<float>(posY) * m_invScaleY);
	return m_source->GetPixel(sampleX, sampleY);
}

/* ============================================================================
 * PictureOverlayCompositor (0x81369F08 - 0x8136A1A3)
 * ============================================================================ */

/* 0x81369F08 */
PictureOverlayCompositor::PictureOverlayCompositor()
	: m_base(NULL), m_overlay(NULL), m_offsetX(0), m_offsetY(0), m_overlayAlpha(0xFFu)
{
}

/* 0x81369F30 / 0x81369F4C */
PictureOverlayCompositor::~PictureOverlayCompositor()
{
	Term(false);
}

/* 0x81369F6C */
int PictureOverlayCompositor::Init(PicturePixelSource *base, PicturePixelSource *overlay,
				   uint8_t overlayAlpha)
{
	if (base == NULL || !base->IsInitialized() || overlay == NULL || !overlay->IsInitialized())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	if (base->GetColorSpace() != overlay->GetColorSpace())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	PictureSize baseSize = base->GetSize();
	int ret = InitBase(baseSize.width, baseSize.height);
	if (ret == 0) {
		m_base = base;
		m_overlay = overlay;
		m_overlayAlpha = overlayAlpha;
		PictureSize ovSize = overlay->GetSize();
		m_offsetX = (static_cast<int32_t>(baseSize.width) -
			     static_cast<int32_t>(ovSize.width)) / 2;
		m_offsetY = (static_cast<int32_t>(baseSize.height) -
			     static_cast<int32_t>(ovSize.height)) / 2;
	}
	return ret;
}

/* 0x8136A046 */
void PictureOverlayCompositor::Term(bool deleteChild)
{
	if (deleteChild) {
		if (m_base != NULL) {
			delete m_base;
			m_base = NULL;
		}
		if (m_overlay != NULL)
			delete m_overlay;
	}
	m_base = NULL;
	m_overlay = NULL;
	PicturePixelSource::Term(deleteChild);
}

int PictureOverlayCompositor::GetColorSpace() const
{
	if (IsInitialized() && m_base != NULL && m_base->IsInitialized())
		return m_base->GetColorSpace();
	return PIC_COLOR_SPACE_NONE;
}

uint32_t PictureOverlayCompositor::GetDefaultPixel() const
{
	if (IsInitialized() && m_base != NULL && m_base->IsInitialized())
		return m_base->GetDefaultPixel();
	return 0;
}

/* 0x8136A07C */
uint32_t PictureOverlayCompositor::GetPixelAt(uint32_t posX, uint32_t posY)
{
	if (!IsInitialized())
		return 0;

	uint32_t basePixel = m_base->GetPixel(posX, posY);
	int32_t ovX = static_cast<int32_t>(posX) - m_offsetX;
	int32_t ovY = static_cast<int32_t>(posY) - m_offsetY;
	if (ovX < 0 || ovY < 0)
		return basePixel;

	uint32_t ovPixel = m_overlay->GetPixel(static_cast<uint32_t>(ovX),
					       static_cast<uint32_t>(ovY));
	uint32_t baseRed = basePixel & 0xFFu;
	uint32_t baseGreen = (basePixel >> 8) & 0xFFu;
	uint32_t baseBlue = (basePixel >> 16) & 0xFFu;
	uint32_t baseAlpha = (basePixel >> 24) & 0xFFu;

	uint32_t ovAlpha = (ovPixel >> 24) & 0xFFu;
	uint32_t blendFactor = ((ovAlpha * static_cast<uint32_t>(m_overlayAlpha)) << 8) / 255u;
	uint32_t invBlend = 65280u - blendFactor;

	uint32_t blendedRed = ((ovPixel & 0xFFu) * blendFactor + baseRed * invBlend) / 0xFF00u;
	uint32_t blendedGreen =
		(((ovPixel >> 8) & 0xFFu) * blendFactor + baseGreen * invBlend) / 0xFF00u;
	uint32_t blendedBlue =
		(((ovPixel >> 16) & 0xFFu) * blendFactor + baseBlue * invBlend) / 0xFF00u;
	uint32_t outAlpha = baseAlpha + ovAlpha;

	return picturePackPixel(
		static_cast<uint8_t>((blendedRed < 255u) ? blendedRed : 255u),
		static_cast<uint8_t>((blendedGreen < 255u) ? blendedGreen : 255u),
		static_cast<uint8_t>((blendedBlue < 255u) ? blendedBlue : 255u),
		static_cast<uint8_t>((outAlpha < 255u) ? outAlpha : 255u));
}

/* ============================================================================
 * PictureBufferSource (0x8136CEE8 - 0x8136D04F)
 * ============================================================================ */

/* 0x8136CEE8 */
PictureBufferSource::PictureBufferSource()
	: m_bufWidth(0),
	  m_bufHeight(0),
	  m_cropX(0),
	  m_cropY(0),
	  m_cropWidth(0),
	  m_cropHeight(0)
{
}

/* 0x8136CF10 / 0x8136CF2C */
PictureBufferSource::~PictureBufferSource()
{
	Term(false);
}

/* 0x8136CF4C */
int PictureBufferSource::InitRegion(uint32_t bufWidth, uint32_t bufHeight, uint32_t cropX,
				    uint32_t cropY, uint32_t cropWidth, uint32_t cropHeight)
{
	if (cropX + cropWidth > bufWidth || cropY + cropHeight > bufHeight ||
	    cropWidth == 0 || cropHeight == 0)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	if (InitBase(cropWidth, cropHeight) == 0) {
		m_bufWidth = bufWidth;
		m_bufHeight = bufHeight;
		m_cropX = cropX;
		m_cropY = cropY;
		m_cropWidth = cropWidth;
		m_cropHeight = cropHeight;
	}
	return 0;
}

/* 0x8136CFDC */
void PictureBufferSource::Term(bool deleteChild)
{
	m_bufWidth = 0;
	m_bufHeight = 0;
	m_cropX = 0;
	m_cropY = 0;
	m_cropWidth = 0;
	m_cropHeight = 0;
	PicturePixelSource::Term(deleteChild);
}

/* 0x8136CFF8 */
uint32_t PictureBufferSource::GetPixel(uint32_t posX, uint32_t posY)
{
	if (IsInitialized() && posX < m_cropWidth && posY < m_cropHeight)
		return GetPixelAt(posX + m_cropX, posY + m_cropY);
	return GetDefaultPixel();
}

/* ============================================================================
 * PictureRgbSourceBase / PictureRgbToRgbSource / PictureRgbToYuvSource
 * (0x8136A1A4 - 0x8136A4E7)
 * ============================================================================ */

PictureRgbSourceBase::PictureRgbSourceBase()
	: m_pixels(NULL)
{
	sce_paf_memset(&m_layoutCode, 0, 0x2Cu);
}

PictureRgbSourceBase::~PictureRgbSourceBase()
{
	Term(false);
}

/* 0x8136A208 */
int PictureRgbSourceBase::Init(const void *pixels, uint32_t bufWidth, uint32_t bufHeight,
			       uint32_t pitchBytes, uint32_t cropX, uint32_t cropY,
			       uint32_t cropWidth, uint32_t cropHeight, int layoutCode)
{
	if (pixels == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	int ret = InitRegion(bufWidth, bufHeight, cropX, cropY, cropWidth, cropHeight);
	if (ret != 0)
		return ret;

	uint32_t hasAlpha = 0;
	uint32_t bytesPerPixel = 1;
	uint32_t alphaOff = 0;
	uint32_t redOff = 0;
	uint32_t greenOff = 0;
	uint32_t blueOff = 0;

	switch (layoutCode) {
	case PIC_RGB_LAYOUT_BGR888:
		bytesPerPixel = 3;
		redOff = 2;
		greenOff = 1;
		blueOff = 0;
		break;
	case PIC_RGB_LAYOUT_RGB888:
		bytesPerPixel = 3;
		redOff = 0;
		greenOff = 1;
		blueOff = 2;
		break;
	case PIC_RGB_LAYOUT_ARGB8888:
		bytesPerPixel = 4;
		hasAlpha = 1;
		alphaOff = 0;
		redOff = 3;
		greenOff = 2;
		blueOff = 1;
		break;
	case PIC_RGB_LAYOUT_ABGR8888:
		bytesPerPixel = 4;
		hasAlpha = 1;
		alphaOff = 3;
		redOff = 0;
		greenOff = 1;
		blueOff = 2;
		break;
	case PIC_RGB_LAYOUT_BGRA8888:
		bytesPerPixel = 4;
		hasAlpha = 1;
		alphaOff = 0;
		redOff = 1;
		greenOff = 2;
		blueOff = 3;
		break;
	case PIC_RGB_LAYOUT_RGBA8888:
		bytesPerPixel = 4;
		hasAlpha = 1;
		alphaOff = 3;
		redOff = 2;
		greenOff = 1;
		blueOff = 0;
		break;
	default:
		break;
	}

	m_pixels = static_cast<const uint8_t *>(pixels);
	m_layoutCode = layoutCode;
	m_bytesPerPixel = bytesPerPixel;
	m_hasAlpha = hasAlpha;
	m_alphaByteOffset = alphaOff;
	m_redByteOffset = redOff;
	m_greenByteOffset = greenOff;
	m_blueByteOffset = blueOff;
	m_sourceWidth = bufWidth;
	m_sourceHeight = bufHeight;
	m_pitchBytes = pitchBytes;
	m_bufferByteSize = pitchBytes * bufHeight;

	if (bytesPerPixel * bufWidth > pitchBytes) {
		Term(false);
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	}
	return 0;
}

/* 0x8136A324 */
void PictureRgbSourceBase::Term(bool deleteChild)
{
	m_pixels = NULL;
	sce_paf_memset(&m_layoutCode, 0, 0x2Cu);
	PictureBufferSource::Term(deleteChild);
}

/* 0x8136A1A4 */
PictureRgbToRgbSource::PictureRgbToRgbSource()
{
}

/* 0x8136A1CC / 0x8136A1E8 */
PictureRgbToRgbSource::~PictureRgbToRgbSource()
{
	Term(false);
}

int PictureRgbToRgbSource::GetColorSpace() const
{
	return PIC_COLOR_SPACE_RGB;
}

/* 0x8136A344 */
uint32_t PictureRgbToRgbSource::GetDefaultPixel() const
{
	return picturePackPixel(0, 0, 0, 0);
}

/* 0x8136A372 */
uint32_t PictureRgbToRgbSource::GetPixelAt(uint32_t posX, uint32_t posY)
{
	const uint8_t *px = m_pixels + posY * m_pitchBytes + posX * m_bytesPerPixel;
	uint8_t alpha = (m_hasAlpha != 0) ? px[m_alphaByteOffset] : 0xFFu;
	return picturePackPixel(px[m_redByteOffset], px[m_greenByteOffset],
				px[m_blueByteOffset], alpha);
}

/* 0x8136A3BC */
PictureRgbToYuvSource::PictureRgbToYuvSource()
{
}

/* 0x8136A3E4 / 0x8136A400 */
PictureRgbToYuvSource::~PictureRgbToYuvSource()
{
	Term(false);
}

int PictureRgbToYuvSource::GetColorSpace() const
{
	return PIC_COLOR_SPACE_YUV;
}

/* 0x8136A420 */
uint32_t PictureRgbToYuvSource::GetDefaultPixel() const
{
	return picturePackPixel(16, 128, 128, 0);
}

/* 0x8136A44E */
uint32_t PictureRgbToYuvSource::GetPixelAt(uint32_t posX, uint32_t posY)
{
	const uint8_t *px = m_pixels + posY * m_pitchBytes + posX * m_bytesPerPixel;
	uint8_t alpha = (m_hasAlpha != 0) ? px[m_alphaByteOffset] : 0xFFu;
	int red = static_cast<int>(px[m_redByteOffset]);
	int green = static_cast<int>(px[m_greenByteOffset]);
	int blue = static_cast<int>(px[m_blueByteOffset]);

	uint8_t luma = static_cast<uint8_t>(
		static_cast<unsigned int>(306 * red + 601 * green + 117 * blue) >> 10);
	uint8_t chromaB = static_cast<uint8_t>(
		static_cast<unsigned int>(-173 * red - 339 * green + (blue << 9) + 0x20000) >> 10);
	uint8_t chromaR = static_cast<uint8_t>(
		static_cast<unsigned int>((red << 9) - 429 * green - 83 * blue + 0x20000) >> 10);

	return picturePackPixel(luma, chromaB, chromaR, alpha);
}

/* ============================================================================
 * PictureYuvSourceBase / PictureYuvToRgbSource / PictureYuvToYuvSource
 * (0x8136B390 - 0x8136B8B7)
 * ============================================================================ */

PictureYuvSourceBase::PictureYuvSourceBase()
	: m_yPlane(NULL), m_cbPlane(NULL), m_crPlane(NULL), m_colorRange(0)
{
	sce_paf_memset(&m_subsampling, 0, 0x28u);
}

PictureYuvSourceBase::~PictureYuvSourceBase()
{
	Term(false);
}

/* 0x8136B3FC */
int PictureYuvSourceBase::Init(const void *pixels, uint32_t bufWidth, uint32_t bufHeight,
			       uint32_t pitchBytes, uint32_t cropX, uint32_t cropY,
			       uint32_t cropWidth, uint32_t cropHeight, int subsampling,
			       int colorRange)
{
	(void)pitchBytes;
	if (pixels == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	int ret = InitRegion(bufWidth, bufHeight, cropX, cropY, cropWidth, cropHeight);
	if (ret != 0)
		return ret;

	uint32_t divX = 1;
	uint32_t divY = 1;
	getYuvChromaDivisors(subsampling, &divX, &divY);

	uint32_t yStride = (bufWidth + 7u) & ~7u;
	uint32_t uvStride = ((yStride / divX) + 7u) & ~7u;
	uint32_t uvHeight = bufHeight / divY;
	uint32_t yPlaneSize = yStride * bufHeight;
	uint32_t uvPlaneSize = uvStride * uvHeight;

	m_subsampling = subsampling;
	m_chromaDivX = divX;
	m_chromaDivY = divY;
	m_yStride = yStride;
	m_yHeight = bufHeight;
	m_uvStride = uvStride;
	m_uvHeight = uvHeight;
	m_yPlaneSize = yPlaneSize;
	m_uvPlaneSize = uvPlaneSize;
	m_totalSize = yPlaneSize + 2u * uvPlaneSize;
	m_colorRange = colorRange;

	const uint8_t *base = static_cast<const uint8_t *>(pixels);
	m_yPlane = base;
	m_cbPlane = base + yPlaneSize;
	m_crPlane = m_cbPlane + uvPlaneSize;
	return 0;
}

/* 0x8136B4F2 */
void PictureYuvSourceBase::Term(bool deleteChild)
{
	m_yPlane = NULL;
	m_cbPlane = NULL;
	m_crPlane = NULL;
	sce_paf_memset(&m_subsampling, 0, 0x28u);
	PictureBufferSource::Term(deleteChild);
}

/* 0x8136B390 */
PictureYuvToRgbSource::PictureYuvToRgbSource()
{
}

/* 0x8136B3C0 / 0x8136B3DC */
PictureYuvToRgbSource::~PictureYuvToRgbSource()
{
	Term(false);
}

int PictureYuvToRgbSource::GetColorSpace() const
{
	return PIC_COLOR_SPACE_RGB;
}

uint32_t PictureYuvToRgbSource::GetDefaultPixel() const
{
	return picturePackPixel(0, 0, 0, 0);
}

/* 0x8136B518 */
uint32_t PictureYuvToRgbSource::ConvertYuvToRgb(uint32_t yuvPixel)
{
	double luma = static_cast<double>(yuvPixel & 0xFFu);
	double chromaB = static_cast<double>(static_cast<int>((yuvPixel >> 8) & 0xFFu) - 128);
	double chromaR = static_cast<double>(static_cast<int>((yuvPixel >> 16) & 0xFFu) - 128);
	uint8_t alpha = static_cast<uint8_t>((yuvPixel >> 24) & 0xFFu);

	int red = static_cast<int>(luma + chromaR * 1.402);
	int green = static_cast<int>(luma - chromaB * 0.344 - chromaR * 0.714);
	int blue = static_cast<int>(luma + chromaB * 1.772);

	return picturePackPixel(clampByteInt(red), clampByteInt(green), clampByteInt(blue), alpha);
}

/* 0x8136B634 */
uint32_t PictureYuvToRgbSource::GetPixelAt(uint32_t posX, uint32_t posY)
{
	uint32_t uvIdx = (posX / m_chromaDivX) + (posY / m_chromaDivY) * m_uvStride;
	uint8_t luma = m_yPlane[posY * m_yStride + posX];
	uint8_t chromaB = m_cbPlane[uvIdx];
	uint8_t chromaR = m_crPlane[uvIdx];

	if (m_colorRange == 1) {
		luma = clampByteInt((static_cast<int>(luma) * 255 - 4080) / 219);
		chromaB = clampByteInt((static_cast<int>(chromaB) * 255 - 4080) / 224);
		chromaR = clampByteInt((static_cast<int>(chromaR) * 255 - 4080) / 224);
	}

	return ConvertYuvToRgb(picturePackPixel(luma, chromaB, chromaR, 0xFFu));
}

/* 0x8136B770 */
PictureYuvToYuvSource::PictureYuvToYuvSource()
{
}

/* 0x8136B7A0 / 0x8136B7BC */
PictureYuvToYuvSource::~PictureYuvToYuvSource()
{
	Term(false);
}

int PictureYuvToYuvSource::GetColorSpace() const
{
	return PIC_COLOR_SPACE_YUV;
}

uint32_t PictureYuvToYuvSource::GetDefaultPixel() const
{
	return picturePackPixel(16, 128, 128, 0);
}

/* 0x8136B7DC */
uint32_t PictureYuvToYuvSource::GetPixelAt(uint32_t posX, uint32_t posY)
{
	uint32_t uvIdx = (posX / m_chromaDivX) + (posY / m_chromaDivY) * m_uvStride;
	uint8_t luma = m_yPlane[posY * m_yStride + posX];
	uint8_t chromaB = m_cbPlane[uvIdx];
	uint8_t chromaR = m_crPlane[uvIdx];

	if (m_colorRange == 1) {
		int adjY = (luma > 16u) ? static_cast<int>(luma - 16u) : 0;
		int adjCb = (chromaB > 16u) ? static_cast<int>(chromaB - 16u) : 0;
		int adjCr = (chromaR > 16u) ? static_cast<int>(chromaR - 16u) : 0;
		luma = clampByteInt((adjY * 298 + 128) >> 8);
		chromaB = clampByteInt((adjCb * 291 + 128) >> 8);
		chromaR = clampByteInt((adjCr * 291 + 128) >> 8);
	}

	return picturePackPixel(luma, chromaB, chromaR, 0xFFu);
}

/* ============================================================================
 * PicturePixelSink & PictureYuvMcuSink (0x8136CD70 - 0x8136CEE7, 0x8136B8B8 - 0x8136BAC3)
 * ============================================================================ */

/* 0x8136CD70 */
PicturePixelSink::PicturePixelSink()
	: m_width(0),
	  m_height(0),
	  m_bufWidth(0),
	  m_bufHeight(0),
	  m_cropX(0),
	  m_cropY(0),
	  m_cropWidth(0),
	  m_cropHeight(0),
	  m_initialized(false)
{
}

/* 0x8136CDA8 / 0x8136CD94 */
PicturePixelSink::~PicturePixelSink()
{
	Term();
}

/* 0x8136CDB6 */
int PicturePixelSink::InitBase(uint32_t bufWidth, uint32_t bufHeight, uint32_t cropX,
			       uint32_t cropY, uint32_t cropWidth, uint32_t cropHeight)
{
	if (m_initialized)
		return PIC_CREATOR_ERROR_INVALID_STATE;
	if (cropX + cropWidth > bufWidth || cropY + cropHeight > bufHeight ||
	    cropWidth == 0 || cropHeight == 0 || bufWidth == 0 || bufHeight == 0)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	m_width = cropWidth;
	m_height = cropHeight;
	m_bufWidth = bufWidth;
	m_bufHeight = bufHeight;
	m_cropX = cropX;
	m_cropY = cropY;
	m_cropWidth = cropWidth;
	m_cropHeight = cropHeight;
	m_initialized = true;
	return 0;
}

/* 0x8136CE26 */
void PicturePixelSink::Term()
{
	if (m_initialized) {
		m_initialized = false;
		m_width = 0;
		m_height = 0;
		m_bufWidth = 0;
		m_bufHeight = 0;
		m_cropX = 0;
		m_cropY = 0;
		m_cropWidth = 0;
		m_cropHeight = 0;
	}
}

bool PicturePixelSink::IsInitialized() const
{
	return m_initialized;
}

PictureSize PicturePixelSink::GetSize() const
{
	PictureSize size;
	size.width = m_width;
	size.height = m_height;
	return size;
}

/* 0x8136CE46 */
void PicturePixelSink::SetPixel(uint32_t posX, uint32_t posY, uint32_t pixel)
{
	if (IsInitialized() && posX >= m_cropX && posX < m_cropX + m_cropWidth &&
	    posY >= m_cropY && posY < m_cropY + m_cropHeight) {
		SetPixelAt(posX + m_cropX, posY + m_cropY, pixel);
	}
}

/* 0x8136B8B8 */
PictureYuvMcuSink::PictureYuvMcuSink()
	: m_yPlane(NULL), m_cbPlane(NULL), m_crPlane(NULL)
{
	sce_paf_memset(&m_subsampling, 0, 0x28u);
}

/* 0x8136B8E4 / 0x8136B900 */
PictureYuvMcuSink::~PictureYuvMcuSink()
{
	Term();
}

/* 0x8136B920 */
int PictureYuvMcuSink::Init(void *buffer, uint32_t bufWidth, uint32_t bufHeight,
			    uint32_t pitchBytes, uint32_t cropX, uint32_t cropY,
			    uint32_t cropWidth, uint32_t cropHeight, int subsampling)
{
	(void)pitchBytes;
	if (buffer == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	int ret = InitBase(bufWidth, bufHeight, cropX, cropY, cropWidth, cropHeight);
	if (ret != 0)
		return ret;

	uint32_t divX = 1;
	uint32_t divY = 1;
	getYuvChromaDivisors(subsampling, &divX, &divY);

	uint32_t yStride = (bufWidth + 7u) & ~7u;
	uint32_t uvStride = ((yStride / divX) + 7u) & ~7u;
	uint32_t uvHeight = bufHeight / divY;
	uint32_t yPlaneSize = yStride * bufHeight;
	uint32_t uvPlaneSize = uvStride * uvHeight;

	m_subsampling = subsampling;
	m_chromaDivX = divX;
	m_chromaDivY = divY;
	m_yStride = yStride;
	m_yHeight = bufHeight;
	m_uvStride = uvStride;
	m_uvHeight = uvHeight;
	m_yPlaneSize = yPlaneSize;
	m_uvPlaneSize = uvPlaneSize;
	m_totalSize = yPlaneSize + 2u * uvPlaneSize;

	uint8_t *base = static_cast<uint8_t *>(buffer);
	m_yPlane = base;
	m_cbPlane = base + yPlaneSize;
	m_crPlane = m_cbPlane + uvPlaneSize;
	return 0;
}

/* 0x8136BA20 */
void PictureYuvMcuSink::Term()
{
	m_yPlane = NULL;
	m_cbPlane = NULL;
	m_crPlane = NULL;
	sce_paf_memset(&m_subsampling, 0, 0x28u);
	PicturePixelSink::Term();
}

int PictureYuvMcuSink::GetColorSpace() const
{
	return PIC_COLOR_SPACE_YUV;
}

int PictureYuvMcuSink::GetSubsampling() const
{
	return m_subsampling;
}

/* 0x8136BA40 */
void PictureYuvMcuSink::Clear()
{
	if (IsInitialized()) {
		sce_paf_memset(m_yPlane, 16, m_yPlaneSize);
		sce_paf_memset(m_cbPlane, 128, m_uvPlaneSize);
		sce_paf_memset(m_crPlane, 128, m_uvPlaneSize);
	}
}

/* 0x8136BA6E */
void PictureYuvMcuSink::SetPixelAt(uint32_t posX, uint32_t posY, uint32_t pixel)
{
	uint32_t uvIdx = (posX / m_chromaDivX) + (posY / m_chromaDivY) * m_uvStride;
	m_yPlane[posY * m_yStride + posX] = static_cast<uint8_t>(pixel & 0xFFu);
	m_cbPlane[uvIdx] = static_cast<uint8_t>((pixel >> 8) & 0xFFu);
	m_crPlane[uvIdx] = static_cast<uint8_t>((pixel >> 16) & 0xFFu);
}

uint8_t *PictureYuvMcuSink::GetYPlane() const
{
	return m_yPlane;
}

uint8_t *PictureYuvMcuSink::GetCbPlane() const
{
	return m_cbPlane;
}

uint8_t *PictureYuvMcuSink::GetCrPlane() const
{
	return m_crPlane;
}

/* ============================================================================
 * PictureMcuFeederBase & 3 Concrete Feeders (0x8136CCCC - 0x8136CD6F,
 *                                            0x8136A4E8 - 0x8136B38F)
 * ============================================================================ */

/* 0x8136CCCC */
PictureMcuFeederBase::PictureMcuFeederBase()
	: m_initialized(false), m_sink(NULL)
{
}

/* 0x8136CCE0 / 0x8136CCEE */
PictureMcuFeederBase::~PictureMcuFeederBase()
{
}

/* 0x8136CD04 */
int PictureMcuFeederBase::InitBase()
{
	if (IsInitialized())
		return PIC_CREATOR_ERROR_INVALID_STATE;
	m_initialized = true;
	return 0;
}

/* 0x8136CD24 */
void PictureMcuFeederBase::TermBase()
{
	m_initialized = false;
	m_sink = NULL;
}

bool PictureMcuFeederBase::IsInitialized() const
{
	return m_initialized;
}

bool PictureMcuFeederBase::IsReady() const
{
	return (m_initialized && m_sink != NULL);
}

/* 0x8136CD2E */
int PictureMcuFeederBase::BindSink(PictureYuvMcuSink *sink)
{
	if (!IsInitialized())
		return PIC_CREATOR_ERROR_INVALID_STATE;
	if (sink != NULL && !CanBindSink(sink))
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	m_sink = sink;
	return 0;
}

/* --- PictureFastRgbaMcuFeeder (0x8136A4E8 - 0x8136A8E3) --- */

/* 0x8136A4E8 */
PictureFastRgbaMcuFeeder::PictureFastRgbaMcuFeeder()
	: m_source(NULL)
{
}

/* 0x8136A504 / 0x8136A520 */
PictureFastRgbaMcuFeeder::~PictureFastRgbaMcuFeeder()
{
	Term();
}

/* 0x8136A540 */
bool PictureFastRgbaMcuFeeder::CanUse(const PictureSourceInfo *source)
{
	if (!validateSourceInfo(source))
		return false;
	if (source->pixelFormat != 4)
		return false;
	if (source->cropX != 0 || source->cropY != 0)
		return false;
	bool uncroppedZero = (source->cropWidth == 0 && source->cropHeight == 0);
	bool uncroppedFull = (source->cropWidth == source->width &&
			      source->cropHeight == source->height);
	return (uncroppedZero || uncroppedFull);
}

/* 0x8136A60E */
int PictureFastRgbaMcuFeeder::Init(const PictureSourceInfo *source)
{
	if (!CanUse(source))
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	int ret = InitBase();
	if (ret == 0)
		m_source = source;
	return ret;
}

/* 0x8136A84A */
bool PictureFastRgbaMcuFeeder::CanBindSink(PictureYuvMcuSink *sink)
{
	if (sink == NULL || !sink->IsInitialized() ||
	    sink->GetColorSpace() != PIC_COLOR_SPACE_YUV ||
	    sink->GetSubsampling() != PIC_YUV_420)
		return false;

	PictureSize srcSize = GetSourceSize();
	PictureSize sinkSize = sink->GetSize();
	if (srcSize.width == 0 || srcSize.height == 0 ||
	    sinkSize.width == 0 || sinkSize.height == 0)
		return false;

	return ((srcSize.width % sinkSize.width) == 0 &&
		(srcSize.height % sinkSize.height) == 0 &&
		(sinkSize.width & 7u) == 0 &&
		(sinkSize.height & 1u) == 0);
}

/* 0x8136AF3C */
int PictureFastRgbaMcuFeeder::Term()
{
	TermBase();
	m_source = NULL;
	return 0;
}

/* 0x8136A63C */
PictureSize PictureFastRgbaMcuFeeder::GetSourceSize() const
{
	PictureSize size;
	if (m_source != NULL) {
		size.width = m_source->width;
		size.height = m_source->height;
	} else {
		size.width = 0;
		size.height = 0;
	}
	return size;
}

/* 0x8136A652 */
int PictureFastRgbaMcuFeeder::FetchNextBlock(PicturePoint *cursor)
{
	if (!IsReady())
		return PIC_CREATOR_ERROR_INVALID_STATE;
	if (cursor == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	PictureSize sinkSize = m_sink->GetSize();
	PictureSize srcSize = GetSourceSize();
	uint8_t *yDst = m_sink->GetYPlane();
	uint8_t *cbDst = m_sink->GetCbPlane();
	uint8_t *crDst = m_sink->GetCrPlane();

	const uint8_t *srcBase = static_cast<const uint8_t *>(m_source->pixelBuffer);
	uint32_t pitch = m_source->pitchBytes;

	for (uint32_t row = 0; row < sinkSize.height; row += 2u) {
		const uint8_t *rowEven = srcBase + (cursor->posY + row) * pitch + cursor->posX * 4u;
		const uint8_t *rowOdd = rowEven + pitch;
		uint8_t *yEven = yDst + row * sinkSize.width;
		uint8_t *yOdd = yEven + sinkSize.width;
		uint8_t *cbRow = cbDst + (row >> 1) * (sinkSize.width >> 1);
		uint8_t *crRow = crDst + (row >> 1) * (sinkSize.width >> 1);

		for (uint32_t col = 0; col < sinkSize.width; col += 2u) {
			const uint8_t *pxTL = rowEven + col * 4u;
			const uint8_t *pxTR = pxTL + 4u;
			const uint8_t *pxBL = rowOdd + col * 4u;
			const uint8_t *pxBR = pxBL + 4u;

			yEven[col] = clampByteInt(
				(77 * pxTL[0] + 150 * pxTL[1] + 29 * pxTL[2] + 128) >> 8);
			yEven[col + 1u] = clampByteInt(
				(77 * pxTR[0] + 150 * pxTR[1] + 29 * pxTR[2] + 128) >> 8);
			yOdd[col] = clampByteInt(
				(77 * pxBL[0] + 150 * pxBL[1] + 29 * pxBL[2] + 128) >> 8);
			yOdd[col + 1u] = clampByteInt(
				(77 * pxBR[0] + 150 * pxBR[1] + 29 * pxBR[2] + 128) >> 8);

			int avgR = (pxTL[0] + pxTR[0] + pxBL[0] + pxBR[0] + 2) >> 2;
			int avgG = (pxTL[1] + pxTR[1] + pxBL[1] + pxBR[1] + 2) >> 2;
			int avgB = (pxTL[2] + pxTR[2] + pxBL[2] + pxBR[2] + 2) >> 2;

			cbRow[col >> 1] = clampByteInt(
				((128 * avgB - 43 * avgR - 85 * avgG + 0x8000) + 128) >> 8);
			crRow[col >> 1] = clampByteInt(
				((128 * avgR - 107 * avgG - 21 * avgB + 0x8000) + 128) >> 8);
		}
	}

	cursor->posX += sinkSize.width;
	if (cursor->posX >= srcSize.width) {
		cursor->posX = 0;
		cursor->posY += sinkSize.height;
	}
	return 0;
}

/* --- PictureFastYuvMcuFeeder (0x8136A8E4 - 0x8136AE93) --- */

/* 0x8136A8E4 */
PictureFastYuvMcuFeeder::PictureFastYuvMcuFeeder()
	: m_source(NULL),
	  m_cropWidth(0),
	  m_cropHeight(0),
	  m_yStart(NULL),
	  m_cbStart(NULL),
	  m_crStart(NULL)
{
	sce_paf_memset(&m_subsampling, 0, 0x28u);
}

/* 0x8136A918 / 0x8136A934 */
PictureFastYuvMcuFeeder::~PictureFastYuvMcuFeeder()
{
	Term();
}

/* 0x8136A954 */
bool PictureFastYuvMcuFeeder::CanUse(const PictureSourceInfo *source)
{
	return (validateSourceInfo(source) && source->pixelFormat == 0x1C);
}

/* 0x8136A9F8 */
int PictureFastYuvMcuFeeder::Init(const PictureSourceInfo *source)
{
	if (!CanUse(source))
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	int ret = InitBase();
	if (ret != 0)
		return ret;

	uint32_t width = source->width;
	uint32_t height = source->height;
	uint32_t yStride = (width + 7u) & ~7u;
	uint32_t uvStride = ((yStride / 2u) + 7u) & ~7u;
	uint32_t uvHeight = height / 2u;
	uint32_t yPlaneSize = yStride * height;
	uint32_t uvPlaneSize = uvStride * uvHeight;

	m_source = source;
	m_subsampling = PIC_YUV_420;
	m_chromaDivX = 2;
	m_chromaDivY = 2;
	m_yStride = yStride;
	m_yHeight = height;
	m_uvStride = uvStride;
	m_uvHeight = uvHeight;
	m_yPlaneSize = yPlaneSize;
	m_uvPlaneSize = uvPlaneSize;
	m_totalSize = yPlaneSize + 2u * uvPlaneSize;

	uint32_t cropX = 0;
	uint32_t cropY = 0;
	if (source->cropWidth != 0 && source->cropHeight != 0) {
		cropX = source->cropX;
		cropY = source->cropY;
		m_cropWidth = source->cropWidth;
		m_cropHeight = source->cropHeight;
	} else {
		m_cropWidth = width;
		m_cropHeight = height;
	}

	const uint8_t *base = static_cast<const uint8_t *>(source->pixelBuffer);
	uint32_t uvOffset = (cropY / 2u) * uvStride + (cropX / 2u);
	m_yStart = base + cropY * yStride + cropX;
	m_cbStart = base + yPlaneSize + uvOffset;
	m_crStart = base + yPlaneSize + uvPlaneSize + uvOffset;
	return 0;
}

/* 0x8136AAE8 */
int PictureFastYuvMcuFeeder::Term()
{
	TermBase();
	m_source = NULL;
	m_cropWidth = 0;
	m_cropHeight = 0;
	m_yStart = NULL;
	m_cbStart = NULL;
	m_crStart = NULL;
	sce_paf_memset(&m_subsampling, 0, 0x28u);
	return 0;
}

/* 0x8136AB0C */
PictureSize PictureFastYuvMcuFeeder::GetSourceSize() const
{
	PictureSize size;
	size.width = m_cropWidth;
	size.height = m_cropHeight;
	return size;
}

/* 0x8136AE00 */
bool PictureFastYuvMcuFeeder::CanBindSink(PictureYuvMcuSink *sink)
{
	if (sink == NULL || !sink->IsInitialized() ||
	    sink->GetColorSpace() != PIC_COLOR_SPACE_YUV ||
	    sink->GetSubsampling() != PIC_YUV_420)
		return false;

	PictureSize srcSize = GetSourceSize();
	PictureSize sinkSize = sink->GetSize();
	if (srcSize.width == 0 || srcSize.height == 0 ||
	    sinkSize.width == 0 || sinkSize.height == 0)
		return false;

	return ((srcSize.width % sinkSize.width) == 0 &&
		(srcSize.height & 1u) == 0 &&
		(sinkSize.width & 0xFu) == 0 &&
		(sinkSize.height & 1u) == 0);
}

/* 0x8136AB16 */
int PictureFastYuvMcuFeeder::FetchNextBlock(PicturePoint *cursor)
{
	if (!IsReady())
		return PIC_CREATOR_ERROR_INVALID_STATE;
	if (cursor == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	PictureSize sinkSize = m_sink->GetSize();
	PictureSize srcSize = GetSourceSize();
	uint8_t *yDst = m_sink->GetYPlane();
	uint8_t *cbDst = m_sink->GetCbPlane();
	uint8_t *crDst = m_sink->GetCrPlane();

	uint32_t copyHeight = srcSize.height - cursor->posY;
	if (sinkSize.height < copyHeight)
		copyHeight = sinkSize.height;
	if (copyHeight != sinkSize.height)
		m_sink->Clear();

	if (m_source->reserved == 1u) {
		for (uint32_t row = 0; row < copyHeight; row += 2u) {
			uint32_t srcY = cursor->posY + row;
			for (uint32_t col = 0; col < sinkSize.width; col += 16u) {
				uint32_t srcX = cursor->posX + col;
				const uint8_t *srcYEven = m_yStart + srcY * m_yStride + srcX;
				const uint8_t *srcYOdd = srcYEven + m_yStride;
				uint8_t *dstYEven = yDst + row * sinkSize.width + col;
				uint8_t *dstYOdd = dstYEven + sinkSize.width;

				for (uint32_t step = 0; step < 16u; ++step) {
					int adjEven = (srcYEven[step] > 16u)
							      ? static_cast<int>(srcYEven[step] - 16u)
							      : 0;
					int adjOdd = (srcYOdd[step] > 16u)
							     ? static_cast<int>(srcYOdd[step] - 16u)
							     : 0;
					dstYEven[step] = clampByteInt((adjEven * 298 + 128) >> 8);
					dstYOdd[step] = clampByteInt((adjOdd * 298 + 128) >> 8);
				}

				uint32_t srcUvOff = (srcY >> 1) * m_uvStride + (srcX >> 1);
				uint32_t dstUvOff = (row >> 1) * (sinkSize.width >> 1) + (col >> 1);
				for (uint32_t step = 0; step < 8u; ++step) {
					int adjCb = (m_cbStart[srcUvOff + step] > 16u)
							    ? static_cast<int>(
								      m_cbStart[srcUvOff + step] - 16u)
							    : 0;
					int adjCr = (m_crStart[srcUvOff + step] > 16u)
							    ? static_cast<int>(
								      m_crStart[srcUvOff + step] - 16u)
							    : 0;
					cbDst[dstUvOff + step] =
						clampByteInt((adjCb * 291 + 128) >> 8);
					crDst[dstUvOff + step] =
						clampByteInt((adjCr * 291 + 128) >> 8);
				}
			}
		}
	} else {
		for (uint32_t row = 0; row < copyHeight; row += 2u) {
			uint32_t srcY = cursor->posY + row;
			for (uint32_t col = 0; col < sinkSize.width; col += 16u) {
				uint32_t srcX = cursor->posX + col;
				const uint8_t *srcYEven = m_yStart + srcY * m_yStride + srcX;
				uint8_t *dstYEven = yDst + row * sinkSize.width + col;
				uint32_t srcUvOff = (srcY >> 1) * m_uvStride + (srcX >> 1);
				uint32_t dstUvOff = (row >> 1) * (sinkSize.width >> 1) + (col >> 1);

				sce_paf_memcpy(dstYEven, srcYEven, 16u);
				sce_paf_memcpy(dstYEven + sinkSize.width, srcYEven + m_yStride, 16u);
				sce_paf_memcpy(cbDst + dstUvOff, m_cbStart + srcUvOff, 8u);
				sce_paf_memcpy(crDst + dstUvOff, m_crStart + srcUvOff, 8u);
			}
		}
	}

	cursor->posX += sinkSize.width;
	if (cursor->posX >= srcSize.width) {
		cursor->posX = 0;
		cursor->posY += sinkSize.height;
	}
	return 0;
}

/* --- PictureGenericMcuFeeder (0x8136AE94 - 0x8136B38F) --- */

/* 0x8136AE94 */
PictureGenericMcuFeeder::PictureGenericMcuFeeder()
	: m_source(NULL)
{
}

/* 0x8136AEB0 / 0x8136AECC */
PictureGenericMcuFeeder::~PictureGenericMcuFeeder()
{
	Term();
}

/* 0x8136AEEC */
int PictureGenericMcuFeeder::Init(PicturePixelSource *source)
{
	if (source == NULL || !source->IsInitialized() ||
	    source->GetColorSpace() != PIC_COLOR_SPACE_YUV)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	int ret = InitBase();
	if (ret == 0)
		m_source = source;
	return ret;
}

/* 0x8136AF3C */
int PictureGenericMcuFeeder::Term()
{
	TermBase();
	m_source = NULL;
	return 0;
}

/* 0x8136B360 */
bool PictureGenericMcuFeeder::CanBindSink(PictureYuvMcuSink *sink)
{
	if (sink == NULL || !sink->IsInitialized())
		return false;
	return (sink->GetColorSpace() == PIC_COLOR_SPACE_YUV);
}

/* 0x8136AF4A */
PictureSize PictureGenericMcuFeeder::GetSourceSize() const
{
	PictureSize size;
	if (m_source != NULL && m_source->IsInitialized()) {
		size = m_source->GetSize();
	} else {
		size.width = 0;
		size.height = 0;
	}
	return size;
}

/* 0x8136AF98 */
int PictureGenericMcuFeeder::FetchNextBlock(PicturePoint *cursor)
{
	if (!IsReady())
		return PIC_CREATOR_ERROR_INVALID_STATE;
	if (cursor == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	PictureSize sinkSize = m_sink->GetSize();
	PictureSize srcSize = m_source->GetSize();
	m_sink->Clear();

	int subsampling = m_sink->GetSubsampling();
	if (subsampling == PIC_YUV_420) {
		for (uint32_t row = 0; row < sinkSize.height; row += 2u) {
			uint32_t srcY = cursor->posY + row;
			for (uint32_t col = 0; col < sinkSize.width; col += 2u) {
				uint32_t srcX = cursor->posX + col;
				uint32_t topLeft = m_source->GetPixel(srcX, srcY);
				uint32_t topRight = m_source->GetPixel(srcX + 1u, srcY);
				uint32_t botLeft = m_source->GetPixel(srcX, srcY + 1u);
				uint32_t botRight = m_source->GetPixel(srcX + 1u, srcY + 1u);

				uint8_t avgCb = static_cast<uint8_t>(
					(((topLeft >> 8) & 0xFFu) + ((topRight >> 8) & 0xFFu) +
					 ((botLeft >> 8) & 0xFFu) + ((botRight >> 8) & 0xFFu)) >> 2);
				uint8_t avgCr = static_cast<uint8_t>(
					(((topLeft >> 16) & 0xFFu) + ((topRight >> 16) & 0xFFu) +
					 ((botLeft >> 16) & 0xFFu) + ((botRight >> 16) & 0xFFu)) >> 2);
				uint32_t avgBotRight = picturePackPixel(
					static_cast<uint8_t>(botRight & 0xFFu), avgCb, avgCr, 0xFFu);

				m_sink->SetPixel(col, row, topLeft);
				m_sink->SetPixel(col + 1u, row, topRight);
				m_sink->SetPixel(col, row + 1u, botLeft);
				m_sink->SetPixel(col + 1u, row + 1u, avgBotRight);
			}
		}
	} else if (subsampling == PIC_YUV_422) {
		for (uint32_t row = 0; row < sinkSize.height; ++row) {
			uint32_t srcY = cursor->posY + row;
			for (uint32_t col = 0; col < sinkSize.width; col += 2u) {
				uint32_t srcX = cursor->posX + col;
				uint32_t leftPx = m_source->GetPixel(srcX, srcY);
				uint32_t rightPx = m_source->GetPixel(srcX + 1u, srcY);

				uint8_t avgCb = static_cast<uint8_t>(
					(((leftPx >> 8) & 0xFFu) + ((rightPx >> 8) & 0xFFu)) >> 1);
				uint8_t avgCr = static_cast<uint8_t>(
					(((leftPx >> 16) & 0xFFu) + ((rightPx >> 16) & 0xFFu)) >> 1);
				uint32_t avgRight = picturePackPixel(
					static_cast<uint8_t>(rightPx & 0xFFu), avgCb, avgCr, 0xFFu);

				m_sink->SetPixel(col, row, leftPx);
				m_sink->SetPixel(col + 1u, row, avgRight);
			}
		}
	} else {
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	}

	cursor->posX += sinkSize.width;
	if (cursor->posX >= srcSize.width) {
		cursor->posX = 0;
		cursor->posY += sinkSize.height;
	}
	return 0;
}

/* ============================================================================
 * Pipeline Source Factories (0x81369914, 0x81369C98)
 * ============================================================================ */

/* 0x81369914 */
static int createPictureSource(const PictureSourceInfo *source, int targetColorSpace,
			       PicturePixelSource **outSource)
{
	if (!validateSourceInfo(source) || outSource == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	int srcColorSpace = PIC_COLOR_SPACE_NONE;
	int layoutOrSubsampling = 0;

	switch (source->pixelFormat) {
	case 3:
		srcColorSpace = PIC_COLOR_SPACE_RGB;
		layoutOrSubsampling = PIC_RGB_LAYOUT_ARGB8888;
		break;
	case 4:
		srcColorSpace = PIC_COLOR_SPACE_RGB;
		layoutOrSubsampling = PIC_RGB_LAYOUT_ABGR8888;
		break;
	case 5:
		srcColorSpace = PIC_COLOR_SPACE_RGB;
		layoutOrSubsampling = PIC_RGB_LAYOUT_BGRA8888;
		break;
	case 6:
		srcColorSpace = PIC_COLOR_SPACE_RGB;
		layoutOrSubsampling = PIC_RGB_LAYOUT_RGBA8888;
		break;
	case 0x19:
		srcColorSpace = PIC_COLOR_SPACE_RGB;
		layoutOrSubsampling = PIC_RGB_LAYOUT_BGR888;
		break;
	case 0x1A:
		srcColorSpace = PIC_COLOR_SPACE_RGB;
		layoutOrSubsampling = PIC_RGB_LAYOUT_RGB888;
		break;
	case 0x1C:
		srcColorSpace = PIC_COLOR_SPACE_YUV;
		layoutOrSubsampling = PIC_YUV_420;
		break;
	default:
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	}

	uint32_t cropX = 0;
	uint32_t cropY = 0;
	uint32_t cropW = source->width;
	uint32_t cropH = source->height;
	if (source->cropWidth != 0 && source->cropHeight != 0) {
		cropX = source->cropX;
		cropY = source->cropY;
		cropW = source->cropWidth;
		cropH = source->cropHeight;
	}

	int ret = 0;
	HeapAllocator *workHeap = pictureWorkHeapGet();
	*outSource = NULL;

	if (srcColorSpace == PIC_COLOR_SPACE_RGB) {
		PictureRgbSourceBase *rgbSource = NULL;
		if (targetColorSpace == PIC_COLOR_SPACE_RGB) {
			rgbSource = new (workHeap) PictureRgbToRgbSource();
		} else if (targetColorSpace == PIC_COLOR_SPACE_YUV) {
			rgbSource = new (workHeap) PictureRgbToYuvSource();
		} else {
			ret = PIC_CREATOR_ERROR_INVALID_ARGUMENT;
		}
		if (rgbSource != NULL) {
			ret = rgbSource->Init(source->pixelBuffer, source->width, source->height,
					      source->pitchBytes, cropX, cropY, cropW, cropH,
					      layoutOrSubsampling);
			*outSource = rgbSource;
		}
	} else if (srcColorSpace == PIC_COLOR_SPACE_YUV) {
		PictureYuvSourceBase *yuvSource = NULL;
		if (targetColorSpace == PIC_COLOR_SPACE_RGB) {
			yuvSource = new (workHeap) PictureYuvToRgbSource();
		} else if (targetColorSpace == PIC_COLOR_SPACE_YUV) {
			yuvSource = new (workHeap) PictureYuvToYuvSource();
		} else {
			ret = PIC_CREATOR_ERROR_INVALID_ARGUMENT;
		}
		if (yuvSource != NULL) {
			ret = yuvSource->Init(source->pixelBuffer, source->width, source->height,
					      source->pitchBytes, cropX, cropY, cropW, cropH,
					      layoutOrSubsampling,
					      static_cast<int>(source->reserved));
			*outSource = yuvSource;
		}
	}

	if (ret != 0) {
		if (*outSource != NULL) {
			(*outSource)->Term(false);
			delete *outSource;
			*outSource = NULL;
		}
	} else if (*outSource == NULL) {
		return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
	}
	return ret;
}

/* 0x81369C98 */
static int createCompositedSource(const PictureSourceInfo *base, const PictureSourceInfo *overlay,
				  uint8_t overlayAlpha, int targetColorSpace,
				  PicturePixelSource **outSource)
{
	if (outSource == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	*outSource = NULL;

	PicturePixelSource *baseSrc = NULL;
	PicturePixelSource *overlaySrc = NULL;
	PictureOverlayCompositor *compositor = NULL;

	int ret = createPictureSource(base, targetColorSpace, &baseSrc);
	if (ret != 0 || baseSrc == NULL || !baseSrc->IsInitialized()) {
		int err = (ret == PIC_CREATOR_ERROR_OUT_OF_MEMORY) ? PIC_CREATOR_ERROR_OUT_OF_MEMORY
								   : PIC_CREATOR_ERROR_GENERIC;
		if (baseSrc != NULL) {
			baseSrc->Term(false);
			delete baseSrc;
		}
		return err;
	}

	if (overlay == NULL || overlayAlpha == 0) {
		*outSource = baseSrc;
		return 0;
	}

	ret = createPictureSource(overlay, targetColorSpace, &overlaySrc);
	if (ret != 0 || overlaySrc == NULL || !overlaySrc->IsInitialized()) {
		int err = (ret == PIC_CREATOR_ERROR_OUT_OF_MEMORY) ? PIC_CREATOR_ERROR_OUT_OF_MEMORY
								   : PIC_CREATOR_ERROR_GENERIC;
		baseSrc->Term(false);
		delete baseSrc;
		if (overlaySrc != NULL) {
			overlaySrc->Term(false);
			delete overlaySrc;
		}
		return err;
	}

	compositor = new (pictureWorkHeapGet()) PictureOverlayCompositor();
	if (compositor == NULL) {
		baseSrc->Term(false);
		delete baseSrc;
		overlaySrc->Term(false);
		delete overlaySrc;
		return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
	}

	ret = compositor->Init(baseSrc, overlaySrc, overlayAlpha);
	if (ret != 0) {
		int err = (ret == PIC_CREATOR_ERROR_OUT_OF_MEMORY) ? PIC_CREATOR_ERROR_OUT_OF_MEMORY
								   : PIC_CREATOR_ERROR_GENERIC;
		baseSrc->Term(false);
		delete baseSrc;
		overlaySrc->Term(false);
		delete overlaySrc;
		compositor->Term(false);
		delete compositor;
		return err;
	}

	*outSource = compositor;
	return 0;
}

/* ============================================================================
 * Format Size Helpers & Low-Level Writers (BMP / JPEG / RAW)
 * ============================================================================ */

/* 0x81366AB6 */
static int getSimpleEncoderWorkSize(const PictureDimensionQuery *query)
{
	(void)query;
	return 512;
}

/* 0x8136BAC4 */
static int getBmpHeaderSize(void)
{
	return 54;
}

/* 0x8136BAC8 */
static uint32_t calcBmpPixelBytes(uint32_t width, uint32_t height)
{
	return ((3u * width + 3u) & ~3u) * height;
}

/* 0x81366ABC */
static int calcBmpFileSize(const PictureDimensionQuery *query)
{
	return getBmpHeaderSize() + static_cast<int>(calcBmpPixelBytes(query->width, query->height));
}

/* 0x813671F8 */
static int getJpegEncoderWorkSize(const PictureJpegSizeQuery *query)
{
	return query->useThumbTempFile ? 11136 : 48512;
}

/* 0x81367206 */
static uint32_t calcJpegMaxOutputSize(const PictureJpegSizeQuery *query)
{
	return ((((query->width + 15u) >> 4) * ((query->height + 15u) >> 4)) << 10) + 98304u;
}

/* 0x8136C144 */
static uint32_t calcRawPixelBytes(uint32_t width, uint32_t height)
{
	return 32u * ((width + 7u) >> 3) * height;
}

/* 0x813672EC */
static int getRawOutputSize(const PictureDimensionQuery *query)
{
	return static_cast<int>(calcRawPixelBytes(query->width, query->height));
}

/* 0x81365C1C */
static int queryRawWorkMemorySize(const PictureEncodeConfig *config, uint32_t *outSize)
{
	if (config == NULL || config->source == NULL || outSize == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	if (config->outputFormat != PIC_FORMAT_RAW)
		return PIC_CREATOR_ERROR_GENERIC;

	PictureDimensionQuery query;
	sce_paf_memset(&query, 0, sizeof(query));
	query.width = sourceEffectiveWidth(config->source);
	query.height = sourceEffectiveHeight(config->source);

	int bytes = getSimpleEncoderWorkSize(&query);
	if (bytes <= 0) {
		*outSize = 0;
		return (bytes < 0) ? bytes : PIC_CREATOR_ERROR_GENERIC;
	}
	*outSize = static_cast<uint32_t>(bytes);
	return 0;
}

/* 0x81365CCA */
static int queryRawOutputBufferSize(const PictureEncodeConfig *config, uint32_t *outSize)
{
	if (config == NULL || config->source == NULL || outSize == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	if (config->outputFormat != PIC_FORMAT_RAW)
		return PIC_CREATOR_ERROR_GENERIC;

	PictureDimensionQuery query;
	sce_paf_memset(&query, 0, sizeof(query));
	query.width = sourceEffectiveWidth(config->source);
	query.height = sourceEffectiveHeight(config->source);

	int bytes = getRawOutputSize(&query);
	if (bytes <= 0) {
		*outSize = 0;
		return (bytes < 0) ? bytes : PIC_CREATOR_ERROR_GENERIC;
	}
	*outSize = static_cast<uint32_t>(bytes);
	return 0;
}

/* 0x81365D78 */
static int queryBmpWorkMemorySize(const PictureEncodeConfig *config, uint32_t *outSize)
{
	if (config == NULL || config->source == NULL || outSize == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	if (config->outputFormat != PIC_FORMAT_BMP)
		return PIC_CREATOR_ERROR_GENERIC;

	PictureDimensionQuery query;
	sce_paf_memset(&query, 0, sizeof(query));
	query.width = sourceEffectiveWidth(config->source);
	query.height = sourceEffectiveHeight(config->source);

	int bytes = getSimpleEncoderWorkSize(&query);
	if (bytes <= 0) {
		*outSize = 0;
		return (bytes < 0) ? bytes : PIC_CREATOR_ERROR_GENERIC;
	}
	*outSize = static_cast<uint32_t>(bytes);
	return 0;
}

/* 0x81365E26 */
static int queryBmpOutputBufferSize(const PictureEncodeConfig *config, uint32_t *outSize)
{
	if (config == NULL || config->source == NULL || outSize == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	if (config->outputFormat != PIC_FORMAT_BMP)
		return PIC_CREATOR_ERROR_GENERIC;

	PictureDimensionQuery query;
	sce_paf_memset(&query, 0, sizeof(query));
	query.width = sourceEffectiveWidth(config->source);
	query.height = sourceEffectiveHeight(config->source);

	int bytes = calcBmpFileSize(&query);
	if (bytes <= 0) {
		*outSize = 0;
		return (bytes < 0) ? bytes : PIC_CREATOR_ERROR_GENERIC;
	}
	*outSize = static_cast<uint32_t>(bytes);
	return 0;
}

/* 0x81365ED4 */
static int queryJpegWorkMemorySize(const PictureEncodeConfig *config, uint32_t *outSize)
{
	if (config == NULL || config->source == NULL || outSize == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	if (config->outputFormat != PIC_FORMAT_JPEG)
		return PIC_CREATOR_ERROR_GENERIC;

	PictureJpegSizeQuery query;
	sce_paf_memset(&query, 0, sizeof(query));
	query.width = sourceEffectiveWidth(config->source);
	query.height = sourceEffectiveHeight(config->source);
	query.useThumbTempFile = ((config->outputFlags & PIC_OUTPUT_FLAG_THUMB_TEMP_FILE) != 0);

	int bytes = getJpegEncoderWorkSize(&query);
	if (bytes <= 0) {
		*outSize = 0;
		return (bytes < 0) ? bytes : PIC_CREATOR_ERROR_GENERIC;
	}
	*outSize = static_cast<uint32_t>(bytes);
	return 0;
}

/* 0x81365F9A */
static int queryJpegOutputBufferSize(const PictureEncodeConfig *config, uint32_t *outSize)
{
	if (config == NULL || config->source == NULL || outSize == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	if (config->outputFormat != PIC_FORMAT_JPEG)
		return PIC_CREATOR_ERROR_GENERIC;

	PictureJpegSizeQuery query;
	sce_paf_memset(&query, 0, sizeof(query));
	query.width = sourceEffectiveWidth(config->source);
	query.height = sourceEffectiveHeight(config->source);
	query.useThumbTempFile = ((config->outputFlags & PIC_OUTPUT_FLAG_THUMB_TEMP_FILE) != 0);

	int bytes = static_cast<int>(calcJpegMaxOutputSize(&query));
	if (bytes <= 0) {
		*outSize = 0;
		return (bytes < 0) ? bytes : PIC_CREATOR_ERROR_GENERIC;
	}
	*outSize = static_cast<uint32_t>(bytes);
	return 0;
}

/* 0x8136BAEC */
static int writeBmpHeader(const PictureBmpHeaderParam *param)
{
	if (param->stream == NULL || !param->stream->IsOpen())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	uint32_t imageBytes = calcBmpPixelBytes(param->width, param->height);
	uint8_t fileHeader[14];
	sce_paf_memset(fileHeader, 0, sizeof(fileHeader));
	fileHeader[0] = 'B';
	fileHeader[1] = 'M';
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
	dibHeader.width = static_cast<int32_t>(param->width);
	dibHeader.height = -static_cast<int32_t>(param->height);
	dibHeader.planes = 1;
	dibHeader.bitsPerPixel = 24;
	dibHeader.imageSize = imageBytes;

	int ret = param->stream->Write(fileHeader, sizeof(fileHeader));
	if (ret == 0) {
		ret = param->stream->Write(&dibHeader, sizeof(dibHeader));
		if (ret == 0)
			return 54;
	}
	return ret;
}

/* 0x8136BBB8 */
static int writeBmpPixels(const PicturePixelWriteParam *param)
{
	if (param->source == NULL || !param->source->IsInitialized())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	if (param->stream == NULL || !param->stream->IsOpen())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	PictureSize size = param->source->GetSize();
	uint32_t rowStride = (3u * size.width + 3u) & ~3u;
	uint32_t rowPadLen = rowStride - 3u * size.width;
	uint8_t zeroPad[4] = { 0, 0, 0, 0 };

	int ret = 0;
	for (uint32_t posY = 0; posY < size.height; ++posY) {
		if (pictureIsCanceled(param->cancelHook))
			return PIC_CREATOR_ERROR_CANCELED;
		for (uint32_t posX = 0; posX < size.width; ++posX) {
			uint32_t rgba = param->source->GetPixel(posX, posY);
			uint8_t bgr[3] = {
				static_cast<uint8_t>((rgba >> 16) & 0xFFu),
				static_cast<uint8_t>((rgba >> 8) & 0xFFu),
				static_cast<uint8_t>(rgba & 0xFFu)
			};
			ret = param->stream->Write(bgr, sizeof(bgr));
			if (ret != 0)
				return ret;
		}
		if (rowPadLen != 0) {
			ret = param->stream->Write(zeroPad, rowPadLen);
			if (ret != 0)
				return ret;
		}
	}
	return static_cast<int>(rowStride * size.height);
}

/* 0x8136C166 */
static int writeRawPixels(const PicturePixelWriteParam *param)
{
	if (param->source == NULL || !param->source->IsInitialized())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	if (param->stream == NULL || !param->stream->IsOpen())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	PictureSize size = param->source->GetSize();
	uint32_t alignedW = (size.width + 7u) & ~7u;
	uint32_t rowPadLen = (alignedW - size.width) * 4u;
	static const uint8_t zeroRowPad[32] = { 0 };

	int ret = 0;
	for (uint32_t posY = 0; posY < size.height; ++posY) {
		if (pictureIsCanceled(param->cancelHook))
			return PIC_CREATOR_ERROR_CANCELED;
		for (uint32_t posX = 0; posX < size.width; ++posX) {
			uint32_t rgba = param->source->GetPixel(posX, posY);
			uint8_t pixel[4] = {
				static_cast<uint8_t>(rgba & 0xFFu),
				static_cast<uint8_t>((rgba >> 8) & 0xFFu),
				static_cast<uint8_t>((rgba >> 16) & 0xFFu),
				static_cast<uint8_t>((rgba >> 24) & 0xFFu)
			};
			ret = param->stream->Write(pixel, sizeof(pixel));
			if (ret != 0)
				return ret;
		}
		if (rowPadLen != 0) {
			ret = param->stream->Write(zeroRowPad, rowPadLen);
			if (ret != 0)
				return ret;
		}
	}
	return static_cast<int>(calcRawPixelBytes(size.width, size.height));
}

/* 0x8136BCE8 */
static int findJpegDqtMarker(const uint8_t *buffer, int length, int *outOffset)
{
	if (buffer == NULL || outOffset == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	*outOffset = -1;

	static const uint8_t soiBytes[2] = { 0xFF, 0xD8 };
	if (sce_paf_memcmp(buffer, soiBytes, 2u) != 0)
		return PIC_CREATOR_ERROR_GENERIC;

	const uint8_t *endPtr = buffer + length;
	const uint8_t *cursor = buffer + 2;
	while (cursor + 4 <= endPtr) {
		if (cursor[0] == 0xFF) {
			uint8_t marker = cursor[1];
			if (marker == 0xDB) {
				*outOffset = static_cast<int>(cursor - buffer);
				return 0;
			}
			if (marker != 0x00 && marker != 0xFF) {
				int segLen = (static_cast<int>(cursor[2]) << 8) | cursor[3];
				cursor += segLen + 2;
				continue;
			}
		}
		++cursor;
	}
	*outOffset = -1;
	return 0;
}

/* 0x8136BDA8 */
static int encodeJpegMcuStream(const PictureJpegMcuParam *param)
{
	sceKernelGetProcessTimeWide();

	PictureYuvMcuSink mcuSink;
	uint32_t mcuH = (param->subsampling == PIC_YUV_422) ? 8u : 16u;

	if (param->feeder == NULL || !param->feeder->IsInitialized() ||
	    param->stream == NULL || !param->stream->IsOpen() ||
	    (param->subsampling != PIC_YUV_420 && param->subsampling != PIC_YUV_422)) {
		sceKernelGetProcessTimeWide();
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	}

	uint32_t uvDivY = (param->subsampling == PIC_YUV_422) ? 1u : 2u;
	uint32_t uvStride = ((16u / 2u) + 7u) & ~7u;
	uint32_t mcuBufSize = 16u * mcuH + 2u * uvStride * (mcuH / uvDivY);

	HeapAllocator *workHeap = pictureWorkHeapGet();
	uint8_t *mcuBuf = static_cast<uint8_t *>(ScePafStdc_3CF19248(mcuBufSize, workHeap));
	uint8_t *jpegChunk = NULL;
	void *encCtx = NULL;
	int ret = 0;

	if (mcuBuf == NULL) {
		ret = PIC_CREATOR_ERROR_OUT_OF_MEMORY;
	} else if (mcuSink.Init(mcuBuf, 16u, mcuH, 0, 0, 0, 16u, mcuH, param->subsampling) != 0) {
		ret = PIC_CREATOR_ERROR_GENERIC;
	} else {
		mcuSink.Clear();
		if (param->feeder->BindSink(&mcuSink) != 0) {
			ret = PIC_CREATOR_ERROR_GENERIC;
		} else {
			jpegChunk = static_cast<uint8_t *>(ScePafStdc_3CF19248(1024, workHeap));
			if (jpegChunk == NULL) {
				ret = PIC_CREATOR_ERROR_OUT_OF_MEMORY;
			} else {
				int ctxSize = sceJpegArmEncoderGetContextSize();
				if (ctxSize > 0)
					encCtx = ScePafStdc_3CF19248(static_cast<size_t>(ctxSize),
								     workHeap);
				if (encCtx == NULL) {
					ret = PIC_CREATOR_ERROR_OUT_OF_MEMORY;
				} else {
					PictureSize srcSize = param->feeder->GetSourceSize();
					uint32_t alignedW = (srcSize.width + 15u) & ~15u;
					uint32_t alignedH = (srcSize.height + mcuH - 1u) & ~(mcuH - 1u);
					int pixelFmt = (param->subsampling == PIC_YUV_420)
							       ? SCE_JPEGENC_PIXEL_YCBCR420
							       : SCE_JPEGENC_PIXEL_YCBCR422;

					ret = sceJpegArmEncoderInit(
						encCtx, static_cast<int>(alignedW),
						static_cast<int>(alignedH), pixelFmt, NULL, 0);
					if (ret == 0) {
						if (alignedW != srcSize.width ||
						    alignedH != srcSize.height) {
							ret = sceJpegArmEncoderSetValidRegion(
								encCtx,
								static_cast<uint16_t>(srcSize.width),
								static_cast<uint16_t>(srcSize.height));
						}
						if (ret == 0)
							ret = sceJpegArmEncoderSetCompressionRatio(
								encCtx, param->compressionRatio);
						if (ret == 0 && !param->omitSoi) {
							static const uint8_t soiMarker[2] = {
								0xFF, 0xD8
							};
							ret = param->stream->Write(soiMarker, 2);
						}
						if (ret == 0) {
							int headerSize = 0;
							int dqtOffset = 0;
							int encRet = SceJpegEncArm_4EE1BAED(
								encCtx, mcuSink.GetYPlane(),
								mcuSink.GetCbPlane(),
								mcuSink.GetCrPlane(), 2, jpegChunk,
								1024, &headerSize);
							if (encRet < 0) {
								ret = encRet;
							} else if (findJpegDqtMarker(
									   jpegChunk, headerSize,
									   &dqtOffset) != 0 ||
								   dqtOffset < 0) {
								ret = PIC_CREATOR_ERROR_GENERIC;
							} else {
								ret = param->stream->Write(
									jpegChunk + dqtOffset,
									static_cast<SceSize>(
										headerSize -
										dqtOffset));
							}

							if (ret == 0) {
								PicturePoint cursor;
								cursor.posX = 0;
								cursor.posY = 0;
								int chunkLen = 0;

								for (;;) {
									if (pictureIsCanceled(
										    param->cancelHook)) {
										ret = PIC_CREATOR_ERROR_CANCELED;
										break;
									}
									if (param->feeder->FetchNextBlock(
										    &cursor) != 0) {
										ret = PIC_CREATOR_ERROR_GENERIC;
										break;
									}
									int status = SceJpegEncArm_4EE1BAED(
										encCtx,
										mcuSink.GetYPlane(),
										mcuSink.GetCbPlane(),
										mcuSink.GetCrPlane(),
										3, jpegChunk, 1024,
										&chunkLen);
									if (status < 0) {
										ret = status;
										break;
									}
									ret = param->stream->Write(
										jpegChunk,
										static_cast<SceSize>(
											chunkLen));
									if (ret != 0)
										break;
									if (status > 0) {
										ret = static_cast<int>(
											param->stream
												->GetWrittenSize());
										break;
									}
								}
							}
						}
						sceJpegArmEncoderEnd(encCtx);
					}
				}
			}
		}
	}

	if (mcuSink.IsInitialized())
		mcuSink.Term();
	if (encCtx != NULL)
		operator delete[](encCtx);
	if (mcuBuf != NULL)
		operator delete[](mcuBuf);
	if (jpegChunk != NULL)
		operator delete[](jpegChunk);

	sceKernelGetProcessTimeWide();
	return ret;
}

/* ============================================================================
 * Top-Level Format Encoders (0x813669A8, 0x81366AF8, 0x81367220)
 * ============================================================================ */

/* 0x813669A8 */
static int encodeBmpPicture(const PictureSimpleEncodeParam *param)
{
	if (param->stream == NULL || !param->stream->IsOpen())
		return PIC_CREATOR_ERROR_GENERIC;

	PicturePixelSource *source = NULL;
	int ret = createCompositedSource(param->source, param->overlay, param->overlayAlpha,
					 PIC_COLOR_SPACE_RGB, &source);
	if (ret == 0 && source != NULL && source->IsInitialized()) {
		PictureSize size = source->GetSize();
		PictureBmpHeaderParam hdrParam;
		sce_paf_memset(&hdrParam, 0, sizeof(hdrParam));
		hdrParam.width = size.width;
		hdrParam.height = size.height;
		hdrParam.stream = param->stream;

		int hdrRet = writeBmpHeader(&hdrParam);
		if (hdrRet > 0) {
			PicturePixelWriteParam pixParam;
			sce_paf_memset(&pixParam, 0, sizeof(pixParam));
			pixParam.source = source;
			pixParam.stream = param->stream;
			pixParam.cancelHook = param->cancelHook;

			int pixRet = writeBmpPixels(&pixParam);
			ret = (pixRet > 0)
				      ? 0
				      : ((pixRet != 0) ? pixRet : PIC_CREATOR_ERROR_GENERIC);
		} else {
			ret = (hdrRet != 0) ? hdrRet : PIC_CREATOR_ERROR_GENERIC;
		}
	} else {
		ret = (ret < 0) ? ret : PIC_CREATOR_ERROR_GENERIC;
	}

	if (source != NULL) {
		source->Term(true);
		delete source;
	}
	return ret;
}

/* 0x81367220 */
static int encodeRawPicture(const PictureSimpleEncodeParam *param)
{
	if (param->stream == NULL || !param->stream->IsOpen())
		return PIC_CREATOR_ERROR_GENERIC;

	PicturePixelSource *source = NULL;
	int ret = createCompositedSource(param->source, param->overlay, param->overlayAlpha,
					 PIC_COLOR_SPACE_RGB, &source);
	if (ret == 0 && source != NULL && source->IsInitialized()) {
		PicturePixelWriteParam pixParam;
		sce_paf_memset(&pixParam, 0, sizeof(pixParam));
		pixParam.source = source;
		pixParam.stream = param->stream;
		pixParam.cancelHook = param->cancelHook;

		int pixRet = writeRawPixels(&pixParam);
		ret = (pixRet > 0) ? 0 : ((pixRet != 0) ? pixRet : PIC_CREATOR_ERROR_GENERIC);
	} else {
		ret = (ret < 0) ? ret : PIC_CREATOR_ERROR_GENERIC;
	}

	if (source != NULL) {
		source->Term(true);
		delete source;
	}
	return ret;
}

/* 0x81366AF8 */
static int encodeJpegPicture(const PictureJpegEncodeParam *param)
{
	PictureNearestScaler thumbScaler;
	PictureBilinearScaler upscaleScaler;
	PictureCanvasSource thumbCanvas;

	if (param->stream == NULL || !param->stream->IsOpen())
		return PIC_CREATOR_ERROR_GENERIC;

	PicturePixelSource *source = NULL;
	PictureOutputStream *thumbStream = NULL;
	uint8_t *thumbMemBuf = NULL;
	PictureMcuFeederBase *feeder = NULL;
	HeapAllocator *workHeap = pictureWorkHeapGet();

	int ret = createCompositedSource(param->source, param->overlay, param->overlayAlpha,
					 PIC_COLOR_SPACE_YUV, &source);
	if (ret != 0 || source == NULL || !source->IsInitialized()) {
		ret = (ret < 0) ? ret : PIC_CREATOR_ERROR_GENERIC;
	} else {
		if (param->thumbTempPath != NULL) {
			PictureFileStream *fileStream =
				new (workHeap) PictureFileStream(PIC_THUMB_IO_BUFFER_SIZE);
			if (fileStream == NULL) {
				ret = PIC_CREATOR_ERROR_OUT_OF_MEMORY;
			} else {
				thumbStream = fileStream;
				ret = fileStream->Open(param->thumbTempPath, param->thumbIoFlags);
			}
		} else {
			thumbMemBuf = static_cast<uint8_t *>(
				ScePafStdc_3CF19248(PIC_THUMB_MEM_BUFFER_SIZE, workHeap));
			if (thumbMemBuf == NULL) {
				ret = PIC_CREATOR_ERROR_OUT_OF_MEMORY;
			} else {
				PictureMemoryStream *memStream =
					new (workHeap) PictureMemoryStream();
				if (memStream == NULL) {
					ret = PIC_CREATOR_ERROR_OUT_OF_MEMORY;
				} else {
					thumbStream = memStream;
					ret = memStream->Open(thumbMemBuf,
							      PIC_THUMB_MEM_BUFFER_SIZE);
				}
			}
		}

		if (ret == 0) {
			PictureSize srcSize = source->GetSize();
			float scaleW = static_cast<float>(PIC_THUMB_WIDTH) /
				       static_cast<float>(srcSize.width);
			float scaleH = static_cast<float>(PIC_THUMB_HEIGHT) /
				       static_cast<float>(srcSize.height);
			float thumbScale = (scaleW < scaleH) ? scaleW : scaleH;

			if (thumbScaler.Init(source, thumbScale) != 0 ||
			    thumbCanvas.Init(&thumbScaler, PIC_THUMB_WIDTH, PIC_THUMB_HEIGHT) != 0) {
				ret = PIC_CREATOR_ERROR_GENERIC;
			} else {
				PictureGenericMcuFeeder *thumbFeeder =
					new (workHeap) PictureGenericMcuFeeder();
				feeder = thumbFeeder;
				if (thumbFeeder == NULL) {
					ret = PIC_CREATOR_ERROR_OUT_OF_MEMORY;
				} else if (thumbFeeder->Init(&thumbCanvas) != 0) {
					ret = PIC_CREATOR_ERROR_GENERIC;
				} else {
					static const uint8_t thumbRatios[6] = {
						8, 32, 64, 128, 192, 255
					};
					int thumbResult = PIC_CREATOR_ERROR_GENERIC;
					for (int idx = 0; idx < 6; ++idx) {
						ret = thumbStream->Rewind();
						if (ret != 0)
							break;
						PictureJpegMcuParam mcuParam;
						sce_paf_memset(&mcuParam, 0, sizeof(mcuParam));
						mcuParam.feeder = feeder;
						mcuParam.stream = thumbStream;
						mcuParam.subsampling = PIC_YUV_422;
						mcuParam.compressionRatio = thumbRatios[idx];
						mcuParam.omitSoi = false;
						mcuParam.cancelHook = param->cancelHook;
						thumbResult = encodeJpegMcuStream(&mcuParam);
						if (thumbResult > 0)
							break;
					}
					feeder->Term();
					delete feeder;
					feeder = NULL;

					if (ret == 0 && thumbResult <= 0)
						ret = (thumbResult != 0)
							      ? thumbResult
							      : PIC_CREATOR_ERROR_GENERIC;
					if (ret == 0)
						ret = thumbStream->Flush();
					if (ret == 0 && pictureIsCanceled(param->cancelHook))
						ret = PIC_CREATOR_ERROR_CANCELED;
					if (ret == 0) {
						static const uint8_t soiMarker[2] = { 0xFF, 0xD8 };
						ret = param->stream->Write(soiMarker, 2);
					}
					if (ret == 0 && param->exif != NULL) {
						PictureMakerNoteWriter makerWriter;
						PictureExifApp1Writer exifWriter;
						void *makerPtr = NULL;
						uint32_t makerLen = 0;

						if (param->exif->makerNote != NULL) {
							int mnRet = makerWriter.Build(
								param->exif->makerNote, &makerPtr,
								&makerLen);
							if (mnRet < 0)
								ret = (mnRet ==
								       PIC_EXIF_ERROR_OUT_OF_MEMORY)
									      ? PIC_CREATOR_ERROR_OUT_OF_MEMORY
									      : PIC_CREATOR_ERROR_GENERIC;
						}
						if (ret == 0) {
							void *app1Ptr = NULL;
							uint32_t app1Len = 0;
							int app1Ret = pictureBuildExifApp1(
								exifWriter, param->exif,
								thumbStream->GetWrittenSize(),
								makerPtr, makerLen, &app1Ptr,
								&app1Len);
							if (app1Ret < 0) {
								ret = (app1Ret ==
								       PIC_EXIF_ERROR_OUT_OF_MEMORY)
									      ? PIC_CREATOR_ERROR_OUT_OF_MEMORY
									      : PIC_CREATOR_ERROR_GENERIC;
							} else {
								ret = param->stream->Write(app1Ptr,
											   app1Len);
								if (ret == 0) {
									if (!param->stream->IsOpen())
										ret = PIC_CREATOR_ERROR_INVALID_STATE;
									else if (!thumbStream->IsOpen())
										ret = PIC_CREATOR_ERROR_INVALID_ARGUMENT;
									else
										ret = thumbStream->CopyTo(
											param->stream);
								}
							}
						}
					}
					if (ret == 0 && pictureIsCanceled(param->cancelHook))
						ret = PIC_CREATOR_ERROR_CANCELED;
					if (ret == 0) {
						bool useFastRgba =
							PictureFastRgbaMcuFeeder::CanUse(
								param->source) &&
							param->overlay == NULL &&
							param->source->width == 960u &&
							param->source->height == 544u;
						bool useUpscale =
							PictureFastRgbaMcuFeeder::CanUse(
								param->source) &&
							param->overlay == NULL &&
							param->source->width < 960u &&
							param->source->height < 544u;
						bool useFastYuv =
							PictureFastYuvMcuFeeder::CanUse(
								param->source) &&
							param->overlay == NULL;

						if (useFastRgba) {
							PictureFastRgbaMcuFeeder *fastRgba =
								new (workHeap)
									PictureFastRgbaMcuFeeder();
							feeder = fastRgba;
							if (fastRgba == NULL)
								ret = PIC_CREATOR_ERROR_OUT_OF_MEMORY;
							else if (fastRgba->Init(param->source) != 0)
								ret = PIC_CREATOR_ERROR_GENERIC;
						} else if (useUpscale) {
							if (upscaleScaler.Init(source,
									       param->scaleX,
									       param->scaleY) != 0) {
								ret = PIC_CREATOR_ERROR_GENERIC;
							} else {
								PictureGenericMcuFeeder *generic =
									new (workHeap)
										PictureGenericMcuFeeder();
								feeder = generic;
								if (generic == NULL)
									ret = PIC_CREATOR_ERROR_OUT_OF_MEMORY;
								else if (generic->Init(
										 &upscaleScaler) !=
									 0)
									ret = PIC_CREATOR_ERROR_GENERIC;
							}
						} else if (useFastYuv) {
							PictureFastYuvMcuFeeder *fastYuv =
								new (workHeap)
									PictureFastYuvMcuFeeder();
							feeder = fastYuv;
							if (fastYuv == NULL)
								ret = PIC_CREATOR_ERROR_OUT_OF_MEMORY;
							else if (fastYuv->Init(param->source) != 0)
								ret = PIC_CREATOR_ERROR_GENERIC;
						} else {
							PictureGenericMcuFeeder *generic =
								new (workHeap)
									PictureGenericMcuFeeder();
							feeder = generic;
							if (generic == NULL)
								ret = PIC_CREATOR_ERROR_OUT_OF_MEMORY;
							else if (generic->Init(source) != 0)
								ret = PIC_CREATOR_ERROR_GENERIC;
						}

						if (ret == 0) {
							PictureJpegMcuParam mcuParam;
							sce_paf_memset(&mcuParam, 0,
								       sizeof(mcuParam));
							mcuParam.feeder = feeder;
							mcuParam.stream = param->stream;
							mcuParam.subsampling = PIC_YUV_420;
							mcuParam.compressionRatio =
								param->compressionRatio;
							mcuParam.omitSoi = true;
							mcuParam.cancelHook = param->cancelHook;

							int mainResult =
								encodeJpegMcuStream(&mcuParam);
							if (mainResult > 0) {
								ret = 0;
							} else {
								ret = (mainResult != 0)
									      ? mainResult
									      : PIC_CREATOR_ERROR_GENERIC;
							}
						}
					}
				}
			}
		}
	}

	if (feeder != NULL) {
		feeder->Term();
		delete feeder;
	}
	thumbCanvas.Term(false);
	thumbScaler.Term(false);
	if (source != NULL) {
		source->Term(true);
		delete source;
	}
	if (thumbStream != NULL)
		delete thumbStream;
	if (thumbMemBuf != NULL)
		operator delete[](thumbMemBuf);
	if (param->thumbTempPath != NULL)
		PictureFileStream::RemoveFile(param->thumbTempPath);

	return ret;
}

/* ============================================================================
 * Public ScePictureFileCreator API (0x81365BFC - 0x81366677)
 * ============================================================================ */

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

	uint32_t minOutputSize = 0;
	uint32_t workBytes = 0;
	int outErr = 0;
	int workErr = 0;

	if (config->outputFormat == PIC_FORMAT_JPEG) {
		if ((config->outputFlags & PIC_OUTPUT_FLAG_FILE) == 0)
			outErr = queryJpegOutputBufferSize(config, &minOutputSize);
		workErr = queryJpegWorkMemorySize(config, &workBytes);
	} else if (config->outputFormat == PIC_FORMAT_BMP) {
		if ((config->outputFlags & PIC_OUTPUT_FLAG_FILE) == 0)
			outErr = queryBmpOutputBufferSize(config, &minOutputSize);
		workErr = queryBmpWorkMemorySize(config, &workBytes);
	} else if (config->outputFormat == PIC_FORMAT_RAW) {
		if ((config->outputFlags & PIC_OUTPUT_FLAG_FILE) == 0)
			outErr = queryRawOutputBufferSize(config, &minOutputSize);
		workErr = queryRawWorkMemorySize(config, &workBytes);
	} else {
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	}

	if ((outErr | workErr) != 0)
		return (outErr != 0) ? outErr : workErr;

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
		(reinterpret_cast<uintptr_t>(output->workMemory) + 15u) &
		~static_cast<uintptr_t>(15u);
	HeapAllocator workHeap(reinterpret_cast<void *>(alignedWork), output->workMemorySize - 16u,
			       "ScePictureFileCreatorWorkMemory", NULL);
	if (pictureWorkHeapSet(&workHeap) != 0) {
		sceKernelGetProcessTimeWide();
		return PIC_CREATOR_ERROR_GENERIC;
	}

	PictureOutputStream *stream = NULL;
	int ret = 0;

	if ((config->outputFlags & PIC_OUTPUT_FLAG_FILE) != 0) {
		PictureFileStream *fileStream =
			new (pictureWorkHeapGet()) PictureFileStream(PIC_FILE_IO_BUFFER_SIZE);
		if (fileStream == NULL) {
			pictureWorkHeapClear();
			sceKernelGetProcessTimeWide();
			return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
		}
		stream = fileStream;
		ret = fileStream->Open(output->outputFilePath, output->ioBufferSize);
	} else {
		PictureMemoryStream *memStream = new (pictureWorkHeapGet()) PictureMemoryStream();
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
			PictureJpegEncodeParam jpegParam;
			sce_paf_memset(&jpegParam, 0, sizeof(jpegParam));
			jpegParam.source = config->source;
			jpegParam.overlay = config->overlay;
			jpegParam.overlayAlpha = config->overlayAlpha;
			jpegParam.exif = input->exif;
			jpegParam.compressionRatio = config->jpegCompressionRatio;
			jpegParam.stream = stream;
			jpegParam.thumbTempPath = thumbPath;
			jpegParam.thumbIoFlags = thumbIoFlags;
			jpegParam.cancelHook = &cancelHook;
			jpegParam.scaleX = config->scaleX;
			jpegParam.scaleY = config->scaleY;
			ret = encodeJpegPicture(&jpegParam);
		} else if (config->outputFormat == PIC_FORMAT_BMP) {
			PictureSimpleEncodeParam bmpParam;
			sce_paf_memset(&bmpParam, 0, sizeof(bmpParam));
			bmpParam.source = config->source;
			bmpParam.overlay = config->overlay;
			bmpParam.overlayAlpha = config->overlayAlpha;
			bmpParam.stream = stream;
			bmpParam.cancelHook = &cancelHook;
			ret = encodeBmpPicture(&bmpParam);
		} else if (config->outputFormat == PIC_FORMAT_RAW) {
			PictureSimpleEncodeParam rawParam;
			sce_paf_memset(&rawParam, 0, sizeof(rawParam));
			rawParam.source = config->source;
			rawParam.overlay = config->overlay;
			rawParam.overlayAlpha = config->overlayAlpha;
			rawParam.stream = stream;
			rawParam.cancelHook = &cancelHook;
			ret = encodeRawPicture(&rawParam);
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
		delete stream;
	}

	pictureWorkHeapClear();
	sceKernelGetProcessTimeWide();
	return (ret == 0) ? static_cast<int>(writtenBytes) : ret;
}
