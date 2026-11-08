/*
 * paf::ui::TextBox, as far as converted C++ uses it, and the listener a
 * text box tells about its editing. Names after the exports and ScePaf-RE (restored from
 * Sony's DWARF); the virtual functions Sony's records do not name are ours.
 */
#ifndef PAF_WIDGET_W_TEXT_BOX_H
#define PAF_WIDGET_W_TEXT_BOX_H

#include <stdint.h>
#include <widget/core/widget.h>
#include <widget/w_text.h>
#include <graphics/text/text_layout.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/string>

/* an event of the system's IME (ime/ime.h) */
extern "C" {
struct SceImeVshEvent;
}

namespace paf {
namespace ui {

class TextBox : public Text {
public:
	/* what the text box's edit listener sends it as the editing begins and ends, and (0x10000009
	   to 0x1000000B) about the IME (names ours, but CB_TEXT_BOX_ENTER_PRESSED's, ScePaf-RE's) */
	enum {
		CB_TEXT_BOX_ZOOM_IN = 0x10000004,
		CB_TEXT_BOX_ZOOM_OUT = 0x10000005,
		CB_TEXT_BOX_IME_OPEN = 0x10000009,
		CB_TEXT_BOX_IME_CHANGE_SIZE = 0x1000000A,
		CB_TEXT_BOX_ENTER_PRESSED = 0x1000000B
	};

	/* how the IME edits the text box: its languages, type and options, as the SDK's SceImeParam
	   names them (names after ScePaf-RE and the SDK's) */
	class ImeParam {
	public:
		ImeParam()
			: supportedLanguages(0), languagesForced(false), type(0), option(0),
			  enterLabel(0), filter(NULL)
		{
		}

		virtual ~ImeParam()
		{
		}

		uint32_t supportedLanguages;
		bool languagesForced;
		int32_t type;
		uint32_t option;
		int32_t enterLabel;
		int32_t (*filter)(wchar_t *out, uint32_t *out_len, const wchar_t *src,
				  uint32_t src_len);
	};

	static const char *TypeName();

	int32_t BeginEdit();
	int32_t EndEdit();
	int32_t SetPasswordMode(bool mode);
	int32_t GetPasswordMode(bool &mode);
	/* its string cut to the length where it changes */
	int32_t SetMaxLength(uint32_t len);
	int32_t SetEditMode(bool mode);
	int32_t GetEditMode(bool &mode);
	int32_t SetImeParam(const ImeParam &param);
	int32_t GetEditAutoTransitionMode(bool &mode);
	int32_t GetImeParam(ImeParam &param);
	/* Text::ReplaceString, and the text box told its string changed (0x81135326, name ours) */
	int32_t ReplaceStringNotify(uint32_t index, uint32_t len, const wstring &str);
};

/* the text box's text filter, which makes the string it shows from its own (stored at +776; name
   ours) */
typedef wstring (*TextBoxTextFilter)(const wstring &in);

}
}

extern "C" {
/* sets the text box's text filter, 0 returned */
int32_t ScePafWidget_3EB376F2(paf::ui::TextBox *text_box, paf::ui::TextBoxTextFilter filter);
/* only returns the error 0x80AF5004 */
int32_t ScePafWidget_10E22E5A(paf::ui::TextBox *text_box, bool onoff);
}

namespace paf {
namespace ui {

/* what edits a text box: the object wutil::InitParam's edit_listener gives, by default the IME's
   (name after the exports; the virtual functions after ScePaf-RE's where it has them, the others
   ours) */
class TextBoxEditListener {
public:
	TextBoxEditListener();

	virtual ~TextBoxEditListener();

	virtual TextBox *GetTextBox() = 0;
	virtual int32_t SetPreeditBackColor(const math::v4 &color) = 0;
	virtual int32_t GetPreeditBackColor(math::v4 &color) = 0;
	virtual int32_t SetPreeditUnderlineColor(const math::v4 &color) = 0;
	virtual int32_t GetPreeditUnderlineColor(math::v4 &color) = 0;
	virtual int32_t BeginEdit(TextBox *text_box) = 0;
	virtual int32_t EndEdit(TextBox *text_box) = 0;
	virtual int32_t Update(TextBox *text_box) = 0;
	virtual int32_t NotifyCaretMove(TextBox *text_box) = 0;
	/* the preedit text made part of the string */
	virtual int32_t Commit(TextBox *text_box) = 0;
	/* the text box's password mode changed */
	virtual int32_t NotifyPasswordMode(TextBox *text_box) = 0;
	virtual int32_t HandleImeEvent(const SceImeVshEvent *e) = 0;
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
/* the scroll view a multi-line text box scrolls in, kept at 0x284 (0x81131ce2 TOOL; no name
   found): 0x80AF4122 without a text object, 0x80AF418F for a NULL view, else 0 */
int32_t ScePafWidget_DA5F91F8(paf::ui::TextBox *text_box, paf::ui::ScrollView *view);
}

#endif
