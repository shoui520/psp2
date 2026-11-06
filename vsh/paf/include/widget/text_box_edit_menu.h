/*
 * The text box edit menu the widget system makes for itself (wutil::Init): while a text box
 * is edited the camera zooms in on it and follows its caret, and a speech balloon menu over the
 * caret offers to cut, copy, paste, select and select all ("_common_template_speech_balloon_menu_*"
 * in the common resource). The template, texture and string IDs are Sony's (their strings); the
 * class, method and member names are ours.
 */
#ifndef PAF_WIDGET_TEXT_BOX_EDIT_MENU_H
#define PAF_WIDGET_TEXT_BOX_EDIT_MENU_H

#include <stdint.h>
#include <widget/w_text_box.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>

namespace paf {
namespace ui {

/* 0x38 bytes */
class TextBoxEditMenu : public TextBoxEditMenuListener {
public:
	/* the menu's items */
	enum {
		ITEM_COPY = 0x01,
		ITEM_CUT = 0x02,
		ITEM_PASTE = 0x04,
		ITEM_SELECT = 0x08,
		ITEM_SELECT_ALL = 0x10
	};

	/* 0x811aaf80 (TOOL) */
	TextBoxEditMenu();
	/* 0x811aafb8 */
	virtual ~TextBoxEditMenu();

	/* 0x811ab1ec: the camera zoomed in on the text box */
	virtual int32_t BeginEdit(TextBox *text_box);
	/* 0x811ab452: the menu closed and the camera back where it was */
	virtual int32_t EndEdit(TextBox *text_box);
	/* 0x811ab764: the camera after the caret at the position */
	virtual int32_t Update(TextBox *text_box, const math::v2 &caret);
	/* 0x811acf54: the camera back, and the menu for the selection */
	virtual int32_t NotifyCaretMove(TextBox *text_box);
	/* 0x811acf76 */
	virtual int32_t CloseMenu(TextBox *text_box);

	/* 0x811aaff4 */
	int32_t ZoomIn();
	/* 0x811ab212 */
	int32_t ZoomOut();
	/* 0x811ab47c: a position in the text box's layout as an offset in its scene */
	math::v4 GetLayoutOffset(const math::v2 &pos);
	/* 0x811ab53c: the caret's offset in the scene, and its line's height */
	math::v4 GetCaretOffset(float *height);
	/* 0x811ab5e2 */
	int32_t FollowCaret(const math::v2 &caret);
	/* 0x811ab780: the menu over the caret, or under it where there is no room */
	int32_t PlaceMenu();
	/* 0x811aba24: the menu the selection and the clipboard call for */
	int32_t OpenMenu();
	/* 0x811acfae */
	int32_t Copy();
	/* 0x811ad03c: copied, unless the text box hides its text, and deleted */
	int32_t Cut();
	/* 0x811ad158 */
	int32_t Paste();

	/* the menu hidden, and deleted when its transition ends */
	void DestroyMenu()
	{
		if (m_menu != NULL) {
			float time = ScePafWidget_F1176ACC(m_menu, common::transition::Type_Popup4,
								   0.0f, NULL, NULL);

			ScePafWidget_5A9793B0(m_menu, time);
			m_menu = NULL;
			m_menu_box = NULL;
		}
		m_menu_items = 0;
	}

	/* the menu's events: 0x811ad226 to 0x811ad2cc */
	static void OnTouchOutside(int32_t type, Handler *self, Event *e, void *data);
	static void OnMenuReady(int32_t type, Handler *self, Event *e, void *data);
	static void OnCopy(int32_t type, Handler *self, Event *e, void *data);
	static void OnCut(int32_t type, Handler *self, Event *e, void *data);
	static void OnPaste(int32_t type, Handler *self, Event *e, void *data);
	static void OnSelect(int32_t type, Handler *self, Event *e, void *data);
	static void OnSelectAll(int32_t type, Handler *self, Event *e, void *data);

	TextBox *m_text_box;
	/* "_common_template_speech_balloon_menu_base" and its "..._box" */
	Widget *m_menu;
	Widget *m_menu_box;
	/* the ITEM_ bits the menu was made with */
	int8_t m_menu_items;
	/* the camera's position and scale before the zoom */
	math::v4 m_camera_pos;
	math::v4 m_camera_scale;
};

}
}

#endif
