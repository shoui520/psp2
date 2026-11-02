/*
 * paf::graph::TextLayout: an attributed string laid out in a box, with a caret and a selection.
 * The text threads lay it out, make its glyphs' geometry and draw them into its graphics
 * (TextGraphics) in turn; a layout is queued with the text queue for that and released when its
 * draw object is. And the attributes that steer the layout, and what it says of a character or
 * a line.
 *
 * Class, method and enum names from the mangled export names (TextLayout::Release and
 * HitTestChar by NID) and ScePaf-RE's header (restored from Sony's DWARF); the layout
 * attributes after the names the text object parameters read them under ("max_line_count",
 * "enlarge_line_pitch", ...), which 3.6x numbers one higher than the DWARF from max_line_count
 * on. TextQueue and its methods are named by us after the mutexes Sony names
 * ScePafTextLayoutMutex and ScePafTextQueueMutex, and after the layout states
 * (TextLayoutState_InvalidGraphics, ...); TextGraphics is in text_graphics.h.
 */
#ifndef PAF_GRAPHICS_TEXT_TEXT_LAYOUT_H
#define PAF_GRAPHICS_TEXT_TEXT_LAYOUT_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/text/attributed_string.h>
#include <graphics/text/text_graphics.h>
#include <graphics/text/text_style.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/string>
#include <std/vector>

namespace paf {

namespace thread {
class RMutex;
}

namespace graph {

class Shader;

enum TextLayoutAttribute {
	TextLayoutAttribute_AlignX = 0,
	TextLayoutAttribute_AlignY = 1,
	TextLayoutAttribute_TextAlign = 2,
	TextLayoutAttribute_MaxLineCount = 3,
	TextLayoutAttribute_LineSpace = 4,
	TextLayoutAttribute_LinePitch = 5,
	TextLayoutAttribute_Size = 6,
	TextLayoutAttribute_VisiblePos = 7,
	TextLayoutAttribute_VisibleSize = 8,
	TextLayoutAttribute_AdjustX = 9,
	TextLayoutAttribute_AdjustY = 10,
	TextLayoutAttribute_CondenseX = 11,
	TextLayoutAttribute_CondenseY = 12,
	TextLayoutAttribute_WordWrap = 13,
	TextLayoutAttribute_Kinsoku = 14,
	TextLayoutAttribute_Ellipsis = 15,
	TextLayoutAttribute_CharPrimitive = 16,
	TextLayoutAttribute_LayoutOnly = 17,
	TextLayoutAttribute_EnlargeLinePitch = 18,
	/* the line pitch drawn: enlarged when the system asks for it (name ours) */
	TextLayoutAttribute_DrawLinePitch = 19
};

enum TextLayoutAlignX {
	TextLayoutAlignX_Left = 0,
	TextLayoutAlignX_Center = 1,
	TextLayoutAlignX_Right = 2
};

enum TextLayoutAlignY {
	TextLayoutAlignY_Top = 0,
	TextLayoutAlignY_Center = 1,
	TextLayoutAlignY_Bottom = 2
};

enum TextLayoutTextAlign {
	TextLayoutTextAlign_Left = 0,
	TextLayoutTextAlign_Center = 1,
	TextLayoutTextAlign_Right = 2
};

enum TextLayoutState {
	TextLayoutState_None = 0,
	TextLayoutState_InvalidLayout = 1,
	TextLayoutState_InvalidGeometry = 2,
	TextLayoutState_InvalidGraphics = 4,
	TextLayoutState_Invalid = 7,
	TextLayoutState_UpdatingLayout = 16,
	TextLayoutState_UpdatingGeometry = 32,
	TextLayoutState_UpdatingGraphics = 64,
	TextLayoutState_Updating = 112,
	TextLayoutState_Queuing = 256,
	TextLayoutState_Waiting = 512,
	TextLayoutState_Abort = 1024
};

class CharMetrics;
class FontRasterizer;
class TextLayoutContext;
class TextSurfaceCache;

class TextLayoutCharInfo {
public:
	TextLayoutCharInfo() : x(0.0f), line(0)
	{
	}

	virtual ~TextLayoutCharInfo()
	{
	}

	float x;
	uint32_t line;
};

class TextLayoutLineInfo {
public:
	TextLayoutLineInfo()
	{
	}

	virtual ~TextLayoutLineInfo()
	{
	}

	math::v4 frame;
	/* 15% of the ascender, to the pixel (name ours) */
	float descender;
	float ascender;
	/* its characters in the string, a control character at the end left out */
	uint32_t index;
	uint32_t len;
};

/* the layout attributes (names ours): the bool ones are bits of flags, one per attribute from
   AdjustX on */
class TextLayoutParam {
public:
	enum {
		Flag_AdjustX = 0x001,
		Flag_AdjustY = 0x002,
		Flag_CondenseX = 0x004,
		Flag_CondenseY = 0x008,
		Flag_WordWrap = 0x010,
		Flag_Kinsoku = 0x020,
		Flag_Ellipsis = 0x040,
		Flag_CharPrimitive = 0x080,
		Flag_LayoutOnly = 0x100,
		Flag_EnlargeLinePitch = 0x200
	};

	TextLayoutParam()
		: align_x(TextLayoutAlignX_Left), align_y(TextLayoutAlignY_Top),
		  text_align(TextLayoutTextAlign_Left), flags(Flag_Kinsoku), max_line_count(0),
		  line_space(0.0f), line_pitch(0.0f), width(0.0f), height(0.0f),
		  visible(0.0f, 0.0f, 0.0f, 0.0f)
	{
	}

	virtual ~TextLayoutParam()
	{
	}

	int8_t align_x;
	int8_t align_y;
	int8_t text_align;
	int16_t flags;
	uint16_t max_line_count;
	float line_space;
	float line_pitch;
	/* the box laid out in */
	float width;
	float height;
	/* the part of the layout drawn: its position, then its size */
	math::v4 visible;
};

/* a run of characters of one style on a line, and the glyph drawn for it (names ours) */
class TextLayoutRun {
public:
	enum {
		/* drawn by the emoji font */
		Flag_Image = 0x01,
		/* cut short: ends in an ellipsis */
		Flag_Ellipsis = 0x02
	};

	TextLayoutRun()
		: width(0.0f), height(0.0f), glyph_offset(0.0f), glyph_width(0.0f), ascender(0.0f),
		  surface_width(0), surface_height(0), index(0), len(0), flags(0), glyph(NULL)
	{
	}

	virtual ~TextLayoutRun()
	{
	}

	/* the box it takes on its line */
	float width;
	float height;
	/* where its glyph is drawn from in the box, and how wide */
	float glyph_offset;
	float glyph_width;
	float ascender;
	/* the glyph surface's size */
	uint16_t surface_width;
	uint16_t surface_height;
	/* its characters in the string */
	uint32_t index;
	uint32_t len;
	common::SharedPtr<TextStyle> style;
	int8_t flags;
	/* in the graphics, once drawn */
	TextGraphics::Char *glyph;
};

/* a line: its frame and ascender, and its runs (names ours) */
class TextLayoutLine {
public:
	TextLayoutLine(const math::v4 &frame) : frame(frame), ascender(0.0f)
	{
	}

	virtual ~TextLayoutLine();

	/* its runs' glyphs in clip drawn into the graphics, the others taken out */
	int32_t Draw(TextLayoutContext *ctx, const math::v4 &clip, TextLayoutCharInfo *infos,
		     TextGraphics *graphics);
	/* the line cut short to end in an ellipsis in the box */
	int32_t Ellipsize(TextLayoutContext *ctx, TextLayoutCharInfo *infos);
	/* a run of len characters from index, size big, its glyph surface width by height */
	int32_t AddRun(const math::v2 &size, float glyph_offset, float glyph_width, float ascender,
		       uint16_t width, uint16_t height, uint32_t index, uint32_t len,
		       const common::SharedPtr<TextStyle> &style, bool image,
		       TextLayoutCharInfo *infos);
	/* the frame's size and the ascender made again from the runs */
	int32_t UpdateFrame(TextLayoutCharInfo *infos);
	/* empty, and called by nothing */
	void Nop();
	/* where the line breaks before the run in style, the runs after it taken off */
	int32_t FindBreak(TextLayoutContext *ctx, float &line_height, float &run_offset,
			  uint32_t &index, uint32_t &count, const common::SharedPtr<TextStyle> &style,
			  TextLayoutCharInfo *infos);
	/* the run's glyph laid out pos away from its place */
	int32_t MoveChar(uint32_t run, const math::v2 &pos, TextLayoutCharInfo *infos,
			 TextGraphics *graphics);

	math::v4 frame;
	float ascender;
	vector<TextLayoutRun *> runs;
};

class TextLayout {
public:
	TextLayout(TextGraphics *graphics, thread::RMutex *mutex);
	TextLayout(const TextStyle &style, TextGraphics *graphics, thread::RMutex *mutex);
	TextLayout(const AttributedString &str, TextGraphics *graphics, thread::RMutex *mutex);
	virtual ~TextLayout();

	int32_t SetLayoutAttribute(TextLayoutAttribute attr, int32_t val);
	int32_t GetLayoutAttribute(TextLayoutAttribute attr, int32_t &val);
	int32_t SetLayoutAttribute(TextLayoutAttribute attr, float val);
	int32_t GetLayoutAttribute(TextLayoutAttribute attr, float &val);
	int32_t SetLayoutAttribute(TextLayoutAttribute attr, const math::v2 &val);
	int32_t GetLayoutAttribute(TextLayoutAttribute attr, math::v2 &val);
	int32_t SetLayoutAttribute(TextLayoutAttribute attr, bool val);
	int32_t GetLayoutAttribute(TextLayoutAttribute attr, bool &val);

	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len, int32_t val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, int32_t &val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len, float val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, float &val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				  const math::v2 &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, math::v2 &val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				  const math::v4 &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, math::v4 &val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len, bool val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, bool &val);
	int32_t SetStyle(uint32_t index, uint32_t len, const TextStyle &style);
	int32_t GetStyle(uint32_t index, TextStyle &style);

	int32_t SetString(const wstring &str);
	/* the len characters from index replaced by str (name ours) */
	int32_t ReplaceString(uint32_t index, uint32_t len, const wstring &str);
	int32_t GetString(wstring &str);
	int32_t GetLength(uint32_t &len);
	int32_t GetStringBuffer(const wchar_t *&buf, uint32_t &len);
	int32_t SetAttributedString(const AttributedString &str);
	int32_t GetAttributedString(AttributedString &str);
	/* the style of the placeholder string (name ours, after ui::Text's by NID) */
	int32_t SetPlaceholderStyle(const TextStyle &style);
	int32_t SetPlaceholderString(const wstring &str);
	int32_t GetPlaceholderString(wstring &str);

	int32_t ReleaseAll();
	int32_t Update(FontRasterizer *rasterizer, FontRasterizer *emoji_rasterizer,
		       TextSurfaceCache *cache);
	int32_t Release();
	int32_t GetBounds(math::v2 &bounds);
	/* the size laid out, unless the layout is being remade (name ours) */
	int32_t GetDrawSize(math::v2 &size);
	int32_t GetLineCount(uint32_t &count);
	int32_t SetCharPos(uint32_t index, const math::v2 &pos);
	int32_t GetCharInfo(uint32_t index, TextLayoutCharInfo &info);
	int32_t GetLineInfo(uint32_t line, TextLayoutLineInfo &info);
	int32_t GetNextUpHit(uint32_t index, uint32_t &next);
	int32_t GetNextDownHit(uint32_t index, uint32_t &next);
	int32_t GetNextLeftHit(uint32_t index, uint32_t &next);
	int32_t GetNextRightHit(uint32_t index, uint32_t &next);
	int32_t HitTestChar(const math::v2 &pos, bool inside, uint32_t &index);
	int32_t MoveCaretUp();
	int32_t MoveCaretDown();
	int32_t MoveCaretLeft();
	int32_t MoveCaretRight();
	int32_t SetCaretPos(const math::v2 &pos);
	int32_t SetSelectionPos(const math::v2 &start, const math::v2 &end);
	int32_t SetCaretIndex(uint32_t index);
	int32_t GetCaretIndex(uint32_t &index);
	int32_t SetSelectionRange(uint32_t index, uint32_t len);
	int32_t GetSelectionRange(uint32_t &index, uint32_t &len);
	int32_t GetState(int32_t &state);
	int32_t SetState(int32_t state);
	int32_t ClearState(int32_t state);
	int32_t Invalidate();
	/* the graphics the glyphs are drawn into (name ours) */
	int32_t GetGraphics(TextGraphics *&graphics);

private:
	/* names ours */
	int32_t CreatePlaceholder();
	/* the selection and the caret given to the graphics */
	int32_t UpdateSelection();
	int32_t UpdateCaret();
	/* the glyphs in sight drawn, the graphics scaled to the box */
	int32_t UpdateGraphics(TextLayoutContext *ctx);
	/* the lines placed in the box by the alignments */
	int32_t UpdateGeometry(TextLayoutContext *ctx);
	/* a run on the line, ellipsized where it leaves the box */
	int32_t AddRun(TextLayoutContext *ctx, uint32_t line, const CharMetrics &metrics, float y,
		       float height, float glyph_offset, uint32_t index, uint32_t len,
		       const common::SharedPtr<TextStyle> &style, bool image);
	/* where the line breaks before the character at index, count characters into its run */
	int32_t FindBreak(TextLayoutContext *ctx, uint32_t line, float &line_height,
			  float &run_offset, uint32_t &index, uint32_t &count,
			  const common::SharedPtr<TextStyle> &style);
	int32_t AllocCharInfos(uint32_t len);
	/* the characters measured and broken into lines */
	int32_t UpdateLayout(TextLayoutContext *ctx);
	/* the character on the line nearest x, or the line's end when it is nearer */
	uint32_t HitTestLine(uint32_t line, bool end, float x);
	/* the line nearest y */
	uint32_t HitTestLineY(float y);
	/* the char infos and the lines let go */
	void ReleaseLines()
	{
		if (m_char_infos != NULL) {
			delete[] m_char_infos;
			m_char_infos = NULL;
			m_char_info_count = 0;
		}
		for (uint32_t i = 0; i < m_lines.size(); i++)
			delete m_lines[i];
		m_lines.clear();
	}

public:

	TextLayoutParam m_param;
	AttributedString m_str;
	/* shown while the string is empty, in the first character's style faded */
	AttributedString *m_placeholder;
	TextGraphics *m_graphics;
	uint32_t m_caret_index;
	uint32_t m_selection_index;
	uint32_t m_selection_len;
	/* the size laid out */
	math::v2 m_bounds;
	int16_t m_state;
	/* one for each character and one past the last */
	TextLayoutCharInfo *m_char_infos;
	uint32_t m_char_info_count;
	vector<TextLayoutLine *> m_lines;
	thread::RMutex *m_mutex;
};

/* the layouts waiting for the text threads (names ours) */
class TextQueue {
public:
	TextQueue();
	virtual ~TextQueue();

	/* queues a layout that is invalid and not being remade or queued */
	int32_t Request(common::SharedPtr<TextLayout> &layout);
	/* takes a queued layout off the queue */
	int32_t Cancel(common::SharedPtr<TextLayout> &layout);
	/* the queue emptied and the text threads waiting on it woken */
	int32_t Abort();

	/* not in C yet: left as bytes until it is */
	uint8_t m_data[0x44];
};

/* the mutexes the text graphics and layouts lock, and the text queue (names ours) */
thread::RMutex *GetTextGraphicsMutex();
thread::RMutex *GetTextLayoutMutex();
TextQueue *GetTextQueue();

}
}

#endif
