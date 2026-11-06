/*
 * paf::graph::FontRasterizerPvf: the system fonts through the PVF library (the file's name
 * ours). Init makes the library with its own heap and glyph cache and records each slot's
 * font: in memory, in a file, or the system's (on shared memory, by index, or the one the PVF
 * library finds for the slot's language). Every rasterizer opens its style's fonts by font
 * index through a map that keeps them open between styles, and draws each character with the
 * slot of its script, as 4-bit glyph coverage into an indexed text surface.
 */
#include <stddef.h>
#include <stdint.h>
#include <font/libpvf.h>
#include <system_param.h>
#include <framework.h>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <graphics/text/font_rasterizer.h>
#include <graphics/text/text_style.h>
#include <graphics/text/text_surface_cache.h>
#include <memory/heap_allocator.h>
#include <std/map>
#include <std/string>
#include <std/string.h>
#include <thread/thread.h>
#include "fontcache.h"
#include "glyph_metrics.h"
#include "../../paf_types.h"

extern "C" int sceAppMgrConvertVs0UserDrivePath(const char *path, char *out, int out_size);

namespace paf {
namespace graph {

/* the system fonts a font index from 0x10000 opens (name ours) */
static const char *const s_font_paths[] = {
	"sa0:data/font/pvf/music_arib.pvf",
	"vs0:data/external/font/pvf/e046323ms.ttf",
	"vs0:data/external/font/pvf/e046323ts.ttf",
	"vs0:data/external/font/pvf/n023055ms.ttf",
	"vs0:data/external/font/pvf/n023055ts.ttf",
	"vs0:data/external/font/pvf/d013013ds.ttf",
	"vs0:data/external/font/pvf/k006004ds.ttf",
	"vs0:data/external/font/pvf/c041056ts.ttf"
};

/* the glyphs' colour table: white, its alpha the 4-bit coverage (name ours) */
static const uint32_t s_clut_table[16] = {
	0x00ffffff, 0x11ffffff, 0x22ffffff, 0x33ffffff,
	0x44ffffff, 0x55ffffff, 0x66ffffff, 0x77ffffff,
	0x88ffffff, 0x99ffffff, 0xaaffffff, 0xbbffffff,
	0xccffffff, 0xddffffff, 0xeeffffff, 0xffffffff
};

/* where a slot's font comes from and how it is drawn (names ours) */
class FontSource {
public:
	FontSource() : mem_addr(NULL), mem_size(0), path(""), font(NULL),
		       mode(SCE_PVF_FILEBASEDSTREAM)
	{
		embolden_rate[0] = 0.0f;
		embolden_rate[1] = 0.0f;
		skew_value[0] = 0.0f;
		skew_value[1] = 0.0f;
	}

	virtual ~FontSource()
	{
	}

	void *mem_addr;
	uint32_t mem_size;
	string path;
	/* the system's font on shared memory */
	ScePvf_t_fontId font;
	/* the ScePvfDataAccessMode it is opened with */
	uint32_t mode;
	/* by the style's boldness and obliqueness */
	float embolden_rate[2];
	float skew_value[2];
};

typedef map<ScePvf_t_fontIndex, common::SharedPtr<FontRasterizerPvf::Font> > FontMap;

static memory::HeapAllocator *s_heap = NULL;
static void *s_cache = NULL;
static ScePvf_t_libId s_lib = NULL;
/* the Locale */
static int32_t s_locale = -1;

static uint32_t s_option;
static intrusive_ptr<SurfaceCLUT> s_clut(NULL);
static FontSource s_fonts[FontRasterizerPvf::Font_Num];
/* the fonts open, by font index */
static FontMap s_font_map;
static thread::Mutex s_mutex("ScePafFontRasterizerPvfMutex", 0);

static ScePvf_t_pointer Alloc(ScePvf_t_pointer user_data, ScePvf_t_u32 size);
static ScePvf_t_pointer Realloc(ScePvf_t_pointer user_data, ScePvf_t_pointer ptr, ScePvf_t_u32 size);
static void Free(ScePvf_t_pointer user_data, ScePvf_t_pointer ptr);
static void *CacheAlloc(int size);
static void CacheFree(void *ptr);
static unsigned char *CacheMemcpy(unsigned char *dst, unsigned char *src, int size);

/* a 26.6 fixed-point value in whole pixels, rounded away from zero (name ours) */
static inline int32_t Round64(int32_t value)
{
	return (value >= 0 ? value + 63 : value - 63) >> 6;
}

int32_t FontRasterizerPvf::Term()
{
	s_font_map.clear();
	for (uint32_t i = 0; i < Font_Num; i++) {
		s_fonts[i].mem_addr = NULL;
		s_fonts[i].mem_size = 0;
		s_fonts[i].path = "";
		if (s_fonts[i].font != NULL) {
			scePvfClose(s_fonts[i].font);
			s_fonts[i].font = NULL;
		}
	}
	s_clut.clear();
	if (s_lib != NULL) {
		scePvfDoneLib(s_lib);
		s_lib = NULL;
	}
	if (s_cache != NULL) {
		myPvfCache_delete(s_cache);
		s_cache = NULL;
	}
	if (s_heap != NULL) {
		delete s_heap;
		s_heap = NULL;
	}
	return 0;
}

int32_t FontRasterizerPvf::SetFont(const FontParam &param)
{
	s_font_map.clear();
	myPvfCache_expireAll(s_cache, 1);
	s_fonts[Font_Japanese].mem_addr = param.mem_japanese_addr;
	s_fonts[Font_Japanese].mem_size = param.mem_japanese_size;
	s_fonts[Font_Japanese].path = param.path_japanese;
	s_fonts[Font_Latin].mem_addr = param.mem_latin_addr;
	s_fonts[Font_Latin].mem_size = param.mem_latin_size;
	s_fonts[Font_Latin].path = param.path_latin;
	s_fonts[Font_Hangul].mem_addr = param.mem_hangul_addr;
	s_fonts[Font_Hangul].mem_size = param.mem_hangul_size;
	s_fonts[Font_Hangul].path = param.path_hangul;
	s_fonts[Font_Chinese].mem_addr = param.mem_chinese_addr;
	s_fonts[Font_Chinese].mem_size = param.mem_chinese_size;
	s_fonts[Font_Chinese].path = param.path_chinese;
	s_fonts[Font_Arib].mem_addr = param.mem_arib_addr;
	s_fonts[Font_Arib].mem_size = param.mem_arib_size;
	s_fonts[Font_Arib].path = param.path_arib;
	return 0;
}

int32_t FontRasterizerPvf::SetLocale(int32_t locale)
{
	s_locale = locale;
	return 0;
}

int32_t FontRasterizerPvf::Init(const InitParam &param)
{
	ScePvf_t_error error = SCE_PAF_ERROR_TEXT_NO_MEMORY;
	uint32_t size = 0x100000;
	bool cdlg;
	memory::HeapAllocator::Option option;

	if (param.heap_size != 0)
		size = param.heap_size;
	cdlg = false;
	option.entry_max = 0;
	option.warning_size = 0;
	option.disable_dbgprint = true;
	option.disable_assert_on_alloc_failure = true;
	option.memblk_type = memory::AllocationBlockType_User;
	option.alignment = 0;
	if (Framework::Instance() != NULL &&
	    Framework::Instance()->GetMode() == Framework::Mode_CommonDialog) {
		cdlg = true;
		option.memblk_type = memory::AllocationBlockType_User_Cdlg;
	}
	s_option = param.option;
	s_locale = 0;
	s_heap = new memory::HeapAllocator(NULL, size, "ScePafFontRasterizerPvfHeap", &option);
	if (s_heap != NULL) {
		myPvfCache_t_initRec cache_param;

		cache_param.allocFunc = CacheAlloc;
		cache_param.freeFunc = CacheFree;
		cache_param.memcpyFunc = CacheMemcpy;
		s_cache = myPvfCache_new(&cache_param, 383);
		if (s_cache != NULL) {
			ScePvf_t_initRec init;

			init.userData = NULL;
			init.maxNumFonts = 18;
			init.cache = myPvfCache_getCacheInterfaceHandle(s_cache);
			init.reserved = NULL;
			init.allocFunc = Alloc;
			init.reallocFunc = Realloc;
			init.freeFunc = Free;
			s_lib = scePvfNewLib(&init, &error);
			if (error == 0)
				error = scePvfSetResolution(s_lib, 72.0f, 72.0f);
		}
	}
	if (error == 0)
		error = SetLocale(param.locale);
	if (error == 0)
		error = SetFont(param.font);
	if (error == 0 && s_fonts[Font_Japanese].mem_addr == NULL &&
	    s_fonts[Font_Japanese].path == "")
		s_fonts[Font_Japanese].font =
			scePvfOpenDefaultJapaneseFontOnSharedMemory(s_lib, &error);
	if (error == 0 && s_fonts[Font_Latin].mem_addr == NULL && s_fonts[Font_Latin].path == "")
		s_fonts[Font_Latin].font = scePvfOpenDefaultLatinFontOnSharedMemory(s_lib, &error);
	if (error == 0) {
		float rate;

		if (s_option & Option_MemoryJapanese)
			s_fonts[Font_Japanese].mode = SCE_PVF_MEMORYBASEDSTREAM;
		if (s_option & Option_MemoryLatin)
			s_fonts[Font_Latin].mode = SCE_PVF_MEMORYBASEDSTREAM;
		if (s_option & Option_MemoryHangul)
			s_fonts[Font_Hangul].mode = SCE_PVF_MEMORYBASEDSTREAM;
		if (s_option & Option_MemoryChinese)
			s_fonts[Font_Chinese].mode = SCE_PVF_MEMORYBASEDSTREAM;
		if (s_option & Option_MemoryArib)
			s_fonts[Font_Arib].mode = SCE_PVF_MEMORYBASEDSTREAM;
		if (s_option & Option_MemoryFamily)
			s_fonts[Font_Family].mode = SCE_PVF_MEMORYBASEDSTREAM;
		rate = 5.0f;
		if (!cdlg)
			rate = 0.0f;
		s_fonts[Font_Japanese].embolden_rate[0] = rate;
		s_fonts[Font_Japanese].embolden_rate[1] = 5.0f;
		s_fonts[Font_Japanese].skew_value[0] = 0.0f;
		s_fonts[Font_Japanese].skew_value[1] = 15.0f;
		s_fonts[Font_Latin].embolden_rate[0] = rate;
		s_fonts[Font_Latin].embolden_rate[1] = 5.0f;
		s_fonts[Font_Latin].skew_value[0] = 0.0f;
		s_fonts[Font_Latin].skew_value[1] = 15.0f;
		s_fonts[Font_Hangul].embolden_rate[0] = rate;
		s_fonts[Font_Hangul].embolden_rate[1] = 5.0f;
		s_fonts[Font_Hangul].skew_value[0] = 0.0f;
		s_fonts[Font_Hangul].skew_value[1] = 15.0f;
		s_fonts[Font_Chinese].embolden_rate[0] = 0.0f;
		s_fonts[Font_Chinese].embolden_rate[1] = 3.0f;
		s_fonts[Font_Chinese].skew_value[0] = 0.0f;
		s_fonts[Font_Chinese].skew_value[1] = 15.0f;
		s_fonts[Font_Arib].embolden_rate[0] = rate;
		s_fonts[Font_Arib].embolden_rate[1] = 5.0f;
		s_fonts[Font_Arib].skew_value[0] = 0.0f;
		s_fonts[Font_Arib].skew_value[1] = 15.0f;
		s_fonts[Font_Family].embolden_rate[0] = rate;
		s_fonts[Font_Family].embolden_rate[1] = 5.0f;
		s_fonts[Font_Family].skew_value[0] = 0.0f;
		s_fonts[Font_Family].skew_value[1] = 15.0f;
	}
	if (error < 0)
		Term();
	return error;
}

FontRasterizerPvf::FontRasterizerPvf()
	: m_dirty(false), m_point(0.0f, 0.0f), m_bold(false), m_oblique(false), m_shadow(false),
	  m_family(0), m_height(0), m_ascender(0)
{
}

int32_t FontRasterizerPvf::ReleaseFonts()
{
	s_mutex.Lock();
	for (uint32_t i = 0; i < Font_Num; i++)
		m_fonts[i].reset();
	s_mutex.Unlock();
	m_bold = false;
	m_oblique = false;
	m_shadow = false;
	m_height = 0;
	m_point = math::v2(0.0f, 0.0f);
	m_ascender = 0;
	return 0;
}

FontRasterizerPvf::~FontRasterizerPvf()
{
	ReleaseFonts();
}

FontRasterizerPvf::Font::Font(int32_t slot, ScePvf_t_fontIndex index) : m_id(NULL), m_opened(false)
{
	ScePvf_t_error error = SCE_PAF_ERROR_TEXT_ABORTED;
	FontSource *source = &s_fonts[slot];

	if (index >= 0x30000) {
		m_id = scePvfOpenUserMemory(s_lib, source->mem_addr, source->mem_size, &error);
	} else if (index >= 0x20000) {
		m_id = scePvfOpenUserFile(s_lib, (ScePvf_t_pointer)source->path.c_str(), source->mode,
					  &error);
	} else if (index >= 0x10000) {
		char path[256];

		sceAppMgrConvertVs0UserDrivePath(s_font_paths[index - 0x10000], path, sizeof(path));
		m_id = scePvfOpenUserFile(s_lib, path, source->mode, &error);
	} else if (index < 0) {
		m_id = source->font;
	} else {
		m_id = scePvfOpen(s_lib, index, source->mode, &error);
	}
	if (error == 0)
		m_opened = true;
}

ScePvf_t_fontIndex FontRasterizerPvf::GetFontIndex(int32_t slot)
{
	ScePvf_t_error error;
	int32_t family = m_family;
	ScePvf_t_fontIndex index = slot;

	if (family > 0) {
		if (slot == Font_Family)
			index = family + 0x10000 - 2;
		else
			family = 0;
	}
	if (family == 0) {
		FontSource *source = &s_fonts[slot];

		if (source->font != NULL)
			index -= 0x10000;
		else if (source->path != "")
			index += 0x20000;
		else if (source->mem_addr != NULL)
			index += 0x30000;
	}
	if (index == slot) {
		if (slot == Font_Arib) {
			index = 0x10000;
		} else {
			ScePvf_t_fontStyleInfo style;

			sce_paf_memset(&style, 0, sizeof(style));
			switch (family) {
			case 1:
				style.familyCode = SCE_PVF_FAMILY_SANSERIF;
				break;
			case 2:
				style.familyCode = SCE_PVF_FAMILY_SERIF;
				break;
			default:
				style.familyCode = SCE_PVF_DEFAULT_FAMILY_CODE;
				break;
			}
			switch (slot) {
			case Font_Japanese:
				style.languageCode = SCE_PVF_LANGUAGE_J;
				break;
			case Font_Latin:
				style.languageCode = SCE_PVF_LANGUAGE_LATIN;
				break;
			case Font_Hangul:
				style.languageCode = SCE_PVF_LANGUAGE_K;
				break;
			case Font_Chinese:
				style.languageCode = SCE_PVF_LANGUAGE_C;
				break;
			default:
				style.languageCode = SCE_PVF_DEFAULT_LANGUAGE_CODE;
				break;
			}
			index = scePvfFindOptimumFont(s_lib, &style, &error);
		}
	}
	return index;
}

int32_t FontRasterizerPvf::LoadFonts()
{
	int32_t slot = Font_Chinese;
	int32_t skip = Font_Arib;

	if (s_option & Option_Arib) {
		slot = Font_Arib;
		skip = -1;
	}
	if (m_family > 0)
		slot = Font_Family;
	s_mutex.Lock();
	scePvfSetEM(s_lib, 1.0f / m_point.extract_y());
	for (; slot >= 0; slot--) {
		if (slot == skip)
			continue;

		ScePvf_t_fontIndex index = GetFontIndex(slot);
		FontMap::iterator it = s_font_map.find(index);

		if (it != s_font_map.end()) {
			m_fonts[slot] = it->second;
		} else {
			it = s_font_map.end();
			while (it != s_font_map.begin() && s_font_map.size() >= 4) {
				--it;
				if (it->second.get() == NULL || it->second.m_ref_counter->m_counter <= 1)
					s_font_map.erase(it++);
			}
			m_fonts[slot] = common::SharedPtr<Font>(new Font(slot, index));
			if (m_fonts[slot].get() != NULL)
				s_font_map[index] = m_fonts[slot];
		}

		Font *font = m_fonts[slot].get();
		ScePvf_t_fontInfo info;

		if (font != NULL && font->m_id != NULL && scePvfGetFontInfo(font->m_id, &info) == 0) {
			int32_t ascender = info.maxIGlyphMetrics.ascender64;

			if (m_height < ascender - info.maxIGlyphMetrics.descender64)
				m_height = ascender - info.maxIGlyphMetrics.descender64;
			if (m_ascender < ascender)
				m_ascender = ascender;
		}
	}
	s_mutex.Unlock();
	m_height = Round64(m_height);
	m_ascender = Round64(m_ascender);

	int32_t margin = (int32_t)(m_point.extract_y() * 0.1f);

	if (m_height > 0)
		m_height = (int32_t)((float)(margin + 2) + m_point.extract_y() * 0.07f + (float)m_height);
	if (m_ascender > 0)
		m_ascender = m_ascender + margin;
	return 0;
}

inline void FontRasterizerPvf::ApplyStyle()
{
	ReleaseFonts();
	if (m_style.get() != NULL) {
		m_style->GetStyleAttribute(TextStyleAttribute_DrawPoint, m_point);
		m_style->GetStyleAttribute(TextStyleAttribute_DrawBold, m_bold);
		m_style->GetStyleAttribute(TextStyleAttribute_Oblique, m_oblique);
		m_style->GetStyleAttribute(TextStyleAttribute_Shadow, m_shadow);
		m_style->GetStyleAttribute(TextStyleAttribute_Family, m_family);
		LoadFonts();
	}
	m_dirty = false;
}

int32_t FontRasterizerPvf::Update()
{
	LoadFonts();
	return 0;
}

int32_t FontRasterizerPvf::SetStyle(const common::SharedPtr<TextStyle> &style)
{
	if (style.get() == NULL)
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
	if (style.get() != m_style.get() &&
	    (m_style.get() == NULL || !style->EqualFontStyle(*m_style))) {
		m_style = style;
		m_dirty = true;
	}
	return 0;
}

int32_t FontRasterizerPvf::ResetStyle()
{
	if (m_style.get() == NULL)
		return SCE_PAF_ERROR_TEXT_ABORTED;
	m_style.reset();
	m_dirty = true;
	ApplyStyle();
	return 0;
}

int32_t FontRasterizerPvf::GetFontMetrics(FontMetrics &metrics)
{
	if (m_style.get() == NULL)
		return SCE_PAF_ERROR_TEXT_ABORTED;
	if (m_dirty)
		ApplyStyle();
	metrics.scale = 1.0f;
	metrics.height = (float)m_height;
	metrics.ascender = (float)m_ascender;
	return 0;
}

int32_t FontRasterizerPvf::GetCharInfo(int32_t slot, ScePvf_t_fontId font, ScePvf_t_charCode c,
				       ScePvf_t_charInfo *info)
{
	int32_t ret = 0;
	math::v2 point = m_point;
	float width = point.extract_x();
	float height = point.extract_y();

	/* the ring above */
	if (c == 0xc5)
		height = height * 0.92f;
	scePvfSetCharSize(font, width, height);
	switch (slot) {
	case Font_Hangul:
		info->glyphMetrics.width64 = HangulWidth64(width, 72.0f);
		info->glyphMetrics.horizontalBearingX64 = HangulBearingX64(width, 72.0f);
		info->glyphMetrics.horizontalAdvance64 = HangulAdvance64(c, width, 72.0f);
		break;
	case Font_Chinese:
		info->glyphMetrics.width64 = ChineseWidth64(width, 72.0f);
		info->glyphMetrics.horizontalBearingX64 = ChineseBearingX64(width, 72.0f);
		info->glyphMetrics.horizontalAdvance64 = ChineseAdvance64(c, width, 72.0f);
		break;
	default:
		ret = scePvfGetCharInfo(font, c, info);
		break;
	}
	return ret;
}

int32_t FontRasterizerPvf::GetFontSlot(ScePvf_t_charCode *c)
{
	int32_t slot = Font_Japanese;
	ScePvf_t_charCode code;

	if (m_family > 0) {
		Font *font = m_fonts[Font_Family].get();
		ScePvf_t_fontId id;

		if (font != NULL && (id = font->m_id) != NULL && scePvfIsElement(id, *c))
			return Font_Family;
	}
	code = *c;
	switch (code & 0xff00) {
	case 0x0000:
	case 0x0300:
	case 0x0400:
		return Font_Latin;
	case 0x0100:
	case 0x0200: {
		Font *font = m_fonts[Font_Latin].get();
		ScePvf_t_fontId id;

		if (font != NULL && (id = font->m_id) != NULL && scePvfIsElement(id, code))
			return Font_Latin;
		code = *c;
		break;
	}
	case 0x2000:
		/* the quotation marks and the ellipsis */
		if (s_locale != SCE_SYSTEM_PARAM_LANG_JAPANESE &&
		    s_locale != SCE_SYSTEM_PARAM_LANG_KOREAN &&
		    s_locale != SCE_SYSTEM_PARAM_LANG_CHINESE_T &&
		    s_locale != SCE_SYSTEM_PARAM_LANG_CHINESE_S &&
		    ((code >= 0x2018 && code <= 0x201f) || code == 0x2026))
			return Font_Latin;
		break;
	case 0x3100:
		/* Hangul compatibility jamo */
		if (code >= 0x3130 && code <= 0x318f)
			return Font_Hangul;
		break;
	case 0x3200:
		/* parenthesized and circled Hangul */
		if ((code >= 0x3200 && code <= 0x321f) || (code >= 0x3260 && code <= 0x327f))
			return Font_Hangul;
		break;
	case 0xff00:
		/* halfwidth Hangul and the won sign */
		if ((code >= 0xffa0 && code <= 0xffdf) || code == 0xffe6)
			return Font_Hangul;
		break;
	default:
		/* Hangul syllables */
		if (code >= 0xac00 && code <= 0xd7af)
			return Font_Hangul;
		/* the CJK ideographs, in Korean from the Chinese font */
		if (s_locale == SCE_SYSTEM_PARAM_LANG_KOREAN &&
		    ((code >= 0x4e00 && code <= 0x9fff) || (code >= 0xf900 && code <= 0xfaff)))
			return Font_Chinese;
		if (((s_option & Option_Arib) &&
		     ((code >= 0x2155 && code <= 0x215a) || code == 0x277f || code == 0x33ca ||
		      (code >= 0xe0c9 && code <= 0xe659))) ||
		    (s_locale != SCE_SYSTEM_PARAM_LANG_KOREAN &&
		     s_locale != SCE_SYSTEM_PARAM_LANG_CHINESE_T &&
		     s_locale != SCE_SYSTEM_PARAM_LANG_CHINESE_S &&
		     (code == 0x3016 || code == 0x3017 || code == 0x5eac || code == 0x7b7f)))
			return Font_Arib;
		/* the combining (semi-)voiced sound marks drawn as the halfwidth ones */
		if (code == 0x3099) {
			code = 0xff9e;
			*c = 0xff9e;
		} else if (code == 0x309a) {
			code = 0xff9f;
			*c = 0xff9f;
		}
		break;
	}

	Font *japanese = m_fonts[Font_Japanese].get();

	if (japanese == NULL || japanese->m_id == NULL)
		return slot;

	Font *chinese = m_fonts[Font_Chinese].get();

	if (chinese == NULL || chinese->m_id == NULL)
		return slot;
	switch (s_locale) {
	case SCE_SYSTEM_PARAM_LANG_KOREAN:
	case SCE_SYSTEM_PARAM_LANG_CHINESE_T:
	case SCE_SYSTEM_PARAM_LANG_CHINESE_S:
		if (scePvfIsElement(chinese->m_id, code) ||
		    !scePvfIsElement(m_fonts[Font_Japanese]->m_id, *c))
			return Font_Chinese;
		break;
	default:
		if (!scePvfIsElement(japanese->m_id, code) &&
		    scePvfIsElement(m_fonts[Font_Chinese]->m_id, *c))
			return Font_Chinese;
		break;
	}
	return slot;
}

int32_t FontRasterizerPvf::GetCharMetrics(wchar_t c, CharMetrics &metrics)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_ABORTED;
	int32_t slot;
	ScePvf_t_charCode code;

	if (m_style.get() == NULL)
		return ret;
	if (m_dirty)
		ApplyStyle();
	ret = 0;
	slot = Font_Latin;
	code = c;
	if (code <= 0x1f || (code >= 0x7f && code <= 0x9f) || code == 0x200b) {
		/* control characters and the zero width space */
		metrics.left = 0.0f;
		metrics.right = 0.0f;
		metrics.advance = 0.0f;
	} else {
		s_mutex.Lock();
		slot = GetFontSlot(&code);

		Font *font = m_fonts[slot].get();
		ScePvf_t_fontId id;

		if (font != NULL && (id = font->m_id) != NULL) {
			ScePvf_t_charInfo info;

			scePvfSetEmboldenRate(id, s_fonts[slot].embolden_rate[m_bold]);
			scePvfSetSkewValue(id, 0.0f, s_fonts[slot].skew_value[m_oblique]);
			ret = GetCharInfo(slot, id, code, &info);
			if (ret == 0) {
				metrics.left = (float)Round64(info.glyphMetrics.horizontalBearingX64);
				metrics.right = (float)Round64(info.glyphMetrics.horizontalBearingX64 +
							       info.glyphMetrics.width64);
				metrics.advance =
					(float)Round64(info.glyphMetrics.horizontalAdvance64);
			}
		}
		s_mutex.Unlock();
		if (ret != 0)
			return ret;
	}
	metrics.left = metrics.left - 1.0f;
	if (s_fonts[slot].embolden_rate[m_bold] > 0.0f)
		metrics.right = metrics.right + (float)(int32_t)(m_point.extract_x() * 0.1f);
	if (s_fonts[slot].skew_value[m_oblique] > 0.0f)
		metrics.right = metrics.right + (float)(int32_t)(m_point.extract_x() * 0.1f);
	if (m_shadow)
		metrics.right = metrics.right + 3.0f;
	metrics.font.scale = 1.0f;
	metrics.font.height = (float)m_height;
	metrics.font.ascender = (float)m_ascender;
	return ret;
}

int32_t FontRasterizerPvf::DrawChars(const wchar_t *str, uint32_t len,
				     intrusive_ptr<Surface> &surface)
{
	uint8_t *bits = (uint8_t *)surface->Lock(0);
	int32_t ret = SCE_PAF_ERROR_TEXT_ABORTED;

	if (bits != NULL) {
		ScePvf_t_userImageBufferRec image;
		bool styled[Font_Num];

		sce_paf_memset(bits, 0, surface->GetPageSize());
		image.pixelFormat = SCE_PVF_USERIMAGE_DIRECT4_L;
		image.rect.width = surface->GetWidth();
		image.rect.height = surface->GetHeight();
		image.bytesPerLine = surface->GetStride();
		image.buffer = bits;
		for (int32_t i = 0; i < Font_Num; i++)
			styled[i] = false;
		image.xPos64 = 0;
		image.yPos64 = m_ascender << 6;
		s_mutex.Lock();
		for (uint32_t i = 0; i < len; i++) {
			ScePvf_t_charCode c = *str++;

			if (c <= 0x1f || (c >= 0x7f && c <= 0x9f) || c == 0x200b)
				continue;

			int32_t slot = GetFontSlot(&c);
			Font *font = m_fonts[slot].get();
			ScePvf_t_fontId id;

			if (font == NULL || (id = font->m_id) == NULL)
				continue;
			if (!styled[slot]) {
				scePvfSetEmboldenRate(id, s_fonts[slot].embolden_rate[m_bold]);
				scePvfSetSkewValue(id, 0.0f, s_fonts[slot].skew_value[m_oblique]);
				styled[slot] = true;
			}

			ScePvf_t_charInfo info;

			ret = GetCharInfo(slot, id, c, &info);
			if (ret != 0)
				continue;

			int32_t left = Round64(info.glyphMetrics.horizontalBearingX64) - 1;

			info.glyphMetrics.horizontalBearingX64 = left << 6;
			info.glyphMetrics.horizontalAdvance64 =
				(info.glyphMetrics.horizontalAdvance64 >= 0 ?
					 info.glyphMetrics.horizontalAdvance64 + 63 :
					 info.glyphMetrics.horizontalAdvance64 - 63) & ~63;
			/* the first glyph starts at the surface's edge */
			if (image.xPos64 == 0 || image.xPos64 + (left << 6) < 0)
				image.xPos64 = -64 * left;
			/* the spaces have nothing to draw */
			if (c != 0x20 && c != 0xa0 && c != 0x3000)
				ret = scePvfGetCharGlyphImage(id, c, &image);
			if (ret == 0)
				image.xPos64 = image.xPos64 + info.glyphMetrics.horizontalAdvance64;
		}
		s_mutex.Unlock();
		surface->Unlock();
		if (s_clut.get() != NULL) {
			ret = 0;
		} else {
			Surface *target = surface.get();

			if (target->GetCLUT().get() != NULL) {
				ret = ScePafGraphics_F4986A3A(target->GetCLUT().get(), s_clut_table);
				if (ret == 0)
					s_clut = target->GetCLUT().get();
			}
		}
	}
	return ret;
}

int32_t FontRasterizerPvf::Rasterize(TextSurfaceCache *cache, int32_t width, int32_t height,
				     const wchar_t *str, uint32_t len, uint32_t hash,
				     intrusive_ptr<Surface> &surface)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;

	if (cache != NULL) {
		ret = cache->Find(width, height, m_style, str, len, hash, surface);
		if (ret < 0) {
			if (m_dirty)
				ApplyStyle();
			ret = cache->Allocate(width, height, ImageMode_index4, 1, true, m_style, str,
					      len, hash, s_clut, surface);
			if (ret == 0)
				DrawChars(str, len, surface);
		}
	}
	return ret;
}

/* the PVF library's memory, from the heap; the glyph cache let go to make room */
static ScePvf_t_pointer Alloc(ScePvf_t_pointer user_data, ScePvf_t_u32 size)
{
	void *ptr = s_heap->sce_paf_malloc(size);

	if (ptr == NULL) {
		myPvfCache_expireAll(s_cache, 1);
		return s_heap->sce_paf_malloc(size);
	}
	return ptr;
}

static ScePvf_t_pointer Realloc(ScePvf_t_pointer user_data, ScePvf_t_pointer ptr, ScePvf_t_u32 size)
{
	void *new_ptr = s_heap->sce_paf_realloc(ptr, size);

	if (new_ptr == NULL) {
		myPvfCache_expireAll(s_cache, 1);
		return s_heap->sce_paf_realloc(ptr, size);
	}
	return new_ptr;
}

static void Free(ScePvf_t_pointer user_data, ScePvf_t_pointer ptr)
{
	s_heap->sce_paf_free(ptr);
}

/* the glyph cache's memory, from the same heap */
static void *CacheAlloc(int size)
{
	return s_heap->sce_paf_malloc(size);
}

static void CacheFree(void *ptr)
{
	s_heap->sce_paf_free(ptr);
}

static unsigned char *CacheMemcpy(unsigned char *dst, unsigned char *src, int size)
{
	return (unsigned char *)sce_paf_memcpy(dst, src, size);
}

FontRasterizerPvf::Font::~Font()
{
	if (m_opened)
		scePvfClose(m_id);
}

}
}
