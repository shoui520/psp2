/*
 * The game plugin's reader of a PSM application's app.info: the document's names, versions,
 * parental level and features read into the application's record, and its icon and LiveArea
 * background written out (0x810673D8..0x810695D0). The file's name and the functions' are ours
 * (no record names them); the element and attribute names are Sony's strings.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <cxml/cxml.h>
#include <file/localfile.h>
#include <framework.h>
#include <image/image.h>
#include <image/image_filter.h>
#include <memory/pool_allocator.h>

#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* an app.info whose document has no root element */
#define GAME_ERROR_NO_DOCUMENT	-2146430694	/* 0x8010111A */

/* what app.info's attributes may hold, with their terminating NUL */
#define GAME_PSM_VERSION_SIZE		6
#define GAME_PSM_SHORT_NAME_SIZE	52
#define GAME_PSM_NAME_SIZE		128
#define GAME_PSM_AUTHOR_SIZE		65
#define GAME_PSM_WEBSITE_SIZE		256
#define GAME_PSM_PARENTAL_LEVEL_MAX	11

/* ATTRIBUTE_MINOR of an application with an app_xml_format: with a PS Vita TV feature or not */
#define GAME_PSM_ATTRIBUTE_MINOR_TV	16
#define GAME_PSM_ATTRIBUTE_MINOR	24

/* the icon, a 128x128 RGBA8888 PNG, written as a DXT1 DDS */
#define GAME_PSM_ICON_SIZE		128
#define GAME_PSM_ICON_PITCH		(GAME_PSM_ICON_SIZE * 4)
#define GAME_PSM_ICON_DDS_SIZE		0x4000

/* the LiveArea background, an 854x480 8-bit indexed PNG */
#define GAME_PSM_PIC_WIDTH		854
#define GAME_PSM_PIC_HEIGHT		480

/* the locales app.info names, in the order of the system's languages (0x81544114) */
static const char *s_locales[] = {
	"ja-JP", "en-US", "fr-FR", "es-ES", "de-DE", "it-IT", "nl-NL", "pt-PT", "ru-RU", "ko-KR",
	"zh-Hant", "zh-Hans", "fi-FI", "sv-SE", "da-DK", "nb-NO", "pl-PL", "pt-BR", "en-GB",
};

GamePsmAppInfo::GamePsmAppInfo() : m_defaultLanguage(1)
{
}

GamePsmAppInfo::~GamePsmAppInfo()
{
	if (m_file.get() != NULL)
		m_file->Close();
}

/*
 * ATTRIBUTE_MINOR of the application: set from its features when app.info has an
 * app_xml_format, else cleared where its record has one (0x810674EE).
 */
int GamePsmAppInfo::SetAttributeMinor(const LsdbAppInfoPtr *info, const bool &appXmlFormat,
				      const bool &psVitaTv, LsdbAppInfoPtr *appInfo)
{
	if (appXmlFormat) {
		SceUInt32 attribute = GAME_PSM_ATTRIBUTE_MINOR_TV;

		if (!psVitaTv)
			attribute = GAME_PSM_ATTRIBUTE_MINOR;

		gameLsdbValueSetInteger((**appInfo)[LSDB_KEY_ATTRIBUTE_MINOR], attribute);
	} else if ((*info)->find(LSDB_KEY_ATTRIBUTE_MINOR) != (*info)->end()) {
		gameLsdbValueSetInteger((**appInfo)[LSDB_KEY_ATTRIBUTE_MINOR], 0);
	}

	return 0;
}

/* The features of a feature_list: the location library, PS Vita TV (0x810675FA). */
int GamePsmAppInfo::ReadFeatureList(const cxml::Element &list, LsdbAppInfoPtr *appInfo,
				    bool *psVitaTv)
{
	*psVitaTv = false;

	cxml::Element feature = list.GetFirstChild();

	while (feature) {
		const char *value = NULL;
		uint32_t len = 0;

		if (sce_paf_strncmp("feature", feature.GetName(), sizeof("feature")) != 0) {
			feature = feature.GetNextSibling();
			continue;
		}

		int ret = scePafResourceGetAttributeString(&feature, "value", &value, &len);

		if (ret < 0)
			return ret;

		if (sce_paf_strncmp("Location", value, sizeof("Location")) == 0)
			gameLsdbValueSetInteger((**appInfo)[LSDB_KEY_LIBLOCATION], 1);

		if (sce_paf_strncmp("PSVitaTV", value, sizeof("PSVitaTV")) == 0)
			*psVitaTv = true;

		feature = feature.GetNextSibling();
	}

	return 0;
}

/* The developer's name, the first child of developer (0x81067726). */
int GamePsmAppInfo::ReadDeveloper(const cxml::Element &developer)
{
	const char *value = NULL;
	uint32_t len = 0;
	cxml::Element name = developer.GetFirstChild();

	if (!name)
		return GAME_ERROR_BAD_DATA;

	if (sce_paf_strncmp("name", name.GetName(), sizeof("name")) != 0)
		return GAME_ERROR_BAD_DATA;

	int ret = scePafResourceGetAttributeString(&name, "value", &value, &len);

	if (ret < 0)
		return ret;

	if (len + 1 > GAME_PSM_AUTHOR_SIZE)
		return GAME_ERROR_BAD_DATA;

	m_author = value;

	return 0;
}

/* The parental lock level of parental_control (0x810677D4). */
__attribute__((noinline)) int GamePsmAppInfo::ReadParentalControl(const cxml::Element &control, LsdbAppInfoPtr *appInfo)
{
	int level = 0;
	int ret = scePafResourceGetAttributeInteger(&control, "lock_level", &level);

	if (ret < 0)
		return ret;

	if (level < 0 || level > GAME_PSM_PARENTAL_LEVEL_MAX)
		return GAME_ERROR_BAD_DATA;

	gameLsdbValueSetInteger((**appInfo)[LSDB_KEY_PARENTAL_LEVEL], (SceUInt32)level);

	return 0;
}

/* The system language of an app.info locale (0x8106787C). */
__attribute__((noinline)) int GamePsmAppInfo::GetLanguage(const string &locale)
{
	for (unsigned int i = 0; i < sizeof(s_locales) / sizeof(s_locales[0]); i++) {
		int found;

		{
			string name(s_locales[i]);

			found = locale == name;
		}

		if (found)
			return i;
	}

	return GAME_ERROR_BAD_STORAGE;
}

/*
 * The localized short names of short_name, each under its language's key, and the default
 * language's as STITLE (0x81067930).
 */
int GamePsmAppInfo::ReadShortName(const cxml::Element &name, LsdbAppInfoPtr *appInfo)
{
	const char *value = NULL;
	uint32_t len = 0;
	unsigned int key = 0;
	cxml::Element item = name.GetFirstChild();

	while (item) {
		if (sce_paf_strncmp("localized_item", item.GetName(),
				    sizeof("localized_item")) != 0) {
			item = item.GetNextSibling();
			continue;
		}

		int ret = scePafResourceGetAttributeString(&item, "locale", &value,
							   &len);

		if (ret < 0)
			return ret;

		int language = GetLanguage(string(value));

		if (language < 0)
			return GAME_ERROR_BAD_DATA;

		if (gameStitleKey(language, &key) < 0)
			return GAME_ERROR_BAD_DATA;

		ret = scePafResourceGetAttributeString(&item, "value", &value, &len);
		if (ret < 0)
			return ret;

		if (len + 1 > GAME_PSM_SHORT_NAME_SIZE)
			return GAME_ERROR_BAD_DATA;

		gameLsdbValueSetText((**appInfo)[key], value);

		item = item.GetNextSibling();
	}

	if (gameStitleKey(m_defaultLanguage, &key) < 0)
		return GAME_ERROR_BAD_DATA;

	LsdbAppInfo::iterator it = (*appInfo)->find(key);

	if (it == (*appInfo)->end())
		return GAME_ERROR_BAD_DATA;

	gameLsdbValueSetText((**appInfo)[LSDB_KEY_STITLE], LsdbValueText(it->second));

	return 0;
}

/*
 * The localized names of name, each under its language's key, and the default language's as
 * TITLE (0x81067BB6).
 */
int GamePsmAppInfo::ReadName(const cxml::Element &name, LsdbAppInfoPtr *appInfo)
{
	const char *value = NULL;
	uint32_t len = 0;
	unsigned int key = 0;
	cxml::Element item = name.GetFirstChild();

	while (item) {
		if (sce_paf_strncmp("localized_item", item.GetName(),
				    sizeof("localized_item")) != 0) {
			item = item.GetNextSibling();
			continue;
		}

		int ret = scePafResourceGetAttributeString(&item, "locale", &value,
							   &len);

		if (ret < 0)
			return ret;

		int language = GetLanguage(string(value));

		if (language < 0)
			return GAME_ERROR_BAD_DATA;

		if (gameTitleKey(language, &key) < 0)
			return GAME_ERROR_BAD_DATA;

		ret = scePafResourceGetAttributeString(&item, "value", &value, &len);
		if (ret < 0)
			return ret;

		if (len + 1 > GAME_PSM_NAME_SIZE)
			return GAME_ERROR_BAD_DATA;

		gameLsdbValueSetText((**appInfo)[key], value);

		item = item.GetNextSibling();
	}

	if (gameTitleKey(m_defaultLanguage, &key) < 0)
		return GAME_ERROR_BAD_DATA;

	LsdbAppInfo::iterator it = (*appInfo)->find(key);

	if (it == (*appInfo)->end())
		return GAME_ERROR_BAD_DATA;

	gameLsdbValueSetText((**appInfo)[LSDB_KEY_TITLE], LsdbValueText(it->second));

	return 0;
}

/*
 * The root's versions: the application's (APP_VER), the runtime's (_disp_ver, and as a number
 * _system_ver), and the default locale (0x81067E3C).
 */
int GamePsmAppInfo::ReadVersions(const cxml::Element &root, LsdbAppInfoPtr *appInfo)
{
	const char *value = NULL;
	uint32_t len = 0;
	int ret = scePafResourceGetAttributeString(&root, "version", &value, &len);

	if (ret < 0)
		return ret;

	if (len + 1 > GAME_PSM_VERSION_SIZE)
		return GAME_ERROR_BAD_DATA;

	ret = value[1] == '.' ? gameVersionCheckShort(value) : gameVersionCheck(value);
	if (ret < 0)
		return ret;

	gameLsdbValueSetText((**appInfo)[LSDB_KEY_APP_VER], value);

	ret = scePafResourceGetAttributeString(&root, "runtime_version", &value, &len);
	if (ret < 0)
		return ret;

	if (value[1] == '.') {
		ret = gameVersionCheckShort(value);
		if (ret < 0)
			return ret;

		gameLsdbValueSetText((**appInfo)[LSDB_KEY_DISP_VER], value);

		SceUInt32 number = 0;

		ret = gameVersionToNumber(value, len + 1, &number);
		if (ret < 0)
			return ret;

		gameLsdbValueSetInteger((**appInfo)[LSDB_KEY_SYSTEM_VER], number);
	} else {
		ret = gameVersionCheck(value);
		if (ret < 0)
			return ret;

		gameLsdbValueSetText((**appInfo)[LSDB_KEY_DISP_VER], value);

		SceUInt32 number = 0;

		ret = gameVersionToNumber(value, len + 1, &number);
		if (ret < 0)
			return ret;

		gameLsdbValueSetInteger((**appInfo)[LSDB_KEY_SYSTEM_VER], number);
	}

	ret = scePafResourceGetAttributeString(&root, "default_locale", &value, &len);
	if (ret < 0)
		return ret;

	ret = GetLanguage(string(value));
	if (ret < 0)
		return ret;

	m_defaultLanguage = ret;

	return 0;
}

/* The developer's link of website (Sony's build inlines it into Load). */
inline int GamePsmAppInfo::ReadWebsite(const cxml::Element &website)
{
	const char *value = NULL;
	uint32_t len = 0;
	int ret = scePafResourceGetAttributeString(&website, "href", &value, &len);

	if (ret < 0)
		return ret;

	if (len + 1 > GAME_PSM_WEBSITE_SIZE)
		return GAME_ERROR_BAD_DATA;

	m_website = value;

	return 0;
}

int GamePsmAppInfo::Load(const char *path, const LsdbAppInfoPtr *info, LsdbAppInfoPtr *appInfo)
{
	int ret = 0;

	m_file = LocalFile::Open(path, SCE_O_RDONLY, 0, &ret);
	if (ret < 0)
		return ret;

	ret = m_doc.CreateFromFile(m_file, false);
	if (ret < 0)
		return ret;

	if (!m_doc.GetDocumentElement())
		return GAME_ERROR_NO_DOCUMENT;

	cxml::Element root = m_doc.GetDocumentElement();

	if (!root)
		return GAME_ERROR_BAD_DATA;

	ret = ReadVersions(root, appInfo);
	if (ret < 0)
		return ret;

	bool appXmlFormat = false;
	bool psVitaTv = false;
	cxml::Element elem = root.GetFirstChild();

	while (elem) {
		string name(elem.GetName());

		if (name == string("name")) {
			ret = ReadName(elem, appInfo);
		} else if (name == string("short_name")) {
			ret = ReadShortName(elem, appInfo);
		} else if (name == string("parental_control")) {
			ret = ReadParentalControl(elem, appInfo);
		} else if (name == string("developer")) {
			ret = ReadDeveloper(elem);
		} else if (name == string("website")) {
			ret = ReadWebsite(elem);
		} else if (name == string("feature_list")) {
			ret = ReadFeatureList(elem, appInfo, &psVitaTv);
		} else if (name == string("app_xml_format")) {
			appXmlFormat = true;
		} else {
			elem = elem.GetNextSibling();
			continue;
		}

		if (ret < 0)
			return ret;

		elem = elem.GetNextSibling();
	}

	ret = SetAttributeMinor(info, appXmlFormat, psVitaTv, appInfo);
	if (ret < 0)
		return ret;

	return 0;
}

int GamePsmAppInfo::WriteIcon(const string &path)
{
	cxml::Element root = m_doc.GetDocumentElement();

	if (!root)
		return GAME_ERROR_BAD_DATA;

	cxml::Element images = cxml::util::FindChildElement(root, "images");

	if (!images)
		return GAME_ERROR_BAD_DATA;

	cxml::File icon;
	int ret = scePafResourceGetAttributeData(&images, "icon_128x128", &icon);

	if (ret < 0)
		return ret;

	if (icon.doc == NULL || icon.offset < 0)
		return GAME_ERROR_BAD_DATA;

	SceSize size = icon.size;
	char *buf = new char[size];

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = icon.Load(m_file, buf, size);
	if (ret < 0) {
		delete[] buf;
		return ret;
	}

	memory::HeapAllocator *heap = Framework::Instance()->GetDecodeHeap();

	if (heap == NULL) {
		delete[] buf;
		return GAME_ERROR_NOT_READY;
	}

	common::SharedPtr<image::Image> img =
		image::Image::Open(buf, size, heap, image::ImageFormat_None);

	if (img.get() == NULL) {
		delete[] buf;
		return image::Image::GetErrorType();
	}

	if (img->GetFormat() != image::ImageFormat_Png) {
		delete[] buf;
		return GAME_ERROR_BAD_ICON;
	}

	if (img->GetMode() != ImageMode_RGBA8888) {
		delete[] buf;
		return GAME_ERROR_BAD_ICON;
	}

	image::ImageExtent extent = img->GetExtent();

	if (extent.w != GAME_PSM_ICON_SIZE) {
		delete[] buf;
		return GAME_ERROR_BAD_ICON;
	}

	if (extent.h != GAME_PSM_ICON_SIZE) {
		delete[] buf;
		return GAME_ERROR_BAD_ICON;
	}

	image::ImageBuffer pixels = img->ToBuffer(true);

	if (pixels.buff == NULL) {
		delete[] buf;
		return image::Image::GetErrorType();
	}

	image::ImageBuffer src = pixels;
	image::ImageBuffer dds;
	void *ddsBuf = heap->sce_paf_malloc(GAME_PSM_ICON_DDS_SIZE);

	if (ddsBuf == NULL) {
		delete[] buf;
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	dds.buff = ddsBuf;
	dds.size = GAME_PSM_ICON_DDS_SIZE;

	image::ImageExtent dstExtent(GAME_PSM_ICON_SIZE, GAME_PSM_ICON_SIZE);
	image::ImageExtent srcExtent(GAME_PSM_ICON_SIZE, GAME_PSM_ICON_SIZE);

	if (!image::filter::ConvertMode(&dds, &src, NULL, 0, GAME_PSM_ICON_PITCH, dstExtent,
					srcExtent, ImageMode_RGBA8888, image::ImageFormat_None,
					ImageMode_DXT1,
					image::ImageConvertModeType_MediumQuality)) {
		delete[] buf;
		heap->sce_paf_free(ddsBuf);
		return GAME_ERROR_SHORT_IO;
	}

	image::ImageExtent ddsExtent(GAME_PSM_ICON_SIZE, GAME_PSM_ICON_SIZE);

	ret = image::Image::SaveAsDds(path.c_str(), dds.buff, GAME_PSM_ICON_SIZE, ddsExtent,
				      ImageMode_DXT1, 6, NULL);
	if (ret < 0) {
		delete[] buf;
		heap->sce_paf_free(ddsBuf);
		return ret;
	}

	heap->sce_paf_free(ddsBuf);
	delete[] buf;

	return 0;
}

/*
 * The image file attribute name of images copied to path through buf, then checked to be the
 * LiveArea background, an 854x480 8-bit indexed PNG (0x81068D7E).
 */
int GamePsmAppInfo::WriteImage(const string &name, const string &path, void *buf,
			       SceSize bufSize)
{
	int ret = 0;
	cxml::Element root = m_doc.GetDocumentElement();

	if (!root)
		return GAME_ERROR_BAD_DATA;

	cxml::Element images = cxml::util::FindChildElement(root, "images");

	if (!images)
		return GAME_ERROR_BAD_DATA;

	cxml::File image;

	ret = scePafResourceGetAttributeData(&images, name.c_str(), &image);
	if (ret < 0)
		return ret;

	if (image.doc == NULL || image.offset < 0)
		return GAME_ERROR_BAD_DATA;

	SceSize remain = image.size;

	ret = m_file->Seek((SceUInt32)(image.doc->header.filetable_offset + image.offset),
			   SCE_SEEK_SET);
	if (ret < 0)
		return ret;

	common::SharedPtr<File> dst =
		LocalFile::Open(path.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 6, &ret);

	if (ret < 0)
		return ret;

	while (remain != 0) {
		if (bufSize > remain)
			bufSize = remain;

		ret = m_file->Read(buf, bufSize);
		if (ret < 0) {
			dst->Close();
			return ret;
		}

		ret = dst->Write(buf, bufSize);
		if (ret < 0) {
			dst->Close();
			return ret;
		}

		remain -= bufSize;
	}

	dst->Close();

	common::SharedPtr<image::Image> img =
		image::Image::Open(path.c_str(), NULL, image::ImageFormat_None, 0, 0);

	if (img.get() == NULL)
		return image::Image::GetErrorType();

	if (img->GetFormat() != image::ImageFormat_Png)
		return GAME_ERROR_BAD_ICON;

	if (img->GetMode() != ImageMode_index8)
		return GAME_ERROR_BAD_ICON;

	image::ImageExtent extent = img->GetExtent();

	if (extent.w != GAME_PSM_PIC_WIDTH)
		return GAME_ERROR_BAD_ICON;

	if (extent.h != GAME_PSM_PIC_HEIGHT)
		return GAME_ERROR_BAD_ICON;

	return 0;
}

int GamePsmAppInfo::WritePic(const string &path)
{
	common::SharedPtr<memory::PoolAllocator> pool;
	int ret = gamePluginGetPool("FileOutLiveAreaBgImage", &pool);

	if (ret < 0)
		return ret;

	SceSize size = pool->GetFreeSize();
	void *buf = pool->Alloc();

	if (buf != NULL) {
		ret = WriteImage(string("splash_854x480"), path, buf, size);
		if (ret >= 0)
			ret = 0;
	} else {
		ret = GAME_ERROR_NO_MEMORY;
	}

	if (buf != NULL)
		pool->Free(buf);

	gamePluginReleasePool(&pool);

	return ret;
}
