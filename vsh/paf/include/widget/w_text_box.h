/*
 * paf::ui::Text and paf::ui::TextBox, as far as converted C++ uses them, and the listener a
 * text box tells about its editing. Names after the exports and ScePaf-RE (restored from
 * Sony's DWARF); the virtual functions Sony's records do not name are ours.
 */
#ifndef PAF_WIDGET_W_TEXT_BOX_H
#define PAF_WIDGET_W_TEXT_BOX_H

#include <stdint.h>
#include <widget/core/widget.h>
#include <graphics/text/text_layout.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/string>

namespace paf {
namespace ui {

class Text : public Widget {
public:
	enum {
		CB_TEXT_SELECTION_BEGIN = 0x10000000,
		CB_TEXT_SELECTION_END = 0x10000001,
		CB_TEXT_SELECTION_POS_CHANGED = 0x10000002,
		CB_TEXT_END = 0x10000003
	};

	/* the text's animations (names after ScePaf-RE; 0x1000D, the auto scroll's fade back,
	   ours) */
	enum {
		ANIM_AUTO_SCROLL = 0x1000C,
		ANIM_AUTO_SCROLL_FADE = 0x1000D,
		ANIM_CARET_SCROLL = 0x1000E,
		ANIM_CARET_BLINK = 0x1000F
	};

	/* how StartAutoScroll scrolls: over and over, or (3 and 4, names ours) once, as long
	   as a long press holds it */
	enum ScrollType {
		SCROLL_TYPE_HORIZONTAL = 1,
		SCROLL_TYPE_VERTICAL = 2,
		SCROLL_TYPE_HORIZONTAL_ONCE = 3,
		SCROLL_TYPE_VERTICAL_ONCE = 4
	};

	static const char *TypeName();

	/* sets the touch modes from its edit and selection state (0x8123aa94, name ours) */
	virtual void UpdateTouchMode();
	/* replaces len characters from index with the string (0x8113187e, name after
	   graph::TextObj's) */
	virtual int32_t ReplaceString(uint32_t index, uint32_t len, const wstring &str);
	virtual int32_t MoveCaretUp();
	virtual int32_t MoveCaretDown();
	virtual int32_t MoveCaretLeft();
	virtual int32_t MoveCaretRight();
	virtual int32_t SetCaretPos(const math::v2 &pos);
	virtual int32_t SetSelectionPos(const math::v2 &start, const math::v2 &end);
	virtual int32_t SetCaretIndex(uint32_t index);
	virtual int32_t SetSelectionRange(uint32_t index, uint32_t len);

	int32_t GetLength(uint32_t &len);
	int32_t GetCaretIndex(uint32_t &index);
	int32_t GetSelectionRange(uint32_t &index, uint32_t &len);
	int32_t GetSelectionMode(bool &mode);
	int32_t GetCharInfo(uint32_t index, graph::TextLayoutCharInfo &info);
	int32_t GetLineInfo(uint32_t line, graph::TextLayoutLineInfo &info);
	int32_t GetLayoutAttribute(graph::TextLayoutAttribute attr, math::v2 &val);
	/* its auto scroll running, or its animation 2 (0x81132452, name ours) */
	bool IsScrolling();
	/* sends the event with the rectangle, as integers (0x8113262e, name ours) */
	int32_t SendRectEvent(int32_t type, const math::v4 &rect);
};

class TextBox : public Text {
public:
	/* what the text box's edit listener sends it as the editing begins and ends (names ours) */
	enum {
		CB_TEXT_BOX_ZOOM_IN = 0x10000004,
		CB_TEXT_BOX_ZOOM_OUT = 0x10000005
	};

	static const char *TypeName();

	int32_t GetPasswordMode(bool &mode);
	int32_t GetEditMode(bool &mode);
};

/* what a text box tells the edit menu about its editing: the object wutil::Init makes and export
   1EE9D37E returns (name ours; the exported TextBoxEditListener, wutil::InitParam's edit_listener,
   is another interface) */
class TextBoxEditMenuListener {
public:
	TextBoxEditMenuListener();

	virtual ~TextBoxEditMenuListener();

	virtual int32_t BeginEdit(TextBox *text_box) = 0;
	virtual int32_t EndEdit(TextBox *text_box) = 0;
	/* with the caret's position in the layout */
	virtual int32_t Update(TextBox *text_box, const math::v2 &caret) = 0;
	virtual int32_t NotifyCaretMove(TextBox *text_box) = 0;
	/* the edit menu closed (name ours) */
	virtual int32_t CloseMenu(TextBox *text_box) = 0;
};

}
}

extern "C" {
/* the text box's paste-enabled flag (bit 2 of its flags at 0x288) */
int32_t ScePafWidget_E0D414F7(paf::ui::TextBox *text_box, bool *enabled);
/* the text's string buffer and its length (Text::GetStringBuffer, by graph::TextObj's) */
int32_t ScePafWidget_62604965(paf::ui::Text *text, const wchar_t **buf, uint32_t *len);
}

#endif
