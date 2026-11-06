/*
 * The text box edit listener wutil::Init makes when its InitParam gives none: it edits the text
 * box with the system's IME (ScePafLowlayer's IME client), shows the IME's preedit text in the
 * text box with its own colors, masks a password but for the last characters typed, and moves
 * the camera so that the caret stays above the IME. The class, method, member and constant names
 * are ours; the IME parameter's fields are named as the SDK's SceImeParam names them.
 */
#ifndef PAF_WIDGET_TEXT_BOX_IME_LISTENER_H
#define PAF_WIDGET_TEXT_BOX_IME_LISTENER_H

#include <stddef.h>
#include <stdint.h>
#include <libime/libime_api.h>
#include <ime/ime.h>
#include <widget/w_text_box.h>
#include <widget/core/event.h>
#include <math/ngp/v4.h>

namespace paf {
namespace ui {

/* 0x50 bytes */
class TextBoxImeListener : public TextBoxEditListener {
public:
	/* what it sends the scroll view the text box is in (Event types) */
	enum {
		EVENT_TEXT_CHANGED = 0x8000,
		EVENT_EDIT_BEGIN = 0x8001,
		EVENT_EDIT_END = 0x8002,
		EVENT_IME_OPEN = 0x8003,
		EVENT_IME_CHANGE_SIZE = 0x8004,
		EVENT_IME_EDIT = 0x8005
	};

	/* the timeout that masks the password's last characters (SetTimeoutEvent's ID) */
	enum {
		TIMER_MASK = 0xFEFF
	};

	/* 0x811ae5dc (TOOL) */
	TextBoxImeListener();
	/* 0x811ae654 */
	virtual ~TextBoxImeListener();

	/* 0x811ae6a6 */
	virtual TextBox *GetTextBox();
	/* 0x811ae6aa */
	virtual int32_t SetPreeditBackColor(const math::v4 &color);
	/* 0x811ae6ba */
	virtual int32_t GetPreeditBackColor(math::v4 &color);
	/* 0x811ae6ca */
	virtual int32_t SetPreeditUnderlineColor(const math::v4 &color);
	/* 0x811ae6da */
	virtual int32_t GetPreeditUnderlineColor(math::v4 &color);

	/* 0x811ae6ea: the event with the rectangle sent to the scroll view the text box is in */
	int32_t PostEvent(uint32_t type, const math::v4 &rect);

	/* 0x811ae97c */
	virtual int32_t BeginEdit(TextBox *text_box);
	/* 0x811aeb58: the password masked but for len characters from index */
	void Mask(uint32_t index, uint32_t len, uint32_t total);
	/* 0x811aec2c: the preedit text shown as the rest of the string */
	int32_t ClearPreedit();
	/* 0x811aece8: the camera back where it was before the editing */
	int32_t RestoreCamera();
	/* 0x811aedc2 */
	virtual int32_t EndEdit(TextBox *text_box);
	/* 0x811aee7a */
	virtual int32_t Update(TextBox *text_box);
	/* 0x811aee96 */
	virtual int32_t NotifyCaretMove(TextBox *text_box);
	/* 0x811aef18 */
	virtual int32_t Commit(TextBox *text_box);
	/* 0x811aef56 */
	virtual int32_t NotifyPasswordMode(TextBox *text_box);
	/* 0x811aef82: the camera moved so that the text box is above the IME's window */
	int32_t FollowCaret(const math::v4 &rect);
	/* 0x811af1d4 */
	virtual int32_t HandleImeEvent(const SceImeVshEvent *e);
	/* 0x811af81a: the string cut after the last line the text box shows */
	int32_t TrimToLayout();

	/* 0x811af8f8: the password masked again */
	static void OnMaskTimeout(int32_t type, Handler *self, Event *e, void *data);
	/* 0x811af910 */
	static void OnStateReady(int32_t type, Handler *self, Event *e, void *data);
	/* 0x811af91a */
	static void OnImeEvent(void *arg, const SceImeVshEvent *e);

	TextBox *m_text_box;
	/* the IME's work buffer, SCE_IME_WORK_BUFFER_SIZE bytes */
	wchar_t *m_work;
	void *m_ime;
	/* the preedit text in the string */
	uint32_t m_preedit_index;
	uint32_t m_preedit_len;
	math::v4 m_preedit_back_color;
	math::v4 m_preedit_underline_color;
	/* how far the camera moved, to move it back */
	math::v4 m_camera_offset;
	/* set while BeginEdit ends the text box's editing for another */
	bool m_switching;
	bool m_password;
	/* whether the focus was shown when the editing began */
	bool m_focus_shown;
};

}
}

#endif
