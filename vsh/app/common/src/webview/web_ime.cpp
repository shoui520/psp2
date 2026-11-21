/*
 * The system IME a web page's text field opens: one, made the first time a field takes the
 * focus, that passes the IME's events to the view (WebViewInputClient) whose field it edits.
 * Sony's file name is not known (the module has no debug records); this one and every name in
 * it are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <libime/libime_api.h>
#include <ime/ime.h>
#include <common/char_encoding.h>
#include "web_view.h"

namespace paf {
namespace ui {

struct WebViewIme {
	WebViewIme()
		: m_work(NULL), m_ime(NULL), m_caret(0)
	{
	}

	/* an IME event, passed on to the client */
	virtual int OnEvent(const SceImeVshEvent *e);

	WebViewInputClient *m_client;	/* the view whose field the IME edits */
	void *m_work;			/* the IME's work, SCE_IME_WORK_BUFFER_SIZE bytes */
	void *m_ime;			/* the open IME (sceImeVshOpen) */
	uint32_t m_caret;		/* the caret the last edit gives the client */
};

static WebViewIme *s_ime;

/* the IME's event handler: its argument is the WebViewIme */
static void ImeEventHandler(void *arg, const SceImeVshEvent *e)
{
	if (arg != NULL)
		((WebViewIme *)arg)->OnEvent(e);
}

/* the page's languages as the IME's */
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

/* an input type as the IME's: from 0x100 the system IME's own */
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
	case 9:
		ret = SCE_IME_VSH_TYPE_NUMBER;
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
		ret = SCE_IME_VSH_TYPE_PHONE_NUMBER;
		break;
	case 8:
		ret = SCE_IME_TYPE_EXTENDED_NUMBER;
		break;
	}
	return ret;
}

/* the IME's parameters for a field of the input type (ImeGetInputType's) */
static void ImeMakeParam(SceImeVshParam *param, int inputType, int enterLabel, bool multiline)
{
	sce_paf_memset(param, 0, sizeof(*param));
	param->size = sizeof(*param);
	param->supportedLanguages = ConvertLanguages(0);
	param->languagesForced = 0;
	switch (inputType) {
	case 0:
		param->type = ConvertType(0);
		break;
	case 1:
		param->type = SCE_IME_TYPE_BASIC_LATIN;
		break;
	case 2:
		param->type = SCE_IME_TYPE_NUMBER;
		break;
	case 3:
		param->type = SCE_IME_TYPE_EXTENDED_NUMBER;
		break;
	case 4:
		param->type = SCE_IME_TYPE_BASIC_LATIN;
		break;
	case 5:
		param->type = 0x103;
		break;
	case 6:
		param->type = SCE_IME_VSH_TYPE_PHONE_NUMBER;
		break;
	case 7:
		param->type = SCE_IME_TYPE_MAIL;
		break;
	case 8:
		param->type = SCE_IME_TYPE_URL;
		break;
	default:
		param->type = ConvertType(0);
		break;
	}
	if (multiline)
		param->option |= SCE_IME_OPTION_MULTILINE;
	switch (enterLabel) {
	case 0:
		param->enterLabel = SCE_IME_ENTER_LABEL_DEFAULT;
		break;
	case 1:
		param->enterLabel = SCE_IME_ENTER_LABEL_SEND;
		break;
	case 2:
		param->enterLabel = SCE_IME_ENTER_LABEL_SEARCH;
		break;
	case 3:
		param->enterLabel = SCE_IME_ENTER_LABEL_GO;
		break;
	default:
		param->enterLabel = SCE_IME_ENTER_LABEL_DEFAULT;
		break;
	}
	param->filter = NULL;
	param->handler = ImeEventHandler;
}

WebViewIme *ImeGetInstance()
{
	if (s_ime == NULL)
		s_ime = new WebViewIme;
	return s_ime;
}

/* whether the field's type is the name */
static inline bool IsType(const string &type, const string &name)
{
	return sce_paf_strcmp(type.c_str(), name.c_str()) == 0;
}

void ImeGetInputType(const string &type, int *inputType, int *enterLabel)
{
	*inputType = 0;
	*enterLabel = 0;
	if (IsType(type, "password"))
		*inputType = 4;
	else if (IsType(type, "number"))
		*inputType = 2;
	else if (IsType(type, "tel"))
		*inputType = 6;
	else if (IsType(type, "email"))
		*inputType = 7;
	else if (IsType(type, "url"))
		*inputType = 8;
	else if (IsType(type, "search"))
		*enterLabel = SCE_IME_ENTER_LABEL_SEARCH;
}

int ImeOpen(WebViewIme *ime, WebViewInputClient *client, int inputType, int enterLabel,
	bool multiline)
{
	int ret = 0x80AF008F;

	if (client == NULL)
		return ret;
	if (ime->m_ime != NULL) {
		SceImeVshParam param;

		sceImeVshConfirmPreedit(ime->m_ime);
		ImeMakeParam(&param, inputType, enterLabel, multiline);
		param.arg = ime;
		param.work = ime->m_work;
		ret = sceImeVshSetParam(ime->m_ime, &param);
	} else {
		if (ime->m_work == NULL)
			ime->m_work = new char[SCE_IME_WORK_BUFFER_SIZE];
		if (ime->m_work != NULL) {
			SceImeVshParam param;

			ImeMakeParam(&param, inputType, enterLabel, multiline);
			param.arg = ime;
			param.work = ime->m_work;
			param.sdkVersion = SCE_PSP2_SDK_VERSION;
			ret = sceImeVshOpen(&ime->m_ime, &param);
			if (ret >= 0) {
				ime->m_client = client;
			} else if (ime->m_work != NULL) {
				delete[] (char *)ime->m_work;
				ime->m_work = NULL;
			}
		} else {
			ret = 0x80AF0022;
		}
	}
	return ret;
}

int ImeClose(WebViewIme *ime)
{
	int ret = 0x801007B9;

	if (ime->m_ime != NULL) {
		ret = sceImeVshClose(ime->m_ime);
		if (ret == 0) {
			if (ime->m_work != NULL) {
				delete[] (char *)ime->m_work;
				ime->m_work = NULL;
			}
			ime->m_ime = NULL;
			ime->m_caret = 0;
		}
	}
	return ret;
}

int ImeTextChanged(WebViewIme *ime, int length)
{
	return ScePafLowlayer_5CE62E49(ime->m_ime, length);
}

int ImeUpdate(WebViewIme *ime)
{
	int ret = 0x80AF008F;

	if (ime->m_client != NULL)
		ret = sceImeVshUpdate(ime->m_ime);
	return ret;
}

int WebViewIme::OnEvent(const SceImeVshEvent *e)
{
	switch (e->id) {
	case SCE_IME_EVENT_OPEN:
		m_client->OnImeOpen(e->param.rect.height);
		break;
	case SCE_IME_EVENT_UPDATE_TEXT:
	case SCE_IME_EVENT_UPDATE_CARET:
	case SCE_IME_EVENT_CHANGE_SIZE:
		break;
	case SCE_IME_EVENT_PRESS_CLOSE:
		ImeClose(this);
		m_client->OnImePressClose();
		break;
	case SCE_IME_EVENT_PRESS_ENTER:
		m_client->OnImePressEnter();
		break;
	case SCE_IME_VSH_EVENT_UPDATE_PREEDIT: {
		string text;

		m_caret = e->param.text.caretIndex;
		common::Utf16ToUtf8(e->param.text.str, &text);
		m_client->OnImeUpdatePreedit(text, e->param.text.caretIndex);
		break;
	}
	case SCE_IME_VSH_EVENT_COMMIT: {
		string text;

		common::Utf16ToUtf8(e->param.text.str, &text);
		if (text.length() == 0)
			m_caret = 0;
		m_client->OnImeCommit(text, m_caret);
		break;
	}
	case SCE_IME_VSH_EVENT_INSERT: {
		string text;

		common::Utf16ToUtf8(e->param.text.str, &text);
		wstring inserted(e->param.text.str);

		m_caret = inserted.length();
		m_client->OnImeCommit(text, m_caret);
		m_caret = 0;
		break;
	}
	case SCE_IME_VSH_EVENT_BACKSPACE:
		m_client->OnImeBackspace();
		break;
	case SCE_IME_VSH_EVENT_DELETE:
		m_client->OnImeDelete();
		break;
	case SCE_IME_VSH_EVENT_CLEAR:
		sceImeVshClearPreedit(m_ime);
		m_client->OnImeClear();
		break;
	case SCE_IME_VSH_EVENT_DELETE_TO_START:
		break;
	case SCE_IME_VSH_EVENT_CARET_LEFT:
		m_client->OnImeMoveCaret(-1, 0);
		break;
	case SCE_IME_VSH_EVENT_CARET_RIGHT:
		m_client->OnImeMoveCaret(1, 0);
		break;
	case SCE_IME_VSH_EVENT_CARET_UP:
		m_client->OnImeMoveCaret(0, -1);
		break;
	case SCE_IME_VSH_EVENT_CARET_DOWN:
		m_client->OnImeMoveCaret(0, 1);
		break;
	}
	return 0;
}

}
}
