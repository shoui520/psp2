/*
 * The dialog's page: the EULA's marked-up text in a rich text view with its scroll bar, the
 * accept, decline and cancel buttons (only the cancel button, as "close", for a notice), and the
 * busy indicator while the text and its images come. The file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <std/string>
#include <timer/timer.h>
#include <math/math.h>
#include <graphics/surface/surface.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/event.h>
#include <widget/w_scene.h>
#include <widget/w_rich_text.h>
#include <widget/w_scroll_bar.h>
#include <widget/w_busyindicator.h>
#include <widget/w_group.h>
#include <widget/w_button.h>
#include "npeula_dialog_plugin.h"

using namespace paf;

/* the page and its widgets (names ours) */
#define NPEULA_PAGE			0xF7E05328
#define NPEULA_TEXT			0xEDA7221A
#define NPEULA_SCROLL_BAR		0xD2C8F0C2
#define NPEULA_BUTTONS			0xAF305488
#define NPEULA_CANCEL_BUTTON		0xBFD1E2A7
#define NPEULA_ACCEPT_BUTTON		0x41C9B9C4
#define NPEULA_DECLINE_BUTTON		0x9E74F9FA
#define NPEULA_BUSY_INDICATOR		0x60332A73

/* the events the page follows (names ours) */
#define NPEULA_EVENT_BUTTON		0x10000008
#define NPEULA_EVENT_IMAGE_SIZE		0x10000007
#define NPEULA_EVENT_LAYOUT		0x10000008
#define NPEULA_EVENT_ITEM		0x10000009

/* RichText::GetItemType of an image */
#define NPEULA_ITEM_IMAGE		2

/* CommonDialogNpEulaParam::option of a notice */
#define NPEULA_MODE_NOTICE		1

/* the text's margins */
#define NPEULA_TEXT_MARGIN		12.0f

/* the widget of the page with the ID, when it is of the type */
#define NPEULA_FIND(page, type, id)							\
	((page)->FindChild(IDParam(s_empty_name, (id)), 0) != NULL &&			\
	 (page)->FindChild(IDParam(s_empty_name, (id)), 0)->IsInherit(type::TypeName()) ?	\
	 (type *)(page)->FindChild(IDParam(s_empty_name, (id)), 0) : NULL)

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

static void OnButtonEvent(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnImageSizeEvent(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnLayoutEvent(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnItemEvent(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);

EulaView::EulaView(EulaDialog *owner)
	: m_owner(owner), m_busy(false), m_page(NULL), m_text(NULL), m_scrollBar(NULL),
	  m_busyIndicator(NULL), m_buttons(NULL), m_cancelButton(NULL), m_acceptButton(NULL),
	  m_declineButton(NULL), m_baseUrl(""), m_laidOut(false), m_mode(0)
{
}

EulaView::~EulaView()
{
}

/* the page, its text hidden until it is laid out; a notice has only the cancel button */
int EulaView::Open(Plugin *plugin)
{
	CommonDialogNpEulaParam *work;

	work = (CommonDialogNpEulaParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_NPEULA);
	m_mode = work->option == NPEULA_MODE_NOTICE;
	{
		IDParam page(s_empty_name, NPEULA_PAGE);
		Plugin::PageOpenParam openParam;

		m_page = plugin->PageOpen(page, openParam);
	}
	m_text = NPEULA_FIND(m_page, ui::RichText, NPEULA_TEXT);
	m_scrollBar = NPEULA_FIND(m_page, ui::ScrollBarTouch, NPEULA_SCROLL_BAR);
	m_buttons = NPEULA_FIND(m_page, ui::Group, NPEULA_BUTTONS);
	m_cancelButton = NPEULA_FIND(m_page, ui::Button, NPEULA_CANCEL_BUTTON);
	m_acceptButton = NPEULA_FIND(m_page, ui::Button, NPEULA_ACCEPT_BUTTON);
	m_declineButton = NPEULA_FIND(m_page, ui::Button, NPEULA_DECLINE_BUTTON);
	if (m_mode == NPEULA_MODE_NOTICE) {
		HideWidget(m_buttons, NULL);
		HideWidget(m_acceptButton, NULL);
		HideWidget(m_declineButton, NULL);
	}
	m_busyIndicator = NPEULA_FIND(m_page, ui::BusyIndicator, NPEULA_BUSY_INDICATOR);
	HideWidget(m_text, NULL);
	m_text->m_margin_x = NPEULA_TEXT_MARGIN;
	m_text->m_margin_width = m_scrollBar->GetSize(0)->extract_x() * 2.0f;
	if (m_mode != 0)
		m_text->m_margin_y = NPEULA_TEXT_MARGIN;
	else
		m_text->m_margin_y = m_buttons->GetSize(0)->extract_y();
	AddButtonListener(m_cancelButton);
	AddButtonListener(m_acceptButton);
	AddButtonListener(m_declineButton);
	AddImageSizeListener(m_text);
	AddItemListener(m_text);
	AddLayoutListener(m_text);
	return 1;
}

int EulaView::Close()
{
	RemoveButtonListener(m_cancelButton);
	RemoveButtonListener(m_acceptButton);
	RemoveButtonListener(m_declineButton);
	RemoveImageSizeListener(m_text);
	RemoveItemListener(m_text);
	RemoveLayoutListener(m_text);
	m_page = NULL;
	m_text = NULL;
	m_scrollBar = NULL;
	m_busyIndicator = NULL;
	m_buttons = NULL;
	m_cancelButton = NULL;
	m_acceptButton = NULL;
	m_declineButton = NULL;
	return 0;
}

void EulaView::SetBusy(bool busy)
{
	if (busy && !m_busy) {
		m_busyIndicator->Start();
		m_busy = !m_busy;
	} else if (m_busy) {
		m_busyIndicator->Stop();
		m_busy = !m_busy;
	}
}

/* the text fades in, or is hidden at once */
void EulaView::ShowText(bool show)
{
	if (show == true) {
		if (m_text != NULL)
			ShowWidget(m_text, new Timer(200.0f));
	} else if (m_text != NULL) {
		HideWidget(m_text, NULL);
	}
}

void EulaView::SetText(const char *text, int size)
{
	m_text->SetCText(text, size);
}

void EulaView::SetBaseUrl(string url)
{
	m_baseUrl = url;
}

/* the image's size in the text: the size the text gives it, else the image's own */
void EulaView::SetImage(intrusive_ptr<graph::Surface> surface, int index)
{
	math::v4 size;

	if (surface.get() == NULL)
		return;
	m_text->GetHtmlImageSize(index, size);
	if (size.extract_x() > 0.0f && size.extract_y() > 0.0f) {
		m_text->SetImageSize(index, size);
		return;
	}
	size = math::v4((float)surface->GetWidth(), (float)surface->GetHeight(), 0.0f, 0.0f);
	m_text->SetImageSize(index, size);
}

void EulaView::OnButton(ui::Widget *widget)
{
	if (widget == m_cancelButton) {
		if (m_mode != 0)
			m_owner->SetState(NPEULA_STATE_CLOSED);
		else
			m_owner->SetState(NPEULA_STATE_CANCELED);
	} else if (widget == m_acceptButton) {
		m_owner->SetState(NPEULA_STATE_ACCEPTED);
	} else if (widget == m_declineButton) {
		m_owner->SetState(NPEULA_STATE_DECLINED);
	}
}

/* the image's size, and its fetch if it has not come */
void EulaView::OnImageSize(ui::Widget *widget, ui::Event *e)
{
	if (widget != m_text)
		return;
	int index = e->m_data[0];
	string url;
	math::v4 size;

	m_text->GetImageUrl(index, url);
	m_text->GetHtmlImageSize(index, size);
	if (size.extract_x() > 0.0f && size.extract_y() > 0.0f)
		m_text->SetImageSize(index, size);
	m_owner->GetImage(url, index);
}

/* the buttons go under the text; the text shows once its images have come */
void EulaView::OnLayout(ui::Widget *widget)
{
	if (widget != m_text)
		return;
	m_buttons->SetPos(math::v4(0.0f, -m_text->m_scroll_extent, 0.0f, 0.0f));
	m_laidOut = true;
	if (m_owner->IsJobQueueEmpty()) {
		ShowText(true);
		SetBusy(false);
	}
}

void EulaView::OnItem(ui::Widget *widget, ui::Event *e)
{
	if (widget != m_text)
		return;
	int index = e->m_data[0];

	if (m_text->GetItemType(index) != NPEULA_ITEM_IMAGE)
		return;
	string url;

	m_text->GetItemUrl(index, url);
	intrusive_ptr<graph::Surface> surface = m_owner->GetImage(url, index);

	if (surface.get() != NULL) {
		SetImage(surface, index);
		intrusive_ptr<graph::Surface> texture = surface;

		if (texture.get() != NULL)
			m_text->SetTexture(texture, index, 0);
	}
}

void EulaView::AddButtonListener(ui::Widget *widget)
{
	if (widget != NULL)
		widget->AddEventListener(NPEULA_EVENT_BUTTON,
					 new ui::EventCBListener(OnButtonEvent, this));
}

void EulaView::RemoveButtonListener(ui::Widget *widget)
{
	if (widget != NULL)
		widget->DeleteEventCallback(NPEULA_EVENT_BUTTON, OnButtonEvent, this);
}

void EulaView::AddImageSizeListener(ui::Widget *widget)
{
	if (widget != NULL)
		widget->AddEventListener(NPEULA_EVENT_IMAGE_SIZE,
					 new ui::EventCBListener(OnImageSizeEvent, this));
}

void EulaView::RemoveImageSizeListener(ui::Widget *widget)
{
	if (widget != NULL)
		widget->DeleteEventCallback(NPEULA_EVENT_IMAGE_SIZE, OnImageSizeEvent, this);
}

void EulaView::AddLayoutListener(ui::Widget *widget)
{
	if (widget != NULL)
		widget->AddEventListener(NPEULA_EVENT_LAYOUT,
					 new ui::EventCBListener(OnLayoutEvent, this));
}

void EulaView::RemoveLayoutListener(ui::Widget *widget)
{
	if (widget != NULL)
		widget->DeleteEventCallback(NPEULA_EVENT_LAYOUT, OnLayoutEvent, this);
}

void EulaView::AddItemListener(ui::Widget *widget)
{
	if (widget != NULL)
		widget->AddEventListener(NPEULA_EVENT_ITEM,
					 new ui::EventCBListener(OnItemEvent, this));
}

void EulaView::RemoveItemListener(ui::Widget *widget)
{
	if (widget != NULL)
		widget->DeleteEventCallback(NPEULA_EVENT_ITEM, OnItemEvent, this);
}

/* the widget of the event, when it is of the type */
#define NPEULA_EVENT_WIDGET(self, type)							\
	((self) != NULL && ((ui::Widget *)(self))->IsInherit(type::TypeName()) ?	\
	 (ui::Widget *)(self) : NULL)

static void OnButtonEvent(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	EulaView *view = (EulaView *)userdata;

	if (view != NULL)
		view->OnButton(NPEULA_EVENT_WIDGET(self, ui::Button));
}

static void OnImageSizeEvent(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	EulaView *view = (EulaView *)userdata;

	if (view != NULL)
		view->OnImageSize(NPEULA_EVENT_WIDGET(self, ui::RichText), e);
}

static void OnLayoutEvent(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	EulaView *view = (EulaView *)userdata;

	if (view != NULL)
		view->OnLayout(NPEULA_EVENT_WIDGET(self, ui::RichText));
}

static void OnItemEvent(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	EulaView *view = (EulaView *)userdata;

	if (view != NULL)
		view->OnItem(NPEULA_EVENT_WIDGET(self, ui::RichText), e);
}
