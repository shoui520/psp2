/*
 * The IME dialog: the caller's title and text on ime_dialog_plugin's page, the text in a text box
 * the system's IME edits, a scroll view that follows the caret through a long text, and the
 * cancel and clear buttons the caller's modes ask for. The page slides in when the title is
 * ready and out when the dialog closes; the dialog's function is called once it is out. The file's
 * name and every name in it are ours: no Sony path or record names them.
 */
#include <stddef.h>
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
#include <graphics/drawobj/draw_obj.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_plane.h>
#include <widget/w_button.h>
#include <widget/w_image_button.h>
#include <widget/w_text_box.h>
#include <widget/w_scroll_view_base.h>
#include "ime_dialog_plugin.h"

using namespace paf;

/* the widgets and textures of the page */
#define IME_DIALOG_TITLE		0x62EE49DC
#define IME_DIALOG_SCROLL_VIEW		0x95CFE89A
#define IME_DIALOG_TEXT_BOX		0x92C3C9CC
#define IME_DIALOG_BACKGROUND		0x76B9E8B1
#define IME_DIALOG_TEX_PAGE		0x9C40C65F
#define IME_DIALOG_TEX_TEXT_BOX		0x68D9F436
#define IME_DIALOG_TEX_CANCEL		0x28BE1C5C
#define IME_DIALOG_TEX_CANCEL_PRESSED	0x22A33E51
#define IME_DIALOG_TEX_CLEAR		0x367AF89B

/* the text box's events the dialog follows (names ours) */
#define IME_DIALOG_EVENT_EDIT_BEGIN	0x10000007
#define IME_DIALOG_EVENT_EDIT_END	0x10000008
#define IME_DIALOG_EVENT_CARET_FIRST	0x1000000E
#define IME_DIALOG_EVENT_CARET_LAST	0x10000012

/* the scroll view's height on the page, and a line of text */
#define IME_DIALOG_VIEW_HEIGHT		221
#define IME_DIALOG_LINE_HEIGHT		31
/* what the infobar takes off the page */
#define IME_DIALOG_INFOBAR_HEIGHT	30

static ImeDialog *s_dialog;

/* the widget of the scene with the ID, when it is of the type */
#define IME_DIALOG_FIND(scene, type, id)						\
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

static void OnTitleReady(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnCancel(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnClear(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static wstring TextFilter(const wstring &text);
static void OnAbortTimeout(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);

ImeDialog::ImeDialog(SceImeDialogParam *param, ImeDialogFunction callback, bool infobarShown)
	: m_callback(callback), m_scene(NULL), m_clearButton(NULL), m_param(param), m_caretLine(0),
	  m_lineCount(-1), m_height(0)
{
	Plugin::PageOpenParam openParam;
	intrusive_ptr<graph::Surface> textBoxTexture;

	m_scene = g_imeDialogPlugin->PageOpen(IDParam(s_empty_name, IME_DIALOG_PAGE_ID), openParam);
	if (m_scene != NULL) {
		intrusive_ptr<graph::Surface> pageTexture =
			g_imeDialogPlugin->GetTexture(IDParam(s_empty_name, IME_DIALOG_TEX_PAGE));
		ui::Text *title;

		pageTexture->SetMagFilter(graph::NEAREST);
		title = IME_DIALOG_FIND(m_scene, ui::Text, IME_DIALOG_TITLE);
		if (title != NULL) {
			title->SetString(wstring((const wchar_t *)param->title));
			title->SetParent(title->m_parent, NULL, 6);
			title->AddEventListener(ui::Handler::CB_STATE_READY,
						new ui::EventCBListener(OnTitleReady, (void *)infobarShown));
		}
		if (param->dialogMode == SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL) {
			ui::ImageButton *button = new ui::ImageButton(title, NULL);

			if (button != NULL) {
				intrusive_ptr<graph::Surface> icon =
					g_imeDialogPlugin->GetTexture(IDParam(s_empty_name, IME_DIALOG_TEX_CANCEL));
				math::v4 pos((float)icon->GetWidth() * 0.5f + -480.0f,
					     (title->GetSize(0)->extract_y() - (float)icon->GetHeight()) * 0.5f,
					     0.0f, 0.0f);

				button->SetTexture(icon);
				button->SetTexture(g_imeDialogPlugin->GetTexture(IDParam(s_empty_name,
										    IME_DIALOG_TEX_CANCEL_PRESSED)),
						   2, 0);
				button->SetSize(math::v4((float)icon->GetWidth(), (float)icon->GetHeight(),
							 0.0f, 0.0f));
				button->SetPos(pos);
				button->GetDrawObj(0)->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
				button->GetDrawObj(2)->SetColor(math::v4(0.0f, 0.0f, 0.0f, 1.0f));
				button->GetDrawObj(3)->SetColor(math::v4(40.0f, 39.0f, 55.0f, 1.0f));
				button->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
							 new ui::EventCBListener(OnCancel, this));
			}
		}
		m_scrollView = IME_DIALOG_FIND(m_scene, ui::ScrollView, IME_DIALOG_SCROLL_VIEW);
		if (m_scrollView != NULL) {
			int infobar;
			int half;

			m_scrollView->m_clip_mode = 1;
			m_scrollView->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			{
				math::v4 area = m_scrollView->m_view_box;

				area.set_y(m_scrollView->GetSize(0)->extract_y() - (float)IME_DIALOG_VIEW_HEIGHT);
				ScePafWidget_46F36554(m_scrollView, area);
			}
			infobar = 0;
			if (infobarShown)
				infobar = IME_DIALOG_INFOBAR_HEIGHT;
			half = infobar / 2;
			if (!(param->option & SCE_IME_OPTION_MULTILINE)) {
				int y;

				textBoxTexture = g_imeDialogPlugin->GetTexture(IDParam(s_empty_name,
										       IME_DIALOG_TEX_TEXT_BOX));
				y = (int)((float)(IME_DIALOG_VIEW_HEIGHT - half) +
					  (float)textBoxTexture->GetHeight() * 0.5f - 13.0f);
				if (param->type == SCE_IME_TYPE_BASIC_LATIN)
					y -= 60;
				m_scrollView->SetSize(math::v4(m_scrollView->GetSize(0)->extract_x(),
							       (float)textBoxTexture->GetHeight(), 0.0f, 0.0f));
				m_scrollView->SetPos(math::v4(m_scrollView->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_x(),
							      (float)(y / 2), 0.0f, 0.0f));
			} else {
				m_scrollView->SetSize(math::v4(m_scrollView->GetSize(0)->extract_x(),
							       m_scrollView->GetSize(0)->extract_y() - (float)infobar,
							       0.0f, 0.0f));
				m_scrollView->SetPos(math::v4(m_scrollView->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_x(),
							      m_scrollView->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_y() -
							      (float)half, 0.0f, 0.0f));
			}
		}
		m_textBox = IME_DIALOG_FIND(m_scene, ui::TextBox, IME_DIALOG_TEXT_BOX);
		if (m_textBox != NULL) {
			int adjust;

			m_textBox->SetMaxLength(SCE_IME_DIALOG_MAX_TEXT_LENGTH);
			m_textBox->SetString(wstring((const wchar_t *)param->initialText));
			m_textBox->StopAutoScroll();
			m_textBox->SetSelectionMode(true);
			ScePafWidget_10E22E5A(m_textBox, true);
			m_textBox->SetAlign(m_textBox->m_align_x, 2, m_textBox->m_align_z, NULL);
			ScePafWidget_3EB376F2(m_textBox, TextFilter);
			adjust = 0;
			if (infobarShown)
				adjust = -IME_DIALOG_INFOBAR_HEIGHT;
			if (param->type != SCE_IME_TYPE_BASIC_LATIN)
				adjust -= 55;
			{
				ui::AdjustParam adjustParam(m_textBox->m_adjust_param != NULL ?
							    *m_textBox->m_adjust_param : ScePafWidget_BE1CEA2E);

				if (!(param->option & SCE_IME_OPTION_MULTILINE)) {
					m_textBox->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, false);
					m_textBox->SetTexture(textBoxTexture);
					m_textBox->SetLayoutAttribute(graph::TextLayoutAttribute_AlignY, 1);
					m_textBox->SetParent(m_textBox->m_parent, NULL, 6);
					adjustParam.m_left_margin = 10.0f;
					adjustParam.m_right_margin = 10.0f;
					ScePafWidget_B1C23134(m_textBox, adjustParam);
					m_height = (int)(float)textBoxTexture->GetHeight();
					m_textBox->SetSize(math::v4(m_textBox->GetSize(0)->extract_x() - 40.0f,
								    (float)textBoxTexture->GetHeight(), 0.0f, 0.0f));
				} else {
					math::v2 focus;

					m_height = IME_DIALOG_VIEW_HEIGHT + adjust;
					focus = m_scrollView->GetViewFocus();
					m_scrollView->SetViewFocus(math::v2(focus.extract_x(), (float)m_height * 0.5f),
								   NULL, true, true);
					m_textBox->SetSize(math::v4(m_textBox->GetSize(0)->extract_x(),
								    m_textBox->GetSize(0)->extract_y() + (float)adjust,
								    0.0f, 0.0f));
					adjustParam.m_top_margin = 13.0f;
					ScePafWidget_B1C23134(m_textBox, adjustParam);
				}
			}
			if (param->textBoxMode == SCE_IME_DIALOG_TEXTBOX_MODE_PASSWORD) {
				m_textBox->SetPasswordMode(true);
			} else if (m_param->textBoxMode == SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR) {
				m_clearButton = new ui::Button(m_textBox, NULL);
				if (m_clearButton != NULL) {
					float x;

					x = (float)g_imeDialogPlugin->GetTexture(IDParam(s_empty_name,
											 IME_DIALOG_TEX_CLEAR))->GetWidth() *
					    0.5f + 10.0f;
					m_clearButton->SetTexture(g_imeDialogPlugin->GetTexture(IDParam(s_empty_name,
											IME_DIALOG_TEX_CLEAR)));
					m_clearButton->SetSize(math::v4(32.0f, 32.0f, 0.0f, 0.0f));
					m_clearButton->SetPos(math::v4(m_textBox->GetSize(0)->extract_x() * 0.5f - x,
								       0.0f, 0.0f, 0.0f));
					m_clearButton->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
									new ui::EventCBListener(OnClear, this));
					HideWidget(m_clearButton, NULL);
				}
			}
		}
	}
}

ImeDialog::~ImeDialog()
{
	if (m_scene != NULL) {
		g_imeDialogPlugin->PageClose(IDParam(s_empty_name, IME_DIALOG_PAGE_ID),
					     Plugin::PageCloseParam());
		m_scene = NULL;
	}
}

/* the text box's editing begins: nothing to do */
static void OnEditBegin(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
}

static void OnClosed(Timer *t, ui::AnimListener *listener, void *data1, void *data2, void *data3);

/* the title and the page slide out; the dialog's function is called when they are out */
void ImeDialog::Close(int reason)
{
	ui::Text *title;
	ui::Plane *background;

	s_dialog->m_textBox->SetMetaAlpha(0.0f);
	title = IME_DIALOG_FIND(m_scene, ui::Text, IME_DIALOG_TITLE);
	if (title != NULL) {
		float y = title->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_y() + title->GetSize(0)->extract_y();

		title->SetPos(math::v4(title->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_x(), y, 0.0f, 0.0f),
			      new Timer(300.0f, Timer::FUNC_LINEAR));
		title->SetMetaAlpha(0.0f, new Timer(200.0f, Timer::FUNC_LINEAR));
	}
	background = IME_DIALOG_FIND(m_scene, ui::Plane, IME_DIALOG_BACKGROUND);
	if (background != NULL) {
		background->SetPos(math::v4(0.0f, -background->GetSize(0)->extract_y(), 0.0f, 0.0f),
				   new Timer(250.0f, Timer::FUNC_FASTUP));
		background->SetMetaAlpha(0.0f, new Timer(250.0f, Timer::FUNC_FASTUP),
					 ui::Widget::ANIM_META_ALPHA, 0, 0.0f, OnClosed, (void *)reason);
	}
}

/* the text, as the text box ends its editing: copied into the caller's buffer, and the dialog
   closes */
static inline void TakeText(int reason)
{
	wstring text;
	ImeDialog *dialog = s_dialog;
	SceSize size;

	dialog->m_textBox->GetString(text);
	dialog->m_textBox->SetEditMode(false);
	size = text.length() * sizeof(SceWChar16);
	sce_paf_memcpy(dialog->m_param->inputTextBuffer, text.c_str(), size);
	*(SceWChar16 *)((char *)dialog->m_param->inputTextBuffer + size) = 0;
	dialog->Close(reason);
}

static void OnEditEnd(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	TakeText(ImeDialog::CLOSE_REASON_ENTER);
}

static void OnEnterPressed(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	s_dialog->m_textBox->EndEdit();
	TakeText(ImeDialog::CLOSE_REASON_CLOSE);
}

static void OnCancel(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	s_dialog->m_textBox->EndEdit();
	TakeText(ImeDialog::CLOSE_REASON_CANCEL);
}

/* the clear button empties the text and hides itself */
static void OnClear(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	s_dialog->m_textBox->SetString(wstring(L""));
	HideWidget(s_dialog->m_clearButton, NULL);
}

/* the text changed: the clear button shows while there is text, the text box grows and shrinks
   with the text's lines, and the scroll view follows the caret */
static void OnTextChanged(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	int viewHeight;
	int halfView;
	int focusY;
	int top;
	int textHeight;
	uint32_t caret;
	uint32_t length;
	ui::Button *clear;
	math::v2 focus;

	viewHeight = (int)s_dialog->m_scrollView->GetSize(0)->extract_y();
	focus = s_dialog->m_scrollView->GetViewFocus();
	halfView = viewHeight / 2;
	focusY = (int)(focus.extract_y() + (float)(viewHeight / 2));
	top = focusY - s_dialog->m_height;
	textHeight = (int)s_dialog->m_textBox->GetSize(0)->extract_y();
	s_dialog->m_textBox->GetCaretIndex(caret);
	clear = s_dialog->m_clearButton;
	if (clear != NULL) {
		if (caret == 0)
			HideWidget(clear, new Timer(50.0f, Timer::FUNC_LINEAR));
		else
			ShowWidget(clear, new Timer(150.0f, Timer::FUNC_LINEAR));
	}
	graph::TextLayoutCharInfo last;

	s_dialog->m_textBox->GetLength(length);
	s_dialog->m_textBox->GetCharInfo(length, last);
	graph::TextLayoutCharInfo current;

	s_dialog->m_textBox->GetCharInfo(caret, current);
	s_dialog->m_caretLine = current.line;
	if (last.line != (uint32_t)s_dialog->m_lineCount) {
		math::v2 bounds;
		int room;
		int boundsHeight;
		int over;
		int caretTop;

		s_dialog->m_lineCount = last.line;
		s_dialog->m_textBox->GetBounds(bounds);
		room = textHeight - viewHeight;
		boundsHeight = (int)bounds.extract_y();
		over = boundsHeight - s_dialog->m_height;
		if (over > room) {
			ui::TextBox *textBox = s_dialog->m_textBox;

			textBox->SetSize(math::v4(textBox->GetSize(0)->extract_x(), (float)(over - room + textHeight),
						  0.0f, 0.0f));
		} else if (boundsHeight > s_dialog->m_height && over < room) {
			ui::TextBox *textBox = s_dialog->m_textBox;

			textBox->SetSize(math::v4(textBox->GetSize(0)->extract_x(), (float)(textHeight - (room - over)),
						  0.0f, 0.0f));
		} else if (boundsHeight < s_dialog->m_height) {
			ui::TextBox *textBox = s_dialog->m_textBox;
			ui::ScrollView *scrollView;

			textBox->SetSize(math::v4(textBox->GetSize(0)->extract_x(), (float)viewHeight, 0.0f, 0.0f));
			scrollView = s_dialog->m_scrollView;
			focus = scrollView->GetViewFocus();
			focus.set_y((float)-viewHeight * 0.5f);
			scrollView->SetViewFocus(focus, NULL, true, true);
		}
		caretTop = -IME_DIALOG_LINE_HEIGHT * s_dialog->m_caretLine;
		if (caretTop > focusY) {
			ui::ScrollView *scrollView = s_dialog->m_scrollView;
			int y = caretTop - halfView;
			math::v2 above = scrollView->GetViewFocus();

			above.set_y((float)y);
			scrollView->SetViewFocus(above, NULL, true, true);
		} else if (top > caretTop - IME_DIALOG_LINE_HEIGHT) {
			ui::ScrollView *scrollView = s_dialog->m_scrollView;
			int y = caretTop + s_dialog->m_height - halfView - IME_DIALOG_LINE_HEIGHT;
			math::v2 below = scrollView->GetViewFocus();

			below.set_y((float)y);
			scrollView->SetViewFocus(below, NULL, true, true);
		}
	}
}

/* the scroll view follows the caret's line */
void ImeDialog::ScrollToCaret()
{
	uint32_t caret;
	math::v4 size;
	int viewHeight;
	math::v2 focus;
	int focusY;
	int top;
	int caretTop;

	m_textBox->GetCaretIndex(caret);
	size = *m_scrollView->GetSize(0);
	viewHeight = (int)size.extract_y();
	focus = m_scrollView->GetViewFocus();
	focusY = (int)(focus.extract_y() + (float)viewHeight * 0.5f);
	top = focusY - m_height;
	graph::TextLayoutCharInfo info;

	m_textBox->GetCharInfo(caret, info);
	if (info.line == (uint32_t)m_caretLine)
		return;
	m_caretLine = info.line;
	caretTop = -IME_DIALOG_LINE_HEIGHT * (int)info.line;
	if (caretTop > focusY) {
		ui::ScrollViewBase *scrollView = m_scrollView;
		math::v2 above = scrollView->GetViewFocus();

		above.set_y((float)(int)((float)caretTop - (float)viewHeight * 0.5f));
		scrollView->SetViewFocus(above, NULL, true, true);
	} else if (top > caretTop - IME_DIALOG_LINE_HEIGHT) {
		ui::ScrollViewBase *scrollView = m_scrollView;
		math::v2 below = scrollView->GetViewFocus();

		below.set_y((float)(caretTop + m_height - viewHeight / 2 - IME_DIALOG_LINE_HEIGHT));
		scrollView->SetViewFocus(below, NULL, true, true);
	}
}

static void OnCaretMoved(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	s_dialog->ScrollToCaret();
}

/* the page is in: the text fades in */
static void OnOpened(Timer *t, ui::AnimListener *listener, void *data1, void *data2, void *data3)
{
	s_dialog->m_textBox->SetMetaAlpha(1.0f, new Timer(400.0f, Timer::FUNC_LINEAR));
}

/* the page is out: the dialog's function, with the button and the result of how it closed */
static void OnClosed(Timer *t, ui::AnimListener *listener, void *data1, void *data2, void *data3)
{
	int reason = (int)data2;
	SceInt32 button = SCE_IME_DIALOG_BUTTON_NONE;
	SceInt32 result = SCE_COMMON_DIALOG_RESULT_OK;

	if (reason <= 0) {
		if (reason >= 0)
			button = SCE_IME_DIALOG_BUTTON_CLOSE;
	} else if (reason <= 1) {
		button = SCE_IME_DIALOG_BUTTON_ENTER;
	} else if (reason <= 2) {
		result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
	}
	s_dialog->m_callback(button, result);
}

/* the IME's filter of the caller's types: the text as the IME lets it be typed, or nothing */
static wstring TextFilter(const wstring &text)
{
	uint32_t length = text.length();
	wstring filtered(text);
	SceImeFilterParam filterParam;
	uint32_t outLength;
	wchar_t *buffer;

	filterParam.supportedLanguages = s_dialog->m_param->supportedLanguages;
	filterParam.type = s_dialog->m_param->type;
	buffer = (wchar_t *)sce_paf_malloc((length + 1) * sizeof(wchar_t));
	if (sceImeFilterText(buffer, &outLength, length + 1, &filterParam, text.c_str(), length) != SCE_OK) {
		sce_paf_free(buffer);
		return NULL;
	}
	filtered = buffer;
	sce_paf_free(buffer);
	return filtered;
}

/* the caller's abort: the dialog closes as entered, after a frame when soon is set, else after
   400 milliseconds */
int ImeDialog::Abort(bool soon)
{
	ImeDialog *dialog = s_dialog;

	if (dialog != NULL)
		dialog->m_scene->SetTimeoutEvent(1, soon ? 16.66 : 400.0, OnAbortTimeout, dialog);
	return SCE_OK;
}

/* the IME's parameter from the caller's: its languages, type, options, enter label and filter */
void ImeDialog::GetImeParam(ui::TextBox::ImeParam *imeParam)
{
	SceImeDialogParam *param = m_param;
	SceUInt64 languages = param->supportedLanguages;
	SceUInt32 type = param->type;
	SceBool forced = param->languagesForced;
	SceImeTextFilter filter = param->filter;
	SceUInt32 option = param->option;
	SceUInt32 enterLabel = param->enterLabel;
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
	else if (type == SCE_IME_TYPE_MAIL || type == 258)
		imeType = 4;
	else if (type == SCE_IME_TYPE_URL || type == 257)
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

/* the text box takes the IME's parameter and the caller's length and starts editing; if it cannot,
   the dialog closes */
void ImeDialog::BeginEdit()
{
	ui::TextBox::ImeParam imeParam;

	GetImeParam(&imeParam);
	m_textBox->SetImeParam(imeParam);
	m_textBox->SetEditMode(true);
	m_textBox->SetMaxLength(m_param->maxTextLength);
	m_textBox->AddEventListener(IME_DIALOG_EVENT_EDIT_BEGIN, new ui::EventCBListener(OnEditBegin, NULL));
	m_textBox->AddEventListener(IME_DIALOG_EVENT_EDIT_END, new ui::EventCBListener(OnEditEnd, NULL));
	m_textBox->AddEventListener(ui::TextBox::CB_TEXT_BOX_ENTER_PRESSED,
				    new ui::EventCBListener(OnEnterPressed, NULL));
	m_textBox->AddEventListener(ui::Handler::CB_STATE_READY, new ui::EventCBListener(OnTextChanged, this));
	m_textBox->AddEventListener(0x10000011, new ui::EventCBListener(OnCaretMoved, this));
	m_textBox->AddEventListener(0x10000010, new ui::EventCBListener(OnCaretMoved, this));
	m_textBox->AddEventListener(0x10000012, new ui::EventCBListener(OnCaretMoved, this));
	m_textBox->AddEventListener(0x1000000E, new ui::EventCBListener(OnCaretMoved, this));
	m_textBox->AddEventListener(0x1000000F, new ui::EventCBListener(OnCaretMoved, this));
	if (m_textBox->BeginEdit() != SCE_OK)
		Abort(false);
}

/* the title is ready: the editing begins, and the title and the page slide in */
static void OnTitleReady(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	bool infobarShown = userdata != NULL;
	int infobar;
	ui::Text *title;
	ui::Plane *background;

	s_dialog->BeginEdit();
	infobar = 0;
	if (infobarShown)
		infobar = IME_DIALOG_INFOBAR_HEIGHT;
	title = IME_DIALOG_FIND(s_dialog->m_scene, ui::Text, IME_DIALOG_TITLE);
	if (title != NULL) {
		float y = title->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_y();
		float height = title->GetSize(0)->extract_y();

		title->SetPos(math::v4(title->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_x(),
				       (float)(int)((y - height) - (float)infobar), 0.0f, 0.0f),
			      new Timer(300.0f, Timer::FUNC_LINEAR));
		title->SetMetaAlpha(1.0f, new Timer(200.0f, Timer::FUNC_LINEAR));
	}
	background = IME_DIALOG_FIND(s_dialog->m_scene, ui::Plane, IME_DIALOG_BACKGROUND);
	if (background != NULL) {
		background->SetPos(math::v4(0.0f, 0.0f, 0.0f, 0.0f), new Timer(400.0f, Timer::FUNC_EXPONENT));
		background->SetMetaAlpha(1.0f, new Timer(400.0f, Timer::FUNC_LINEAR),
					 ui::Widget::ANIM_META_ALPHA, 0, 0.0f, OnOpened, NULL);
	}
}

/* the abort's timer: the dialog closes as entered */
static void OnAbortTimeout(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	if (userdata != NULL)
		((ImeDialog *)userdata)->Close(ImeDialog::CLOSE_REASON_ENTER);
}

int ImeDialog::Reset()
{
	s_dialog = NULL;
	return SCE_OK;
}

/* the plugin's error: the dialog is open already (name ours) */
#define IME_DIALOG_PLUGIN_ERROR_BUSY	-2146426878	/* 0x80102602 */

int ImeDialog::Create(SceImeDialogParam *param, ImeDialogFunction callback, bool infobarShown)
{
	if (s_dialog != NULL)
		return IME_DIALOG_PLUGIN_ERROR_BUSY;
	s_dialog = new ImeDialog(param, callback, infobarShown);
	return SCE_OK;
}

int ImeDialog::Destroy()
{
	if (s_dialog != NULL) {
		delete s_dialog;
		s_dialog = NULL;
	}
	return SCE_OK;
}
