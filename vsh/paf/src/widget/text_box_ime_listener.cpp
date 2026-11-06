/*
 * ui::TextBoxImeListener (text_box_ime_listener.h): a text box edited with the system's IME. The
 * IME's preedit text sits in the string with the listener's colors and underlined until it is
 * committed; a password shows the characters just typed for two seconds. The file's name is
 * ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <libime/libime_api.h>
#include <widget/text_box_ime_listener.h>
#include <widget/w_scene.h>
#include <widget/w_camera.h>
#include <widget/w_scroll_view_base.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>
#include <graphics/text/text_layout.h>
#include <graphics/text/text_style.h>
#include <timer/timer.h>
#include <std/string>
#include <std/string.h>
#include "../paf_types.h"

namespace paf {
namespace ui {

/* the vectors below, made in the static initializer with the constructor inlined (name ours) */
static inline __attribute__((always_inline)) math::v4 Vector(float x, float y, float z, float w)
{
	return math::v4(x, y, z, w);
}

/* the preedit colors cleared when the preedit text becomes part of the string (names ours) */
static math::v4 s_no_back_color = Vector(0.0f, 0.0f, 0.0f, 0.0f);
static math::v4 s_no_underline_color = Vector(0.0f, 0.0f, 0.0f, 0.0f);

TextBoxImeListener::TextBoxImeListener()
	: m_text_box(NULL), m_work(NULL), m_ime(NULL), m_preedit_index(0), m_preedit_len(0),
	  m_preedit_back_color(0.6f, 0.6f, 0.6f, 0.3f),
	  m_preedit_underline_color(0.549f, 0.969f, 0.0f, 1.0f),
	  m_camera_offset(0.0f, 0.0f, 0.0f, 0.0f), m_switching(false), m_password(false),
	  m_focus_shown(false)
{
}

TextBoxImeListener::~TextBoxImeListener()
{
	if (m_work != NULL) {
		delete[] m_work;
		m_work = NULL;
	}
}

TextBox *TextBoxImeListener::GetTextBox()
{
	return m_text_box;
}

int32_t TextBoxImeListener::SetPreeditBackColor(const math::v4 &color)
{
	m_preedit_back_color = color;
	return 0;
}

int32_t TextBoxImeListener::GetPreeditBackColor(math::v4 &color)
{
	color = m_preedit_back_color;
	return 0;
}

int32_t TextBoxImeListener::SetPreeditUnderlineColor(const math::v4 &color)
{
	m_preedit_underline_color = color;
	return 0;
}

int32_t TextBoxImeListener::GetPreeditUnderlineColor(math::v4 &color)
{
	color = m_preedit_underline_color;
	return 0;
}

int32_t TextBoxImeListener::PostEvent(uint32_t type, const math::v4 &rect)
{
	Event e(type, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);
	TextBox *text_box;
	Widget *w;
	Environment *env;

	e.m_data[0] = (int32_t)rect.extract_x();
	e.m_data[1] = (int32_t)rect.extract_y();
	e.m_data[2] = (int32_t)rect.extract_z();
	e.m_data[3] = (int32_t)rect.extract_w();
	text_box = m_text_box;
	w = text_box->m_parent;
	e.SetDetail((uint32_t)text_box);
	env = text_box->m_env;
	for (; w != NULL; w = w->m_parent) {
		if (w->IsInherit(ScrollViewBase::TypeName()))
			break;
		if (w == w->m_scene)
			return 0;
	}
	e.SetTarget(w);
	e.Term();
	env->m_ev_queue.PushBack(e);
	return 0;
}

/* the text box's enter label as the IME's (name ours) */
int32_t ConvertEnterLabel(int32_t label)
{
	int32_t ret = SCE_IME_ENTER_LABEL_DEFAULT;

	switch (label) {
	case 0:
		ret = SCE_IME_ENTER_LABEL_DEFAULT;
		break;
	case 1:
		ret = SCE_IME_ENTER_LABEL_SEND;
		break;
	case 2:
		ret = SCE_IME_ENTER_LABEL_SEARCH;
		break;
	case 3:
		ret = SCE_IME_ENTER_LABEL_GO;
		break;
	}
	return ret;
}

/* unreferenced, empty (what it was for is not known; name ours) */
void ImeListenerUnused()
{
}

/* the text box's input type as the IME's: from 0x100 the system IME's own (name ours) */
int32_t ConvertType(int32_t type)
{
	int32_t ret = -1;

	switch (type) {
	case 0:
		ret = SCE_IME_TYPE_DEFAULT;
		break;
	case 1:
		ret = SCE_IME_TYPE_BASIC_LATIN;
		break;
	case 2:
		ret = SCE_IME_TYPE_NUMBER;
		break;
	case 3:
		ret = 0x100;
		break;
	case 4:
		ret = SCE_IME_TYPE_MAIL;
		break;
	case 5:
		ret = SCE_IME_TYPE_URL;
		break;
	case 6:
		ret = 0x103;
		break;
	case 7:
		ret = 0x104;
		break;
	case 8:
		ret = SCE_IME_TYPE_EXTENDED_NUMBER;
		break;
	case 9:
		ret = 0x105;
		break;
	}
	return ret;
}

/* the text box's languages as the IME's (name ours) */
uint64_t ConvertLanguages(uint32_t languages)
{
	uint64_t ret = 0;

	if (languages & 0x1)
		ret |= SCE_IME_LANGUAGE_DANISH;
	if (languages & 0x2)
		ret |= SCE_IME_LANGUAGE_GERMAN;
	if (languages & 0x4)
		ret |= SCE_IME_LANGUAGE_ENGLISH_US;
	if (languages & 0x8)
		ret |= SCE_IME_LANGUAGE_SPANISH;
	if (languages & 0x10)
		ret |= SCE_IME_LANGUAGE_FRENCH;
	if (languages & 0x20)
		ret |= SCE_IME_LANGUAGE_ITALIAN;
	if (languages & 0x40)
		ret |= SCE_IME_LANGUAGE_DUTCH;
	if (languages & 0x80)
		ret |= SCE_IME_LANGUAGE_NORWEGIAN;
	if (languages & 0x100)
		ret |= SCE_IME_LANGUAGE_POLISH;
	if (languages & 0x200)
		ret |= SCE_IME_LANGUAGE_PORTUGUESE_PT;
	if (languages & 0x400)
		ret |= SCE_IME_LANGUAGE_RUSSIAN;
	if (languages & 0x800)
		ret |= SCE_IME_LANGUAGE_FINNISH;
	if (languages & 0x1000)
		ret |= SCE_IME_LANGUAGE_SWEDISH;
	if (languages & 0x40000)
		ret |= SCE_IME_LANGUAGE_TURKISH;
	if (languages & 0x2000)
		ret |= SCE_IME_LANGUAGE_JAPANESE;
	if (languages & 0x4000)
		ret |= SCE_IME_LANGUAGE_KOREAN;
	if (languages & 0x8000)
		ret |= SCE_IME_LANGUAGE_SIMPLIFIED_CHINESE;
	if (languages & 0x10000)
		ret |= SCE_IME_LANGUAGE_TRADITIONAL_CHINESE;
	if (languages & 0x20000)
		ret |= SCE_IME_LANGUAGE_PORTUGUESE_BR;
	if (languages & 0x20000000)
		ret |= SCE_IME_LANGUAGE_ENGLISH_GB;
	if (languages & 0x40000000)
		ret |= 0x10000000000ULL;
	if (languages & 0x80000000)
		ret |= 0x20000000000ULL;
	return ret;
}

int32_t TextBoxImeListener::BeginEdit(TextBox *text_box)
{
	int32_t ret = SCE_PAF_ERROR_INVALID_ARGUMENT;

	if (text_box != NULL) {
		Environment *env;

		if (m_work == NULL)
			m_work = (wchar_t *)new char[SCE_IME_WORK_BUFFER_SIZE];
		env = text_box->m_env;
		if (m_work != NULL) {
			TextBox::ImeParam ime_param;
			SceImeVshParam param;
			uint32_t option;

			text_box->GetImeParam(ime_param);
			sce_paf_memset(&param, 0, sizeof(param));
			param.size = sizeof(param);
			param.sdkVersion = SCE_PSP2_SDK_VERSION;
			param.supportedLanguages = ConvertLanguages(ime_param.supportedLanguages);
			param.languagesForced = ime_param.languagesForced;
			param.type = ConvertType(ime_param.type);
			option = 0;
			if (ime_param.option & 0x1)
				option |= SCE_IME_OPTION_MULTILINE;
			if (ime_param.option & 0x2)
				option |= SCE_IME_OPTION_NO_AUTO_CAPITALIZATION;
			if (ime_param.option & 0x4)
				option |= SCE_IME_OPTION_NO_ASSISTANCE;
			if (ime_param.option & 0x8)
				option |= 0x8;
			param.option = option;
			param.enterLabel = ConvertEnterLabel(ime_param.enterLabel);
			param.filter = ime_param.filter;
			param.arg = this;
			param.handler = OnImeEvent;
			param.work = m_work;
			if (env != NULL)
				param.focusShown = ScePafWidget_529FB877(env, -1);

			if (m_text_box == NULL) {
				ret = sceImeVshOpen(&m_ime, &param);
				if (ret < 0 && m_work != NULL) {
					delete[] m_work;
					m_work = NULL;
				}
			} else {
				ret = sceImeVshSetParam(m_ime, &param);
				if (ret != 0)
					return ret;
				m_switching = true;
				m_text_box->EndEdit();
				m_switching = false;
			}
			if (ret == 0) {
				m_text_box = text_box;
				text_box->AddEventCallback(Handler::CB_STATE_READY, OnStateReady, this);
				m_text_box->GetPasswordMode(m_password);
				if (env != NULL) {
					m_focus_shown = ScePafWidget_A6B659BB(env, 0);
					if (m_focus_shown)
						ScePafWidget_B4A4660B(env, 0, -1, 1);
				}
				PostEvent(EVENT_EDIT_BEGIN, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			}
		} else {
			ret = SCE_PAF_ERROR_NO_OBJECT;
		}
	}
	return ret;
}

void TextBoxImeListener::Mask(uint32_t index, uint32_t len, uint32_t total)
{
	m_text_box->KillTimeoutEvent(TIMER_MASK);
	if (!m_password)
		return;
	if (len == 0) {
		m_text_box->SetStyleAttribute(graph::TextStyleAttribute_Mask, 0, 0, true);
		return;
	}
	if (index != 0)
		m_text_box->SetStyleAttribute(graph::TextStyleAttribute_Mask, 0, index, true);
	m_text_box->SetStyleAttribute(graph::TextStyleAttribute_Mask, index, len, false);
	if (index + len < total)
		m_text_box->SetStyleAttribute(graph::TextStyleAttribute_Mask, index + len,
					      total - (index + len), true);
	m_text_box->SetTimeoutEvent(TIMER_MASK, 2000.0f, OnMaskTimeout);
}

int32_t TextBoxImeListener::ClearPreedit()
{
	if (m_preedit_len != 0) {
		const wchar_t *buf;
		uint32_t buf_len;
		uint32_t index, len;

		ScePafWidget_62604965(m_text_box, &buf, &buf_len);
		m_text_box->SetStyleAttribute(graph::TextStyleAttribute_BackColor, m_preedit_index,
					      m_preedit_len, s_no_back_color);
		m_text_box->SetStyleAttribute(graph::TextStyleAttribute_UnderlineColor,
					      m_preedit_index, m_preedit_len, s_no_underline_color);
		m_text_box->SetStyleAttribute(graph::TextStyleAttribute_Underline, m_preedit_index,
					      m_preedit_len, false);
		index = m_preedit_index;
		len = m_preedit_len;
		m_preedit_len = 0;
		m_text_box->ReplaceString(index, len, wstring(buf + index, len));
	}
	return 0;
}

int32_t TextBoxImeListener::RestoreCamera()
{
	if (!(m_camera_offset == math::v4(0.0f, 0.0f, 0.0f, 0.0f))) {
		Camera *camera = m_text_box->GetCamera();

		if (camera != NULL)
			camera->SetPos(m_camera_offset, new Timer(200.0f, Timer::FUNC_EXPONENT),
				       Widget::ANIM_MOVE, 2);
		m_camera_offset = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	return 0;
}

int32_t TextBoxImeListener::EndEdit(TextBox *text_box)
{
	int32_t ret = SCE_PAF_ERROR_INVALID_ARGUMENT;

	if (text_box != NULL && text_box == m_text_box) {
		Environment *env;

		if (!m_switching) {
			ret = sceImeVshClose(m_ime);
			if (ret == 0) {
				if (m_work != NULL) {
					delete[] m_work;
					m_work = NULL;
				}
				RestoreCamera();
			}
		} else {
			ret = 0;
		}
		if (ret == 0) {
			ClearPreedit();
			Mask(0, 0, 0);
			env = m_text_box->m_env;
			if (env != NULL && m_focus_shown)
				ScePafWidget_B4A4660B(env, 1, -1, 1);
			PostEvent(EVENT_EDIT_END, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			m_text_box->DeleteEventCallback(Handler::CB_STATE_READY, OnStateReady,
							this);
			m_text_box = NULL;
		}
	}
	return ret;
}

int32_t TextBoxImeListener::Update(TextBox *text_box)
{
	int32_t ret = SCE_PAF_ERROR_INVALID_ARGUMENT;

	if (text_box != NULL && text_box == m_text_box)
		return sceImeVshUpdate(m_ime);
	return ret;
}

int32_t TextBoxImeListener::NotifyCaretMove(TextBox *text_box)
{
	int32_t ret = SCE_PAF_ERROR_INVALID_ARGUMENT;

	if (text_box != NULL && text_box == m_text_box) {
		uint32_t caret;
		const wchar_t *buf;
		uint32_t buf_len;

		m_text_box->GetCaretIndex(caret);
		if (m_preedit_len != 0) {
			if (caret >= m_preedit_index && m_preedit_index + m_preedit_len >= caret)
				ScePafLowlayer_5CE62E49(m_ime, caret - m_preedit_index);
			else
				sceImeVshConfirmPreedit(m_ime);
		}
		ScePafWidget_62604965(m_text_box, &buf, &buf_len);
		ret = sceImeVshUpdateContext(m_ime, buf, caret);
		if (ret == 0)
			Mask(0, 0, 0);
	}
	return ret;
}

int32_t TextBoxImeListener::Commit(TextBox *text_box)
{
	int32_t ret = SCE_PAF_ERROR_INVALID_ARGUMENT;

	if (text_box != NULL && text_box == m_text_box) {
		if (m_preedit_len == 0)
			return SCE_PAF_ERROR_NO_ENTRY;
		ret = sceImeVshClearPreedit(m_ime);
		if (ret == 0) {
			ClearPreedit();
			Mask(0, 0, 0);
		}
	}
	return ret;
}

int32_t TextBoxImeListener::NotifyPasswordMode(TextBox *text_box)
{
	if (text_box != NULL && text_box == m_text_box) {
		m_text_box->KillTimeoutEvent(TIMER_MASK);
		m_text_box->GetPasswordMode(m_password);
	}
	return SCE_PAF_ERROR_INVALID_ARGUMENT;
}

int32_t TextBoxImeListener::FollowCaret(const math::v4 &rect)
{
	bool auto_transition;

	m_text_box->GetEditAutoTransitionMode(auto_transition);
	if (auto_transition) {
		Scene *scene = m_text_box->m_scene;
		Camera *camera = m_text_box->GetCamera();

		if (scene != NULL && camera != NULL) {
			if (m_camera_offset == math::v4(0.0f, 0.0f, 0.0f, 0.0f))
				m_camera_offset = camera->GetPos(1, NULL,
								 math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			math::v4 size = *scene->GetSize(1);
			/* the screen above the IME, and how far its middle is from the scene's */
			math::v4 area(size.extract_x(), rect.extract_y() - 32.0f, 0.0f, 0.0f);
			math::v4 center(0.0f,
					((size.extract_y() - rect.extract_y()) - 32.0f) * 0.5f,
					0.0f, 0.0f);
			math::v4 world = m_text_box->GetWorldMatrix().w();
			math::v4 pos = math::v4(world.extract_x(), world.extract_y(), 0.0f, 0.0f) +
				       m_camera_offset - center;
			math::v4 min = (area - size) * 0.5f - center;
			math::v4 max = (size - area) * 0.5f - center;

			if (pos.extract_x() < min.extract_x())
				pos.set_x(min.extract_x());
			else if (pos.extract_x() > max.extract_x())
				pos.set_x(max.extract_x());
			if (pos.extract_y() < min.extract_y())
				pos.set_y(min.extract_y());
			else if (pos.extract_y() > max.extract_y())
				pos.set_y(max.extract_y());
			camera->SetPos(pos, new Timer(200.0f, Timer::FUNC_EXPONENT),
				       Widget::ANIM_MOVE, 2);
		}
	}
	return 0;
}

int32_t TextBoxImeListener::HandleImeEvent(const SceImeVshEvent *e)
{
	switch (e->id) {
	case SCE_IME_EVENT_OPEN: {
		math::v4 rect((float)e->param.rect.x, (float)e->param.rect.y,
			      (float)e->param.rect.width, (float)e->param.rect.height);

		FollowCaret(rect);
		PostEvent(EVENT_IME_OPEN, rect);
		m_text_box->SendRectEvent(TextBox::CB_TEXT_BOX_IME_OPEN, rect);
		break;
	}
	case SCE_IME_EVENT_CHANGE_SIZE: {
		math::v4 rect((float)e->param.rect.x, (float)e->param.rect.y,
			      (float)e->param.rect.width, (float)e->param.rect.height);

		FollowCaret(rect);
		PostEvent(EVENT_IME_CHANGE_SIZE, rect);
		m_text_box->SendRectEvent(TextBox::CB_TEXT_BOX_IME_CHANGE_SIZE, rect);
		break;
	}
	case SCE_IME_EVENT_PRESS_CLOSE: {
		Environment *env = m_text_box->m_env;

		m_text_box->EndEdit();
		if (e->param.showFocus && env != NULL)
			env->ShowFocus(-1);
		break;
	}
	case SCE_IME_EVENT_PRESS_ENTER:
		Mask(0, 0, 0);
		m_text_box->SendRectEvent(TextBox::CB_TEXT_BOX_ENTER_PRESSED,
					  math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		break;
	case SCE_IME_VSH_EVENT_UPDATE_PREEDIT: {
		uint32_t len = m_preedit_len;

		if (len == 0) {
			m_text_box->GetCaretIndex(m_preedit_index);
			len = m_preedit_len;
		}
		m_text_box->ReplaceStringNotify(m_preedit_index, len, wstring(e->param.text.str));
		m_preedit_len = sce_paf_wcslen(e->param.text.str);
		if (m_preedit_len != 0) {
			m_text_box->SetStyleAttribute(graph::TextStyleAttribute_BackColor,
						      m_preedit_index, m_preedit_len,
						      m_preedit_back_color);
			m_text_box->SetStyleAttribute(graph::TextStyleAttribute_UnderlineColor,
						      m_preedit_index, m_preedit_len,
						      m_preedit_underline_color);
			m_text_box->SetStyleAttribute(graph::TextStyleAttribute_Underline,
						      m_preedit_index, m_preedit_len, true);
			m_text_box->SetCaretIndex(m_preedit_index + e->param.text.caretIndex);
		}
		PostEvent(EVENT_IME_EDIT, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		break;
	}
	case SCE_IME_VSH_EVENT_COMMIT: {
		uint32_t index = m_preedit_index;
		uint32_t len = m_preedit_len;
		uint32_t total;

		if (len != 0) {
			m_text_box->SetStyleAttribute(graph::TextStyleAttribute_BackColor, index, len,
						      s_no_back_color);
			m_text_box->SetStyleAttribute(graph::TextStyleAttribute_UnderlineColor,
						      m_preedit_index, m_preedit_len,
						      s_no_underline_color);
			m_text_box->SetStyleAttribute(graph::TextStyleAttribute_Underline,
						      m_preedit_index, m_preedit_len, false);
			index = m_preedit_index;
			len = m_preedit_len;
		}
		m_preedit_len = 0;
		m_text_box->ReplaceString(index, len, wstring(e->param.text.str));
		m_text_box->GetLength(total);
		Mask(m_preedit_index, len, total);
		PostEvent(EVENT_IME_EDIT, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		break;
	}
	case SCE_IME_VSH_EVENT_INSERT: {
		uint32_t caret;
		uint32_t total;
		uint32_t len;

		m_text_box->GetCaretIndex(caret);
		m_text_box->ReplaceString(caret, 0, wstring(e->param.text.str));
		len = sce_paf_wcslen(e->param.text.str);
		m_text_box->GetLength(total);
		Mask(caret, len, total);
		PostEvent(EVENT_IME_EDIT, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		break;
	}
	case SCE_IME_VSH_EVENT_BACKSPACE: {
		uint32_t caret;

		m_text_box->GetCaretIndex(caret);
		if (caret != 0) {
			m_text_box->ReplaceString(caret - 1, 1, wstring((const wchar_t *)NULL));
			Mask(0, 0, 0);
			PostEvent(EVENT_IME_EDIT, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		}
		break;
	}
	case SCE_IME_VSH_EVENT_DELETE_TO_START: {
		uint32_t caret;

		m_text_box->GetCaretIndex(caret);
		if (caret != 0) {
			m_text_box->ReplaceString(0, caret, wstring((const wchar_t *)NULL));
			Mask(0, 0, 0);
			PostEvent(EVENT_IME_EDIT, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		}
		break;
	}
	case SCE_IME_VSH_EVENT_CLEAR: {
		uint32_t total;

		m_text_box->GetLength(total);
		if (total != 0) {
			m_text_box->SetString(wstring((const wchar_t *)NULL));
			Mask(0, 0, 0);
			PostEvent(EVENT_IME_EDIT, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		}
		break;
	}
	case SCE_IME_VSH_EVENT_DELETE: {
		uint32_t total;
		uint32_t caret;

		m_text_box->GetLength(total);
		m_text_box->GetCaretIndex(caret);
		if (caret < total) {
			m_text_box->ReplaceString(caret, 1, wstring((const wchar_t *)NULL));
			Mask(0, 0, 0);
			PostEvent(EVENT_IME_EDIT, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		}
		break;
	}
	case SCE_IME_VSH_EVENT_CARET_LEFT:
		m_text_box->MoveCaretLeft();
		PostEvent(EVENT_IME_EDIT, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		break;
	case SCE_IME_VSH_EVENT_CARET_RIGHT:
		m_text_box->MoveCaretRight();
		PostEvent(EVENT_IME_EDIT, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		break;
	case SCE_IME_VSH_EVENT_CARET_UP:
		m_text_box->MoveCaretUp();
		PostEvent(EVENT_IME_EDIT, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		break;
	case SCE_IME_VSH_EVENT_CARET_DOWN:
		m_text_box->MoveCaretDown();
		PostEvent(EVENT_IME_EDIT, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		break;
	}
	return 0;
}

int32_t TextBoxImeListener::TrimToLayout()
{
	int32_t lines;

	m_text_box->GetLayoutAttribute(graph::TextLayoutAttribute_MaxLineCount, lines);
	if (lines > 0) {
		graph::TextLayoutLineInfo info;

		if (m_text_box->GetLineInfo(lines - 1, info) == 0) {
			uint32_t total;
			uint32_t end;

			m_text_box->GetLength(total);
			end = info.index + info.len;
			if (end < total) {
				if (m_preedit_len != 0) {
					if (sceImeVshClearPreedit(m_ime) == 0) {
						ClearPreedit();
						Mask(0, 0, 0);
					}
				} else {
					m_text_box->ReplaceString(end, total - end,
								  wstring((const wchar_t *)NULL));
				}
			}
		}
	}
	return 0;
}

void TextBoxImeListener::OnMaskTimeout(int32_t type, Handler *self, Event *e, void *data)
{
	((Text *)self)->SetStyleAttribute(graph::TextStyleAttribute_Mask, 0, 0, true);
}

void TextBoxImeListener::OnStateReady(int32_t type, Handler *self, Event *e, void *data)
{
	((TextBoxImeListener *)data)->TrimToLayout();
}

void TextBoxImeListener::OnImeEvent(void *arg, const SceImeVshEvent *e)
{
	((TextBoxImeListener *)arg)->HandleImeEvent(e);
}

}
}
