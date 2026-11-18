/*
 * The game plugin's icons of a PSP application: its ICON0 read from the EBOOT and checked, and
 * drawn on the system's PSP icon base as its 128x128 DXT1 icon0.dds (0x81053840..0x81054B14).
 * The file's name and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <kernel/iofilemgr.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/localfile.h>
#include <framework.h>
#include <image/image.h>
#include <image/image_filter.h>
#include <memory/pool_allocator.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

/* the share of each step in a writer's progress */
#define GAME_PSP_APPMETA_WEIGHT_CHECK	0.05f
#define GAME_PSP_APPMETA_WEIGHT_WRITE	0.9f
#define GAME_PSP_APPMETA_WEIGHT_COMMIT	0.05f

/* the largest ICON0 of a PSP application, and its largest extent */
#define GAME_PSP_ICON_MAX_SIZE		0xC800
#define GAME_PSP_ICON_MAX_WIDTH		144
#define GAME_PSP_ICON_MAX_HEIGHT	80

/* the icon drawn: 128x128 RGBA pixels, 512 bytes a row, on the base vs0 gives */
#define GAME_PSP_ICON_SIZE		128
#define GAME_PSP_ICON_PITCH		512
#define GAME_PSP_ICON_BUF_SIZE		0x10000
#define GAME_PSP_ICON_DDS_SIZE		0x4000

GamePspAppmetaWriter::GamePspAppmetaWriter(GameCancelFunc cancel, void *arg,
					   const GameJobProgress *progress, string *detail,
					   float scale)
	: m_cancel(cancel), m_arg(arg), m_progress(*progress, scale), m_detail(detail)
{
	m_weights[0] = GAME_PSP_APPMETA_WEIGHT_CHECK;
	m_weights[1] = GAME_PSP_APPMETA_WEIGHT_WRITE;
	m_weights[2] = GAME_PSP_APPMETA_WEIGHT_COMMIT;
}

/* the commit's share of the progress added, when the registry has room for the application */
int GamePspAppmetaWriter::Run(GameAppRegistration *reg, const string *dir)
{
	bool hasRoom = false;
	int ret = gameLsdbHasRoom(&hasRoom);

	if (ret < 0)
		return ret;

	if (hasRoom)
		m_progress.Add(m_weights[2] * 100.0f);

	return 0;
}

int gameFileReadAt(const string *path, SceUInt32 offset, SceSize size, void *buf)
{
	if (size > GAME_PSP_ICON_MAX_SIZE)
		return GAME_ERROR_BAD_PARAM_SIZE;

	GameImageInfo info;
	int ret = 0;
	common::SharedPtr<File> file = LocalFile::Open(path->c_str(), SCE_O_RDONLY, 0, &ret);

	if (ret != 0)
		return ret;

	ret = file->Seek(offset, SCE_SEEK_SET);
	if (ret < 0) {
		file->Close();
		return ret;
	}

	ret = file->Read(buf, size);
	if (ret < 0) {
		file->Close();
		return ret;
	}

	file->Close();

	ret = gameImageGetInfo(NULL, buf, size, &info);
	if (ret < 0)
		return ret;

	if (info.width > GAME_PSP_ICON_MAX_WIDTH || info.height > GAME_PSP_ICON_MAX_HEIGHT ||
	    size > GAME_PSP_ICON_MAX_SIZE || info.format != image::ImageFormat_Png)
		return GAME_ERROR_BAD_PARAM_SIZE;

	return ret;
}

int gameFileWrite(const string *path, const void *buf, SceSize size)
{
	int ret = 0;
	common::SharedPtr<File> file =
		LocalFile::Open(path->c_str(), SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6, &ret);

	if (file.get() == NULL)
		return ret;

	ret = file->Write(buf, size);
	if (ret < 0) {
		file->Close();
		return ret;
	}

	file->Close();

	return 0;
}

/*
 * The PSP icon png drawn, fitted to its kind's frame, on the base in work, and saved to path as
 * DXT1 (0x81053C64).
 */
static int gamePspIconCompose(int kind, int attributeKind, void *work, size_t workSize,
			      const void *png, size_t pngSize, string path)
{
	unsigned int maxWidth;
	unsigned int left;
	unsigned int width;
	unsigned int height;
	int ret = 0;

	if (kind == 2) {
		if (attributeKind > 0 && attributeKind <= 2) {
			maxWidth = 80;
			left = 18;
			width = 92;
		} else {
			maxWidth = 144;
			left = 9;
			width = 110;
		}
		height = width;
	} else if (kind == 3) {
		width = 92;
		height = 92;
		maxWidth = 80;
		left = 18;
	} else {
		return GAME_ERROR_INVALID_ARGUMENT;
	}

	memory::HeapAllocator *heap = Framework::Instance()->GetDecodeHeap();

	if (heap == NULL)
		return GAME_ERROR_NOT_READY;

	common::SharedPtr<File> base =
		LocalFile::Open("vs0:vsh/game/psp_icon_base.raw", SCE_O_RDONLY, 0, &ret);

	if (ret < 0)
		return ret;

	ret = base->Read(work, GAME_PSP_ICON_BUF_SIZE);
	if (ret < 0) {
		base->Close();
		return ret;
	}

	base->Close();

	common::SharedPtr<image::Image> icon =
		image::Image::Open((void *)png, pngSize, heap, image::ImageFormat_None);

	if (icon.get() == NULL)
		return image::Image::GetErrorType();

	if (icon->GetFormat() != image::ImageFormat_Png)
		return GAME_ERROR_BAD_ICON;

	image::ImageExtent extent = icon->GetExtent();

	if ((unsigned int)extent.w > maxWidth)
		return GAME_ERROR_BAD_ICON;
	if ((unsigned int)extent.h > GAME_PSP_ICON_MAX_HEIGHT)
		return GAME_ERROR_BAD_ICON;

	float fitWidth = width;
	float fitHeight = height;
	float aspect = (float)extent.w / (float)extent.h;

	if (fitWidth / fitHeight > aspect) {
		fitWidth = aspect * fitHeight;
		if (fitWidth < 1.0f)
			fitWidth = 1.0f;
	} else {
		fitHeight = fitWidth / aspect;
		if (fitHeight < 1.0f)
			fitHeight = 1.0f;
	}

	image::ImageExtent fit((int)fitWidth, (int)fitHeight);
	common::SharedPtr<image::Image> fitted;

	if (icon->GetMode() == ImageMode_RGBA8888)
		fitted = icon->Resize(fit, image::ImageResizeType_Lanczos3Filter);
	else
		fitted = icon->ResizeWithConvertMode(fit, image::ImageResizeType_Lanczos3Filter,
						     ImageMode_RGBA8888,
						     image::ImageConvertModeType_Default);

	if (fitted.get() == NULL)
		return image::Image::GetErrorType();

	image::ImageBuffer pixels = fitted->ToBuffer(true);

	if (pixels.buff == NULL)
		return image::Image::GetErrorType();

	size_t rowSize = fitted->GetMemPitch(0) *
			 ((fitted->GetMemPitch(0) + fitted->GetPitchAlign() - 1) /
			  fitted->GetMemPitch(0));

	if (rowSize > GAME_PSP_ICON_PITCH)
		rowSize = GAME_PSP_ICON_PITCH;

	unsigned int rows = GAME_PSP_ICON_SIZE;

	if (fitted->GetMemHeight(0) <= GAME_PSP_ICON_SIZE)
		rows = fitted->GetMemHeight(0);

	unsigned int top = GAME_PSP_ICON_SIZE / 2 - fitted->GetMemHeight(0) / 2;

	if (rows != 0) {
		char *dst = (char *)work + top * GAME_PSP_ICON_PITCH + left * 4;
		const char *src = (const char *)pixels.buff;

		for (unsigned int i = 0; i < rows; i++) {
			sce_paf_memcpy(dst, src, rowSize);
			dst += GAME_PSP_ICON_PITCH;
			src += rowSize;
		}
	}

	image::ImageBuffer composed;
	image::ImageBuffer dds;

	composed.buff = work;
	composed.size = GAME_PSP_ICON_BUF_SIZE;

	void *ddsBuf = heap->sce_paf_malloc(GAME_PSP_ICON_DDS_SIZE);

	if (ddsBuf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	dds.buff = ddsBuf;
	dds.size = GAME_PSP_ICON_DDS_SIZE;

	image::ImageExtent dstExtent(GAME_PSP_ICON_SIZE, GAME_PSP_ICON_SIZE);
	image::ImageExtent srcExtent(GAME_PSP_ICON_SIZE, GAME_PSP_ICON_SIZE);

	if (!image::filter::ConvertMode(&dds, &composed, NULL, 0, GAME_PSP_ICON_PITCH, dstExtent,
					srcExtent, ImageMode_RGBA8888, image::ImageFormat_None,
					ImageMode_DXT1,
					image::ImageConvertModeType_MediumQuality)) {
		heap->sce_paf_free(ddsBuf);
		return GAME_ERROR_SHORT_IO;
	}

	image::ImageExtent ddsExtent(GAME_PSP_ICON_SIZE, GAME_PSP_ICON_SIZE);

	ret = image::Image::SaveAsDds(path.c_str(), dds.buff, GAME_PSP_ICON_SIZE, ddsExtent,
				      ImageMode_DXT1, 6, NULL);
	if (ret < 0) {
		heap->sce_paf_free(ddsBuf);
		return ret;
	}

	heap->sce_paf_free(ddsBuf);

	return 0;
}

int gameIconWrite(int kind, int attributeKind, const string *path, const void *buf, SceSize size)
{
	common::SharedPtr<memory::PoolAllocator> pool;
	int ret = gamePluginGetPool("GamePromoter", &pool);

	if (ret < 0)
		return ret;

	void *work = pool->Alloc();

	if (work != NULL) {
		ret = gamePspIconCompose(kind, attributeKind, work, GAME_PSP_ICON_BUF_SIZE, buf, size,
					 path->c_str());
		if (ret >= 0)
			ret = 0;

		pool->Free(work);
	}

	gamePluginReleasePool(&pool);

	return ret;
}
