/*
 * paf::ui::WebView ("webview"): 2014 PAF composite web browser widget wrapping
 * WebViewBase, its bound UI parts (title, URL, back/forward/reload, progressbar),
 * and resource/style parameter classes ("web_view_base", "style_web_view").
 */
#ifndef PAF_WIDGET_W_WEBVIEW_H
#define PAF_WIDGET_W_WEBVIEW_H

#include <stdint.h>
#include <std/list>
#include <common/string_util.h>
#include <math/ngp/v2.h>
#include <url/url.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/web_context_menu_info.h>
#include <widget/web_controller.h>
#include <widget/w_webview_base.h>

namespace paf {
namespace ui {

class WebView;

/* Application callback interface registered on WebView (23 virtual slots). */
class WebViewPageListener {
public:
	WebViewPageListener()
	{
	}

	virtual ~WebViewPageListener()
	{
	}

	virtual void OnStartLoad(WebView *view)
	{
	}

	virtual void OnFinishLoad(WebView *view)
	{
	}

	virtual void OnLoadResult(WebView *view, int32_t status)
	{
	}

	virtual void OnLoadError(WebView *view, int32_t error_code,
				 const common::String &message)
	{
	}

	virtual void OnRedirect(WebView *view)
	{
	}

	virtual void OnBrowserComponentReady(WebView *view)
	{
	}

	virtual void OnProgressChanged(WebView *view, int32_t progress)
	{
	}

	virtual void OnTitleChanged(WebView *view, const common::String &title)
	{
	}

	virtual void OnLocationChanged(WebView *view, const Url &url)
	{
	}

	virtual void OnSecurityStatusChanged(WebView *view, int32_t status)
	{
	}

	virtual void OnScrollOffsetChanged(WebView *view, const math::v2 &offset)
	{
	}

	virtual void OnFaviconChanged(WebView *view, const WebFaviconInfo &info)
	{
	}

	virtual void OnContextMenu(WebView *view, const ContextMenuInfo &info)
	{
	}

	virtual void OnOpenNewWindow(WebView *view, const Url &url)
	{
	}

	virtual void OnWindowCloseRequested(WebView *view)
	{
	}

	virtual void OnOpenNewWindowWithTarget(WebView *view, const Url &url,
					       const common::String &target)
	{
	}

	virtual void OnNavigationStateChanged(WebView *view, int32_t state)
	{
	}

	virtual void OnSelectPopupClosed(WebView *view)
	{
	}

	virtual void OnHistoryChanged(WebView *view, bool can_go_back, bool can_go_forward)
	{
	}

	virtual void OnImeOpened(WebView *view, int32_t mode, int32_t param)
	{
	}

	virtual void OnImeClosed(WebView *view, int32_t mode, int32_t param)
	{
	}
};

/* Bound UI part entry attached to WebView. */
struct WebViewPartEntry {
	WebViewPartEntry(int32_t part, Widget *widget, bool destroy_on_detach)
		: m_part(part), m_widget(widget), m_destroy_on_detach(destroy_on_detach)
	{
	}

	int32_t m_part;
	Widget *m_widget;
	bool m_destroy_on_detach;
};

class WebView : public Widget, public WebControllerObserver {
public:
	enum PartId {
		PART_TITLE = 0,
		PART_URL = 1,
		PART_BACK = 2,
		PART_FORWARD = 3,
		PART_RELOAD = 4,
		PART_PROGRESS = 5,
		PART_ALL = 6
	};

	static const char m_widget_type[];
	static const char *TypeName();

	WebView(Widget *parent, CreateParam *param);
	virtual ~WebView();

	virtual int32_t EnableTouchRearEvent(bool on_off);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();

	/* WebControllerObserver overrides. */
	virtual void OnStartLoad();
	virtual void OnFinishLoad();
	virtual void OnLoadResult(int32_t status);
	virtual void OnLoadError(int32_t error_code, const common::String &message);
	virtual void OnRedirect();
	virtual void OnBrowserComponentReady();
	virtual void OnProgressChanged(int32_t progress);
	virtual void OnTitleChanged(const common::String &title);
	virtual void OnLocationChanged(const Url &url);
	virtual void OnSecurityStatusChanged(int32_t status);
	virtual void OnScrollOffsetChanged(const math::v2 &offset);
	virtual void OnFaviconChanged(const WebFaviconInfo &info);
	virtual void OnContextMenu(const ContextMenuInfo &info);
	virtual void OnOpenNewWindow(const Url &url);
	virtual void OnWindowCloseRequested();
	virtual void OnOpenNewWindowWithTarget(const Url &url, const common::String &target);
	virtual void OnNavigationStateChanged(int32_t state);
	virtual void OnSelectPopupClosed();
	virtual void OnHistoryChanged(bool can_go_back, bool can_go_forward);
	virtual void OnImeOpened(int32_t mode, int32_t param);
	virtual void OnImeClosed(int32_t mode, int32_t param);

	int32_t SetPart(Widget *widget, int32_t part, bool destroy_on_detach);
	void RemovePart(int32_t part);

	static void OnUrlEntered(int32_t type, Handler *self, Event *e, void *userdata);
	static void OnBackPressed(int32_t type, Handler *self, Event *e, void *userdata);
	static void OnForwardPressed(int32_t type, Handler *self, Event *e, void *userdata);
	static void OnReloadPressed(int32_t type, Handler *self, Event *e, void *userdata);

	WebViewPageListener *m_page_listener;
	WebViewBase *m_base;
	list<WebViewPartEntry> m_parts;
};

/* Resource parameters for WebView ("web_view_base"). */
class WebViewParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	WebViewParam();
	virtual ~WebViewParam();

	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Create(int32_t mode);

	const char *m_home_url;
	int32_t m_cursor_mode;
	IDParam m_title_field;
	IDParam m_url_field;
	IDParam m_back_btn;
	IDParam m_forward_btn;
	IDParam m_reload_btn;
	IDParam m_progressbar;
};

/* Style parameters for WebView ("style_web_view"). */
class WebViewStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	WebViewStyleParam()
	{
	}

	virtual const char *GetParamName() const;
};

}
}

#endif
