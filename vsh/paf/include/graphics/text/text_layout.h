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
#include <graphics/text/text_style.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/string>

namespace paf {

namespace thread {
class RMutex;
}

namespace graph {

class AttributedString;
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
	TextLayoutAttribute_EnlargeLinePitch = 18
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

class TextGraphics;
class TextLayoutCharInfo;
class TextLayoutLineInfo;

class TextLayout {
public:
	TextLayout(TextGraphics *graphics, thread::RMutex *mutex);
	virtual ~TextLayout();

	int32_t SetLayoutAttribute(TextLayoutAttribute attr, int32_t val);
	int32_t SetLayoutAttribute(TextLayoutAttribute attr, float val);
	int32_t SetLayoutAttribute(TextLayoutAttribute attr, const math::v2 &val);
	int32_t SetLayoutAttribute(TextLayoutAttribute attr, bool val);
	int32_t GetLayoutAttribute(TextLayoutAttribute attr, int32_t &val);
	int32_t GetLayoutAttribute(TextLayoutAttribute attr, float &val);
	int32_t GetLayoutAttribute(TextLayoutAttribute attr, math::v2 &val);
	int32_t GetLayoutAttribute(TextLayoutAttribute attr, bool &val);

	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len, int32_t val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len, float val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				  const math::v2 &val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				  const math::v4 &val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len, bool val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, int32_t &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, float &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, math::v2 &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, math::v4 &val);
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
	/* the graphics the glyphs are drawn into (name ours) */
	int32_t GetGraphics(TextGraphics *&graphics);

	int32_t Invalidate();
	int32_t Release();
	int32_t ReleaseAll();

	uint8_t m_data[0x8c];
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
