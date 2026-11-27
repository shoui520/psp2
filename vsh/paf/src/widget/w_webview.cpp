/*
 * paf::ui::WebView ("webview") implementation for 2014 PAF.
 */
#include <widget/w_webview.h>
#include <graphics/text/text_style.h>
#include <versatileparam/resourceparam.h>
#include <widget/factory.h>
#include <widget/w_progress_bar.h>
#include <widget/w_text_box.h>

namespace paf {
namespace ui {

static const int32_t s_button_decide_event = 0x10000008;
static const int32_t s_textbox_enter_event = 0x10000009;

const char WebView::m_widget_type[] = "webview";
const char WebViewParam::m_param_name[] = "web_view_base";
const char WebViewStyleParam::m_param_name[] = "style_web_view";

const char *WebView::TypeName()
{
	return m_widget_type;
}

bool WebView::IsInherit(const char *type) const
{
	if (type == m_widget_type)
		return true;
	return Widget::IsInherit(type);
}

char *WebView::GetTypeName() const
{
	return const_cast<char *>(m_widget_type);
}

WebView::WebView(Widget *parent, CreateParam *param)
	: Widget(parent, param),
	  WebControllerObserver(),
	  m_page_listener(NULL),
	  m_base(NULL),
	  m_parts()
{
	EnableEvent(0x7410);
	m_state = (m_state & ~0xFU) | 1U;

	m_base = new WebViewBase(this, NULL);
	m_base->SetAdjust(0, 0, 0);
	m_base->Wakeup();
	m_parts.clear();
	ChangeState(0, -1);
}

WebView::~WebView()
{
}

void WebView::RemovePart(int32_t part)
{
	if (part == PART_ALL) {
		for (list<WebViewPartEntry>::iterator it = m_parts.begin();
		     it != m_parts.end(); ++it) {
			if (it->m_destroy_on_detach)
				it->m_widget->DestroyWidget();
		}
		m_parts.clear();
		return;
	}

	for (list<WebViewPartEntry>::iterator it = m_parts.begin();
	     it != m_parts.end(); ++it) {
		if (it->m_part == part) {
			if (it->m_destroy_on_detach)
				it->m_widget->DestroyWidget();
			m_parts.erase(it);
			return;
		}
	}
}

void WebView::DestroyWidget()
{
	m_page_listener = NULL;
	RemovePart(PART_ALL);
	m_parts.clear();
	Widget::DestroyWidget();
}

int32_t WebView::EnableTouchRearEvent(bool on_off)
{
	int32_t ret = Widget::EnableTouchRearEvent(on_off);

	if (m_base != NULL)
		m_base->EnableTouchRearEvent(on_off);
	return ret;
}

int32_t WebView::SetPart(Widget *widget, int32_t part, bool destroy_on_detach)
{
	for (list<WebViewPartEntry>::iterator it = m_parts.begin();
	     it != m_parts.end(); ++it) {
		if (it->m_part == part)
			return 0;
	}

	switch (part) {
	case PART_TITLE:
	case PART_PROGRESS:
		m_parts.push_back(WebViewPartEntry(part, widget, destroy_on_detach));
		return 0;
	case PART_URL: {
		m_parts.push_back(WebViewPartEntry(part, widget, destroy_on_detach));

		TextBox *url_box = static_cast<TextBox *>(widget);
		math::v4 caret_color(0.0f, 0.0f, 0.0f, 0.0f);

		url_box->GetStyleAttribute(graph::TextStyleAttribute_Color, 0, caret_color);
		url_box->SetCaretColor(caret_color);
		url_box->AddEventCallback(s_textbox_enter_event, OnUrlEntered, this);
		return 0;
	}
	case PART_BACK:
		m_parts.push_back(WebViewPartEntry(part, widget, destroy_on_detach));
		widget->AddEventCallback(s_button_decide_event, OnBackPressed, this);
		return 0;
	case PART_FORWARD:
		m_parts.push_back(WebViewPartEntry(part, widget, destroy_on_detach));
		widget->AddEventCallback(s_button_decide_event, OnForwardPressed, this);
		return 0;
	case PART_RELOAD:
		m_parts.push_back(WebViewPartEntry(part, widget, destroy_on_detach));
		widget->AddEventCallback(s_button_decide_event, OnReloadPressed, this);
		return 0;
	default:
		return -1;
	}
}

void WebView::OnStartLoad()
{
	if (m_page_listener != NULL)
		m_page_listener->OnStartLoad(this);
}

void WebView::OnFinishLoad()
{
	if (m_page_listener != NULL)
		m_page_listener->OnFinishLoad(this);
}

void WebView::OnLoadResult(int32_t status)
{
	if (m_page_listener != NULL)
		m_page_listener->OnLoadResult(this, status);
}

void WebView::OnLoadError(int32_t error_code, const common::String &message)
{
	if (m_page_listener != NULL)
		m_page_listener->OnLoadError(this, error_code, message);
}

void WebView::OnRedirect()
{
	if (m_page_listener != NULL)
		m_page_listener->OnRedirect(this);
}

void WebView::OnBrowserComponentReady()
{
	if (m_page_listener != NULL)
		m_page_listener->OnBrowserComponentReady(this);
}

void WebView::OnProgressChanged(int32_t progress)
{
	for (list<WebViewPartEntry>::iterator it = m_parts.begin();
	     it != m_parts.end(); ++it) {
		if (it->m_part == PART_PROGRESS) {
			ProgressBar *bar = static_cast<ProgressBar *>(it->m_widget);

			if (bar != NULL) {
				float max_val = bar->GetMaxValue();

				if (progress < 100) {
					float span = max_val - bar->GetMinValue();
					float value = bar->GetMinValue() +
						      (span * (float)progress) / 100.0f;

					bar->SetValue(value, false, ProgressBar::BAR_FORE);
				} else {
					bar->SetValue(max_val, false, ProgressBar::BAR_FORE);
				}
			}
			break;
		}
	}

	if (m_page_listener != NULL)
		m_page_listener->OnProgressChanged(this, progress);
}

void WebView::OnTitleChanged(const common::String &title)
{
	for (list<WebViewPartEntry>::iterator it = m_parts.begin();
	     it != m_parts.end(); ++it) {
		if (it->m_part == PART_TITLE) {
			Widget *title_widget = it->m_widget;

			if (title_widget != NULL) {
				wstring title_w = title.GetWString();
				wstring copy(title_w.c_str());

				title_widget->SetString(copy);
			}
			break;
		}
	}

	if (m_page_listener != NULL)
		m_page_listener->OnTitleChanged(this, title);
}

void WebView::OnLocationChanged(const Url &url)
{
	for (list<WebViewPartEntry>::iterator it = m_parts.begin();
	     it != m_parts.end(); ++it) {
		if (it->m_part == PART_URL) {
			Widget *url_widget = it->m_widget;

			if (url_widget != NULL) {
				wstring url_w = url.GetUrlWString();
				wstring copy(url_w.c_str());

				url_widget->SetString(copy);
			}
			break;
		}
	}

	if (m_page_listener != NULL)
		m_page_listener->OnLocationChanged(this, url);
}

void WebView::OnSecurityStatusChanged(int32_t status)
{
	if (m_page_listener != NULL)
		m_page_listener->OnSecurityStatusChanged(this, status);
}

void WebView::OnScrollOffsetChanged(const math::v2 &offset)
{
	if (m_page_listener != NULL)
		m_page_listener->OnScrollOffsetChanged(this, offset);
}

void WebView::OnFaviconChanged(const WebFaviconInfo &info)
{
	if (m_page_listener != NULL) {
		WebFaviconInfo copy(info);

		m_page_listener->OnFaviconChanged(this, copy);
	}
}

void WebView::OnContextMenu(const ContextMenuInfo &info)
{
	if (m_page_listener != NULL)
		m_page_listener->OnContextMenu(this, info);
}

void WebView::OnOpenNewWindow(const Url &url)
{
	if (m_page_listener != NULL)
		m_page_listener->OnOpenNewWindow(this, url);
}

void WebView::OnWindowCloseRequested()
{
	if (m_page_listener != NULL)
		m_page_listener->OnWindowCloseRequested(this);
}

void WebView::OnOpenNewWindowWithTarget(const Url &url, const common::String &target)
{
	if (m_page_listener != NULL)
		m_page_listener->OnOpenNewWindowWithTarget(this, url, target);
}

void WebView::OnNavigationStateChanged(int32_t state)
{
	if (m_page_listener != NULL)
		m_page_listener->OnNavigationStateChanged(this, state);
}

void WebView::OnSelectPopupClosed()
{
	if (m_page_listener != NULL)
		m_page_listener->OnSelectPopupClosed(this);
}

void WebView::OnHistoryChanged(bool can_go_back, bool can_go_forward)
{
	if (m_page_listener != NULL)
		m_page_listener->OnHistoryChanged(this, can_go_back, can_go_forward);
}

void WebView::OnImeOpened(int32_t mode, int32_t param)
{
	if (m_page_listener != NULL)
		m_page_listener->OnImeOpened(this, mode, param);
}

void WebView::OnImeClosed(int32_t mode, int32_t param)
{
	if (m_page_listener != NULL)
		m_page_listener->OnImeClosed(this, mode, param);
}

void WebView::OnUrlEntered(int32_t type, Handler *self, Event *e, void *userdata)
{
	(void)type;
	(void)self;
	(void)e;

	WebView *view = static_cast<WebView *>(userdata);
	TextBox *url_box = NULL;

	for (list<WebViewPartEntry>::iterator it = view->m_parts.begin();
	     it != view->m_parts.end(); ++it) {
		if (it->m_part == PART_URL) {
			url_box = static_cast<TextBox *>(it->m_widget);
			break;
		}
	}

	if (url_box != NULL) {
		url_box->EndEdit();

		wstring url_text(L"");

		url_box->GetString(url_text);

		Url target_url(url_text);

		if (view->m_base != NULL)
			view->m_base->LoadUrl(target_url);
	}
}

void WebView::OnBackPressed(int32_t type, Handler *self, Event *e, void *userdata)
{
	(void)type;
	(void)self;
	(void)e;

	WebView *view = static_cast<WebView *>(userdata);

	if (view->m_base != NULL)
		view->m_base->GoBack();
}

void WebView::OnForwardPressed(int32_t type, Handler *self, Event *e, void *userdata)
{
	(void)type;
	(void)self;
	(void)e;

	WebView *view = static_cast<WebView *>(userdata);

	if (view->m_base != NULL)
		view->m_base->GoForward();
}

void WebView::OnReloadPressed(int32_t type, Handler *self, Event *e, void *userdata)
{
	(void)type;
	(void)self;
	(void)e;

	WebView *view = static_cast<WebView *>(userdata);

	if (view->m_base != NULL)
		view->m_base->Reload();
}

WebViewParam::WebViewParam()
	: m_home_url(NULL), m_cursor_mode(-1), m_title_field(), m_url_field(),
	  m_back_btn(), m_forward_btn(), m_reload_btn(), m_progressbar()
{
}

WebViewParam::~WebViewParam()
{
}

const char *WebViewParam::ParamName()
{
	return m_param_name;
}

const char *WebViewParam::GetParamName() const
{
	return m_param_name;
}

int32_t WebViewParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret == 0 && m_serial != NULL) {
		m_serial->AddParam("home_url", &m_home_url, ResourceParam::SEMANTIC_STRING);
		m_serial->AddParam("title_field", &m_title_field, ResourceParam::SEMANTIC_WIDGETREF);
		m_serial->AddParam("url_field", &m_url_field, ResourceParam::SEMANTIC_WIDGETREF);
		m_serial->AddParam("back_btn", &m_back_btn, ResourceParam::SEMANTIC_WIDGETREF);
		m_serial->AddParam("forward_btn", &m_forward_btn, ResourceParam::SEMANTIC_WIDGETREF);
		m_serial->AddParam("reload_btn", &m_reload_btn, ResourceParam::SEMANTIC_WIDGETREF);
		m_serial->AddParam("progressbar", &m_progressbar, ResourceParam::SEMANTIC_WIDGETREF);
	}
	return ret;
}

int32_t WebViewParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret == 0) {
		WebView *view = static_cast<WebView *>(m_widget);
		Widget *root = view;

		while (root->m_parent != NULL)
			root = root->m_parent;

		if (m_title_field.GetIDHash() != 0) {
			Widget *w = root->FindChild(m_title_field, 0);

			if (w != NULL)
				view->SetPart(w, WebView::PART_TITLE, true);
		}
		if (m_url_field.GetIDHash() != 0) {
			Widget *w = root->FindChild(m_url_field, 0);

			if (w != NULL)
				view->SetPart(w, WebView::PART_URL, true);
		}
		if (m_back_btn.GetIDHash() != 0) {
			Widget *w = root->FindChild(m_back_btn, 0);

			if (w != NULL)
				view->SetPart(w, WebView::PART_BACK, true);
		}
		if (m_forward_btn.GetIDHash() != 0) {
			Widget *w = root->FindChild(m_forward_btn, 0);

			if (w != NULL)
				view->SetPart(w, WebView::PART_FORWARD, true);
		}
		if (m_reload_btn.GetIDHash() != 0) {
			Widget *w = root->FindChild(m_reload_btn, 0);

			if (w != NULL)
				view->SetPart(w, WebView::PART_RELOAD, true);
		}
		if (m_progressbar.GetIDHash() != 0) {
			Widget *w = root->FindChild(m_progressbar, 0);

			if (w != NULL)
				view->SetPart(w, WebView::PART_PROGRESS, true);
		}
		if (m_home_url != NULL) {
			string url_str(m_home_url);
			Url home_url(url_str);

			view->m_base->LoadUrl(home_url);
		}
		if (m_cursor_mode >= 0)
			view->m_base->SetImeCursorMode(m_cursor_mode != 0);
	}
	return ret;
}

const char *WebViewStyleParam::ParamName()
{
	return m_param_name;
}

const char *WebViewStyleParam::GetParamName() const
{
	return m_param_name;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewWebView(Widget *parent)
{
	return new WebView(parent, NULL);
}

static WidgetParam *NewWebViewParam()
{
	return new WebViewParam;
}

static VersatileParam *NewWebViewLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewWebViewStyleParam()
{
	return new WebViewStyleParam;
}

static VersatileParam *NewWebViewHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewWebViewSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryWebView = {
	PAF_FACTORY_TYPE_NAME(WebView),
	NewWebView,
	NewWebViewParam,
	NewWebViewLayoutParam,
	NewWebViewStyleParam,
	NewWebViewHandlerParam,
	NewWebViewSoundParam,
};
