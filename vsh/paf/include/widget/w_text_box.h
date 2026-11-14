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

class TextBoxEditListener;
class ScrollBar;

namespace wutil {
/* the edit listener the text boxes made now have (a setting of wutil::Init) */
TextBoxEditListener *GetEditListener();
}

class TextBox : public Text {
public:
	/* how long the glow fades out as the box is tapped or its edit ends (a constant of the
	   file; name ours) */
	static const float c_glowFadeMsec;

	/* what the text box tells its listeners of its editing (ScePaf-RE's names where it has them,
	   with its values moved to this firmware's; the others ours); the edit listener's sends
	   it as the editing begins and ends, and about the IME (0x10000009 to 0x1000000B) */
	enum {
		CB_TEXT_BOX_ZOOM_IN = 0x10000004,
		CB_TEXT_BOX_ZOOM_OUT = 0x10000005,
		CB_TEXT_BOX_TAPPED = 0x10000006,
		CB_TEXT_BOX_EDIT_BEGIN = 0x10000007,
		CB_TEXT_BOX_EDIT_END = 0x10000008,
		CB_TEXT_BOX_IME_OPEN = 0x10000009,
		CB_TEXT_BOX_IME_CHANGE_SIZE = 0x1000000A,
		CB_TEXT_BOX_ENTER_PRESSED = 0x1000000B,
		CB_TEXT_BOX_STRING_CHANGED = 0x1000000C,
		CB_TEXT_BOX_STRING_REPLACED = 0x1000000D,
		CB_TEXT_BOX_CARET_UP = 0x1000000E,
		CB_TEXT_BOX_CARET_DOWN = 0x1000000F,
		CB_TEXT_BOX_CARET_LEFT = 0x10000010,
		CB_TEXT_BOX_CARET_RIGHT = 0x10000011,
		CB_TEXT_BOX_CARET_POS_CHANGED = 0x10000012
	};

	/* the draw object of the glow (names ours) */
	enum {
		OBJ_GLOW = 2
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

	static const char m_widget_type[];
	static const char *TypeName();
	/* the caret's width (name after the export) */
	static const float c_caretWidth;

	TextBox(Widget *parent, CreateParam *param);
	virtual ~TextBox();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();
	virtual uint32_t ChangeState(uint32_t state, int32_t ev_type);
	virtual void Update();
	virtual void UpdateSize();
	virtual int32_t OnDraw(graph::DrawInfo &info);
	using Text::SetString;
	using Text::GetString;
	using Text::SetTexture;
	virtual int32_t SetString(const wstring &str, int32_t obj);
	/* the glow object's texture, the glow made first (obj OBJ_GLOW) */
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	/* it takes the text events while edited, or where event 0x20004 or a tap has a listener */
	virtual bool IsTextEventEnabled()
	{
		return m_edit_mode ||
		       m_map_e_listener.find(0x20004) != m_map_e_listener.end() ||
		       m_map_e_listener.find(CB_TEXT_BOX_TAPPED) != m_map_e_listener.end();
	}
	virtual void UpdateTouchMode();
	/* where the focus moves: down only once tapping or editing takes it (name ours) */
	virtual int32_t GetFocusDirectionMask()
	{
		int32_t mask = Widget::GetFocusDirectionMask();

		if (mask == 0) {
			if (m_map_e_listener.find(CB_TEXT_BOX_TAPPED) != m_map_e_listener.end())
				return 4;
			mask = 5;
			if (m_edit_mode)
				return 4;
		}
		return mask;
	}
	virtual int32_t ReplaceString(uint32_t index, uint32_t len, const wstring &str);
	virtual int32_t MoveCaretUp();
	virtual int32_t MoveCaretDown();
	virtual int32_t MoveCaretLeft();
	virtual int32_t MoveCaretRight();
	virtual int32_t SetCaretPos(const math::v2 &pos);
	virtual int32_t SetCaretIndex(uint32_t index);
	virtual int32_t SetSelectionRange(uint32_t index, uint32_t len);
	virtual int32_t SetCaretVisible(bool visible);
	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual int32_t HandleTouchPress(Event *e);
	virtual int32_t HandleTouchRelease();
	/* a tap: the glow shown and the text box edited where it is no edit text yet */
	virtual int32_t ScePafWidget_695200BA(Event *e);
	virtual int32_t UpdateTouch(const math::v2 &pos);
	virtual int32_t SetVisiblePos(const math::v2 &pos);
	virtual int32_t AdjustSize();

	int32_t BeginEdit();
	int32_t EndEdit();
	bool IsEditing();
	int32_t SetPasswordMode(bool mode);
	int32_t GetPasswordMode(bool &mode);
	/* its string cut to the length where it changes */
	int32_t SetMaxLength(uint32_t len);
	int32_t GetMaxLength(uint32_t &len);
	int32_t SetEditMode(bool mode);
	int32_t GetEditMode(bool &mode);
	int32_t SetEditAutoTransitionMode(bool mode);
	int32_t GetEditAutoTransitionMode(bool &mode);
	int32_t SetEditListener(TextBoxEditListener *listener);
	int32_t GetEditListener(TextBoxEditListener *&listener);
	int32_t SetImeParam(const ImeParam &param);
	int32_t GetImeParam(ImeParam &param);
	int32_t AttachHScrollBar(ScrollBar *bar);
	int32_t DetachHScrollBar();
	int32_t AttachVScrollBar(ScrollBar *bar);
	int32_t DetachVScrollBar();
	int32_t SetCaretAlpha(float alpha);
	int32_t SetCaretBlink();
	int32_t KillCaretBlink();
	int32_t SetCaretScroll();
	int32_t KillCaretScroll();
	/* Text::ReplaceString, and the text box told its string changed (0x81135326, name ours) */
	int32_t ReplaceStringNotify(uint32_t index, uint32_t len, const wstring &str);
	/* replaces len characters from index with the string cut to the maximum length, the edit
	   listener and the string changed event told (0x811351be, name ours) */
	int32_t ReplaceStringCut(uint32_t index, uint32_t len, const wstring &str);

	static void OnVScrollBarChanged(int32_t type, Handler *self, Event *e, void *data);
	static void OnHScrollBarChanged(int32_t type, Handler *self, Event *e, void *data);
	static void OnCaretScrollPos(Timer *t, AnimListener *listener, void *data1, void *data2,
				     void *data3);
	static void OnCaretBlinkFadeIn(Timer *t, AnimListener *listener, void *data1, void *data2,
				       void *data3);
	static void OnCaretBlinkFadeOut(Timer *t, AnimListener *listener, void *data1, void *data2,
					void *data3);

	/* the glow plane (OBJ_GLOW), made by ScePafWidget_AFE7A212 (names ours) */
	graph::PlaneObj *m_glow;
	ScrollBar *m_hscroll_bar;
	ScrollBar *m_vscroll_bar;
	uint32_t m_max_length;
	ImeParam m_ime_param;
	TextBoxEditListener *m_edit_listener;
	bool m_edit_auto_transition;
	/* the layout's ellipsis was on and is off while the text box is edited */
	bool m_ellipsis_saved;
	/* makes the string shown from its own (name ours) */
	wstring (*m_text_filter)(const wstring &in);
	/* how far the text scrolls, and the caret scroll's target and start (names ours) */
	math::v2 m_scroll_max;
	math::v2 m_caret_scroll_to;
	math::v2 m_caret_scroll_from;
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

/* a text box's resource parameters (names after the resource's) */
class TextBoxParam : public TextParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	TextBoxParam();
	virtual const char *GetParamName() const;
	virtual ~TextBoxParam();
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	int32_t m_max_length;
	bool m_password_mode;
	bool m_paste_mode;
	bool m_edit_mode;
	bool m_edit_auto_transition_mode;
	TextBox::ImeParam m_ime;
	IDParam m_h_scrollbar;
	IDParam m_v_scrollbar;
};

/* a text box's style: the text style and the glow plane's parameters */
class TextBoxStyleParam : public TextStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	TextBoxStyleParam();
	virtual const char *GetParamName() const;
	virtual ~TextBoxStyleParam();
	virtual int32_t SetEach(Widget *widget);

	graph::PlaneObjParam m_glow_param;
};

/* a text box's sounds: its link tapped and its tap (class name ours) */
class TextBoxSoundParam : public TextSoundParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	TextBoxSoundParam();
	virtual const char *GetParamName() const;
	virtual ~TextBoxSoundParam();
};

}
}

extern "C" {
/* the glow plane made, added as OBJ_GLOW and sized to its scale 1 (0x80AF0020 where it has one;
   name ours) */
int32_t ScePafWidget_AFE7A212(paf::ui::TextBox *text_box);
/* the string set with the maximum length, the edit listener and the string changed event told
   (name ours) */
int32_t ScePafWidget_C4157A69(paf::ui::TextBox *text_box, const paf::wstring &str, int32_t obj);
/* the characters of the source copied to the destination but the spaces; the length given
   is the destination's left, then it is the characters copied where there was a space, and
   whether there was one returned (name ours) */
int32_t ScePafWidget_48D5B0D2(uint16_t *dst, uint32_t *dst_len, const uint16_t *src,
			      uint32_t src_len);
/* the text box's text filter */
int32_t ScePafWidget_D459D8B6(paf::ui::TextBox *text_box, paf::ui::TextBoxTextFilter *filter);
/* the text box's paste-enabled flag (bit 2 of its flags at 0x288) */
int32_t ScePafWidget_E0D414F7(paf::ui::TextBox *text_box, bool *enabled);
/* the scroll view a multi-line text box scrolls in, kept at 0x284 (0x81131ce2 TOOL; no name
   found): 0x80AF4122 without a text object, 0x80AF418F for a NULL view, else 0 */
int32_t ScePafWidget_DA5F91F8(paf::ui::TextBox *text_box, paf::ui::ScrollView *view);
}

#endif
