/*
 * The remote OSK dialog: the text a companion device's keyboard types, in a text box on
 * remote_osk_dialog_plugin's page, with the caller's title and the enter button the companion's
 * text field asks for. While no text field of the companion's is open the page waits, dimmed;
 * when one opens the text box takes its IME parameter and edits, and every change of the text, the
 * span the IME composes and the caret goes back to the companion. The page slides in when the
 * title is ready and out when the dialog closes; the dialog's function is called once it is out.
 * The file's name and every name in it are ours: no Sony path or record names them.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libime.h>
#include <ime/ime.h>
#include <std/stdlib.h>
#include <ime_dialog.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <std/string>
#include <std/memory>
#include <timer/timer.h>
#include <math/math.h>
#include <graphics/surface/surface.h>
#include <graphics/text/text_layout.h>
#include <graphics/text/attributed_string.h>
#include <graphics/drawobj/draw_obj.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_plane.h>
#include <widget/w_button.h>
#include <widget/w_image_button.h>
#include <widget/w_text_box.h>
#include <widget/w_scroll_view_base.h>
#include "remote_osk_dialog_plugin.h"

using namespace paf;

/* the widgets, templates, textures and strings of the page */
#define REMOTE_OSK_DIALOG_BACKGROUND		0x50A93CFD
#define REMOTE_OSK_DIALOG_TEMPLATE		0x0B2C016C
#define REMOTE_OSK_DIALOG_SCROLL_VIEW		0xEF5D982C
#define REMOTE_OSK_DIALOG_TEXT_BOX		0x5F1B9A2E
#define REMOTE_OSK_DIALOG_TEXT_FRAME		0x2D9F3541
#define REMOTE_OSK_DIALOG_ENTER_BUTTON		0x92CE62B5
#define REMOTE_OSK_DIALOG_WAIT_TEXT		0x6EFDDF67
#define REMOTE_OSK_DIALOG_TITLE			0x0A106C44
#define REMOTE_OSK_DIALOG_TEX_PAGE		0x9C40C65F
#define REMOTE_OSK_DIALOG_TEX_CANCEL		0x28BE1C5C
#define REMOTE_OSK_DIALOG_TEX_CANCEL_PRESSED	0x22A33E51
#define REMOTE_OSK_DIALOG_TEX_CLEAR		0x367AF89B
#define REMOTE_OSK_DIALOG_STR_ENTER		0x6DE30107
#define REMOTE_OSK_DIALOG_STR_SEND		0xC321C694
#define REMOTE_OSK_DIALOG_STR_SEARCH		0x5E72CAB8
#define REMOTE_OSK_DIALOG_STR_GO		0xF1017E2D

/* the text box's events the dialog follows (names ours) */
#define REMOTE_OSK_DIALOG_EVENT_EDIT_BEGIN		0x10000007
#define REMOTE_OSK_DIALOG_EVENT_EDIT_END		0x10000008
#define REMOTE_OSK_DIALOG_EVENT_EDIT_DONE		0x1000000C
#define REMOTE_OSK_DIALOG_EVENT_SELECTION_CHANGED	0x1000000D

/* what the infobar takes off the page */
#define REMOTE_OSK_DIALOG_INFOBAR_HEIGHT	30

/* the alpha of the text box and its frame while no text field is open */
#define REMOTE_OSK_DIALOG_IDLE_ALPHA		0.4f

static RemoteOskDialog *s_dialog;

/* the widget of the scene with the ID, when it is of the type */
#define REMOTE_OSK_DIALOG_FIND(scene, type, id)						\
	((scene)->FindChild(IDParam(s_empty_name, (id)), 0) != NULL &&			\
	 (scene)->FindChild(IDParam(s_empty_name, (id)), 0)->IsInherit(type::TypeName()) ?	\
	 (type *)(scene)->FindChild(IDParam(s_empty_name, (id)), 0) : NULL)

/* shows and hides a widget over the timer's time (inline in paf's callers) */
static inline void ShowWidget(ui::Widget *widget, Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

static inline void HideWidget(ui::Widget *widget, Timer *timer)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(timer != NULL ? timer->GetTotalTime() : 0.0f, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

/* the IME's parameter of no text field: the system IME's defaults and the most text */
void RemoteOskImeParamInit(RemoteOskImeParam *param)
{
	param->inputMethod = 0;
	param->type = SCE_IME_TYPE_DEFAULT;
	param->option = 0;
	param->enterLabel = SCE_IME_ENTER_LABEL_DEFAULT;
	param->textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;
	param->dialogMode = SCE_IME_DIALOG_DIALOG_MODE_DEFAULT;
	param->maxTextLength = REMOTE_OSK_DIALOG_MAX_TEXT_LENGTH;
	param->textChanged = NULL;
}

/* the IME parameter's checks, as sceImeDialogInit checks the caller's */
int RemoteOskImeParamCheck(const RemoteOskImeParam *param)
{
	if (param->inputMethod != 0)
		return SCE_IME_ERROR_INVALID_INPUT_METHOD;
	if (param->type > REMOTE_OSK_IME_TYPE_MAX)
		return SCE_IME_ERROR_INVALID_TYPE;
	if ((param->option & (param->option ^ REMOTE_OSK_IME_OPTION_MASK)) != 0)
		return SCE_IME_ERROR_INVALID_OPTION;
	if (param->enterLabel != SCE_IME_ENTER_LABEL_DEFAULT &&
	    param->enterLabel != SCE_IME_ENTER_LABEL_SEND &&
	    param->enterLabel != SCE_IME_ENTER_LABEL_SEARCH &&
	    param->enterLabel != SCE_IME_ENTER_LABEL_GO)
		return SCE_IME_ERROR_INVALID_ENTER_LABEL;
	if (param->textBoxMode != SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT &&
	    param->textBoxMode != SCE_IME_DIALOG_TEXTBOX_MODE_PASSWORD &&
	    param->textBoxMode != SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR)
		return REMOTE_OSK_DIALOG_ERROR_TEXTBOX_MODE;
	if (param->dialogMode != SCE_IME_DIALOG_DIALOG_MODE_DEFAULT &&
	    param->dialogMode != SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL)
		return REMOTE_OSK_DIALOG_ERROR_DIALOG_MODE;
	if (param->maxTextLength < 1 || param->maxTextLength > REMOTE_OSK_DIALOG_MAX_TEXT_LENGTH)
		return SCE_IME_ERROR_INVALID_MAX_TEXT_LENGTH;
	if ((param->supportedLanguages &
	     (param->supportedLanguages ^ REMOTE_OSK_IME_LANGUAGE_MASK)) != 0)
		return SCE_IME_ERROR_INVALID_SUPPORTED_LANGUAGES;
	if (param->languagesForced > 1)
		return SCE_IME_ERROR_INTERNAL;
	return SCE_OK;
}

int RemoteOskDialog::Reset()
{
	s_dialog = NULL;
	return SCE_OK;
}

static wstring TextFilter(const wstring &text);
static void OnClear(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnEnter(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);

/*
 * The text box and its scroll view from the page's template, laid out for the IME parameter: one
 * line or several, the enter button's label, the clear button and the password mode; when the
 * page is open already the new widgets are hidden, for FadeIn to show.
 */
void RemoteOskDialog::Layout(bool open)
{
	ui::Widget *frame;
	ui::Button *enter;

	if (m_textBox != NULL) {
		m_textBox->DestroyWidget();
		m_textBox = NULL;
	}
	if (m_scrollView != NULL) {
		m_scrollView->DestroyWidget();
		m_scrollView = NULL;
	}
	frame = m_scene->FindChild(IDParam(s_empty_name, REMOTE_OSK_DIALOG_BACKGROUND), 0);
	g_remoteOskDialogPlugin->TemplateOpen(frame, IDParam(s_empty_name, REMOTE_OSK_DIALOG_TEMPLATE),
					      Plugin::TemplateOpenParam());
	m_scrollView = REMOTE_OSK_DIALOG_FIND(m_scene, ui::ScrollView, REMOTE_OSK_DIALOG_SCROLL_VIEW);
	m_textBox = REMOTE_OSK_DIALOG_FIND(m_scene, ui::TextBox, REMOTE_OSK_DIALOG_TEXT_BOX);
	frame = m_scene->FindChild(IDParam(s_empty_name, REMOTE_OSK_DIALOG_TEXT_FRAME), 0);
	frame->SetMetaAlpha(0.0f);
	frame->SetSize(math::v4(624.0f, 174.0f, 0.0f, 0.0f));
	frame->SetPos(math::v4(-90.0f, 127.0f, 0.0f, 0.0f));
	enter = REMOTE_OSK_DIALOG_FIND(m_scene, ui::Button, REMOTE_OSK_DIALOG_ENTER_BUTTON);
	enter->SetMetaAlpha(0.0f);
	enter->SetPos(math::v4(315.0f, 65.0f, 0.0f, 0.0f));
	if (m_scrollView != NULL) {
		int infobar;

		m_scrollView->m_clip_mode = 1;
		m_scrollView->SetColor(math::v4(0.5f, 0.5f, 0.5f, 0.0f));
		infobar = 0;
		if (m_infobarShown)
			infobar = REMOTE_OSK_DIALOG_INFOBAR_HEIGHT;
		if (!(m_imeParam.option & SCE_IME_OPTION_MULTILINE)) {
			float x = m_scrollView->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_x();
			float width = 804.0f - m_scrollView->GetSize(0)->extract_x();
			float top = 0.0f;

			if (m_idle)
				top = 164.0f;
			m_scrollView->SetSize(math::v4(796.0f, 50.0f, 0.0f, 0.0f));
			m_scrollView->SetPos(math::v4(x + width * 0.5f, (169.0f - (float)infobar) - top,
						      0.0f, 0.0f));
		} else {
			m_scrollView->SetSize(math::v4(m_scrollView->GetSize(0)->extract_x() - 8.0f,
						       (m_scrollView->GetSize(0)->extract_y() - 8.0f) - (float)infobar,
						       0.0f, 0.0f));
			m_scrollView->SetPos(math::v4(m_scrollView->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_x(),
						      m_scrollView->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_y() -
						      (float)(infobar / 2), 0.0f, 0.0f));
		}
		frame = m_scene->FindChild(IDParam(s_empty_name, REMOTE_OSK_DIALOG_TEXT_FRAME), 0);
		if (frame != NULL) {
			math::v4 size = *m_scrollView->GetSize(0);

			size.set_x(size.extract_x() + 8.0f);
			size.set_y(size.extract_y() + 8.0f);
			frame->SetSize(size);
			frame->SetPos(m_scrollView->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)));
		}
	}
	if (m_textBox != NULL) {
		m_textBox->SetMaxLength(REMOTE_OSK_DIALOG_MAX_TEXT_LENGTH);
		m_textBox->SetString(wstring(L""));
		m_textBox->StopAutoScroll();
		m_textBox->SetSelectionMode(true);
		ScePafWidget_10E22E5A(m_textBox, true);
		m_textBox->SetAlign(m_textBox->m_align_x, 2, m_textBox->m_align_z, NULL);
		ScePafWidget_3EB376F2(m_textBox, TextFilter);
		{
			ui::AdjustParam adjustParam(m_textBox->m_adjust_param != NULL ?
						    *m_textBox->m_adjust_param : ScePafWidget_BE1CEA2E);

			/* one line: no word wrap, centred; more: wrapped from the top, scrolled with the
			   scroll view */
			if (!(m_imeParam.option & SCE_IME_OPTION_MULTILINE)) {
				m_textBox->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, false);
				m_textBox->SetLayoutAttribute(graph::TextLayoutAttribute_AlignY, 1);
				m_textBox->SetParent(m_textBox->m_parent, NULL, 6);
				adjustParam.m_top_margin = 6.0f;
				adjustParam.m_bottom_margin = 6.0f;
				adjustParam.m_left_margin = 8.0f;
				adjustParam.m_right_margin = 8.0f;
				ScePafWidget_B1C23134(m_textBox, adjustParam);
				m_height = (int)m_scrollView->GetSize(0)->extract_y();
				m_textBox->SetSize(math::v4(m_scrollView->GetSize(0)->extract_x(),
							    m_scrollView->GetSize(0)->extract_y(), 0.0f, 0.0f));
			} else {
				m_textBox->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, true);
				m_textBox->SetLayoutAttribute(graph::TextLayoutAttribute_AlignY, 0);
				m_textBox->SetParent(m_textBox->m_parent, NULL, 6);
				adjustParam.m_top_margin = 6.0f;
				adjustParam.m_bottom_margin = 6.0f;
				adjustParam.m_left_margin = 8.0f;
				adjustParam.m_right_margin = 8.0f;
				ScePafWidget_B1C23134(m_textBox, adjustParam);
				m_height = (int)m_scrollView->GetSize(0)->extract_y();
				m_textBox->SetSize(math::v4(m_scrollView->GetSize(0)->extract_x(),
							    m_scrollView->GetSize(0)->extract_y(), 0.0f, 0.0f));
				ScePafWidget_DA5F91F8(m_textBox, m_scrollView);
			}
		}
		if (m_imeParam.textBoxMode == SCE_IME_DIALOG_TEXTBOX_MODE_PASSWORD) {
			m_textBox->SetPasswordMode(true);
		} else if (m_imeParam.textBoxMode == SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR) {
			m_clearButton = new ui::Button(m_textBox, NULL);
			if (m_clearButton != NULL) {
				float half;

				half = (float)g_remoteOskDialogPlugin->GetTexture(IDParam(s_empty_name,
										      REMOTE_OSK_DIALOG_TEX_CLEAR))->GetWidth() *
				       0.5f;
				m_clearButton->SetTexture(g_remoteOskDialogPlugin->GetTexture(IDParam(s_empty_name,
											REMOTE_OSK_DIALOG_TEX_CLEAR)));
				m_clearButton->SetSize(math::v4(32.0f, 32.0f, 0.0f, 0.0f));
				m_clearButton->SetPos(math::v4(m_textBox->GetSize(0)->extract_x() * 0.5f - half,
							       0.0f, 0.0f, 0.0f));
				m_clearButton->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
								new ui::EventCBListener(OnClear, this));
				HideWidget(m_clearButton, NULL);
			}
		}
	}
	enter = REMOTE_OSK_DIALOG_FIND(m_scene, ui::Button, REMOTE_OSK_DIALOG_ENTER_BUTTON);
	if (enter != NULL) {
		uint32_t label;

		if (!(m_imeParam.option & SCE_IME_OPTION_MULTILINE)) {
			float x = enter->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_x();
			float y = m_scrollView->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_y();

			enter->SetPos(math::v4(x, y - 60.0f, 0.0f, 0.0f));
		}
		enter->Disable(false);
		enter->DeleteEventCallback(ui::ButtonBase::CB_BTN_DECIDE, OnEnter, this);
		if (m_idle) {
			enter->SetString(wstring(g_remoteOskDialogPlugin->GetString(
				IDParam(s_empty_name, REMOTE_OSK_DIALOG_STR_ENTER))));
		} else {
			switch (m_imeParam.enterLabel) {
			case SCE_IME_ENTER_LABEL_SEND:
				label = REMOTE_OSK_DIALOG_STR_SEND;
				break;
			case SCE_IME_ENTER_LABEL_SEARCH:
				label = REMOTE_OSK_DIALOG_STR_SEARCH;
				break;
			case SCE_IME_ENTER_LABEL_GO:
				label = REMOTE_OSK_DIALOG_STR_GO;
				break;
			default:
				label = REMOTE_OSK_DIALOG_STR_ENTER;
				break;
			}
			enter->SetString(wstring(g_remoteOskDialogPlugin->GetString(
				IDParam(s_empty_name, label))));
		}
	}
	if (!open) {
		m_scrollView->SetMetaAlpha(1.0f);
		m_scene->FindChild(IDParam(s_empty_name, REMOTE_OSK_DIALOG_TEXT_FRAME), 0)->SetMetaAlpha(1.0f);
		m_scene->FindChild(IDParam(s_empty_name, REMOTE_OSK_DIALOG_ENTER_BUTTON), 0)->SetMetaAlpha(1.0f);
		m_scene->FindChild(IDParam(s_empty_name, REMOTE_OSK_DIALOG_WAIT_TEXT), 0)->SetMetaAlpha(1.0f);
	}
}

static void OnTitleReady(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnCancel(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);

RemoteOskDialog::RemoteOskDialog(RemoteOskDialogParam *param, RemoteOskDialogFunction callback,
				 bool infobarShown, const RemoteOskImeParam *imeParam, bool idle)
	: m_callback(callback), m_scene(NULL), m_textBox(NULL), m_scrollView(NULL),
	  m_clearButton(NULL), m_param(param), m_height(0), m_infobarShown(infobarShown),
	  m_imeParam(*imeParam), m_idle(idle), m_textPending(false), m_composeStart(0),
	  m_composeLength(0), m_length(0), m_caretSent(false), m_caret(0)
{
	if (RemoteOskImeParamCheck(&m_imeParam) != SCE_OK)
		RemoteOskImeParamInit(&m_imeParam);
	Plugin::PageOpenParam openParam;

	m_scene = g_remoteOskDialogPlugin->PageOpen(IDParam(s_empty_name, REMOTE_OSK_DIALOG_PAGE_ID),
						    openParam);
	if (m_scene == NULL)
		return;
	{
		ui::Text *title;

		g_remoteOskDialogPlugin->GetTexture(IDParam(s_empty_name, REMOTE_OSK_DIALOG_TEX_PAGE))->
			SetMagFilter(graph::NEAREST);
		title = REMOTE_OSK_DIALOG_FIND(m_scene, ui::Text, REMOTE_OSK_DIALOG_TITLE);
		if (title != NULL) {
			title->SetString(wstring((const wchar_t *)m_param->title));
			if (m_param->title[0] == 0)
				title->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
			title->SetParent(title->m_parent, NULL, 6);
			title->AddEventListener(ui::Handler::CB_STATE_READY,
						new ui::EventCBListener(OnTitleReady, (void *)m_infobarShown));
		}
		if (m_imeParam.dialogMode == SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL) {
			ui::ImageButton *button = new ui::ImageButton(title, NULL);

			if (button != NULL) {
				intrusive_ptr<graph::Surface> icon =
					g_remoteOskDialogPlugin->GetTexture(IDParam(s_empty_name,
										    REMOTE_OSK_DIALOG_TEX_CANCEL));
				float y = title->GetSize(0)->extract_y() * 0.5f - 32.0f;

				button->SetTexture(icon);
				button->SetTexture(g_remoteOskDialogPlugin->GetTexture(IDParam(s_empty_name,
											 REMOTE_OSK_DIALOG_TEX_CANCEL_PRESSED)),
						   2, 0);
				button->SetSize(math::v4((float)icon->GetWidth(), (float)icon->GetHeight(),
							 0.0f, 0.0f));
				button->SetPos(math::v4(-448.0f, y, 0.0f, 0.0f));
				button->GetDrawObj(0)->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
				button->GetDrawObj(2)->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
				button->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
							 new ui::EventCBListener(OnCancel, this));
				ScePafWidget_EC6175D6(button, true);
			}
		}
		Layout(true);
	}
}

/* the plugin's error: the dialog is open already */
int RemoteOskDialog::Create(RemoteOskDialogParam *param, RemoteOskDialogFunction callback,
			    bool infobarShown, const RemoteOskImeParam *imeParam, bool idle)
{
	if (s_dialog != NULL)
		return REMOTE_OSK_DIALOG_ERROR_BUSY;
	s_dialog = new RemoteOskDialog(param, callback, infobarShown, imeParam, idle);
	return SCE_OK;
}

/* the companion's text, in the text box; when the text box edits, it is set again once the
   editing is done */
int RemoteOskDialog::SetText(const wstring &text)
{
	int ret;
	bool editing;

	{
		wstring current;

		ret = m_textBox->GetString(current);
		if (ret != SCE_OK)
			return ret;
		if (sce_paf_wcscmp(current.c_str(), text.c_str()) == 0)
			return SCE_OK;
	}
	ret = m_textBox->SetString(text);
	if (ret != SCE_OK)
		return ret;
	m_composeStart = 0;
	m_composeLength = 0;
	m_length = text.length();
	m_caretSent = false;
	editing = false;
	ret = m_textBox->GetEditMode(editing);
	if (ret != SCE_OK)
		return ret;
	if (editing) {
		m_textPending = true;
		m_pendingText = text;
	}
	return SCE_OK;
}

int RemoteOskDialog::SetCurrentText(const wstring &text)
{
	if (s_dialog == NULL)
		return SCE_OK;
	return s_dialog->SetText(text);
}

static void OnAbortTimeout(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);

/* the caller's abort: the dialog closes, after a frame when soon is set, else after 400
   milliseconds */
int RemoteOskDialog::Abort(bool soon)
{
	RemoteOskDialog *dialog = s_dialog;

	if (dialog != NULL)
		dialog->m_scene->SetTimeoutEvent(1, soon ? 16.66 : 400.0, OnAbortTimeout, dialog);
	return SCE_OK;
}

static void OnEditBegin(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnEditEnd(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnEnterPressed(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnTextChanged(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnCaretMoved(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnSelectionChanged(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnEditDone(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);

/* the text box stops editing, and it and its frame dim; if it cannot, the dialog closes */
void RemoteOskDialog::EndEdit()
{
	if (m_textBox->EndEdit() != SCE_OK)
		Abort(false);
	m_textBox->DeleteEventCallback(REMOTE_OSK_DIALOG_EVENT_EDIT_BEGIN, OnEditBegin, this);
	m_textBox->DeleteEventCallback(REMOTE_OSK_DIALOG_EVENT_EDIT_END, OnEditEnd, this);
	m_textBox->DeleteEventCallback(ui::TextBox::CB_TEXT_BOX_ENTER_PRESSED, OnEnterPressed, this);
	m_textBox->DeleteEventCallback(ui::Handler::CB_STATE_READY, OnTextChanged, this);
	m_textBox->DeleteEventCallback(0x10000011, OnCaretMoved, this);
	m_textBox->DeleteEventCallback(0x10000010, OnCaretMoved, this);
	m_textBox->DeleteEventCallback(0x10000012, OnCaretMoved, this);
	m_textBox->DeleteEventCallback(0x1000000E, OnCaretMoved, this);
	m_textBox->DeleteEventCallback(0x1000000F, OnCaretMoved, this);
	m_textBox->DeleteEventCallback(REMOTE_OSK_DIALOG_EVENT_SELECTION_CHANGED, OnSelectionChanged, this);
	m_textBox->DeleteEventCallback(REMOTE_OSK_DIALOG_EVENT_EDIT_DONE, OnEditDone, this);
	m_textBox->SetColor(math::v4(1.0f, 1.0f, 1.0f, REMOTE_OSK_DIALOG_IDLE_ALPHA));
	m_textBox->SetEditMode(false);
	m_scene->FindChild(IDParam(s_empty_name, REMOTE_OSK_DIALOG_TEXT_FRAME), 0)->
		SetColor(math::v4(1.0f, 1.0f, 1.0f, REMOTE_OSK_DIALOG_IDLE_ALPHA));
}

/* the IME's parameter from the companion's text field: its languages, type, options, enter label
   and filter */
void RemoteOskDialog::GetImeParam(ui::TextBox::ImeParam *imeParam)
{
	SceUInt64 languages = m_imeParam.supportedLanguages;
	SceUInt32 type = m_imeParam.type;
	SceUInt32 forced = m_imeParam.languagesForced;
	SceImeTextFilter filter = m_imeParam.filter;
	SceUInt32 option = m_imeParam.option;
	SceUInt32 enterLabel = m_imeParam.enterLabel;
	uint32_t supported;
	int32_t imeType;

	supported = (languages & SCE_IME_LANGUAGE_DANISH) != 0;
	if (languages & SCE_IME_LANGUAGE_GERMAN)
		supported |= 0x2;
	if (languages & SCE_IME_LANGUAGE_ENGLISH_US)
		supported |= 0x4;
	if (languages & SCE_IME_LANGUAGE_SPANISH)
		supported |= 0x8;
	if (languages & SCE_IME_LANGUAGE_FRENCH)
		supported |= 0x10;
	if (languages & SCE_IME_LANGUAGE_ITALIAN)
		supported |= 0x20;
	if (languages & SCE_IME_LANGUAGE_DUTCH)
		supported |= 0x40;
	if (languages & SCE_IME_LANGUAGE_NORWEGIAN)
		supported |= 0x80;
	if (languages & SCE_IME_LANGUAGE_POLISH)
		supported |= 0x100;
	if (languages & SCE_IME_LANGUAGE_PORTUGUESE_PT)
		supported |= 0x200;
	if (languages & SCE_IME_LANGUAGE_RUSSIAN)
		supported |= 0x400;
	if (languages & SCE_IME_LANGUAGE_FINNISH)
		supported |= 0x800;
	if (languages & SCE_IME_LANGUAGE_SWEDISH)
		supported |= 0x1000;
	if (languages & SCE_IME_LANGUAGE_TURKISH)
		supported |= 0x40000;
	if (languages & SCE_IME_LANGUAGE_JAPANESE)
		supported |= 0x2000;
	if (languages & SCE_IME_LANGUAGE_KOREAN)
		supported |= 0x4000;
	if (languages & SCE_IME_LANGUAGE_SIMPLIFIED_CHINESE)
		supported |= 0x8000;
	if (languages & SCE_IME_LANGUAGE_TRADITIONAL_CHINESE)
		supported |= 0x10000;
	if (languages & SCE_IME_LANGUAGE_PORTUGUESE_BR)
		supported |= 0x20000;
	if (languages & SCE_IME_LANGUAGE_ENGLISH_GB)
		supported |= 0x20000000;
	if (languages & 0x10000000000ULL)
		supported |= 0x40000000;
	if (languages & 0x20000000000ULL)
		supported |= 0x80000000;
	imeType = 0;
	if (type == SCE_IME_TYPE_DEFAULT)
		imeType = 0;
	else if (type == SCE_IME_TYPE_BASIC_LATIN)
		imeType = 1;
	else if (type == SCE_IME_TYPE_NUMBER)
		imeType = 2;
	else if (type == SCE_IME_TYPE_EXTENDED_NUMBER)
		imeType = 8;
	else if (type == SCE_IME_TYPE_MAIL)
		imeType = 4;
	else if (type == SCE_IME_TYPE_URL)
		imeType = 5;
	else if (type == 260)
		imeType = 7;
	else if (type == 256)
		imeType = 3;
	imeParam->supportedLanguages = supported;
	imeParam->type = imeType;
	imeParam->languagesForced = forced != 0;
	imeParam->option = option;
	imeParam->enterLabel = enterLabel;
	imeParam->filter = (int32_t (*)(wchar_t *, uint32_t *, const wchar_t *, uint32_t))filter;
}

/*
 * The text box takes the IME's parameter and the most text; while no text field is open it stays
 * dimmed, else it brightens, follows the IME's events and starts editing. If it cannot, the dialog
 * closes.
 */
void RemoteOskDialog::BeginEdit()
{
	ui::TextBox::ImeParam imeParam;
	bool editing;
	int ret;

	GetImeParam(&imeParam);
	m_textBox->SetImeParam(imeParam);
	m_textBox->SetMaxLength(m_imeParam.maxTextLength);
	m_textBox->GetEditMode(editing);
	if (m_idle) {
		m_scene->FindChild(IDParam(s_empty_name, REMOTE_OSK_DIALOG_TEXT_FRAME), 0)->
			SetColor(math::v4(1.0f, 1.0f, 1.0f, REMOTE_OSK_DIALOG_IDLE_ALPHA));
		m_textBox->SetColor(math::v4(1.0f, 1.0f, 1.0f, REMOTE_OSK_DIALOG_IDLE_ALPHA));
		m_textBox->SetEditMode(false);
		ret = SCE_OK;
	} else {
		m_scene->FindChild(IDParam(s_empty_name, REMOTE_OSK_DIALOG_TEXT_FRAME), 0)->
			SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		m_textBox->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		m_textBox->SetEditMode(true);
		m_textBox->AddEventListener(REMOTE_OSK_DIALOG_EVENT_EDIT_BEGIN,
					    new ui::EventCBListener(OnEditBegin, this));
		m_textBox->AddEventListener(REMOTE_OSK_DIALOG_EVENT_EDIT_END,
					    new ui::EventCBListener(OnEditEnd, this));
		m_textBox->AddEventListener(ui::TextBox::CB_TEXT_BOX_ENTER_PRESSED,
					    new ui::EventCBListener(OnEnterPressed, this));
		m_textBox->AddEventListener(ui::Handler::CB_STATE_READY,
					    new ui::EventCBListener(OnTextChanged, this));
		m_textBox->AddEventListener(0x10000011, new ui::EventCBListener(OnCaretMoved, this));
		m_textBox->AddEventListener(0x10000010, new ui::EventCBListener(OnCaretMoved, this));
		m_textBox->AddEventListener(0x10000012, new ui::EventCBListener(OnCaretMoved, this));
		m_textBox->AddEventListener(0x1000000E, new ui::EventCBListener(OnCaretMoved, this));
		m_textBox->AddEventListener(0x1000000F, new ui::EventCBListener(OnCaretMoved, this));
		m_textBox->AddEventListener(REMOTE_OSK_DIALOG_EVENT_SELECTION_CHANGED,
					    new ui::EventCBListener(OnSelectionChanged, this));
		m_textBox->AddEventListener(REMOTE_OSK_DIALOG_EVENT_EDIT_DONE,
					    new ui::EventCBListener(OnEditDone, this));
		ret = m_textBox->BeginEdit();
	}
	m_caretSent = false;
	if (ret != SCE_OK)
		Abort(false);
}

/* a text field of the companion's opens (active) or closes: the text box is laid out for it and
   starts or stops editing */
int RemoteOskDialog::SetActive(bool active, const RemoteOskImeParam *imeParam)
{
	RemoteOskDialog *dialog = s_dialog;

	if (dialog != NULL && active != !dialog->m_idle) {
		memcpy(&dialog->m_imeParam, imeParam, 52);
		dialog->m_idle = !active;
		if (active) {
			dialog->Layout(false);
			dialog->BeginEdit();
		} else {
			dialog->EndEdit();
			dialog->Layout(false);
		}
	}
	return SCE_OK;
}

int RemoteOskDialog::GetActive(bool *active)
{
	if (s_dialog != NULL)
		*active = !s_dialog->m_idle;
	return SCE_OK;
}

RemoteOskDialog::~RemoteOskDialog()
{
	if (m_scene != NULL) {
		g_remoteOskDialogPlugin->PageClose(IDParam(s_empty_name, REMOTE_OSK_DIALOG_PAGE_ID),
						   Plugin::PageCloseParam());
		m_scene = NULL;
	}
}

int RemoteOskDialog::Destroy()
{
	if (s_dialog != NULL) {
		delete s_dialog;
		s_dialog = NULL;
	}
	return SCE_OK;
}

/* the text box's editing begins and ends: nothing to do */
static void OnEditBegin(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
}

static void OnEditEnd(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
}

/* the IME's enter: the companion's text field is entered */
static void OnEnterPressed(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	s_dialog->m_callback(2, SCE_COMMON_DIALOG_RESULT_OK);
}

static void OnClosed(Timer *t, ui::AnimListener *listener, void *data1, void *data2, void *data3);

/* the page slides out; the dialog's function is called when it is out */
void RemoteOskDialog::Close(int reason)
{
	ui::Plane *background;

	background = REMOTE_OSK_DIALOG_FIND(m_scene, ui::Plane, REMOTE_OSK_DIALOG_BACKGROUND);
	REMOTE_OSK_DIALOG_FIND(m_scene, ui::Text, REMOTE_OSK_DIALOG_TITLE);
	if (background != NULL)
		background->SetPos(math::v4(0.0f, -background->GetSize(0)->extract_y(), 0.0f, 0.0f),
				   new Timer(200.0f, Timer::FUNC_FASTUP), ui::Widget::ANIM_MOVE, 0, 0.0f,
				   OnClosed, (void *)reason);
}

/* the cancel button: the text box stops editing, and the dialog closes as canceled */
static void OnCancel(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	RemoteOskDialog *dialog;

	s_dialog->m_textBox->EndEdit();
	dialog = s_dialog;
	dialog->m_textBox->SetEditMode(false);
	dialog->Close(2);
}

/* the enter button: the companion's text field is entered */
static void OnEnter(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	s_dialog->m_callback(2, SCE_COMMON_DIALOG_RESULT_OK);
}

/* the clear button empties the text and hides itself */
static void OnClear(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	s_dialog->SetText(wstring(L""));
	HideWidget(s_dialog->m_clearButton, NULL);
}

/* the text changed: the clear button shows while there is text, and a text taller than the
   scroll view is aligned to its bottom */
static void OnTextChanged(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	math::v2 bounds;
	uint32_t align;

	if (s_dialog->m_clearButton != NULL) {
		uint32_t length;

		s_dialog->m_textBox->GetLength(length);
		if (length != 0)
			ShowWidget(s_dialog->m_clearButton, new Timer(150.0f, Timer::FUNC_LINEAR));
		else
			HideWidget(s_dialog->m_clearButton, new Timer(50.0f, Timer::FUNC_LINEAR));
	}
	s_dialog->m_textBox->GetBounds(bounds);
	align = 2;
	if (bounds.extract_y() + 12.0f <= s_dialog->m_scrollView->GetSize(0)->extract_y())
		align = 1;
	if (s_dialog->m_textBox->m_adjust_y != align)
		s_dialog->m_textBox->SetAdjust(s_dialog->m_textBox->m_adjust_x, align,
					       s_dialog->m_textBox->m_adjust_z);
}

/*
 * The text box's text, the span the IME composes and the caret, to the companion, with the span
 * the change replaced: the text it added after the composing span or before the caret. The enter
 * button then enters the companion's text field.
 */
void RemoteOskDialog::TextChanged()
{
	int composeStart;
	int composeLength;
	int replaceStart;
	int replaceLength;
	uint32_t caret;
	ui::Button *enter;
	bool underline;
	_list_iterator<graph::TextStyleRun *> it;
	_list_iterator<graph::TextStyleRun *> end;

	if (m_idle || m_imeParam.textChanged == NULL)
		return;
	graph::AttributedString attributed;

	m_textBox->GetAttributedString(attributed);
	wstring text;

	attributed.GetString(text);
	/* the span the IME composes: the underlined style runs after the first runs that are not;
	   none when no run is underlined or a style cannot be read */
	underline = false;
	if (attributed.GetStyleRuns(it, end) != SCE_OK)
		goto no_compose;
	composeStart = 0;
	for (; it != end; ++it) {
		composeLength = (*it)->len;
		if (composeLength == 0)
			continue;
		if ((*it)->style.GetStyleAttribute(graph::TextStyleAttribute_Underline,
						   underline) != SCE_OK)
			goto no_compose;
		if (underline) {
			++it;
			break;
		}
		composeStart += composeLength;
	}
	if (!underline)
		goto no_compose;
	for (; it != end; ++it) {
		int length = (*it)->len;

		if (length == 0)
			continue;
		if ((*it)->style.GetStyleAttribute(graph::TextStyleAttribute_Underline,
						   underline) != SCE_OK)
			goto no_compose;
		if (!underline)
			break;
		composeLength += length;
	}
	goto composed;
no_compose:
	composeLength = 0;
	composeStart = 0;
composed:
	m_textBox->GetCaretIndex(caret);
	if (composeLength != 0) {
		replaceLength = 0;
		replaceStart = 0;
	} else {
		int added = text.length() - m_length;

		if (m_composeLength != 0) {
			if (added >= 0) {
				replaceStart = m_composeStart;
				replaceLength = added + m_composeLength;
			} else {
				replaceLength = 0;
				replaceStart = 0;
			}
		} else if (added != 0) {
			replaceStart = caret - added;
			replaceLength = added;
		} else {
			replaceLength = 0;
			replaceStart = 0;
		}
	}
	m_imeParam.textChanged(composeStart, composeLength, caret, replaceStart, replaceLength,
			       (const SceWChar16 *)text.c_str());
	m_composeStart = composeStart;
	m_composeLength = composeLength;
	m_length = text.length();
	enter = REMOTE_OSK_DIALOG_FIND(m_scene, ui::Button, REMOTE_OSK_DIALOG_ENTER_BUTTON);
	enter->Enable(false);
	enter->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE, new ui::EventCBListener(OnEnter, this));
}

/* the caret moved: the companion hears of it once for each place */
static void OnCaretMoved(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	uint32_t caret;
	RemoteOskDialog *dialog;

	s_dialog->m_textBox->GetCaretIndex(caret);
	if (!s_dialog->m_caretSent || s_dialog->m_caret != caret) {
		s_dialog->TextChanged();
		dialog = s_dialog;
		dialog->m_caretSent = true;
		dialog->m_caret = caret;
	}
}

static void OnSelectionChanged(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	s_dialog->TextChanged();
}

/* the IME's editing is done: the companion hears of the text, unless it is the text the
   companion set while the editing went on */
static void OnEditDone(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	if (!s_dialog->m_textPending) {
		s_dialog->TextChanged();
		return;
	}
	wstring pending(s_dialog->m_pendingText);

	s_dialog->m_pendingText.clear();
	s_dialog->m_textPending = false;
	wstring current;

	if (s_dialog->m_textBox->GetString(current) == SCE_OK &&
	    sce_paf_wcscmp(pending.c_str(), current.c_str()) != 0)
		s_dialog->TextChanged();
}

/* the page is in: the text box, its frame and the enter button show, and while no text field is
   open the waiting text */
static __attribute__((noinline)) void FadeIn(Timer *t, ui::AnimListener *listener, void *data1,
					    void *data2, void *data3)
{
	ui::Widget *widget;

	s_dialog->m_scrollView->SetMetaAlpha(1.0f);
	s_dialog->m_scene->FindChild(IDParam(s_empty_name, REMOTE_OSK_DIALOG_TEXT_FRAME), 0)->
		SetMetaAlpha(1.0f);
	widget = s_dialog->m_scene->FindChild(IDParam(s_empty_name, REMOTE_OSK_DIALOG_ENTER_BUTTON), 0);
	widget->SetMetaAlpha(1.0f);
	if (s_dialog->m_idle) {
		widget = s_dialog->m_scene->FindChild(IDParam(s_empty_name, REMOTE_OSK_DIALOG_WAIT_TEXT), 0);
		widget->SetMetaAlpha(1.0f);
	}
}

/* the page is out: the dialog's function, with the status of the companion's text field and the
   result of how it closed */
static void OnClosed(Timer *t, ui::AnimListener *listener, void *data1, void *data2, void *data3)
{
	int reason = (int)data2;
	SceUInt32 status = 0;
	SceUInt32 result = SCE_COMMON_DIALOG_RESULT_OK;

	if (reason <= 0) {
		if (reason >= 0)
			status = 1;
	} else if (reason <= 1) {
		status = 2;
	} else if (reason <= 2) {
		result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
	}
	s_dialog->m_callback(status, result);
}

/* the IME's filter of the companion's type: the text as the IME lets it be typed, or nothing */
static wstring TextFilter(const wstring &text)
{
	uint32_t length = text.length();
	wstring filtered(text);

	if (length != 0) {
		SceImeFilterParam filterParam;
		uint32_t outLength;
		wchar_t *buffer;

		filterParam.supportedLanguages = s_dialog->m_imeParam.supportedLanguages;
		filterParam.type = s_dialog->m_imeParam.type;
		buffer = (wchar_t *)sce_paf_malloc((length + 1) * sizeof(wchar_t));
		if (sceImeFilterText(buffer, &outLength, length + 1, &filterParam, text.c_str(),
				     length) != SCE_OK)
			filtered = L"";
		else
			filtered = buffer;
		sce_paf_free(buffer);
	}
	return filtered;
}

/* the title is ready: the editing begins, and the title and the page slide in */
static void OnTitleReady(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	bool infobarShown = userdata != NULL;
	int infobar;
	ui::Plane *background;
	ui::Text *title;

	s_dialog->BeginEdit();
	infobar = 0;
	if (infobarShown)
		infobar = REMOTE_OSK_DIALOG_INFOBAR_HEIGHT;
	background = REMOTE_OSK_DIALOG_FIND(s_dialog->m_scene, ui::Plane, REMOTE_OSK_DIALOG_BACKGROUND);
	title = REMOTE_OSK_DIALOG_FIND(s_dialog->m_scene, ui::Text, REMOTE_OSK_DIALOG_TITLE);
	if (title != NULL) {
		float y = title->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_y();
		float height = background->GetSize(0)->extract_y();

		title->SetPos(math::v4(title->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_x(),
				       (float)(int)((y + height) - (float)infobar), 0.0f, 0.0f));
		title->SetMetaAlpha(1.0f);
	}
	if (background != NULL) {
		background->SetPos(math::v4(0.0f, 0.0f, 0.0f, 0.0f), new Timer(200.0f, Timer::FUNC_EXPONENT));
		background->SetMetaAlpha(1.0f);
	}
	FadeIn(NULL, NULL, NULL, NULL, NULL);
}

/* the abort's timer: the dialog closes */
static void OnAbortTimeout(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	if (userdata != NULL)
		((RemoteOskDialog *)userdata)->Close(0);
}
