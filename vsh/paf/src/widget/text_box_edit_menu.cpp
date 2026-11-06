/*
 * ui::TextBoxEditMenu (text_box_edit_menu.h): the camera zoomed in on the text box being edited,
 * and the speech balloon menu that cuts, copies and pastes through the shell's text clipboard.
 * The file's name is ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <vectormath.h>
#include <widget/text_box_edit_menu.h>
#include <widget/w_button_base.h>
#include <widget/w_camera.h>
#include <widget/w_scene.h>
#include <widget/core/environment.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/text/text_layout.h>
#include <common/resource_hooks.h>
#include <common/transition.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <std/stdio.h>
#include <std/string>
#include "../../../lowlayer/src/shell_svc/text_clipboard.h"
#include "../paf_types.h"

namespace paf {
namespace ui {

using sce::Vectormath::Simd::Aos::Vector4;
using sce::Vectormath::Simd::Aos::divPerElem;
using sce::Vectormath::Simd::Aos::mulPerElem;

/* the vectors below, made in the static initializer with the constructor inlined (name ours) */
static inline __attribute__((always_inline)) math::v4 Vector(float x, float y, float z, float w)
{
	return math::v4(x, y, z, w);
}

/* the zoom on the text box: the camera's scale, and the box's hit box made as much larger */
static const math::v4 s_zoom = Vector(1.5f, 1.5f, 1.0f, 1.0f);
/* where the camera looks from, under the text box, and the hit box's center moved with it */
static const math::v4 s_zoom_offset = Vector(0.0f, -50.0f, 0.0f, 0.0f);
/* the menu's room around its box */
static const math::v4 s_menu_margin = Vector(20.0f, 40.0f, 0.0f, 0.0f);

/* the clipboard holds at most this many characters */
enum { CLIPBOARD_LENGTH = 8189 };

TextBoxEditMenu::TextBoxEditMenu()
	: m_text_box(NULL), m_menu(NULL), m_menu_box(NULL), m_menu_items(0),
	  m_camera_pos(0.0f, 0.0f, 0.0f, 0.0f), m_camera_scale(0.0f, 0.0f, 0.0f, 0.0f)
{
}

TextBoxEditMenu::~TextBoxEditMenu()
{
}

int32_t TextBoxEditMenu::ZoomIn()
{
	Camera *camera = m_text_box->GetCamera();

	if (camera != NULL) {
		if (m_camera_pos == math::v4(0.0f, 0.0f, 0.0f, 0.0f)) {
			m_camera_pos = camera->GetPos(Widget::GET_ANIMTARGET, NULL,
						      math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			m_camera_scale = *camera->GetScale(Widget::GET_ANIMTARGET);
		}
		camera->SetScale(math::v4(divPerElem(Vector4(1.0f), s_zoom.getImpl())),
				 new Timer(200.0f, Timer::FUNC_EXPONENT), Widget::ANIM_SCALE, 2);
		m_text_box->SetHitBox(2, s_zoom, s_zoom_offset);
		ScePafWidget_42C5A4D4(m_text_box, 0, 0);
		m_text_box->SendRectEvent(TextBox::CB_TEXT_BOX_ZOOM_IN,
					  math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	}
	return 0;
}

int32_t TextBoxEditMenu::BeginEdit(TextBox *text_box)
{
	if (text_box == NULL)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	if (m_text_box != NULL)
		EndEdit(m_text_box);
	m_text_box = text_box;
	ZoomIn();
	return 0;
}

int32_t TextBoxEditMenu::ZoomOut()
{
	if (!(m_camera_pos == math::v4(0.0f, 0.0f, 0.0f, 0.0f))) {
		Camera *camera = m_text_box->GetCamera();

		if (camera != NULL) {
			camera->SetPos(m_camera_pos, new Timer(200.0f, Timer::FUNC_EXPONENT),
				       Widget::ANIM_MOVE, 2);
			camera->SetScale(m_camera_scale, new Timer(200.0f, Timer::FUNC_EXPONENT),
					 Widget::ANIM_SCALE, 2);

			m_text_box->SetHitBox(0, math::v4(1.0f, 1.0f, 1.0f, 0.0f),
					      math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			TextBox *text_box = m_text_box;

			if (text_box->m_env != NULL) {
				text_box->m_modal_mode = 2;
				text_box->m_env->m_modal_widgets.Remove(text_box);
			}
		}
		m_camera_pos = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		m_camera_scale = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		m_text_box->SendRectEvent(TextBox::CB_TEXT_BOX_ZOOM_OUT,
					  math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	}
	return 0;
}

int32_t TextBoxEditMenu::EndEdit(TextBox *text_box)
{
	int32_t ret = SCE_PAF_ERROR_INVALID_ARGUMENT;

	if (text_box != NULL && text_box == m_text_box) {
		CloseMenu(m_text_box);
		ZoomOut();
		m_text_box = NULL;
		ret = 0;
	}
	return ret;
}

math::v4 TextBoxEditMenu::GetLayoutOffset(const math::v2 &pos)
{
	TextBox *text_box = m_text_box;
	math::v4 world = text_box->GetWorldMatrix().w();
	math::v2 offset(world.extract_x(), world.extract_y());
	graph::DrawObj *obj = m_text_box->GetDrawObj(0);

	if (obj != NULL) {
		math::v2 obj_pos(obj->m_pos.extract_x(), obj->m_pos.extract_y());
		math::v2 size(obj->m_size.extract_x(), obj->m_size.extract_y());
		math::v2 visible;

		m_text_box->GetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, visible);
		offset = offset + ((pos - visible - size * 0.5f) * math::v2(1.0f, -1.0f) + obj_pos);
	}
	return math::v4(offset.extract_x(), offset.extract_y(), 0.0f, 0.0f);
}

math::v4 TextBoxEditMenu::GetCaretOffset(float *height)
{
	uint32_t index;

	m_text_box->GetCaretIndex(index);

	graph::TextLayoutCharInfo char_info;

	m_text_box->GetCharInfo(index, char_info);

	graph::TextLayoutLineInfo line_info;

	m_text_box->GetLineInfo(char_info.line, line_info);

	math::v4 offset = GetLayoutOffset(math::v2(char_info.x, line_info.frame.extract_y()));

	*height = line_info.frame.extract_w();
	return offset;
}

int32_t TextBoxEditMenu::FollowCaret(const math::v2 &caret)
{
	Scene *scene = m_text_box->m_scene;
	Camera *camera = m_text_box->GetCamera();

	if (scene != NULL && camera != NULL) {
		math::v4 scene_size = *scene->GetSize(Widget::GET_ANIMTARGET);
		float height;
		math::v4 caret_offset = GetCaretOffset(&height);
		Vector4 ones(1.0f);
		Vector4 unzoom = divPerElem(ones, s_zoom.getImpl());
		math::v4 offset = GetLayoutOffset(caret);
		math::v4 pos((mulPerElem(mulPerElem(offset.getImpl(), s_zoom.getImpl() - ones) +
						 m_camera_pos.getImpl(),
					 unzoom)) +
			     s_zoom_offset.getImpl());
		float top = caret_offset.extract_y() -
			    (scene_size.extract_y() * 0.5f - 32.0f) * unzoom.getY();

		if (pos.extract_y() < top)
			pos.set_y(top);
		camera->SetPos(pos, new Timer(200.0f, Timer::FUNC_EXPONENT), Widget::ANIM_MOVE, 2);
	}
	return 0;
}

int32_t TextBoxEditMenu::Update(TextBox *text_box, const math::v2 &caret)
{
	if (text_box == NULL || text_box != m_text_box)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	FollowCaret(caret);
	return 0;
}

int32_t TextBoxEditMenu::PlaceMenu()
{
	if (m_menu_box->m_state == 0) {
		Scene *scene = m_text_box->m_scene;

		if (scene != NULL) {
			intrusive_ptr<graph::Surface> texture =
				ScePafCommon_F820A54B(IDParam("_common_texture_copy_paste_menu_base_down"));
			math::v4 menu_size = *m_menu_box->GetSize(Widget::GET_ANIMTARGET) + s_menu_margin;
			math::v4 scene_size = *scene->GetSize(Widget::GET_ANIMTARGET);
			float height;
			math::v4 caret = GetCaretOffset(&height);
			math::v4 room = (scene_size - menu_size) * 0.5f - math::v4(0.0f, 32.0f, 0.0f, 0.0f);
			math::v4 pos = caret + math::v4(0.0f, menu_size.extract_y() * 0.5f, 0.0f, 0.0f);

			if (pos.extract_x() < -room.extract_x())
				pos.set_x(-room.extract_x());
			else if (pos.extract_x() > room.extract_x())
				pos.set_x(room.extract_x());
			/* Sony's tests the height against the horizontal room */
			if (pos.extract_y() > room.extract_x()) {
				texture = ScePafCommon_F820A54B(
					IDParam("_common_texture_copy_paste_menu_base_up"));
				pos.set_y(pos.extract_y() - menu_size.extract_y() - height);
			}
			m_menu->SetTexture(texture);
			m_menu->SetPos(pos);
			ScePafWidget_44B22ACA(m_menu, common::transition::Type_Popup4, 0.0f, NULL, NULL);
		}
	}
	return 0;
}

/* the button's label: the common resource's string */
static inline void SetLabel(Widget *button, const char *id)
{
	button->SetString(wstring(common::GetCommonString(IDParam(id))));
}

/* what the button calls when it is pressed */
static inline void SetAction(Widget *button, HandlerCB func, TextBoxEditMenu *menu)
{
	button->AddEventListener(ButtonBase::CB_BTN_DECIDE, new EventCBListener(func, menu), true);
}

int32_t TextBoxEditMenu::OpenMenu()
{
	uint32_t len;
	uint32_t sel_index;
	uint32_t sel_len;
	bool selection_mode;
	int32_t ret = 0;

	m_text_box->GetLength(len);
	m_text_box->GetSelectionRange(sel_index, sel_len);
	m_text_box->GetSelectionMode(selection_mode);

	bool password = false;
	bool paste = false;
	bool edit = false;
	int8_t items = 0;
	int32_t count = 0;

	if (m_text_box != NULL && m_text_box->IsInherit(TextBox::TypeName())) {
		TextBox *text_box = m_text_box;

		if (text_box != NULL) {
			text_box->GetPasswordMode(password);
			ScePafWidget_E0D414F7(text_box, &paste);
			text_box->GetEditMode(edit);
		}
	}
	if (sel_len != 0) {
		if (edit) {
			items = ITEM_CUT;
			count = 1;
		}
		if (selection_mode && !password) {
			items |= ITEM_COPY;
			count++;
		}
		if (len > sel_len) {
			items |= ITEM_SELECT_ALL;
			count++;
		}
	} else {
		if (paste && SceShellUtil_D0DDEDBC() != 0) {
			items = ITEM_PASTE;
			count = 1;
		}
		if (selection_mode && len != 0) {
			items |= ITEM_SELECT | ITEM_SELECT_ALL;
			count += 2;
		}
	}
	if (items != m_menu_items) {
		if (m_menu != NULL) {
			float time = ScePafWidget_F1176ACC(m_menu, common::transition::Type_Popup4,
								   0.0f, NULL, NULL);

			ScePafWidget_5A9793B0(m_menu, time);
			m_menu = NULL;
			m_menu_box = NULL;
		}
		if (items != 0) {
			Scene *scene = m_text_box->m_scene;

			if (scene != NULL) {
				char name[64];

				m_menu_items = 0;

				Widget *base = scene->FindChild(
					IDParam("_common_template_speech_balloon_menu_base"), 0);

				if (base != NULL)
					base->DestroyWidget();
				sce_paf_snprintf(name, sizeof(name), "_common_template_speech_balloon_menu_%d",
						 count);
				ret = ScePafCommon_CAAB7E61(scene, IDParam(name));
				if (ret == 0) {
					Widget *buttons[3];

					m_menu = scene->FindChild(
						IDParam("_common_template_speech_balloon_menu_base"), 0);
					m_menu->SetAlign(0, 0, 0, NULL);
					m_menu->SetAnchor(0, 0, 0, NULL);
					ScePafWidget_42C5A4D4(m_menu, 1, 0);
					m_menu->AddEventListener(Handler::CB_MODAL_TOUCH_OUTSIDE,
								 new EventCBListener(OnTouchOutside, this), true);

					Widget *menu = m_menu;

					menu->m_transition_complete = false;
					menu->m_transition = 0xff;
					menu->DoTransitionReverse_core(0.0f, true);
					ScePafWidget_4588699E(menu, NULL, 2, 0.0f, NULL);
					m_menu_box = m_menu->FindChild(
						IDParam("_common_template_speech_balloon_menu_box"), 0);
					m_menu_box->AddEventListener(Handler::CB_STATE_READY,
								     new EventCBListener(OnMenuReady, this),
								     true);
					for (int32_t i = 0; i < 3; i++) {
						sce_paf_snprintf(name, sizeof(name),
								 "_common_template_speech_balloon_menu_button_%d",
								 i + 1);
						buttons[i] = m_menu_box->FindChild(IDParam(name), 0);
					}
					switch (items) {
					case ITEM_COPY:
						SetLabel(buttons[0], "msg_copy_vb");
						SetAction(buttons[0], OnCopy, this);
						break;
					case ITEM_CUT:
						SetLabel(buttons[0], "msg_cut_vb");
						SetAction(buttons[0], OnCut, this);
						break;
					case ITEM_CUT | ITEM_COPY:
						SetLabel(buttons[0], "msg_cut_vb");
						SetLabel(buttons[1], "msg_copy_vb");
						SetAction(buttons[0], OnCut, this);
						SetAction(buttons[1], OnCopy, this);
						break;
					case ITEM_PASTE:
						SetLabel(buttons[0], "msg_paste_vb");
						SetAction(buttons[0], OnPaste, this);
						break;
					case ITEM_COPY | ITEM_SELECT_ALL:
						SetLabel(buttons[0], "msg_copy_vb");
						SetLabel(buttons[1], "msg_select_all");
						SetAction(buttons[0], OnCopy, this);
						SetAction(buttons[1], OnSelectAll, this);
						break;
					case ITEM_CUT | ITEM_SELECT_ALL:
						SetLabel(buttons[0], "msg_cut_vb");
						SetLabel(buttons[1], "msg_select_all");
						SetAction(buttons[0], OnCut, this);
						SetAction(buttons[1], OnSelectAll, this);
						break;
					case ITEM_CUT | ITEM_COPY | ITEM_SELECT_ALL:
						SetLabel(buttons[0], "msg_cut_vb");
						SetLabel(buttons[1], "msg_copy_vb");
						SetLabel(buttons[2], "msg_select_all");
						SetAction(buttons[0], OnCut, this);
						SetAction(buttons[1], OnCopy, this);
						SetAction(buttons[2], OnSelectAll, this);
						break;
					case ITEM_SELECT | ITEM_SELECT_ALL:
						SetLabel(buttons[0], "msg_select_vb");
						SetLabel(buttons[1], "msg_select_all");
						SetAction(buttons[0], OnSelect, this);
						SetAction(buttons[1], OnSelectAll, this);
						break;
					case ITEM_PASTE | ITEM_SELECT | ITEM_SELECT_ALL:
						SetLabel(buttons[0], "msg_paste_vb");
						SetLabel(buttons[1], "msg_select_vb");
						SetLabel(buttons[2], "msg_select_all");
						SetAction(buttons[0], OnPaste, this);
						SetAction(buttons[1], OnSelect, this);
						SetAction(buttons[2], OnSelectAll, this);
						break;
					default:
						break;
					}
				}
			}
		}
		m_menu_items = items;
	}
	if (m_menu != NULL)
		PlaceMenu();
	return ret;
}

int32_t TextBoxEditMenu::NotifyCaretMove(TextBox *text_box)
{
	if (text_box == NULL || text_box != m_text_box)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	ZoomOut();
	OpenMenu();
	return 0;
}

int32_t TextBoxEditMenu::CloseMenu(TextBox *text_box)
{
	if (text_box == NULL || text_box != m_text_box)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	DestroyMenu();
	return 0;
}

int32_t TextBoxEditMenu::Copy()
{
	int32_t ret = 0;
	uint32_t index;
	uint32_t len;

	m_text_box->GetSelectionRange(index, len);
	if (len != 0) {
		const wchar_t *buf;
		uint32_t buf_len;

		m_text_box->SetSelectionRange(0, 0);
		ScePafWidget_62604965(m_text_box, &buf, &buf_len);
		if (len > CLIPBOARD_LENGTH)
			len = CLIPBOARD_LENGTH;
		ret = SceShellUtil_C4810C56((const SceWChar16 *)(buf + index), len);
	}
	DestroyMenu();
	return ret;
}

int32_t TextBoxEditMenu::Cut()
{
	int32_t ret = 0;
	uint32_t index;
	uint32_t len;

	m_text_box->GetSelectionRange(index, len);
	if (len != 0) {
		m_text_box->SetSelectionRange(0, 0);

		bool password = false;

		if (m_text_box != NULL && m_text_box->IsInherit(TextBox::TypeName())) {
			TextBox *text_box = m_text_box;

			if (text_box != NULL)
				text_box->GetPasswordMode(password);
		}
		if (!password) {
			const wchar_t *buf;
			uint32_t buf_len;

			ScePafWidget_62604965(m_text_box, &buf, &buf_len);
			if (len > CLIPBOARD_LENGTH)
				len = CLIPBOARD_LENGTH;
			ret = SceShellUtil_C4810C56((const SceWChar16 *)(buf + index), len);
		}
		m_text_box->ReplaceString(index, len, wstring((const wchar_t *)NULL, 0));
	}
	DestroyMenu();
	return ret;
}

int32_t TextBoxEditMenu::Paste()
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;
	wchar_t *buf = new wchar_t[CLIPBOARD_LENGTH + 1];

	if (buf != NULL) {
		ret = SceShellUtil_1B186905((SceWChar16 *)buf, CLIPBOARD_LENGTH, NULL);
		if (ret == 0) {
			uint32_t index;

			m_text_box->GetCaretIndex(index);
			m_text_box->ReplaceString(index, 0, wstring(buf));
		}
		/* freed with operator delete, not delete[], as Sony's does */
		delete buf;
	}
	DestroyMenu();
	return ret;
}

void TextBoxEditMenu::OnTouchOutside(int32_t type, Handler *self, Event *e, void *data)
{
	static_cast<TextBoxEditMenu *>(data)->DestroyMenu();
}

void TextBoxEditMenu::OnMenuReady(int32_t type, Handler *self, Event *e, void *data)
{
	static_cast<TextBoxEditMenu *>(data)->PlaceMenu();
}

void TextBoxEditMenu::OnCopy(int32_t type, Handler *self, Event *e, void *data)
{
	static_cast<TextBoxEditMenu *>(data)->Copy();
}

void TextBoxEditMenu::OnCut(int32_t type, Handler *self, Event *e, void *data)
{
	static_cast<TextBoxEditMenu *>(data)->Cut();
}

void TextBoxEditMenu::OnPaste(int32_t type, Handler *self, Event *e, void *data)
{
	static_cast<TextBoxEditMenu *>(data)->Paste();
}

void TextBoxEditMenu::OnSelect(int32_t type, Handler *self, Event *e, void *data)
{
	TextBoxEditMenu *menu = static_cast<TextBoxEditMenu *>(data);
	uint32_t len;
	uint32_t index;

	menu->m_text_box->GetLength(len);
	menu->m_text_box->GetCaretIndex(index);
	if (len != 0 && index >= len)
		index = len - 1;
	menu->m_text_box->SetSelectionRange(index, 1);
	menu->OpenMenu();
}

void TextBoxEditMenu::OnSelectAll(int32_t type, Handler *self, Event *e, void *data)
{
	TextBoxEditMenu *menu = static_cast<TextBoxEditMenu *>(data);
	uint32_t len;

	menu->m_text_box->GetLength(len);
	menu->m_text_box->SetSelectionRange(0, len);
	menu->OpenMenu();
}

}
}
