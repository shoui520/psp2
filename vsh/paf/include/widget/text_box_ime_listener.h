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
#include <widget/w_text_box.h>
#include <widget/core/event.h>
#include <math/ngp/v4.h>

extern "C" {

/* the IME's events besides the SDK's SCE_IME_EVENT_*: the edits from 0x100 and the caret moves
   from 0x200 are the system IME's own */
enum {
	/* the preedit text replaced */
	SCE_IME_VSH_EVENT_UPDATE_PREEDIT = 0x100,
	/* the preedit text made part of the string */
	SCE_IME_VSH_EVENT_COMMIT = 0x101,
	/* text inserted at the caret */
	SCE_IME_VSH_EVENT_INSERT = 0x102,
	/* the character before the caret deleted */
	SCE_IME_VSH_EVENT_BACKSPACE = 0x103,
	/* everything before the caret deleted */
	SCE_IME_VSH_EVENT_DELETE_TO_START = 0x104,
	/* the string emptied */
	SCE_IME_VSH_EVENT_CLEAR = 0x105,
	/* the character at the caret deleted */
	SCE_IME_VSH_EVENT_DELETE = 0x106,
	SCE_IME_VSH_EVENT_CARET_LEFT = 0x200,
	SCE_IME_VSH_EVENT_CARET_RIGHT = 0x201,
	SCE_IME_VSH_EVENT_CARET_UP = 0x202,
	SCE_IME_VSH_EVENT_CARET_DOWN = 0x203
};

/* the IME's window, in screen pixels */
struct SceImeVshRect {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
};

struct SceImeVshEvent {
	uint32_t id;
	union {
		/* SCE_IME_EVENT_OPEN and SCE_IME_EVENT_CHANGE_SIZE */
		SceImeVshRect rect;
		/* the edits: the text, and where the caret goes in the preedit text */
		struct {
			const wchar_t *str;
			uint32_t caretIndex;
		} text;
		/* SCE_IME_EVENT_PRESS_CLOSE: the focus is shown again */
		bool showFocus;
	} param;
};

typedef void (*SceImeVshEventHandler)(void *arg, const SceImeVshEvent *e);

/* 0x40 bytes; a caller of 0x30 bytes (no sdkVersion) is taken as one of SDK 1.03 */
struct SceImeVshParam {
	uint32_t size;
	uint32_t inputMethod;
	uint64_t supportedLanguages;
	uint32_t languagesForced;
	uint32_t type;
	uint32_t option;
	void *arg;
	SceImeVshEventHandler handler;
	int32_t (*filter)(wchar_t *out, uint32_t *out_len, const wchar_t *src, uint32_t src_len);
	wchar_t *work;
	uint8_t enterLabel;
	/* whether a focus is shown (ScePafWidget_529FB877) */
	uint8_t focusShown;
	uint8_t reserved0[2];
	uint32_t sdkVersion;
	uint8_t reserved[12];
};

/* the IME client libpaf exports (ScePafLowlayer); no names found for their NIDs */
/* opens the IME with the parameter */
int32_t ScePafLowlayer_DBDC964A(void **ime, const SceImeVshParam *param);
/* gives the open IME a new parameter */
int32_t ScePafLowlayer_58939BC4(void *ime, const SceImeVshParam *param);
/* closes the IME */
int32_t ScePafLowlayer_0ABE4841(void *ime);
int32_t ScePafLowlayer_7294ADDF(void *ime);
/* the caret at the index in the preedit text */
int32_t ScePafLowlayer_5CE62E49(void *ime, uint32_t index);
/* the caret outside the preedit text */
int32_t ScePafLowlayer_7593DFF0(void *ime);
/* the text and the caret's index in it */
int32_t ScePafLowlayer_2131CEE6(void *ime, const wchar_t *text, uint32_t caret);
/* makes the preedit text part of the string */
int32_t ScePafLowlayer_B7FA6F32(void *ime);

}

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
