/*
 * paf::graph::TextLayout: an attributed string laid out in a box. The text threads lay it out
 * line by line (the characters measured with the style's font, lines broken at the box with
 * word wrap and kinsoku, the last line ended in an ellipsis), place the lines in the box by the
 * alignments, and draw each run of characters into a glyph surface of the graphics; the caret
 * and the selection follow the characters. Its methods change the string, the styles and the
 * attributes under the layout mutex, each change marking what has to be made again.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/text/attributed_string.h>
#include <graphics/text/font_rasterizer.h>
#include <graphics/text/text_graphics.h>
#include <graphics/text/text_init.h>
#include <graphics/text/text_layout.h>
#include <graphics/text/text_style.h>
#include <graphics/text/text_surface_cache.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/memory>
#include <std/string>
#include <std/string.h>
#include <std/vector>
#include <system/system.h>
#include <thread/thread.h>
#include "../../paf_types.h"
#include "../../std/math.h"

using sce::Vectormath::Simd::floatInVec;

namespace paf {
namespace graph {

/* the layout's invalid states each attribute marks when it changes */
#define LAYOUT_CHANGED		(TextLayoutState_InvalidLayout | TextLayoutState_InvalidGeometry | \
				 TextLayoutState_InvalidGraphics)
#define GEOMETRY_CHANGED	(TextLayoutState_InvalidGeometry | TextLayoutState_InvalidGraphics)
#define GRAPHICS_CHANGED	TextLayoutState_InvalidGraphics

/* the line pitch enlarged when the system asks for larger text (name ours) */
#define ENLARGED_LINE_PITCH	1.35f

TextLayout::TextLayout(TextGraphics *graphics, thread::RMutex *mutex)
	: m_placeholder(NULL), m_graphics(graphics), m_caret_index(0), m_selection_index(0),
	  m_selection_len(0), m_bounds(0.0f, 0.0f), m_state(TextLayoutState_None),
	  m_char_infos(NULL), m_char_info_count(0), m_mutex(mutex)
{
}

TextLayout::TextLayout(const TextStyle &style, TextGraphics *graphics, thread::RMutex *mutex)
	: m_str(style), m_placeholder(NULL), m_graphics(graphics), m_caret_index(0),
	  m_selection_index(0), m_selection_len(0), m_bounds(0.0f, 0.0f),
	  m_state(TextLayoutState_None), m_char_infos(NULL), m_char_info_count(0), m_mutex(mutex)
{
}

TextLayout::TextLayout(const AttributedString &str, TextGraphics *graphics,
		       thread::RMutex *mutex)
	: m_str(str), m_placeholder(NULL), m_graphics(graphics), m_caret_index(0),
	  m_selection_index(0), m_selection_len(0), m_bounds(0.0f, 0.0f),
	  m_state(TextLayoutState_None), m_char_infos(NULL), m_char_info_count(0), m_mutex(mutex)
{
}

TextLayout::~TextLayout()
{
	ReleaseLines();
	delete m_placeholder;
	delete m_graphics;
}

int32_t TextLayout::SetLayoutAttribute(TextLayoutAttribute attr, int32_t val)
{
	int32_t ret = 0;

	switch (attr) {
	case TextLayoutAttribute_AlignX:
		if (val != m_param.align_x) {
			ret = GEOMETRY_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Lock();
			m_param.align_x = val;
			m_state |= GEOMETRY_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Unlock();
		}
		break;
	case TextLayoutAttribute_AlignY:
		if (val != m_param.align_y) {
			ret = GEOMETRY_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Lock();
			m_param.align_y = val;
			m_state |= GEOMETRY_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Unlock();
		}
		break;
	case TextLayoutAttribute_TextAlign:
		if (val != m_param.text_align) {
			ret = GEOMETRY_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Lock();
			m_param.text_align = val;
			m_state |= GEOMETRY_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Unlock();
		}
		break;
	case TextLayoutAttribute_MaxLineCount:
		if (val != m_param.max_line_count) {
			ret = LAYOUT_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Lock();
			m_param.max_line_count = val;
			m_state |= LAYOUT_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Unlock();
		}
		break;
	default:
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
	}
	return ret;
}

int32_t TextLayout::GetLayoutAttribute(TextLayoutAttribute attr, int32_t &val)
{
	switch (attr) {
	case TextLayoutAttribute_AlignX:
		val = m_param.align_x;
		break;
	case TextLayoutAttribute_AlignY:
		val = m_param.align_y;
		break;
	case TextLayoutAttribute_TextAlign:
		val = m_param.text_align;
		break;
	case TextLayoutAttribute_MaxLineCount:
		val = m_param.max_line_count;
		break;
	default:
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
	}
	return SCE_PAF_OK;
}

int32_t TextLayout::SetLayoutAttribute(TextLayoutAttribute attr, float val)
{
	int32_t ret = 0;

	switch (attr) {
	case TextLayoutAttribute_LineSpace:
		if (val != m_param.line_space) {
			ret = LAYOUT_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Lock();
			m_param.line_space = val;
			m_state |= LAYOUT_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Unlock();
		}
		break;
	case TextLayoutAttribute_LinePitch:
		if (val != m_param.line_pitch) {
			ret = LAYOUT_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Lock();
			m_param.line_pitch = val;
			m_state |= LAYOUT_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Unlock();
		}
		break;
	default:
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
	}
	return ret;
}

int32_t TextLayout::GetLayoutAttribute(TextLayoutAttribute attr, float &val)
{
	switch (attr) {
	case TextLayoutAttribute_LineSpace:
		val = m_param.line_space;
		break;
	case TextLayoutAttribute_LinePitch:
		val = m_param.line_pitch;
		break;
	case TextLayoutAttribute_DrawLinePitch: {
		float pitch = m_param.line_pitch;

		if ((ScePafMisc_C51EF81D() & 1) != 0 &&
		    (m_param.flags & TextLayoutParam::Flag_EnlargeLinePitch) != 0)
			pitch *= ENLARGED_LINE_PITCH;
		val = pitch;
		break;
	}
	default:
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
	}
	return SCE_PAF_OK;
}

int32_t TextLayout::SetLayoutAttribute(TextLayoutAttribute attr, const math::v2 &val)
{
	int32_t ret = 0;

	switch (attr) {
	case TextLayoutAttribute_Size:
		if (!((val.getImpl().getX() == floatInVec(m_param.width)) &
		      (val.getImpl().getY() == floatInVec(m_param.height))).getAsBool()) {
			uint16_t flags = m_param.flags;

			/* a size an adjusted side follows changes only the geometry */
			if (((flags & TextLayoutParam::Flag_AdjustX) == 0 &&
			     val.getImpl().getX() != m_param.width) ||
			    ((flags & TextLayoutParam::Flag_AdjustY) == 0 &&
			     val.getImpl().getY() != m_param.height)) {
				ret = GEOMETRY_CHANGED;
				if ((flags & (TextLayoutParam::Flag_WordWrap |
					      TextLayoutParam::Flag_Ellipsis)) != 0)
					ret = LAYOUT_CHANGED;
			}
			if (m_mutex != NULL)
				m_mutex->Lock();
			m_state |= ret;
			math::v2 size = val;

			m_param.width = size.extract_x();
			m_param.height = size.extract_y();
			if (m_mutex != NULL)
				m_mutex->Unlock();
		}
		break;
	case TextLayoutAttribute_VisiblePos:
		if (!((val.getImpl().getX() == m_param.visible.getImpl().getX()) &
		      (val.getImpl().getY() == m_param.visible.getImpl().getY())).getAsBool()) {
			ret = GRAPHICS_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Lock();

			math::v4 visible = m_param.visible;

			visible.getImpl().setX(val.getImpl().getX());
			visible.getImpl().setY(val.getImpl().getY());
			m_state |= GRAPHICS_CHANGED;
			m_param.visible = visible;
			if (m_mutex != NULL)
				m_mutex->Unlock();
			if (m_graphics != NULL) {
				m_graphics->SetPos(val);
				m_graphics->Update();
			}
		}
		break;
	case TextLayoutAttribute_VisibleSize:
		if (!((val.getImpl().getX() == m_param.visible.getImpl().getZ()) &
		      (val.getImpl().getY() == m_param.visible.getImpl().getW())).getAsBool()) {
			ret = GRAPHICS_CHANGED;
			if (m_mutex != NULL)
				m_mutex->Lock();

			math::v4 visible = m_param.visible;

			visible.getImpl().setZ(val.getImpl().getX());
			visible.getImpl().setW(val.getImpl().getY());
			m_state |= GRAPHICS_CHANGED;
			m_param.visible = visible;
			if (m_mutex != NULL)
				m_mutex->Unlock();
		}
		break;
	default:
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		break;
	}
	return ret;
}

int32_t TextLayout::GetLayoutAttribute(TextLayoutAttribute attr, math::v2 &val)
{
	switch (attr) {
	case TextLayoutAttribute_Size:
		val = math::v2(m_param.width, m_param.height);
		break;
	case TextLayoutAttribute_VisiblePos:
		val = math::v2(m_param.visible.getImpl().getX(), m_param.visible.getImpl().getY());
		break;
	case TextLayoutAttribute_VisibleSize:
		val = math::v2(m_param.visible.getImpl().getZ(), m_param.visible.getImpl().getW());
		break;
	default:
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
	}
	return SCE_PAF_OK;
}

/* the flag of a bool attribute set to val; changed, it marks changed invalid */
#define SET_FLAG(flag, changed)						\
	if (val != ((m_param.flags & (flag)) != 0)) {			\
		ret = (changed);					\
		if (m_mutex != NULL)					\
			m_mutex->Lock();				\
		m_state |= (changed);					\
		if (val)						\
			m_param.flags |= (flag);			\
		else							\
			m_param.flags &= ~(flag);			\
		if (m_mutex != NULL)					\
			m_mutex->Unlock();				\
	}

int32_t TextLayout::SetLayoutAttribute(TextLayoutAttribute attr, bool val)
{
	int32_t ret = 0;

	switch (attr) {
	case TextLayoutAttribute_AdjustX:
		SET_FLAG(TextLayoutParam::Flag_AdjustX, GEOMETRY_CHANGED);
		break;
	case TextLayoutAttribute_AdjustY:
		SET_FLAG(TextLayoutParam::Flag_AdjustY, GEOMETRY_CHANGED);
		break;
	case TextLayoutAttribute_CondenseX:
		SET_FLAG(TextLayoutParam::Flag_CondenseX, GRAPHICS_CHANGED);
		break;
	case TextLayoutAttribute_CondenseY:
		SET_FLAG(TextLayoutParam::Flag_CondenseY, GRAPHICS_CHANGED);
		break;
	case TextLayoutAttribute_WordWrap:
		SET_FLAG(TextLayoutParam::Flag_WordWrap, LAYOUT_CHANGED);
		break;
	case TextLayoutAttribute_Kinsoku:
		SET_FLAG(TextLayoutParam::Flag_Kinsoku, LAYOUT_CHANGED);
		break;
	case TextLayoutAttribute_Ellipsis:
		SET_FLAG(TextLayoutParam::Flag_Ellipsis, LAYOUT_CHANGED);
		break;
	case TextLayoutAttribute_CharPrimitive:
		SET_FLAG(TextLayoutParam::Flag_CharPrimitive, LAYOUT_CHANGED);
		break;
	case TextLayoutAttribute_LayoutOnly:
		/* only drawing again what was left undrawn */
		SET_FLAG(TextLayoutParam::Flag_LayoutOnly, val ? 0 : GRAPHICS_CHANGED);
		break;
	case TextLayoutAttribute_EnlargeLinePitch:
		SET_FLAG(TextLayoutParam::Flag_EnlargeLinePitch, LAYOUT_CHANGED);
		break;
	default:
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		break;
	}
	return ret;
}

#undef SET_FLAG

int32_t TextLayout::GetLayoutAttribute(TextLayoutAttribute attr, bool &val)
{
	int32_t ret = SCE_PAF_OK;

	switch (attr) {
	case TextLayoutAttribute_AdjustX:
		val = (m_param.flags & TextLayoutParam::Flag_AdjustX) != 0;
		break;
	case TextLayoutAttribute_AdjustY:
		val = (m_param.flags & TextLayoutParam::Flag_AdjustY) != 0;
		break;
	case TextLayoutAttribute_CondenseX:
		val = (m_param.flags & TextLayoutParam::Flag_CondenseX) != 0;
		break;
	case TextLayoutAttribute_CondenseY:
		val = (m_param.flags & TextLayoutParam::Flag_CondenseY) != 0;
		break;
	case TextLayoutAttribute_WordWrap:
		val = (m_param.flags & TextLayoutParam::Flag_WordWrap) != 0;
		break;
	case TextLayoutAttribute_Kinsoku:
		val = (m_param.flags & TextLayoutParam::Flag_Kinsoku) != 0;
		break;
	case TextLayoutAttribute_Ellipsis:
		val = (m_param.flags & TextLayoutParam::Flag_Ellipsis) != 0;
		break;
	case TextLayoutAttribute_CharPrimitive:
		val = (m_param.flags & TextLayoutParam::Flag_CharPrimitive) != 0;
		break;
	case TextLayoutAttribute_LayoutOnly:
		val = (m_param.flags & TextLayoutParam::Flag_LayoutOnly) != 0;
		break;
	case TextLayoutAttribute_EnlargeLinePitch:
		val = (m_param.flags & TextLayoutParam::Flag_EnlargeLinePitch) != 0;
		break;
	default:
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		break;
	}
	return ret;
}

/* what a change to the string gives: the layout invalid, or unchanged */
#define STRING_CHANGED(ret)						\
	if ((ret) == SCE_PAF_OK) {					\
		(ret) = LAYOUT_CHANGED;					\
		m_state |= LAYOUT_CHANGED;				\
	} else if ((ret) == SCE_PAF_ERROR_TEXT_UNCHANGED) {		\
		(ret) = SCE_PAF_OK;					\
	}

int32_t TextLayout::SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				      int32_t val)
{
	int32_t ret;

	if (m_mutex != NULL)
		m_mutex->Lock();
	ret = m_str.SetStyleAttribute(attr, index, len, val);
	STRING_CHANGED(ret);
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return ret;
}

int32_t TextLayout::GetStyleAttribute(TextStyleAttribute attr, uint32_t index, int32_t &val)
{
	return m_str.GetStyleAttribute(attr, index, val);
}

int32_t TextLayout::SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				      float val)
{
	int32_t ret;

	if (m_mutex != NULL)
		m_mutex->Lock();
	ret = m_str.SetStyleAttribute(attr, index, len, val);
	STRING_CHANGED(ret);
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return ret;
}

int32_t TextLayout::GetStyleAttribute(TextStyleAttribute attr, uint32_t index, float &val)
{
	return m_str.GetStyleAttribute(attr, index, val);
}

int32_t TextLayout::SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				      const math::v2 &val)
{
	int32_t ret;

	if (m_mutex != NULL)
		m_mutex->Lock();
	ret = m_str.SetStyleAttribute(attr, index, len, val);
	STRING_CHANGED(ret);
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return ret;
}

int32_t TextLayout::GetStyleAttribute(TextStyleAttribute attr, uint32_t index, math::v2 &val)
{
	return m_str.GetStyleAttribute(attr, index, val);
}

int32_t TextLayout::SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				      const math::v4 &val)
{
	int32_t ret;

	if (m_mutex != NULL)
		m_mutex->Lock();
	ret = m_str.SetStyleAttribute(attr, index, len, val);
	STRING_CHANGED(ret);
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return ret;
}

int32_t TextLayout::GetStyleAttribute(TextStyleAttribute attr, uint32_t index, math::v4 &val)
{
	return m_str.GetStyleAttribute(attr, index, val);
}

int32_t TextLayout::SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				      bool val)
{
	int32_t ret;

	if (m_mutex != NULL)
		m_mutex->Lock();
	ret = m_str.SetStyleAttribute(attr, index, len, val);
	STRING_CHANGED(ret);
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return ret;
}

int32_t TextLayout::GetStyleAttribute(TextStyleAttribute attr, uint32_t index, bool &val)
{
	return m_str.GetStyleAttribute(attr, index, val);
}

int32_t TextLayout::SetStyle(uint32_t index, uint32_t len, const TextStyle &style)
{
	int32_t ret;

	if (m_mutex != NULL)
		m_mutex->Lock();
	ret = m_str.SetStyle(index, len, style);
	STRING_CHANGED(ret);
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return ret;
}

int32_t TextLayout::GetStyle(uint32_t index, TextStyle &style)
{
	return m_str.GetStyle(index, style);
}

/* the caret after the new string, nothing selected */
int32_t TextLayout::SetString(const wstring &str)
{
	int32_t ret;

	if (m_mutex != NULL)
		m_mutex->Lock();
	ret = m_str.SetString(str);
	if (ret == SCE_PAF_OK) {
		ret = LAYOUT_CHANGED;
		m_caret_index = str.size();
		m_selection_index = 0;
		m_selection_len = 0;
		m_state |= LAYOUT_CHANGED;
	} else if (ret == SCE_PAF_ERROR_TEXT_UNCHANGED) {
		ret = SCE_PAF_OK;
	}
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return ret;
}

/* the caret and the selection moved with the characters around them */
int32_t TextLayout::ReplaceString(uint32_t index, uint32_t len, const wstring &str)
{
	int32_t ret;

	if (m_mutex != NULL)
		m_mutex->Lock();
	ret = m_str.ReplaceString(index, len, str);
	if (ret == SCE_PAF_OK) {
		uint32_t caret = m_caret_index;
		uint32_t end = index + len;
		uint32_t sel_index = m_selection_index;
		uint32_t sel_len = m_selection_len;
		uint32_t sel_end = sel_index + sel_len;
		uint32_t left;

		if (index <= caret) {
			if (end >= caret)
				m_caret_index = caret = index;
			else
				m_caret_index = caret = caret - len;
			m_caret_index = caret + str.size();
		}
		left = 0;
		if (index > sel_index)
			left = index - sel_index;
		if (end < sel_end)
			left += sel_end - end;
		if (left < sel_len)
			m_selection_len = left;
		if (index <= sel_index) {
			if (end >= sel_index)
				m_selection_index = index;
			else
				m_selection_index = index = sel_index - len;
			m_selection_index = index + str.size();
		}
		ret = LAYOUT_CHANGED;
		m_state |= LAYOUT_CHANGED;
	} else if (ret == SCE_PAF_ERROR_TEXT_UNCHANGED) {
		ret = SCE_PAF_OK;
	}
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return ret;
}

int32_t TextLayout::GetString(wstring &str)
{
	return m_str.GetString(str);
}

int32_t TextLayout::GetLength(uint32_t &len)
{
	return m_str.GetLength(len);
}

int32_t TextLayout::GetStringBuffer(const wchar_t *&buf, uint32_t &len)
{
	return m_str.GetStringBuffer(buf, len);
}

int32_t TextLayout::SetAttributedString(const AttributedString &str)
{
	if (m_mutex != NULL)
		m_mutex->Lock();
	m_state |= LAYOUT_CHANGED;
	m_str = str;
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return LAYOUT_CHANGED;
}

int32_t TextLayout::GetAttributedString(AttributedString &str)
{
	str = m_str;
	return SCE_PAF_OK;
}

/* the placeholder's color: the string's faded to half (name ours); made in the static
   initializer with the vector's constructor inlined */
static inline __attribute__((always_inline)) math::v4 PlaceholderColor()
{
	return math::v4(1.0f, 1.0f, 1.0f, 0.5f);
}

static const math::v4 s_placeholder_color = PlaceholderColor();

/* the placeholder made, in the first character's style faded and unmasked */
int32_t TextLayout::CreatePlaceholder()
{
	int32_t ret = SCE_PAF_OK;

	if (m_placeholder == NULL) {
		TextStyle style;
		math::v4 color;

		m_str.GetStyle(0, style);
		style.GetStyleAttribute(TextStyleAttribute_Color, color);
		style.SetStyleAttribute(TextStyleAttribute_Color,
					math::v4(mulPerElem(color.getImpl(),
							    s_placeholder_color.getImpl())));
		style.SetStyleAttribute(TextStyleAttribute_Mask, false);
		m_placeholder = new AttributedString(style);
		if (m_placeholder == NULL)
			ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;
	}
	return ret;
}

int32_t TextLayout::SetPlaceholderStyle(const TextStyle &style)
{
	int32_t ret;

	if (m_mutex != NULL)
		m_mutex->Lock();
	ret = CreatePlaceholder();
	if (ret == SCE_PAF_OK) {
		ret = m_placeholder->SetStyle(0, 0, style);
		STRING_CHANGED(ret);
	}
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return ret;
}

/* an empty placeholder is made only to clear one there is */
int32_t TextLayout::SetPlaceholderString(const wstring &str)
{
	int32_t ret = SCE_PAF_OK;

	if (m_placeholder != NULL || str.c_str()[0] != L'\0' || sce_paf_wcscmp(str.c_str(), L"") != 0) {
		if (m_mutex != NULL)
			m_mutex->Lock();
		ret = CreatePlaceholder();
		if (ret == SCE_PAF_OK) {
			ret = m_placeholder->SetString(str);
			STRING_CHANGED(ret);
		}
		if (m_mutex != NULL)
			m_mutex->Unlock();
	}
	return ret;
}

int32_t TextLayout::GetPlaceholderString(wstring &str)
{
	if (m_placeholder != NULL)
		return m_placeholder->GetString(str);
	str = L"";
	return SCE_PAF_OK;
}

#undef STRING_CHANGED

/* the lines and characters let go, the string emptied and the glyphs taken off */
int32_t TextLayout::ReleaseAll()
{
	ReleaseLines();
	m_str.SetString(wstring(L""));
	if (m_graphics != NULL)
		m_graphics->RemoveAllChars();
	return SCE_PAF_OK;
}


/* the selection's rectangles, a line's frame each, cut to the characters selected */
int32_t TextLayout::UpdateSelection()
{
	if (m_graphics != NULL) {
		vector<math::v4> rects;

		if (m_selection_index < m_char_info_count &&
		    m_selection_index + m_selection_len < m_char_info_count) {
			uint32_t first = m_char_infos[m_selection_index].line;
			uint32_t last = m_char_infos[m_selection_index + m_selection_len].line;

			if (first < m_lines.size() && last < m_lines.size()) {
				for (uint32_t i = first; i <= last; i++) {
					math::v4 rect = m_lines[i]->frame;

					if (i == last)
						rect.set_z(m_char_infos[m_selection_index +
									m_selection_len].x);
					if (i == first) {
						float x = m_char_infos[m_selection_index].x;

						rect.set_x(rect.extract_x() + x);
						rect.set_z(rect.extract_z() - x);
					}
					if (rect.extract_z() > 0.0f && rect.extract_w() > 0.0f)
						rects.push_back(rect);
				}
			}
		}
		m_graphics->SetSelection(rects);
	}
	return SCE_PAF_OK;
}

/* the caret before the character at the caret index, as high as its line */
int32_t TextLayout::UpdateCaret()
{
	if (m_graphics != NULL) {
		math::v2 pos(0.0f, 0.0f);
		float height = 0.0f;

		if (m_caret_index < m_char_info_count) {
			TextLayoutCharInfo *info = &m_char_infos[m_caret_index];

			if (info->line < m_lines.size()) {
				math::v4 frame = m_lines[info->line]->frame;

				pos = math::v2(frame.extract_x() + info->x, frame.extract_y());
				height = frame.extract_w();
			}
		}
		m_graphics->SetCaret(pos, height);
	}
	return SCE_PAF_OK;
}

/* what Update lays a layout out with, copied under the layout mutex: the rasterizers and the
   cache, the attributes, the string and its style runs (names ours) */
class TextLayoutStyleRun {
public:
	TextLayoutStyleRun() : len(0)
	{
	}

	virtual ~TextLayoutStyleRun()
	{
	}

	common::SharedPtr<TextStyle> style;
	uint32_t len;
};

}

/* Update adds the style runs out of line, the other vectors' push_back inlined */
template<>
__attribute__((noinline)) int
vector<graph::TextLayoutStyleRun>::push_back(const graph::TextLayoutStyleRun &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) graph::TextLayoutStyleRun(val);
		result = ++count;
	}
	return result;
}

namespace graph {

class TextLayoutContext {
public:
	TextLayoutContext()
		: rasterizer(NULL), emoji_rasterizer(NULL), cache(NULL), str(L"")
	{
	}

	virtual ~TextLayoutContext()
	{
	}

	/* the character's metrics at x: from the emoji font for an emoji, its glyph kept from
	   reaching left of the line */
	int32_t GetCharMetrics(wchar_t c, int32_t family, bool mask, float x, FontMetrics &font,
			       CharMetrics &metrics, bool &image);
	/* whether a line may break after c */
	bool IsBreakChar(wchar_t c);

	FontRasterizer *rasterizer;
	FontRasterizer *emoji_rasterizer;
	TextSurfaceCache *cache;
	TextLayoutParam param;
	wstring str;
	vector<TextLayoutStyleRun> runs;
};

/* what a control character the string ends a run with is: not drawn */
static inline bool IsControlChar(wchar_t c)
{
	return c <= 0x1F || (c >= 0x7F && c <= 0x9F) || c == 0x200B;
}

int32_t TextLayoutLine::Draw(TextLayoutContext *ctx, const math::v4 &clip,
			     TextLayoutCharInfo *infos, TextGraphics *graphics)
{
	int32_t ret = SCE_PAF_OK;

	for (vector<TextLayoutRun *>::iterator it = runs.begin(); it != runs.end(); ++it) {
		TextLayoutRun *run = *it;
		math::v4 rect(frame.extract_x() + infos[run->index].x,
			      frame.extract_y() + ascender - run->ascender, run->width, run->height);
		math::v4 visible;

		IntersectRect(rect, clip, visible);
		if (visible.extract_z() > 0.0f && visible.extract_w() > 0.0f) {
			if (run->glyph == NULL) {
				const wchar_t *str = ctx->str.c_str() + run->index;
				uint32_t len = run->len;
				intrusive_ptr<Surface> surface;

				if ((run->flags & TextLayoutRun::Flag_Image) == 0) {
					if ((run->flags & TextLayoutRun::Flag_Ellipsis) == 0 &&
					    IsControlChar(str[len - 1]) && --len == 0)
						continue;

					wstring text;
					bool mask;

					run->style->GetStyleAttribute(TextStyleAttribute_Mask, mask);
					if (mask) {
						text = wstring(len, L'*');
						str = text.c_str();
						len = text.size();
					}
					if ((run->flags & TextLayoutRun::Flag_Ellipsis) != 0) {
						text = wstring(str, len) + wstring(3, L'.');
						str = text.c_str();
						len = text.size();
					}
					ret = ctx->rasterizer->SetStyle(run->style);
					if (ret == SCE_PAF_OK)
						ret = ctx->rasterizer->Rasterize(ctx->cache,
										 run->surface_width,
										 run->surface_height, str,
										 len, HashString(str, len),
										 surface);
				} else {
					ret = ctx->emoji_rasterizer->Rasterize(ctx->cache,
									       run->surface_width,
									       run->surface_height, str,
									       len, HashString(str, len),
									       surface);
				}
				if (ret == SCE_PAF_OK)
					ret = graphics->AddChar(rect, run->glyph_offset, run->glyph_width,
								run->style, surface, run->glyph);
			}
		} else if (run->glyph != NULL) {
			/* out of sight */
			graphics->RemoveChar(run->glyph);
			run->glyph = NULL;
		}
		if (ret < 0)
			break;
	}
	return ret;
}

/* how far the glyphs in sight are drawn beyond the box, all round (name ours) */
#define GRAPHICS_MARGIN		64.0f

int32_t TextLayout::UpdateGraphics(TextLayoutContext *ctx)
{
	if ((m_state & TextLayoutState_Abort) != 0)
		return SCE_PAF_ERROR_TEXT_ABORTED;

	int32_t ret = SCE_PAF_OK;
	if (m_graphics != NULL) {
		math::v2 scale(1.0f, 1.0f);
		math::v4 visible = ctx->param.visible;
		uint16_t flags = ctx->param.flags;

		const float *bounds = (const float *)&m_bounds;

		/* a layout wider or higher than the box is drawn condensed into it */
		if ((flags & TextLayoutParam::Flag_CondenseX) != 0 && visible.extract_z() > 0.0f &&
		    visible.extract_z() < bounds[0]) {
			scale.set_x(visible.extract_z() / bounds[0]);
			visible.set_z(bounds[0]);
		}
		if ((flags & TextLayoutParam::Flag_CondenseY) != 0 && visible.extract_w() > 0.0f &&
		    visible.extract_w() < bounds[1]) {
			scale.set_y(visible.extract_w() / bounds[1]);
			visible.set_w(bounds[1]);
		}

		math::v4 clip = visible + math::v4(-GRAPHICS_MARGIN, -GRAPHICS_MARGIN,
						   2.0f * GRAPHICS_MARGIN, 2.0f * GRAPHICS_MARGIN);

		for (uint32_t i = 0; i < m_lines.size(); i++) {
			if ((m_state & TextLayoutState_Abort) != 0) {
				ret = SCE_PAF_ERROR_TEXT_ABORTED;
				break;
			}
			m_lines[i]->Draw(ctx, clip, m_char_infos, m_graphics);
		}
		m_graphics->SetScale(scale);
		m_graphics->SetSize(math::v2(sce::Vectormath::Simd::Aos::Vector2(
			vget_high_f32(visible.getImpl().get128()))));
	}
	return ret;
}

/* the line pitch drawn: enlarged when the system asks for larger text */
static inline float GetDrawLinePitch(const TextLayoutParam &param)
{
	float pitch = param.line_pitch;

	if ((ScePafMisc_C51EF81D() & 1) != 0 &&
	    (param.flags & TextLayoutParam::Flag_EnlargeLinePitch) != 0)
		pitch *= ENLARGED_LINE_PITCH;
	return pitch;
}

int32_t TextLayout::UpdateGeometry(TextLayoutContext *ctx)
{
	if ((m_state & TextLayoutState_Abort) != 0)
		return SCE_PAF_ERROR_TEXT_ABORTED;

	int32_t ret = SCE_PAF_OK;
	const float *bounds = (const float *)&m_bounds;

	if (m_graphics != NULL)
		m_graphics->RemoveAllChars();

	math::v2 pos(0.0f, 0.0f);

	switch (ctx->param.align_x) {
	case TextLayoutAlignX_Center:
		pos.set_x((ctx->param.width - bounds[0]) * 0.5f);
		break;
	case TextLayoutAlignX_Right:
		pos.set_x(ctx->param.width - bounds[0]);
		break;
	}
	switch (ctx->param.align_y) {
	case TextLayoutAlignY_Top:
		pos.set_y(0.0f);
		break;
	case TextLayoutAlignY_Center:
		pos.set_y((ctx->param.height - bounds[1]) * 0.5f);
		break;
	case TextLayoutAlignY_Bottom:
		pos.set_y(ctx->param.height - bounds[1]);
		break;
	}
	for (uint32_t i = 0; i < m_lines.size(); i++) {
		if ((m_state & TextLayoutState_Abort) != 0) {
			ret = SCE_PAF_ERROR_TEXT_ABORTED;
			break;
		}

		TextLayoutLine *line = m_lines[i];
		float x = 0.0f;

		switch (ctx->param.text_align) {
		case TextLayoutTextAlign_Left:
			x = 0.0f;
			break;
		case TextLayoutTextAlign_Center:
			x = (bounds[0] - line->frame.extract_z()) * 0.5f;
			break;
		case TextLayoutTextAlign_Right:
			x = bounds[0] - line->frame.extract_z();
			break;
		}
		line->frame.set_x(pos.extract_x() + x);
		line->frame.set_y(pos.extract_y());
		/* the glyphs were all taken out */
		vector<TextLayoutRun *> &runs = m_lines[i]->runs;

		for (vector<TextLayoutRun *>::iterator it = runs.begin(); it != runs.end(); ++it)
			(*it)->glyph = NULL;
		if (ctx->param.line_pitch == 0.0f)
			pos.set_y(pos.extract_y() + line->frame.extract_w() + ctx->param.line_space);
		else
			pos.set_y(pos.extract_y() + GetDrawLinePitch(m_param));
	}
	return ret;
}

/* the ellipsis drawn after a run: three of the font's full stops (name ours) */
#define ELLIPSIS_CHAR		L'.'

/* from the last run back, the characters that leave no room for an ellipsis in the box are
   taken off and the run left ends in one; an image run is followed by an ellipsis run of its
   own */
int32_t TextLayoutLine::Ellipsize(TextLayoutContext *ctx, TextLayoutCharInfo *infos)
{
	int32_t ret = SCE_PAF_OK;
	/* the last character of the last run gives way first; of the runs before it, none */
	uint32_t trim = 1;
	/* the first run keeps a character */
	uint32_t min_len = 1;

	while (runs.size() != 0) {
		TextLayoutRun *run = runs[runs.size() - 1];

		if ((run->flags & TextLayoutRun::Flag_Ellipsis) != 0)
			break;
		ret = ctx->rasterizer->SetStyle(run->style);
		if (ret < 0)
			break;

		CharMetrics dot;

		ret = ctx->rasterizer->GetCharMetrics(ELLIPSIS_CHAR, dot);
		if (ret < 0)
			break;
		if (runs.size() == 1)
			min_len = 0;

		uint32_t len = run->len - trim;
		float dots = dot.right + dot.advance * 2.0f;

		if (len >= min_len) {
			for (;;) {
				if (ctx->param.width >= dots + infos[run->index + len].x)
					break;
				if (len != 0 && --len >= min_len)
					continue;
				goto next;
			}
			if ((run->flags & TextLayoutRun::Flag_Image) == 0) {
				run->width = infos[run->index + len].x - infos[run->index].x + dots;
				if (dot.font.scale == 1.0f) {
					run->surface_width = (uint16_t)sce_paf_ceilf(run->width - run->glyph_offset);
					run->glyph_width = run->surface_width;
				} else {
					run->surface_width = (uint16_t)sce_paf_ceilf((run->width - run->glyph_offset) /
									   dot.font.scale);
					run->glyph_width = run->surface_width * dot.font.scale;
				}
				run->len = len;
				run->flags |= TextLayoutRun::Flag_Ellipsis;
				frame.set_z((float)(((int32_t)(infos[run->index].x + run->width) + 1) &
						    ~1));
			} else {
				TextLayoutRun *ellipsis = new TextLayoutRun();

				if (ellipsis == NULL) {
					ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;
					goto next;
				}
				ellipsis->width = dots;
				ellipsis->height = dot.font.height;
				if (dot.font.scale == 1.0f) {
					ellipsis->surface_width = (uint16_t)sce_paf_ceilf(dots - dot.left);
					ellipsis->surface_height = (uint16_t)sce_paf_ceilf(dot.font.height);
					ellipsis->glyph_width = ellipsis->surface_width;
				} else {
					ellipsis->surface_width =
						(uint16_t)sce_paf_ceilf((dots - dot.left) / dot.font.scale);
					ellipsis->surface_height =
						(uint16_t)sce_paf_ceilf(dot.font.height / dot.font.scale);
					ellipsis->glyph_width = ellipsis->surface_width * dot.font.scale;
				}
				ellipsis->glyph_offset = dot.left;
				ellipsis->ascender = dot.font.ascender;
				ellipsis->len = 0;
				ellipsis->index = run->index + len;
				ellipsis->style = run->style;
				ellipsis->flags |= TextLayoutRun::Flag_Ellipsis;
				frame.set_z((float)(((int32_t)(infos[run->index].x + ellipsis->width) +
						     1) & ~1));
				if (len == 0) {
					runs.pop_back();
					delete run;
				}
				runs.push_back(ellipsis);
				run = ellipsis;
			}
		}
next:
		if (ret < 0 || (run->flags & TextLayoutRun::Flag_Ellipsis) != 0)
			break;
		runs.pop_back();
		delete run;
		trim = 0;
	}
	if (runs.size() == 0)
		ret = SCE_PAF_ERROR_TEXT_ABORTED;
	return ret;
}

int32_t TextLayoutLine::AddRun(const math::v2 &size, float glyph_offset, float glyph_width,
			       float ascender, uint16_t width, uint16_t height, uint32_t index,
			       uint32_t len, const common::SharedPtr<TextStyle> &style, bool image,
			       TextLayoutCharInfo *infos)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;
	TextLayoutRun *run = new TextLayoutRun();

	if (run != NULL) {
		run->width = size.extract_x();
		run->glyph_offset = glyph_offset;
		run->glyph_width = glyph_width;
		run->ascender = ascender;
		run->surface_width = width;
		run->surface_height = height;
		run->height = size.extract_y();
		run->index = index;
		run->len = len;
		run->style = style;
		if (image)
			run->flags |= TextLayoutRun::Flag_Image;
		runs.push_back(run);
		/* an empty run widens nothing, unless it is the first */
		if (runs.size() == 1 || size.extract_x() > 0.0f) {
			frame.set_z((float)(((int32_t)(infos[index].x + size.extract_x()) + 1) & ~1));
			if (frame.extract_w() < size.extract_y())
				frame.set_w(size.extract_y());
			if (this->ascender < ascender)
				this->ascender = ascender;
		}
		ret = SCE_PAF_OK;
	}
	return ret;
}

/* the run goes on the line, made first; past the box's height or the line count the lines
   end in an ellipsis instead, and past its width the run is ellipsized */
int32_t TextLayout::AddRun(TextLayoutContext *ctx, uint32_t line, const CharMetrics &metrics,
			   float y, float height, float glyph_offset, uint32_t index, uint32_t len,
			   const common::SharedPtr<TextStyle> &style, bool image)
{
	int32_t ret = SCE_PAF_OK;
	float x = m_char_infos[index].x;
	float last = m_char_infos[index + len - 1].x - x;
	float width = last + metrics.advance;
	bool ellipsize = false;
	bool full = false;

	if ((ctx->param.flags & TextLayoutParam::Flag_Ellipsis) != 0) {
		if (((ctx->param.flags & TextLayoutParam::Flag_AdjustY) == 0 &&
		     ctx->param.height < y + height) ||
		    (ctx->param.max_line_count > 0 && ctx->param.max_line_count <= line)) {
			full = true;
			goto ellipsize;
		}
		if ((ctx->param.flags & TextLayoutParam::Flag_AdjustX) == 0 &&
		    ctx->param.width < x + width) {
			if (ctx->param.width < x)
				return ret;
			ellipsize = true;
		}
	} else if (ctx->param.max_line_count > 0 && ctx->param.max_line_count <= line) {
		return SCE_PAF_ERROR_TEXT_LINE_LIMIT;
	}

	if (m_lines.size() < line + 1) {
		uint32_t i = m_lines.size();

		m_lines.resize(line + 1);
		for (; i < m_lines.size(); i++)
			m_lines[i] = NULL;
	}
	if (m_lines[line] == NULL) {
		m_lines[line] = new TextLayoutLine(math::v4(x, y, 0.0f, 0.0f));
		if (m_lines[line] == NULL)
			return SCE_PAF_ERROR_TEXT_NO_MEMORY;
	}
	{
		float scale = metrics.font.scale;
		float glyph = last + metrics.right - glyph_offset;
		uint16_t surface_width;
		uint16_t surface_height;
		float glyph_width;

		if (scale == 1.0f) {
			surface_width = (uint16_t)sce_paf_ceilf(glyph);
			surface_height = (uint16_t)sce_paf_ceilf(metrics.font.height);
			glyph_width = surface_width;
		} else {
			surface_width = (uint16_t)sce_paf_ceilf(glyph / scale);
			surface_height = (uint16_t)sce_paf_ceilf(metrics.font.height / metrics.font.scale);
			glyph_width = surface_width * metrics.font.scale;
		}
		ret = m_lines[line]->AddRun(math::v2(width, metrics.font.height), glyph_offset,
					    glyph_width, metrics.font.ascender, surface_width,
					    surface_height, index, len, style, image, m_char_infos);
	}
	if (ret != SCE_PAF_OK || !ellipsize)
		return ret;
ellipsize:
	/* from this line back, the first that can end in an ellipsis does */
	for (int32_t i = line; i >= 0; i--) {
		if (i < (int32_t)m_lines.size() &&
		    m_lines[i]->Ellipsize(ctx, m_char_infos) == SCE_PAF_OK)
			break;
	}
	ret = ctx->rasterizer->SetStyle(style);
	if (ret == SCE_PAF_OK && full)
		ret = SCE_PAF_ERROR_TEXT_LINE_LIMIT;
	return ret;
}

/* the emoji: the Private Use Area, and the service mark in the first families (names ours) */
#define EMOJI_FIRST		0xF000
#define EMOJI_LAST		0xF8FF
#define SERVICE_MARK		0x2120
#define SERVICE_MARK_FAMILIES	3

int32_t TextLayoutContext::GetCharMetrics(wchar_t c, int32_t family, bool mask, float x,
					  FontMetrics &font, CharMetrics &metrics, bool &image)
{
	int32_t ret;

	image = false;
	if (mask)
		c = L'*';
	if ((c >= EMOJI_FIRST && c <= EMOJI_LAST) ||
	    (c == SERVICE_MARK && family < SERVICE_MARK_FAMILIES)) {
		if (emoji_rasterizer != NULL) {
			ret = emoji_rasterizer->GetCharMetrics(c, metrics);
			if (ret == SCE_PAF_OK) {
				/* the image scaled to the font, to the pixel */
				float scale = font.ascender / metrics.font.ascender;

				metrics.left = (float)(int32_t)(metrics.left * scale);
				metrics.right = (float)(int32_t)(metrics.right * scale);
				metrics.advance = (float)(int32_t)(metrics.advance * scale);
				image = true;
				metrics.font.height = font.height;
				metrics.font.ascender = font.ascender;
				metrics.font.scale = scale;
			}
		}
	}
	if (!image) {
		ret = rasterizer->GetCharMetrics(c, metrics);
		if (ret == SCE_PAF_OK) {
			float left = x + metrics.left;

			if (left < 0.0f) {
				metrics.left = -x;
				metrics.right -= left;
				metrics.advance -= left;
			}
		}
	}
	return ret;
}

int32_t TextLayoutLine::UpdateFrame(TextLayoutCharInfo *infos)
{
	ascender = 0.0f;
	frame = math::v4(frame.extract_x(), frame.extract_y(), 0.0f, 0.0f);
	for (vector<TextLayoutRun *>::iterator it = runs.begin(); it != runs.end(); ++it) {
		TextLayoutRun *run = *it;

		if (it == runs.begin() || run->width > 0.0f) {
			frame.set_z(infos[run->index].x + run->width);
			if (frame.extract_w() < run->height)
				frame.set_w(run->height);
			if (ascender < run->ascender)
				ascender = run->ascender;
		}
	}
	return SCE_PAF_OK;
}

void TextLayoutLine::Nop()
{
}

/* where lines break (names ours): the characters a line breaks after, below the CJK blocks
   (beyond them a line breaks after any character), and with kinsoku the ones that may not end
   a line and the ones that may not begin one */
static const wchar_t s_break_chars[] = {
	0x0009, 0x000A, 0x0020, 0x0021, 0x0023, 0x0025, 0x0026, 0x0029, 0x002A, 0x002B,
	0x002C, 0x002D, 0x002E, 0x002F, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
	0x0040, 0x005D, 0x005E, 0x005F, 0x007D, 0x007E
};

static const wchar_t s_kinsoku_tail[] = {
	0x0024, 0x0028, 0x005B, 0x005C, 0x007B, 0x0083, 0x00A0, 0x00A9, 0x00AE, 0x2018,
	0x201C, 0x2120, 0x2122, 0x3008, 0x300A, 0x300C, 0x300E, 0x3010, 0x3014, 0xFF08,
	0xFF3B, 0xFF40, 0xFF5B
};

static const wchar_t s_kinsoku_head[] = {
	0x0021, 0x0029, 0x002C, 0x002E, 0x003A, 0x003B, 0x003F, 0x005D, 0x007D, 0x0083,
	0x00A0, 0x00A9, 0x00AE, 0x00B4, 0x2019, 0x201D, 0x2025, 0x2026, 0x2120, 0x2122,
	0x3001, 0x3002, 0x3003, 0x3005, 0x3009, 0x300B, 0x300D, 0x300F, 0x3011, 0x3015,
	0x309B, 0x309C, 0x309D, 0x309E, 0x30FB, 0x30FD, 0x30FE, 0xFF01, 0xFF09, 0xFF0C,
	0xFF0E, 0xFF1A, 0xFF1B, 0xFF1F, 0xFF3D, 0xFF5D
};

#define BREAK_CHARS_LAST	0x052F

/* c in the sorted table */
static inline bool IsInTable(const wchar_t *table, uint32_t len, wchar_t c)
{
	for (uint32_t i = 0; i < len; i++) {
		if (c <= table[i])
			return c == table[i];
	}
	return false;
}

#define COUNTOF(a)		(sizeof(a) / sizeof((a)[0]))

bool TextLayoutContext::IsBreakChar(wchar_t c)
{
	if ((param.flags & TextLayoutParam::Flag_Kinsoku) != 0 &&
	    IsInTable(s_kinsoku_tail, COUNTOF(s_kinsoku_tail), c))
		return false;
	if (c > BREAK_CHARS_LAST)
		return true;
	return IsInTable(s_break_chars, COUNTOF(s_break_chars), c);
}

/* no line starts with next */
static inline bool CanBreakBefore(TextLayoutContext *ctx, wchar_t next)
{
	return (ctx->param.flags & TextLayoutParam::Flag_Kinsoku) == 0 ||
	       !IsInTable(s_kinsoku_head, COUNTOF(s_kinsoku_head), next);
}

/* a line may break before the character at base + n: the next character is read, from base
   as it is then, once the one before it is known to allow a break */
static inline bool CanBreak(TextLayoutContext *ctx, const uint32_t &base, uint32_t n)
{
	if (!ctx->IsBreakChar(ctx->str.c_str()[base + n - 1]))
		return false;

	return CanBreakBefore(ctx, ctx->str.c_str()[base + n]);
}

/* back from the last run: in a run of style, the last place it may break; a run of another
   style ends where it breaks. The runs from there on are taken off, to be laid out again on
   the next line */
int32_t TextLayoutLine::FindBreak(TextLayoutContext *ctx, float &line_height, float &run_offset,
				  uint32_t &index, uint32_t &count,
				  const common::SharedPtr<TextStyle> &style,
				  TextLayoutCharInfo *infos)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_ABORTED;
	vector<TextLayoutRun *>::iterator it = runs.end();

	if (it != runs.begin()) {
		--it;
		for (;;) {
			TextLayoutRun *run = *it;

			if (style.get() == run->style.get()) {
				for (uint32_t n = run->len; n != 0; n--) {
					if (CanBreak(ctx, run->index, n)) {
						run_offset = run->glyph_offset;
						index = run->index + n - 1;
						count = n;
						ret = SCE_PAF_OK;
						break;
					}
				}
			} else {
				++it;
				index = run->index + run->len - 1;
				count = 0;
				ret = SCE_PAF_OK;
			}
			if (ret == SCE_PAF_OK)
				break;
			if (it == runs.begin())
				return ret;
			--it;
		}
		while (it != runs.end()) {
			TextLayoutRun *run = *it;

			it = runs.erase(it);
			delete run;
		}
		if (count == 0) {
			UpdateFrame(infos);
			line_height = frame.extract_w();
		}
	}
	return ret;
}

/* the last place the run of count characters to index may break, else one on the line */
int32_t TextLayout::FindBreak(TextLayoutContext *ctx, uint32_t line, float &line_height,
			      float &run_offset, uint32_t &index, uint32_t &count,
			      const common::SharedPtr<TextStyle> &style)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_ABORTED;

	for (uint32_t i = 0; i < count; i++) {
		if (!ctx->IsBreakChar(ctx->str.c_str()[index - i]))
			continue;
		uint32_t base = index - i;
		if (CanBreakBefore(ctx, ctx->str.c_str()[base + 1])) {
			index = base;
			count -= i;
			return SCE_PAF_OK;
		}
	}
	if (line < m_lines.size())
		ret = m_lines[line]->FindBreak(ctx, line_height, run_offset, index, count, style,
					       m_char_infos);
	return ret;
}

/* one for each character and one past the last */
int32_t TextLayout::AllocCharInfos(uint32_t len)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;

	m_char_infos = new TextLayoutCharInfo[len + 1];
	if (m_char_infos != NULL) {
		ret = SCE_PAF_OK;
		m_char_info_count = len + 1;
	}
	return ret;
}

/* the widest a run is drawn in one glyph surface (name ours) */
#define RUN_WIDTH_MAX		480.0f

/* each style run's characters measured and gathered into runs on lines: a run ends at a line
   break, at the box's width with word wrap (back at the last place the line may break), at an
   emoji or every character with CharPrimitive, and at RUN_WIDTH_MAX */
int32_t TextLayout::UpdateLayout(TextLayoutContext *ctx)
{
	int32_t ret;

	if ((m_state & TextLayoutState_Abort) != 0)
		return SCE_PAF_ERROR_TEXT_ABORTED;

	float *bounds = (float *)&m_bounds;

	bounds[0] = 0.0f;
	bounds[1] = 0.0f;
	ReleaseLines();

	/* the tallest font on the line, and where the run's glyph starts */
	float line_height = 0.0f;
	float run_offset = 0.0f;
	/* the character, and how many the run has to it */
	uint32_t index = 0;
	uint32_t count = 0;

	ret = AllocCharInfos(ctx->str.size());
	if (ret < 0)
		return ret;

	FontMetrics font;
	/* the run's last character */
	CharMetrics last;
	CharMetrics metrics;
	float y = 0.0f;
	uint32_t line = 0;
	/* how far the last character reaches */
	float extent = 0.0f;
	int32_t err = ret;
	/* whether the run is an emoji's, and the character */
	bool image = false;
	bool image_char;

	for (vector<TextLayoutStyleRun>::iterator it = ctx->runs.begin(); it != ctx->runs.end();
	     ++it) {
		if ((m_state & TextLayoutState_Abort) != 0) {
			err = SCE_PAF_ERROR_TEXT_ABORTED;
			goto out;
		}

		int32_t family;
		bool mask;

		it->style->GetStyleAttribute(TextStyleAttribute_Family, family);
		it->style->GetStyleAttribute(TextStyleAttribute_Mask, mask);
		err = ctx->rasterizer->SetStyle(it->style);
		if (err < 0)
			goto out;
		err = ctx->rasterizer->GetFontMetrics(font);
		if (err < 0)
			goto out;
		last.font = font;

		uint32_t end = index + it->len;

		while (index < end) {
			if ((m_state & TextLayoutState_Abort) != 0) {
				err = SCE_PAF_ERROR_TEXT_ABORTED;
				goto out;
			}
			count++;

			TextLayoutCharInfo *infos = m_char_infos;
			wchar_t c = ctx->str.c_str()[index];
			float last_x;
			float first_x;
			uint32_t next;
			bool add;
			bool new_line;

			if (c == L'\n') {
				float right = 0.0f;

				if (count > 1) {
					infos[index].x = infos[index - 1].x + extent;
					right = last.right - extent;
				}
				last.right = right;
				last.font = font;
				last.advance = 0.0f;
				last.left = 0.0f;
				image = false;
				if (line_height == 0.0f)
					line_height = font.height * font.scale;
				last_x = infos[index].x;
				first_x = infos[index - count + 1].x;
				add = true;
				next = index + 1;
				new_line = true;
			} else {
				err = ctx->GetCharMetrics(c, family, mask, infos[index].x, font, metrics,
							  image_char);
				if (err < 0)
					goto out;
				infos = m_char_infos;
				extent = metrics.right;
				if (metrics.right < metrics.advance)
					extent = metrics.advance;
				if (line_height < metrics.font.height)
					line_height = metrics.font.height;
				if ((ctx->param.flags & TextLayoutParam::Flag_WordWrap) != 0 &&
				    infos[index].x > 0.0f &&
				    ctx->param.width < infos[index].x + extent) {
					/* past the box: the line breaks at the last place it may */
					uint32_t at;

					if (c == L' ') {
						last.left = metrics.left;
						last.advance = metrics.advance;
						last.font = metrics.font;
						last.right = 0.0f;
					} else {
						index--;
						count--;
					}
					at = index;
					FindBreak(ctx, line, line_height, run_offset, index, count,
						  it->style);
					if (index != at) {
						err = ctx->GetCharMetrics(ctx->str.c_str()[index], family, mask,
									  infos[index].x, font, last,
									  image);
						if (err < 0)
							goto out;
					}
					infos = m_char_infos;
					last.advance = last.right;
					last_x = infos[index].x;
					first_x = infos[index - count + 1].x;
					add = true;
					next = index + 1;
					new_line = true;
				} else if ((image_char && count > 1) ||
					   infos[index].x - infos[index - count + 1].x + extent >
						   RUN_WIDTH_MAX) {
					/* the run ends before the character, which starts the next */
					index--;
					count--;
					last_x = infos[index].x;
					first_x = infos[index - count + 1].x;
					add = true;
					next = index + 1;
					new_line = false;
				} else {
					last.left = metrics.left;
					last.right = metrics.right;
					last.advance = metrics.advance;
					last.font = metrics.font;
					last_x = infos[index].x;
					first_x = infos[index - count + 1].x;
					next = index + 1;
					new_line = false;
					if (image_char ||
					    (ctx->param.flags & TextLayoutParam::Flag_CharPrimitive) != 0) {
						image = image_char;
						add = true;
					} else {
						image = false;
						add = false;
					}
				}
			}
			if (last_x == first_x)
				run_offset = last.left;
			index = next;
			if (add && count != 0) {
				err = AddRun(ctx, line, last, y, line_height, run_offset, next - count,
					     count, it->style, image);
				if (err < 0)
					goto out;
				count = 0;
				infos = m_char_infos;
			}
			if (new_line) {
				line++;
				if (ctx->param.line_pitch == 0.0f)
					y = line_height + ctx->param.line_space + y;
				else
					y = y + GetDrawLinePitch(m_param);
				line_height = 0.0f;
				infos = m_char_infos;
				infos[index].x = 0.0f;
			} else {
				infos[index].x = infos[index - 1].x + last.advance;
			}
			infos[index].line = line;
		}

		/* the string's end, after the last style run */
		if (ctx->str.c_str()[index] == L'\0') {
			count++;

			float right = 0.0f;

			if (count > 1) {
				m_char_infos[index].x = m_char_infos[index - 1].x + extent;
				right = last.right - extent;
			}
			last.right = right;
			last.advance = 0.0f;
			last.left = 0.0f;
			last.font = font;
			if (line_height == 0.0f)
				line_height = font.height * font.scale;
			index++;
		}
		if (count != 0) {
			err = AddRun(ctx, line, last, y, line_height, run_offset, index - count, count,
				     it->style, false);
			if (err < 0)
				goto out;
			count = 0;
		}
	}
out:
	/* the lines filled up: laid out as far as they go */
	if (err == SCE_PAF_ERROR_TEXT_LINE_LIMIT)
		err = SCE_PAF_OK;
	ret = err;
	if (ret == SCE_PAF_OK && m_lines.size() != 0) {
		math::v4 frame;

		for (uint32_t i = 0; i < m_lines.size(); i++) {
			if ((m_state & TextLayoutState_Abort) != 0) {
				ret = SCE_PAF_ERROR_TEXT_ABORTED;
				break;
			}
			frame = m_lines[i]->frame;
			if (bounds[0] < frame.extract_z())
				bounds[0] = frame.extract_z();
		}
		bounds[1] = (float)(((int32_t)(frame.extract_y() + frame.extract_w()) + 1) & ~1);
	}
	return ret;
}

/* the layout made again where it is invalid, with the rasterizers and the cache given (the
   text thread's; the factories' when none is given): the string, the styles and the
   attributes copied under the layout mutex, the lines laid out, placed and drawn outside it */
int32_t TextLayout::Update(FontRasterizer *rasterizer, FontRasterizer *emoji_rasterizer,
			   TextSurfaceCache *cache)
{
	TextLayoutContext ctx;
	bool own_rasterizer;
	bool own_emoji_rasterizer;

	if (rasterizer != NULL) {
		ctx.rasterizer = rasterizer;
		own_rasterizer = false;
	} else {
		ctx.rasterizer = CreateFontRasterizer();
		own_rasterizer = false;
		if (ctx.rasterizer != NULL)
			own_rasterizer = true;
	}
	if (emoji_rasterizer != NULL) {
		ctx.emoji_rasterizer = emoji_rasterizer;
		own_emoji_rasterizer = false;
	} else {
		ctx.emoji_rasterizer = CreateEmojiFontRasterizer();
		own_emoji_rasterizer = false;
		if (ctx.emoji_rasterizer != NULL)
			own_emoji_rasterizer = true;
	}
	ctx.cache = cache;

	if (m_mutex != NULL)
		m_mutex->Lock();

	uint16_t state = m_state;

	/* what is invalid is being made again */
	m_state = (state | ((state & TextLayoutState_Invalid) << 4)) &
		  ~(TextLayoutState_Invalid | TextLayoutState_Waiting);
	if (m_state == TextLayoutState_None) {
		if (m_mutex != NULL)
			m_mutex->Unlock();
		return SCE_PAF_OK;
	}
	ctx.param = m_param;

	AttributedString *str = &m_str;

	if (m_placeholder != NULL) {
		uint32_t len;

		m_str.GetLength(len);
		if (len == 0)
			str = m_placeholder;
	}
	str->GetString(ctx.str);

	_list_iterator<TextStyleRun *> begin(NULL);
	_list_iterator<TextStyleRun *> end(NULL);

	str->GetStyleRuns(begin, end);
	for (_list_iterator<TextStyleRun *> it = begin; it != end; ++it) {
		TextLayoutStyleRun run;
		TextStyleRun *src = *it;

		run.style = common::SharedPtr<TextStyle>(new TextStyle(src->style));
		if (run.style.get() == NULL) {
			if (m_mutex != NULL)
				m_mutex->Unlock();
			return SCE_PAF_ERROR_TEXT_NO_MEMORY;
		}
		run.len = src->len;
		ctx.runs.push_back(run);
	}
	if (m_mutex != NULL)
		m_mutex->Unlock();

	/* nothing to wrap in a box as wide as the layout */
	if ((ctx.param.flags & TextLayoutParam::Flag_AdjustX) != 0 && ctx.param.width <= 0.0f)
		ctx.param.flags &= ~TextLayoutParam::Flag_WordWrap;

	int32_t ret = SCE_PAF_OK;

	if ((state & TextLayoutState_InvalidLayout) == 0 || (ret = UpdateLayout(&ctx)) == 0) {
		const float *bounds = (const float *)&m_bounds;

		/* an adjusted side is the layout's */
		if ((ctx.param.flags & TextLayoutParam::Flag_AdjustX) != 0)
			ctx.param.width = bounds[0];
		if ((ctx.param.flags & TextLayoutParam::Flag_AdjustY) != 0)
			ctx.param.height = bounds[1];
		/* no visible size: the box's */
		if (ctx.param.visible.extract_z() <= 0.0f)
			ctx.param.visible.set_z(ctx.param.width);
		if (ctx.param.visible.extract_w() <= 0.0f)
			ctx.param.visible.set_w(ctx.param.height);
		if (ctx.param.width < bounds[0])
			ctx.param.width = bounds[0];
		if (ctx.param.height < bounds[1])
			ctx.param.height = bounds[1];
		if ((state & TextLayoutState_InvalidGeometry) != 0)
			ret = UpdateGeometry(&ctx);
		if (ret == SCE_PAF_OK &&
		    (ctx.param.flags & TextLayoutParam::Flag_LayoutOnly) == 0 &&
		    (state & TextLayoutState_InvalidGraphics) != 0)
			ret = UpdateGraphics(&ctx);
	}

	if (m_mutex != NULL)
		m_mutex->Lock();
	if ((m_state & TextLayoutState_Abort) != 0) {
		/* released while it was being made */
		ret = SCE_PAF_ERROR_TEXT_CANCELED;
		m_state &= ~TextLayoutState_Abort;
	}
	if (ret != SCE_PAF_OK) {
		ReleaseAll();
	} else {
		UpdateCaret();
		UpdateSelection();
	}
	m_state &= ~TextLayoutState_Updating;
	if (m_mutex != NULL)
		m_mutex->Unlock();
	if (own_rasterizer && ctx.rasterizer != NULL)
		delete ctx.rasterizer;
	if (own_emoji_rasterizer && ctx.emoji_rasterizer != NULL)
		delete ctx.emoji_rasterizer;
	return ret;
}

/* the layout let go; one being made is told to stop, and released when it does */
int32_t TextLayout::Release()
{
	uint16_t state;

	if (m_mutex != NULL)
		m_mutex->Lock();
	if ((m_state & TextLayoutState_Updating) != 0) {
		state = m_state | TextLayoutState_Abort;
	} else {
		ReleaseAll();
		state = m_state;
	}
	m_state = state & ~TextLayoutState_Invalid;
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return SCE_PAF_OK;
}

/* what a layout being laid out or placed cannot answer yet */
#define GEOMETRY_BUSY		(TextLayoutState_InvalidGeometry | \
				 TextLayoutState_UpdatingGeometry)
#define LAYOUT_BUSY		(TextLayoutState_InvalidLayout | TextLayoutState_UpdatingLayout)
#define GRAPHICS_BUSY		(TextLayoutState_InvalidGraphics | \
				 TextLayoutState_UpdatingGraphics)

int32_t TextLayout::GetBounds(math::v2 &bounds)
{
	if ((m_state & GEOMETRY_BUSY) != 0)
		return SCE_PAF_ERROR_TEXT_BUSY;
	bounds = m_bounds;
	return SCE_PAF_OK;
}

int32_t TextLayout::GetDrawSize(math::v2 &size)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_BUSY;

	if ((m_state & GEOMETRY_BUSY) == 0) {
		const float *bounds = (const float *)&m_bounds;
		float *size_xy = (float *)&size;

		size_xy[0] = (m_param.flags & TextLayoutParam::Flag_AdjustX) != 0 ? bounds[0] :
										     m_param.width;
		size_xy[1] = (m_param.flags & TextLayoutParam::Flag_AdjustY) != 0 ? bounds[1] :
										     m_param.height;
		ret = SCE_PAF_OK;
	}
	return ret;
}

int32_t TextLayout::GetLineCount(uint32_t &count)
{
	if ((m_state & LAYOUT_BUSY) != 0)
		return SCE_PAF_ERROR_TEXT_BUSY;
	count = m_lines.size();
	return SCE_PAF_OK;
}

int32_t TextLayoutLine::MoveChar(uint32_t run, const math::v2 &pos, TextLayoutCharInfo *infos,
				 TextGraphics *graphics)
{
	if (run < runs.size()) {
		TextLayoutRun *r = runs[run];

		if (r->glyph != NULL)
			graphics->MoveChar(r->glyph,
					   math::v4(frame.extract_x() + infos[r->index].x +
							    pos.extract_x(),
						    frame.extract_y() + ascender - r->ascender +
							    pos.extract_y(),
						    r->width, r->height));
	}
	return SCE_PAF_OK;
}

/* with CharPrimitive, each character its own glyph: the character's laid out pos away */
int32_t TextLayout::SetCharPos(uint32_t index, const math::v2 &pos)
{
	if ((m_state & GRAPHICS_BUSY) != 0 ||
	    (m_param.flags & TextLayoutParam::Flag_CharPrimitive) == 0)
		return SCE_PAF_ERROR_TEXT_BUSY;
	if (index < m_char_info_count) {
		uint32_t line = m_char_infos[index].line;

		if (line < m_lines.size()) {
			TextLayoutLine *l = m_lines[line];
			uint32_t first = 0;

			if (l->runs.size() != 0)
				first = l->runs[0]->index;
			if (index >= first) {
				if (m_graphics != NULL)
					l->MoveChar(index - first, pos, m_char_infos, m_graphics);
				return SCE_PAF_OK;
			}
		}
	}
	return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
}

int32_t TextLayout::GetCharInfo(uint32_t index, TextLayoutCharInfo &info)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_BUSY;

	if ((m_state & GEOMETRY_BUSY) == 0) {
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		if (index < m_char_info_count) {
			TextLayoutCharInfo *ci = &m_char_infos[index];
			uint32_t line = ci->line;

			if (line < m_lines.size()) {
				math::v4 frame = m_lines[line]->frame;

				ret = SCE_PAF_OK;
				info.line = line;
				info.x = frame.extract_x() + ci->x;
			}
		}
	}
	return ret;
}

/* the share of the ascender a line's descender is taken to be (name ours) */
#define DESCENDER_RATIO		0.15f

int32_t TextLayout::GetLineInfo(uint32_t line, TextLayoutLineInfo &info)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_BUSY;

	if ((m_state & GEOMETRY_BUSY) == 0) {
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		if (line < m_lines.size()) {
			TextLayoutLine *l = m_lines[line];

			info.frame = l->frame;
			info.ascender = l->ascender;
			info.descender = (float)(int32_t)(l->ascender * DESCENDER_RATIO);
			if (l->runs.size() != 0) {
				uint32_t index = l->runs[0]->index;
				TextLayoutRun *last;

				info.index = index;
				last = l->runs[l->runs.size() - 1];
				info.len = last->index + last->len - index;
			} else {
				info.index = 0;
				info.len = 0;
			}
			if (info.len != 0) {
				AttributedString *str = &m_str;
				const wchar_t *buf;
				uint32_t buf_len;

				if (m_placeholder != NULL) {
					uint32_t len;

					m_str.GetLength(len);
					if (len == 0)
						str = m_placeholder;
				}
				str->GetStringBuffer(buf, buf_len);
				if (IsControlChar(buf[info.index + info.len - 1]))
					info.len--;
			}
			ret = SCE_PAF_OK;
		}
	}
	return ret;
}

uint32_t TextLayout::HitTestLine(uint32_t line, bool end, float x)
{
	uint32_t index = 0;
	uint32_t len;

	m_str.GetLength(len);
	if (len != 0 && line < m_lines.size()) {
		TextLayoutLine *l = m_lines[line];
		math::v4 frame = l->frame;
		uint32_t first = 0;
		uint32_t last = 0;
		float nearest = 0.0f;

		if (l->runs.size() != 0) {
			TextLayoutRun *run = l->runs[l->runs.size() - 1];

			first = l->runs[0]->index;
			last = run->index + run->len;
		}
		for (uint32_t i = first; i < last; i++) {
			float d = __builtin_fabsf(frame.extract_x() + m_char_infos[i].x - x);

			if (i > first && d > nearest)
				break;
			index = i;
			nearest = d;
		}
		if (end && __builtin_fabsf(frame.extract_x() + frame.extract_z() - x) < nearest)
			index = last;
	}
	return index;
}

int32_t TextLayout::GetNextUpHit(uint32_t index, uint32_t &next)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_BUSY;

	if ((m_state & GEOMETRY_BUSY) == 0) {
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		if (index < m_char_info_count) {
			TextLayoutCharInfo *ci = &m_char_infos[index];
			uint32_t line = ci->line;

			if (line < m_lines.size()) {
				math::v4 frame = m_lines[line]->frame;

				if (line != 0)
					index = HitTestLine(line - 1, false, frame.extract_x() + ci->x);
				next = index;
				return SCE_PAF_OK;
			}
		}
	}
	return ret;
}

int32_t TextLayout::GetNextDownHit(uint32_t index, uint32_t &next)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_BUSY;

	if ((m_state & GEOMETRY_BUSY) == 0) {
		ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
		if (index < m_char_info_count) {
			TextLayoutCharInfo *ci = &m_char_infos[index];
			uint32_t line = ci->line;

			if (line < m_lines.size()) {
				math::v4 frame = m_lines[line]->frame;

				if (line < m_lines.size() - 1)
					index = HitTestLine(line + 1, false, frame.extract_x() + ci->x);
				next = index;
				return SCE_PAF_OK;
			}
		}
	}
	return ret;
}

int32_t TextLayout::GetNextLeftHit(uint32_t index, uint32_t &next)
{
	if ((m_state & GEOMETRY_BUSY) != 0)
		return SCE_PAF_ERROR_TEXT_BUSY;
	if (index >= m_char_info_count)
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;

	uint32_t line = m_char_infos[index].line;

	if (line >= m_lines.size())
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;

	TextLayoutLine *l = m_lines[line];
	uint32_t first = 0;

	if (l->runs.size() != 0)
		first = l->runs[0]->index;
	if (index > first) {
		next = index - 1;
	} else if (line != 0) {
		/* the last character of the line before */
		TextLayoutLine *prev = m_lines[line - 1];
		uint32_t last = (uint32_t)-1;

		if (prev->runs.size() != 0) {
			TextLayoutRun *run = prev->runs[prev->runs.size() - 1];

			last = run->index + run->len - 1;
		}
		next = last;
	} else {
		next = index;
	}
	return SCE_PAF_OK;
}

int32_t TextLayout::GetNextRightHit(uint32_t index, uint32_t &next)
{
	if ((m_state & GEOMETRY_BUSY) != 0)
		return SCE_PAF_ERROR_TEXT_BUSY;
	if (index >= m_char_info_count)
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;

	uint32_t line = m_char_infos[index].line;

	if (line >= m_lines.size())
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;

	TextLayoutLine *l = m_lines[line];
	uint32_t last = (uint32_t)-1;

	if (l->runs.size() != 0) {
		TextLayoutRun *run = l->runs[l->runs.size() - 1];

		last = run->index + run->len - 1;
	}
	if (index < last) {
		next = index + 1;
	} else if (line < m_lines.size() - 1) {
		/* the first character of the next line */
		TextLayoutLine *following = m_lines[line + 1];
		uint32_t first = 0;

		if (following->runs.size() != 0)
			first = following->runs[0]->index;
		next = first;
	} else {
		next = index;
	}
	return SCE_PAF_OK;
}

/* by the lines' middles */
uint32_t TextLayout::HitTestLineY(float y)
{
	uint32_t line = 0;
	float nearest = 0.0f;

	for (uint32_t i = 0; i < m_lines.size(); i++) {
		math::v4 &frame = m_lines[i]->frame;
		float d = __builtin_fabsf(frame.extract_y() + frame.extract_w() * 0.5f - y);

		if (i != 0 && d > nearest)
			break;
		line = i;
		nearest = d;
	}
	return line;
}

/* the character nearest pos; inside, only on a line's frame */
int32_t TextLayout::HitTestChar(const math::v2 &pos, bool inside, uint32_t &index)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_BUSY;

	if ((m_state & GEOMETRY_BUSY) == 0) {
		uint32_t line = HitTestLineY(pos.extract_y());
		bool hit = true;

		ret = SCE_PAF_ERROR_TEXT_NOT_FOUND;
		if (inside) {
			hit = false;
			if (line < m_lines.size()) {
				math::v4 frame = m_lines[line]->frame;

				hit = frame.extract_x() <= pos.extract_x() &&
				      pos.extract_x() <= frame.extract_x() + frame.extract_z() &&
				      frame.extract_y() <= pos.extract_y() &&
				      pos.extract_y() <= frame.extract_y() + frame.extract_w();
			}
		}
		if (hit) {
			ret = SCE_PAF_OK;
			index = HitTestLine(line, false, pos.extract_x());
		}
	}
	return ret;
}

int32_t TextLayout::MoveCaretUp()
{
	uint32_t index;
	int32_t ret = GetNextUpHit(m_caret_index, index);

	if (ret == SCE_PAF_OK && index != m_caret_index) {
		m_caret_index = index;
		UpdateCaret();
	}
	return ret;
}

int32_t TextLayout::MoveCaretDown()
{
	uint32_t index;
	int32_t ret = GetNextDownHit(m_caret_index, index);

	if (ret == SCE_PAF_OK && index != m_caret_index) {
		m_caret_index = index;
		UpdateCaret();
	}
	return ret;
}

int32_t TextLayout::MoveCaretLeft()
{
	uint32_t index;
	int32_t ret = GetNextLeftHit(m_caret_index, index);

	if (ret == SCE_PAF_OK && index != m_caret_index) {
		m_caret_index = index;
		UpdateCaret();
	}
	return ret;
}

int32_t TextLayout::MoveCaretRight()
{
	uint32_t index;
	int32_t ret = GetNextRightHit(m_caret_index, index);

	if (ret == SCE_PAF_OK && index != m_caret_index) {
		m_caret_index = index;
		UpdateCaret();
	}
	return ret;
}

int32_t TextLayout::SetCaretPos(const math::v2 &pos)
{
	uint32_t index;
	int32_t ret = HitTestChar(pos, false, index);

	if (ret == SCE_PAF_OK && index != m_caret_index) {
		m_caret_index = index;
		UpdateCaret();
	}
	return ret;
}

int32_t TextLayout::SetSelectionPos(const math::v2 &start, const math::v2 &end)
{
	uint32_t len;

	if ((m_state & GEOMETRY_BUSY) != 0)
		return SCE_PAF_ERROR_TEXT_BUSY;
	m_str.GetLength(len);
	if (len != 0) {
		uint32_t first = HitTestLine(HitTestLineY(start.extract_y()), true, start.extract_x());
		uint32_t last = HitTestLine(HitTestLineY(end.extract_y()), true, end.extract_x());

		if (last > first) {
			uint32_t i = first;

			first = last;
			last = i;
		}
		m_selection_index = last;
		m_selection_len = first - last;
		UpdateSelection();
	}
	return SCE_PAF_OK;
}

int32_t TextLayout::SetCaretIndex(uint32_t index)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_BUSY;

	if (m_mutex != NULL)
		m_mutex->Lock();
	m_caret_index = index;
	if ((m_state & GEOMETRY_BUSY) == 0) {
		UpdateCaret();
		ret = SCE_PAF_OK;
	}
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return ret;
}

int32_t TextLayout::GetCaretIndex(uint32_t &index)
{
	index = m_caret_index;
	return SCE_PAF_OK;
}

int32_t TextLayout::SetSelectionRange(uint32_t index, uint32_t len)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_BUSY;

	if (m_mutex != NULL)
		m_mutex->Lock();
	m_selection_index = index;
	m_selection_len = len;
	if ((m_state & GEOMETRY_BUSY) == 0) {
		UpdateSelection();
		ret = SCE_PAF_OK;
	}
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return ret;
}

int32_t TextLayout::GetSelectionRange(uint32_t &index, uint32_t &len)
{
	index = m_selection_index;
	len = m_selection_len;
	return SCE_PAF_OK;
}

int32_t TextLayout::GetState(int32_t &state)
{
	state = m_state;
	return SCE_PAF_OK;
}

int32_t TextLayout::SetState(int32_t state)
{
	m_state |= state;
	return SCE_PAF_OK;
}

int32_t TextLayout::ClearState(int32_t state)
{
	m_state &= ~state;
	return SCE_PAF_OK;
}

int32_t TextLayout::Invalidate()
{
	if (m_mutex != NULL)
		m_mutex->Lock();
	m_state |= TextLayoutState_Invalid;
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return TextLayoutState_Invalid;
}

int32_t TextLayout::GetGraphics(TextGraphics *&graphics)
{
	graphics = m_graphics;
	return SCE_PAF_OK;
}

TextLayoutLine::~TextLayoutLine()
{
	for (vector<TextLayoutRun *>::iterator it = runs.begin(); it != runs.end(); ++it)
		delete *it;
}

}
}
