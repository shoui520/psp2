/*
 * paf::ui::WebViewBase ("webview_base"): 2014 PAF scroll-view web container hosting
 * DocumentWidget, WebController, WebViewLoadingPlane, IME sessions, and dialog callbacks.
 */
#ifndef PAF_WIDGET_W_WEBVIEW_BASE_H
#define PAF_WIDGET_W_WEBVIEW_BASE_H

#include <stdint.h>
#include <std/list>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <common/string_util.h>
#include <datetime/datetime.h>
#include <inputdevice/inputlistener.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <widget/core/uiinputlistener.h>
#include <widget/w_document_widget.h>
#include <widget/w_plane.h>
#include <widget/w_scroll_view_base.h>
#include <widget/web_controller.h>
#include <widget/wutil.h>

namespace paf {
namespace ui {

/* SSL certificate metadata forwarded to MessageDialogListener::OpenBadCertificateDetailDialog. */
struct WebSslCertInfo {
	WebSslCertInfo()
		: m_issuer(), m_subject(), m_valid_from(0, 0, 0, 0, 0, 0, 0),
		  m_valid_to(0, 0, 0, 0, 0, 0, 0), m_host()
	{
	}

	common::String m_issuer;
	common::String m_subject;
	datetime::DateTime m_valid_from;
	datetime::DateTime m_valid_to;
	common::String m_host;
};

/* Select popup item / group / info structures passed to WbcBrowserManager::OpenSelectPopup. */
class WebViewSelectItem {
public:
	WebViewSelectItem() : m_text(), m_selectable(false), m_enabled(false)
	{
	}

	virtual ~WebViewSelectItem()
	{
	}

	string m_text;
	bool m_selectable;
	bool m_enabled;
};

}
}

PAF_VECTOR_INLINE_DTOR(paf::ui::WebViewSelectItem)
PAF_VECTOR_NOINLINE_PUSH_BACK(paf::ui::WebViewSelectItem, paf::allocator<paf::ui::WebViewSelectItem>)

namespace paf {
namespace ui {

class WebViewSelectGroup {
public:
	WebViewSelectGroup() : m_items(), m_label(), m_disabled(false)
	{
	}

	virtual ~WebViewSelectGroup()
	{
	}

	vector<WebViewSelectItem> m_items;
	string m_label;
	bool m_disabled;
};

}
}

PAF_VECTOR_INLINE_DTOR(paf::ui::WebViewSelectGroup)
PAF_VECTOR_NOINLINE_PUSH_BACK(paf::ui::WebViewSelectGroup,
			      paf::allocator<paf::ui::WebViewSelectGroup>)

namespace paf {
namespace ui {

class WebViewSelectInfo {
public:
	WebViewSelectInfo() : m_groups(), m_title(), m_multiple(false)
	{
	}

	virtual ~WebViewSelectInfo()
	{
	}

	vector<WebViewSelectGroup> m_groups;
	string m_title;
	bool m_multiple;
};

/* Abstract WebCore IME session handle owned by WebViewBase while a document exists. */
class WebImeSession {
public:
	virtual ~WebImeSession()
	{
	}

	virtual void Release() = 0;
	virtual int32_t SetComposition(const char *text, uint32_t text_len,
				       const int32_t *clause_types, uint32_t clause_type_count,
				       const int32_t *clause_ranges, uint32_t clause_range_count,
				       int32_t cursor_pos, void *request_handle) = 0;
	virtual int32_t Commit(void *request_handle) = 0;
	virtual int32_t Cancel(void *request_handle) = 0;
	virtual int32_t Close(void *request_handle) = 0;
};

/* Abstract web edit listener retrieved from wutil::GetWebEditListener(). */
class WebEditListener {
public:
	virtual ~WebEditListener()
	{
	}

	virtual int32_t BeginEdit(WebViewBase *view) = 0;
	virtual int32_t EndEdit(bool force, WebViewBase *view) = 0;
	virtual int32_t Update(WebViewBase *view) = 0;
};

/* Queued asynchronous WebCore permission request (e.g. geolocation). */
class WebPendingRequest {
public:
	WebPendingRequest(int32_t kind, void *request_handle)
		: m_kind(kind), m_request_handle(request_handle)
	{
	}

	virtual ~WebPendingRequest()
	{
	}

	int32_t m_kind;
	void *m_request_handle;
};

/* Analog input gesture listener registered by WebViewBase with inputdevice::AddInputListener. */
class WebViewAnalogListener : public UIInputGestureListener {
public:
	WebViewAnalogListener(Environment *env);
	virtual ~WebViewAnalogListener();
	virtual void OnUpdate(const inputdevice::Data *data);

	float m_deadzone;
	float m_sensitivity;
	int32_t m_active_port;
};

/* Highlight / loading overlay plane drawn over tapped links and images inside WebViewBase. */
class WebViewLoadingPlane : public Plane {
public:
	WebViewLoadingPlane(Widget *parent);
	virtual ~WebViewLoadingPlane()
	{
	}

	void Show();
	void Flash();
	void Hide();
	void SetStyle(int32_t style);

	int32_t m_state;
	int32_t m_delay;
	int32_t m_style;
};

class WebViewBase
	: public ScrollView,
	  public WebController,
	  public DocumentEventObserver {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	WebViewBase(Widget *parent, CreateParam *param);
	virtual ~WebViewBase();

	/* Widget / ScrollViewBase overrides. */
	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual int32_t HandleGestureEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();
	virtual void Update();
	virtual void UpdatePost();
	virtual void OnViewLayoutUpdated(bool is_dirty);

	/* WebController virtual overrides. */
	virtual int32_t Wakeup();
	virtual void OnResume();
	virtual int32_t Sleep(bool notify_error);
	virtual math::v4 GetViewportSize();
	virtual void SetMaxSurfaceSize(math::v2 size);
	virtual int32_t OnBrowserComponentCreated(WbcBrowserComponent *component);

	/* WebDialogCallback overrides. */
	virtual void OnNormalDialogResult(int32_t result);
	virtual void OnJsDialogResult(int32_t result);
	virtual void OnNavigationWarningResult(int32_t result);
	virtual void OnConfirmPluginResult(int32_t result);
	virtual void OnBadCertificateResult(int32_t result, int32_t show_detail);
	virtual void OnBasicAuthResult(int32_t result);

	/* WebViewBase IME / popup / file-chooser callbacks. */
	virtual int32_t GetImeTypeAndEnterLabel(int32_t *out_input_type);
	virtual int32_t OnMultiLineImeCommit(const common::String &text);
	virtual int32_t OnSelectPopupClosed(int32_t selected_index);
	virtual int32_t OnFileChooserResult(const common::String &path,
					    const common::String &mime);

	/* WebCorePageClient & WebCoreDocumentHost overrides. */
	virtual void OnStartLoad();
	virtual void OnFinishLoad();
	virtual void OnLocationChanged(const char *url, uint32_t url_len);
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
	virtual void OnImeCursorChanged(int32_t cursor_pos);
	virtual void OnImeClose(bool force);
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
	virtual void OnCreateFaviconSurface(int32_t width, int32_t height, int32_t format,
					    const void *pixels);
	virtual void OnRequestFileChooser(int32_t mode, const char *accept,
					  void *request_handle);
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

	/* DocumentEventObserver override. */
	virtual void OnDocumentEvent(int32_t event_type, math::v2 touch_pos);

	/* Helper methods. */
	DocumentWidget *FindDocumentWidget();
	void EndTextEdit(bool send_enter);
	void UpdateScrollOffset();
	void OnImeAreaActivated();
	int32_t GetImeCursorPos() const;
	common::String GetImeText() const;
	void SendKeyLeft();
	void SendKeyRight();
	void SendKeyBackspace();
	int32_t CommitImeText(const wstring &text);
	int32_t UpdateImePreedit(int32_t caret_start, int32_t caret_end, const wstring &text);
	int32_t ReplaceImeText(const wstring &text);
	void RequestHitTest(const math::v2 *pos, int32_t hit_type);
	void HandleDebugMagic(const string &command);
	void SetImeCursorMode(bool enabled);

	math::v2 m_top_offset;
	math::v4 m_saved_margin;
	math::v2 m_zoom_scale;
	math::v2 m_scroll_origin;
	WebEditListener *m_web_edit_listener;
	MessageDialogListener *m_message_dialog_listener;
	void *m_ime_request;
	void *m_dialog_request;
	common::String m_ime_text;
	uint32_t m_edit_state;
	MessageDialogParam m_dialog_param;
	WebSslCertInfo m_ssl_cert_info;
	bool m_debug_enabled;
	int32_t m_ime_type;
	int32_t m_ime_enter_label;
	bool m_ime_multiline;
	math::v2 m_pending_max_surface_size;
	WebImeSession *m_ime_session;
	common::SharedPtr<inputdevice::InputListener> m_analog_listener;
	bool m_ime_open;
	int32_t m_geolocation_policy;
	int32_t m_ime_cursor_pos;
	WebViewLoadingPlane *m_loading_plane;
	bool m_hit_pending;
	math::v2 m_hit_center;
	math::v2 m_hit_size;
	math::v2 m_tap_pos;
	bool m_gesture_active;
	list<WebPendingRequest> m_pending_requests;
};

}
}

#endif
