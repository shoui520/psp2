/*
 * paf::ui::WebImeListener (2014 PAF, 0x81117dfc..0x81118316): default WebEditListener
 * created by wutil::Init for WebViewBase text input over ScePafLowlayer's VSH IME client.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/string_util.h>
#include <ime/ime.h>
#include <libime/libime_api.h>
#include <std/string>
#include <std/string.h>
#include <widget/web_ime_listener.h>
#include "../paf_types.h"

namespace paf {
namespace ui {

/* 2014 SceImeVshParam header size (before sdkVersion was appended). */
static const uint32_t SCE_IME_VSH_PARAM_SIZE_LEGACY = 0x30;

WebImeListener::WebImeListener()
	: m_view(NULL),
	  m_work(NULL),
	  m_ime(NULL),
	  m_preedit_index(0),
	  m_preedit_len(0),
	  m_caret_pos(0),
	  m_preedit_back_color(0.0f, 0.0f, 0.0f, 0.0f),
	  m_preedit_underline_color(0.0f, 1.0f, 1.0f, 0.5f),
	  m_switching(false)
{
}

WebImeListener::~WebImeListener()
{
	if (m_work != NULL)
		delete[] m_work;
}

uint64_t WebImeListener::ConvertLanguages(uint32_t languages)
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
	if (languages & 0x10000000)
		ret |= SCE_IME_LANGUAGE_ENGLISH_US;
	if (languages & 0x20000000)
		ret |= SCE_IME_LANGUAGE_ENGLISH_GB;
	if (languages & 0x40000000)
		ret |= 0x10000000000ULL;
	if (languages & 0x80000000)
		ret |= 0x20000000000ULL;
	return ret;
}

int32_t WebImeListener::BeginEdit(WebViewBase *view)
{
	int32_t option = 0;
	int32_t type = view->GetImeTypeAndEnterLabel(&option);

	m_work = (wchar_t *)new char[SCE_IME_WORK_BUFFER_SIZE];
	if (m_work == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;

	SceImeVshParam param;

	sce_paf_memset(&param, 0, SCE_IME_VSH_PARAM_SIZE_LEGACY);
	param.size = SCE_IME_VSH_PARAM_SIZE_LEGACY;
	param.supportedLanguages = ConvertLanguages(0);
	param.languagesForced = 0;
	param.type = (uint32_t)type;
	param.option = (uint32_t)option;
	param.arg = this;
	param.handler = OnImeEvent;
	param.work = m_work;

	int32_t ret;

	if (m_switching) {
		ret = sceImeVshSetParam(m_ime, &param);
		if (ret != 0)
			return ret;
		m_switching = false;
	} else {
		ret = sceImeVshOpen(&m_ime, &param);
		if (ret < 0) {
			delete[] m_work;
			m_work = NULL;
		}
	}
	if (ret == 0) {
		m_view = view;
		OnBeginEdit(view);
	}
	return ret;
}

int32_t WebImeListener::EndEdit(bool switching, WebViewBase *view)
{
	int32_t ret = 0;

	m_switching = switching;
	if (view != NULL && view == m_view) {
		if (!switching) {
			ret = sceImeVshClose(m_ime);
			if (ret != 0)
				return ret;
			if (m_work != NULL) {
				delete[] m_work;
				m_work = NULL;
			}
		}
		m_view = NULL;
	}
	return ret;
}

int32_t WebImeListener::Update(WebViewBase *view)
{
	if (view != NULL && view == m_view)
		sceImeVshUpdate(m_ime);
	return 0;
}

int32_t WebImeListener::OnBeginEdit(WebViewBase *view)
{
	(void)view;

	common::String text;

	text = m_view->GetImeText();
	return m_view->GetImeCursorPos();
}

int32_t WebImeListener::HandleImeEvent(const SceImeVshEvent *e)
{
	switch (e->id) {
	case SCE_IME_EVENT_OPEN:
	case SCE_IME_EVENT_CHANGE_SIZE:
		m_view->OnImeAreaActivated();
		break;
	case SCE_IME_EVENT_PRESS_ENTER:
		m_view->EndTextEdit(false);
		break;
	case SCE_IME_EVENT_PRESS_CLOSE:
		m_view->EndTextEdit(true);
		break;
	case SCE_IME_VSH_EVENT_UPDATE_PREEDIT: {
		uint32_t len = (uint32_t)sce_paf_wcslen(e->param.text.str);

		m_caret_pos = len;

		wstring text(e->param.text.str);

		m_view->UpdateImePreedit((int32_t)e->param.text.caretIndex, (int32_t)len, text);
		break;
	}
	case SCE_IME_VSH_EVENT_COMMIT: {
		wstring text(e->param.text.str);

		m_view->CommitImeText(text);
		break;
	}
	case SCE_IME_VSH_EVENT_INSERT: {
		wstring text(e->param.text.str);

		m_view->ReplaceImeText(text);
		break;
	}
	case SCE_IME_VSH_EVENT_BACKSPACE:
		m_view->SendKeyBackspace();
		break;
	case SCE_IME_VSH_EVENT_CARET_LEFT:
		m_view->SendKeyLeft();
		break;
	case SCE_IME_VSH_EVENT_CARET_RIGHT:
		m_view->SendKeyRight();
		break;
	default:
		break;
	}
	return 0;
}

void WebImeListener::OnImeEvent(void *arg, const SceImeVshEvent *e)
{
	((WebImeListener *)arg)->HandleImeEvent(e);
}

}
}
