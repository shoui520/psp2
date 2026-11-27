/*
 * paf::ui::WebController and WbcBrowserComponent/WebCore client interfaces for 2014 PAF.
 */
#ifndef PAF_WIDGET_WEB_CONTROLLER_H
#define PAF_WIDGET_WEB_CONTROLLER_H

#include <stdint.h>
#include <std/string>
#include <std/vector>
#include <std/memory>
#include <common/string_util.h>
#include <graphics/surface/surface.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <url/url.h>
#include <widget/web_context_menu_info.h>
#include <widget/web_core.h>

namespace paf {
namespace ui {

class DocumentWidget;
class WebViewBase;
class WebViewSelectInfo;

/* Favicon surface and source URL passed to WebViewPageListener::OnFaviconChanged. */
class WebFaviconInfo {
public:
	WebFaviconInfo() : m_surface(), m_url()
	{
	}

	WebFaviconInfo(const WebFaviconInfo &other)
		: m_surface(other.m_surface), m_url(other.m_url)
	{
	}

	virtual ~WebFaviconInfo()
	{
	}

	intrusive_ptr<graph::Surface> m_surface;
	Url m_url;
};

/* Variant value exchanged between JavaScript external object handlers and Wbc. */
class JsVariant {
public:
	JsVariant() : m_value(L""), m_type(0)
	{
	}

	JsVariant(const wstring &value, int32_t type) : m_value(value), m_type(type)
	{
	}

	~JsVariant()
	{
	}

	wstring m_value;
	int32_t m_type;
};

/* Wbc wire representation of an external JavaScript value. */
struct WbcJsValue {
	void *m_buffer;
	int32_t m_type;
};

/* Wbc wire descriptor for a registered JavaScript external object. */
struct WbcExternalObjectInfo {
	uint32_t m_index;
	wchar_t *m_name;
	uint32_t m_name_length;
	void *m_user;
};

struct WbcExternalObjectList {
	WbcExternalObjectInfo **m_items;
	uint32_t m_count;
};

/* Registered JavaScript external object exposed to WebCore. */
class JsExternalObject {
public:
	virtual ~JsExternalObject()
	{
	}

	virtual bool HasProperty(const wstring &name, bool is_method) = 0;
	virtual void GetProperty(const wstring &name, JsVariant *out_value) = 0;
	virtual void SetProperty(const wstring &name, JsVariant *value) = 0;

	wstring m_name;
	vector<wstring> m_property_names;
	vector<wstring> m_method_names;
};

/* Abstract WebCore browser component (WbcBrowserComponent) managed by WebController (38 slots). */
class WbcBrowserComponent {
public:
	virtual ~WbcBrowserComponent()
	{
	}

	virtual int32_t SetPageClient(class WebCorePageClient *client) = 0;
	virtual void Close() = 0;
	virtual int32_t LoadUrl(const char *url, uint32_t length) = 0;
	virtual int32_t GoBack() = 0;
	virtual int32_t GoForward() = 0;
	virtual int32_t Reload() = 0;
	virtual int32_t StopLoad() = 0;
	virtual int32_t LoadHtml(const char *html, uint32_t length) = 0;
	virtual int32_t ExecuteJavaScript(const char *script, uint32_t length) = 0;
	virtual int32_t SetJavaScriptEnabled(bool enabled) = 0;
	virtual int32_t SetCookieEnabled(bool enabled) = 0;
	virtual int32_t SetViewportSize(int32_t width, int32_t height) = 0;
	virtual int32_t SetUserAgent(const char *agent, uint32_t length) = 0;
	virtual int32_t SetDefaultEncoding(const char *encoding, uint32_t length) = 0;
	virtual int32_t SetFontSize(int32_t size) = 0;
	virtual int32_t SetMinimumFontSize(int32_t size) = 0;
	virtual int32_t ClearHistory() = 0;
	virtual int32_t ClearCache() = 0;
	virtual int32_t ReplyJavaScriptAlert(void *request_handle) = 0;
	virtual int32_t ReplyJavaScriptConfirm(bool accepted, void *request_handle) = 0;
	virtual int32_t ReplyJavaScriptPrompt(bool accepted, const char *text,
					      uint32_t length, void *request_handle) = 0;
	virtual int32_t ReplyNavigationPolicy(bool allow, void *request_handle) = 0;
	virtual int32_t ReplyConfirmPlugin(bool allow, void *request_handle) = 0;
	virtual int32_t ReplyBadCertificate(bool allow, void *request_handle) = 0;
	virtual int32_t ReplyBadCertificateDetail(bool allow, void *request_handle) = 0;
	virtual int32_t ReplyNavigationWarning(bool allow, void *request_handle) = 0;
	virtual int32_t ReplyBasicAuthentication(bool accepted, const char *user,
						 uint32_t user_len, const char *pass,
						 uint32_t pass_len, bool save,
						 void *request_handle) = 0;
	virtual int32_t ReplyLoadError(bool handled, void *request_handle) = 0;
	virtual int32_t ReplyPluginAction(bool allow, void *request_handle) = 0;
	virtual int32_t SetExtensionClient(class WebCoreExtensionClient *client) = 0;
	virtual int32_t ReplyFileChooser(const char *path, uint32_t path_len,
					 const char *mime, uint32_t mime_len,
					 void *request_handle) = 0;
	virtual int32_t ReplyCustomScheme(void *request_handle) = 0;
	virtual int32_t HitTest(int32_t pos_x, int32_t pos_y, int32_t hit_type) = 0;
	virtual int32_t ReplyInputMethod(const char *text, uint32_t length,
					 void *request_handle) = 0;
	virtual int32_t ReplySelectPopup(int32_t index, void *request_handle) = 0;
	virtual int32_t ReplyGeolocation(bool allow, bool remember,
					 void *request_handle) = 0;
};

/* Global WebCore browser manager / listener singleton retrieved by WebController. */
class WbcBrowserManager {
public:
	virtual ~WbcBrowserManager()
	{
	}

	virtual int32_t RequestBrowserComponent(WebController *controller) = 0;
	virtual int32_t CancelRequestBrowserComponent(WebController *controller) = 0;
	virtual int32_t RegisterController(WebController *controller) = 0;
	virtual int32_t NotifyBrowserComponentCreated(WebController *controller) = 0;
	virtual int32_t OpenSelectPopup(WebController *controller,
					const WebViewSelectInfo *info) = 0;
	virtual int32_t OpenFileChooser(WebController *controller) = 0;
	virtual int32_t OpenMultiLineIme(WebController *controller) = 0;
	virtual int32_t CloseMultiLineIme() = 0;
	virtual int32_t GetState() = 0;
	virtual bool IsRestartNeeded() = 0;
	virtual bool CanRestartNow() = 0;
	virtual void RequestRestart(bool force) = 0;
};

/*
 * Callback sink notified by WebController / WebViewBase (25 virtual slots, vtable 0x812f3738).
 * Implemented by WebView and MapCore::CanvasObserver.
 */
class WebControllerObserver {
public:
	WebControllerObserver()
	{
	}

	virtual ~WebControllerObserver()
	{
	}

	virtual void OnStartLoad()
	{
	}

	virtual void OnFinishLoad()
	{
	}

	virtual void OnLoadResult(int32_t status)
	{
	}

	virtual void OnLoadError(int32_t error_code, const common::String &message)
	{
	}

	virtual void OnRedirect()
	{
	}

	virtual void OnBrowserComponentReady()
	{
	}

	virtual void OnProgressChanged(int32_t progress)
	{
	}

	virtual void OnTitleChanged(const common::String &title)
	{
	}

	virtual void OnLocationChanged(const Url &url)
	{
	}

	virtual void OnSecurityStatusChanged(int32_t status)
	{
	}

	virtual void OnScrollOffsetChanged(const math::v2 &offset)
	{
	}

	virtual void OnFaviconChanged(const WebFaviconInfo &info)
	{
	}

	virtual void OnContextMenu(const ContextMenuInfo &info)
	{
	}

	virtual void OnOpenNewWindow(const Url &url)
	{
	}

	virtual void OnWindowCloseRequested()
	{
	}

	virtual void OnOpenNewWindowWithTarget(const Url &url, const common::String &target)
	{
	}

	virtual void OnJavaScriptAlert(const common::String &message)
	{
	}

	virtual void OnJavaScriptConfirm(const common::String &message)
	{
	}

	virtual void OnNavigationStateChanged(int32_t state)
	{
	}

	virtual void OnSelectPopupClosed()
	{
	}

	virtual void OnHistoryChanged(bool can_go_back, bool can_go_forward)
	{
	}

	virtual void OnImeOpened(int32_t mode, int32_t param)
	{
	}

	virtual void OnImeClosed(int32_t mode, int32_t param)
	{
	}
};

/* Base 0 (+0): WebCore JavaScript extension callback interface (8 slots). */
class WebCoreExtensionClient {
public:
	virtual ~WebCoreExtensionClient()
	{
	}

	virtual void GetExternalObjectList(WbcExternalObjectList *out_list) = 0;
	virtual bool HasExternalObjectProperty(uint32_t object_id, const wchar_t *name,
					       bool is_method) = 0;
	virtual void GetExternalObjectMembers(uint32_t object_id) = 0;
	virtual void GetExternalObjectProperty(uint32_t object_id, uint32_t property_id,
					       WbcJsValue *out_value) = 0;
	virtual void SetExternalObjectProperty(uint32_t object_id, uint32_t property_id,
					       WbcJsValue *value) = 0;
	virtual void OnExtensionEvent() = 0;
};

/* Base 1 (+4): MessageDialog callback interface implemented by WebViewBase (8 slots). */
class WebDialogCallback {
public:
	virtual ~WebDialogCallback()
	{
	}

	virtual void OnJsDialogResult(int32_t result)
	{
	}

	virtual void OnNavigationWarningResult(int32_t result)
	{
	}

	virtual void OnConfirmPluginResult(int32_t result)
	{
	}

	virtual void OnBadCertificateResult(int32_t result, int32_t show_detail)
	{
	}

	virtual void OnBasicAuthResult(int32_t result)
	{
	}

	virtual void OnNormalDialogResult(int32_t result)
	{
	}
};

/* Base 2 (+8): WebCore page client callback interface (35 slots). */
class WebCorePageClient {
public:
	virtual ~WebCorePageClient()
	{
	}

	virtual void OnStartLoad() = 0;
	virtual void OnFinishLoad() = 0;
	virtual void OnOpenNewWindow(const char *url, uint32_t url_len,
				     const char *target, uint32_t target_len) = 0;
	virtual void OnExternalUri(const char *uri, uint32_t uri_len) = 0;
	virtual void OnLocationChanged(const char *url, uint32_t url_len) = 0;
	virtual void OnTitleChanged(const char *title, uint32_t title_len) = 0;
	virtual void OnHistoryChanged(bool can_go_back, bool can_go_forward) = 0;
	virtual void OnStatusMessage(const char *text, uint32_t text_len,
				     const char *source, uint32_t source_len) = 0;
	virtual void OnSecurityStatusChanged(int32_t status) = 0;
	virtual void OnJavaScriptAlert(const char *message, uint32_t len,
				       void *request_handle) = 0;
	virtual void OnJavaScriptConfirm(const char *message, uint32_t len,
					 void *request_handle) = 0;
	virtual void OnJavaScriptPrompt(const char *message, uint32_t msg_len,
					const char *default_text, uint32_t default_len,
					void *request_handle) = 0;
	virtual void OnNavigationPolicy(int32_t policy_type, void *request_handle) = 0;
	virtual void OnLoadProgressChanged(int32_t progress) = 0;
	virtual void OnContentBegin() = 0;
	virtual void OnContentEnd() = 0;
	virtual void OnRequestConfirmPlugin(int32_t mode, void *request_handle) = 0;
	virtual void OnRequestBadCertificate(int32_t error_mask, void *request_handle) = 0;
	virtual void OnRequestBadCertificateDetail(int32_t error_mask, const char *issuer,
						   uint32_t issuer_len, const char *subject,
						   uint32_t subject_len, double valid_from,
						   double valid_to, const char *host,
						   uint32_t host_len, void *request_handle) = 0;
	virtual void OnRequestNavigationWarning(int32_t reason, void *request_handle) = 0;
	virtual void OnRequestBasicAuthentication(const char *host, uint32_t host_len,
						  const char *realm, uint32_t realm_len,
						  const char *user, uint32_t user_len,
						  const char *pass, uint32_t pass_len,
						  bool is_proxy, void *request_handle) = 0;
	virtual void OnAuthenticationCancelled() = 0;
	virtual void OnLoadError(int32_t error_code, void *request_handle) = 0;
	virtual void OnCreateFaviconSurface(int32_t width, int32_t height, int32_t format,
					    const void *pixels) = 0;
	virtual void OnRequestFileChooser(int32_t mode, const char *accept,
					  void *request_handle) = 0;
	virtual void OnRequestCustomScheme(const char *scheme, uint32_t scheme_len,
					   const char *url, uint32_t url_len,
					   void *request_handle) = 0;
	virtual void OnFocusInputStart() = 0;
	virtual void OnFocusInputEnd() = 0;
	virtual void OnHitTestResult(int32_t pos_x, int32_t pos_y, int32_t width,
				     int32_t height, const char *link_url, uint32_t link_len,
				     const char *image_url, uint32_t image_len,
				     int32_t hit_type) = 0;
	virtual void OnRequestOpenNewWindow(const char *url, uint32_t url_len,
					    bool user_gesture) = 0;
	virtual void OnSelectPopupOpen(const vector<string> *items, void *request_handle) = 0;
	virtual void OnSelectPopupClose() = 0;
	virtual void OnGeolocationRequest(void *request_handle) = 0;
};

/* Base 3 (+12): WebCore document host & IME interface (8 slots). */
class WebCoreDocumentHost {
public:
	virtual ~WebCoreDocumentHost()
	{
	}

	virtual int32_t CreateDocumentWidget(void *ime_session, WebCoreView *view,
					     WebCoreViewport *viewport,
					     WebRenderClient **out_render_client,
					     WebCoreFrame *frame,
					     WebSurfaceHost **out_surface_host) = 0;
	virtual int32_t DestroyDocumentWidget() = 0;
	virtual int32_t GetDocumentSize() = 0;
	virtual void OnImeOpen(const char *text, uint32_t length, int32_t cursor_pos,
			       int32_t input_type, int32_t enter_label, bool multiline,
			       int32_t rect_x, int32_t rect_y, int32_t rect_w,
			       int32_t rect_h, void *request_handle) = 0;
	virtual void OnImeClose(bool force) = 0;
	virtual void OnImeCursorChanged(int32_t cursor_pos) = 0;
};

/*
 * Controller owning a WbcBrowserComponent session and marshalling WebCore
 * callbacks between the browser process and PAF WebViewBase / MapCanvas.
 */
class WebController
	: public WebCoreExtensionClient,
	  public WebDialogCallback,
	  public WebCorePageClient,
	  public WebCoreDocumentHost {
public:
	WebController();
	virtual ~WebController();

	/* WebCoreExtensionClient overrides (slots 2..7). */
	virtual void GetExternalObjectList(WbcExternalObjectList *out_list);
	virtual bool HasExternalObjectProperty(uint32_t object_id, const wchar_t *name,
					       bool is_method);
	virtual void GetExternalObjectMembers(uint32_t object_id);
	virtual void GetExternalObjectProperty(uint32_t object_id, uint32_t property_id,
					       WbcJsValue *out_value);
	virtual void SetExternalObjectProperty(uint32_t object_id, uint32_t property_id,
					       WbcJsValue *value);
	virtual void OnExtensionEvent();

	/* WebController virtuals (slots 8..17). */
	virtual int32_t Wakeup();
	virtual int32_t OnBrowserComponentCreated(WbcBrowserComponent *component);
	virtual int32_t Sleep(bool notify_error);
	virtual void OnResume();
	virtual void OnOtherControllerWakeup();
	virtual void OnSuspend();
	virtual math::v4 GetViewportSize();
	virtual void SetMaxSurfaceSize(math::v2 size);
	virtual int32_t GetControllerState();
	virtual void OnControllerStateChanged();

	/* WebCorePageClient & WebCoreDocumentHost overrides (slots 18..56, exact order). */
	virtual void OnStartLoad();
	virtual void OnFinishLoad();
	virtual void OnOpenNewWindow(const char *url, uint32_t url_len,
				     const char *target, uint32_t target_len);
	virtual void OnExternalUri(const char *uri, uint32_t uri_len);
	virtual void OnLocationChanged(const char *url, uint32_t url_len);
	virtual void OnTitleChanged(const char *title, uint32_t title_len);
	virtual void OnHistoryChanged(bool can_go_back, bool can_go_forward);
	virtual void OnStatusMessage(const char *text, uint32_t text_len,
				     const char *source, uint32_t source_len);
	virtual void OnSecurityStatusChanged(int32_t status);
	virtual void OnJavaScriptAlert(const char *message, uint32_t len,
				       void *request_handle);
	virtual void OnJavaScriptConfirm(const char *message, uint32_t len,
					 void *request_handle);
	virtual void OnJavaScriptPrompt(const char *message, uint32_t msg_len,
					const char *default_text, uint32_t default_len,
					void *request_handle);
	virtual void OnNavigationPolicy(int32_t policy_type, void *request_handle);

	virtual void OnImeOpen(const char *text, uint32_t length, int32_t cursor_pos,
			       int32_t input_type, int32_t enter_label, bool multiline,
			       int32_t rect_x, int32_t rect_y, int32_t rect_w,
			       int32_t rect_h, void *request_handle);
	virtual void OnImeClose(bool force);
	virtual void OnImeCursorChanged(int32_t cursor_pos);

	virtual void OnLoadProgressChanged(int32_t progress);
	virtual void OnContentBegin();
	virtual void OnContentEnd();
	virtual void OnRequestConfirmPlugin(int32_t mode, void *request_handle);
	virtual void OnRequestBadCertificate(int32_t error_mask, void *request_handle);
	virtual void OnRequestBadCertificateDetail(int32_t error_mask, const char *issuer,
						   uint32_t issuer_len, const char *subject,
						   uint32_t subject_len, double valid_from,
						   double valid_to, const char *host,
						   uint32_t host_len, void *request_handle);
	virtual void OnRequestNavigationWarning(int32_t reason, void *request_handle);
	virtual void OnRequestBasicAuthentication(const char *host, uint32_t host_len,
						  const char *realm, uint32_t realm_len,
						  const char *user, uint32_t user_len,
						  const char *pass, uint32_t pass_len,
						  bool is_proxy, void *request_handle);
	virtual void OnAuthenticationCancelled();
	virtual void OnLoadError(int32_t error_code, void *request_handle);
	virtual void OnCreateFaviconSurface(int32_t width, int32_t height, int32_t format,
					    const void *pixels);
	virtual void OnRequestFileChooser(int32_t mode, const char *accept,
					  void *request_handle);
	virtual void OnRequestCustomScheme(const char *scheme, uint32_t scheme_len,
					   const char *url, uint32_t url_len,
					   void *request_handle);
	virtual void OnFocusInputStart();
	virtual void OnFocusInputEnd();
	virtual void OnHitTestResult(int32_t pos_x, int32_t pos_y, int32_t width,
				     int32_t height, const char *link_url, uint32_t link_len,
				     const char *image_url, uint32_t image_len,
				     int32_t hit_type);
	virtual void OnRequestOpenNewWindow(const char *url, uint32_t url_len,
					    bool user_gesture);
	virtual void OnSelectPopupOpen(const vector<string> *items, void *request_handle);
	virtual void OnSelectPopupClose();
	virtual void OnGeolocationRequest(void *request_handle);

	virtual int32_t CreateDocumentWidget(void *ime_session, WebCoreView *view,
					     WebCoreViewport *viewport,
					     WebRenderClient **out_render_client,
					     WebCoreFrame *frame,
					     WebSurfaceHost **out_surface_host);
	virtual int32_t DestroyDocumentWidget();
	virtual int32_t GetDocumentSize();

	/* Non-virtual public operations. */
	bool Finalize();
	int32_t LoadUrl(const Url &url);
	int32_t GoBack();
	int32_t GoForward();
	int32_t Reload();
	Url GetUrl() const;
	int32_t SetJavaScriptEnabled(bool enabled);
	void SetObserver(WebControllerObserver *observer);
	bool IsWakeup() const;

	bool m_busy;
	bool m_closed;
	Url m_url;
	common::String m_title;
	bool m_can_go_back;
	bool m_can_go_forward;
	WebCore *m_web_core;
	vector<JsExternalObject *> m_external_objects;
	void *m_active_plugin;
	void *m_plugin_user;
	int32_t m_load_status;
	bool m_load_failed;
	uint64_t m_request_tick;
	DocumentWidget *m_document;
	WbcBrowserComponent *m_browser_component;
	WebControllerObserver *m_observer;
	WbcBrowserManager *m_manager;
};

}
}

#endif
