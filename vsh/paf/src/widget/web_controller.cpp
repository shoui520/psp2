/*
 * paf::ui::WebController implementation for 2014 PAF WebView/MapView.
 */
#include <widget/web_controller.h>
#include <widget/w_document_widget.h>
#include <widget/wutil.h>
#include <common/char_encoding.h>
#include <rtc/rtc.h>
#include <std/string.h>

extern "C" {
int32_t SceShellUtil_96F2D37A(const char *uri, int32_t flags);
}

namespace paf {
namespace ui {

static const char s_app_contents_prefix[] = "file:///app0/etc/contents";
static const char s_app_contents_dir[] = "/app0/etc/contents/";
static const char s_app_contents_root[] = "/app0/etc/contents";
static const char s_about_html_path[] = "file:///app0/etc/contents/webcore/about.html";
static const char s_about_html_suffix[] = "/webcore/about.html";

WebController::WebController()
	: m_busy(false), m_closed(false), m_url(), m_title(), m_can_go_back(false),
	  m_can_go_forward(false), m_web_core(NULL), m_external_objects(),
	  m_active_plugin(NULL), m_plugin_user(NULL), m_load_status(0),
	  m_load_failed(false), m_request_tick(0), m_document(NULL),
	  m_browser_component(NULL), m_observer(NULL), m_manager(NULL)
{
	m_manager = static_cast<WbcBrowserManager *>(wutil::GetWebViewListener());
	if (!m_closed && !m_busy && m_browser_component == NULL) {
		m_busy = true;
		if (m_manager != NULL)
			m_manager->RequestBrowserComponent(this);
	}
	rtc::GetCurrentTick(&m_request_tick);
	if (m_web_core != NULL)
		m_web_core->NotifyControllerWakeup(this);
}

WebController::~WebController()
{
}

int32_t WebController::Wakeup()
{
	int32_t ret = (int32_t)0x80AF0001;

	if (m_closed) {
		ret = 0;
	} else if (m_busy) {
		ret = 0;
	} else if (m_browser_component != NULL) {
		ret = 0;
	} else {
		m_busy = true;
		if (m_manager != NULL)
			ret = m_manager->RequestBrowserComponent(this);
	}
	rtc::GetCurrentTick(&m_request_tick);
	if (m_web_core != NULL)
		m_web_core->NotifyControllerWakeup(this);
	return ret;
}

bool WebController::Finalize()
{
	if (m_observer != NULL)
		m_observer = NULL;
	if (m_web_core != NULL) {
		m_web_core->RemoveController(this);
		m_web_core = NULL;
	}
	if (m_document != NULL)
		m_document = NULL;
	Sleep(false);
	m_closed = true;
	return true;
}

int32_t WebController::LoadUrl(const Url &url)
{
	string target_url = url.GetUrlString();

	if (target_url.length() == 0)
		return (int32_t)0x80AF0001;

	if (url.GetSchemeString() == string("file")) {
		string path = url.GetPathString();
		string prefix(s_app_contents_prefix);

		target_url = prefix + path;
	}

	if (url.GetSchemeString() == string("about")) {
		if (url.GetUrlString() == string("about:"))
			target_url = string(s_about_html_path);
	}

	m_url = url;
	m_title = common::String("");

	WebCore *core = WebCore::GetInstance();

	if (m_manager != NULL && core != NULL && m_manager->IsRestartNeeded()) {
		if (core->GetSuspendFlag() && m_manager->CanRestartNow()) {
			core->Shutdown(true);
			m_manager->RequestRestart(true);
		}
		core->SetSuspendFlag(false);
	}

	Wakeup();
	if (m_browser_component != NULL)
		return m_browser_component->LoadUrl(target_url.c_str(), target_url.length());

	OnLoadProgressChanged(10);
	return 0;
}

int32_t WebController::GoBack()
{
	if (!m_can_go_back)
		return -1;
	if (m_browser_component != NULL)
		return m_browser_component->GoBack();
	return (int32_t)0x80AF0001;
}

int32_t WebController::GoForward()
{
	if (!m_can_go_forward)
		return -1;
	if (m_browser_component != NULL)
		return m_browser_component->GoForward();
	return (int32_t)0x80AF0001;
}

int32_t WebController::Reload()
{
	Wakeup();
	if (m_browser_component != NULL)
		return m_browser_component->Reload();
	return 0;
}

Url WebController::GetUrl() const
{
	return m_url;
}

int32_t WebController::SetJavaScriptEnabled(bool enabled)
{
	if (m_browser_component != NULL)
		return m_browser_component->SetJavaScriptEnabled(enabled);
	return (int32_t)0x80AF0001;
}

void WebController::SetObserver(WebControllerObserver *observer)
{
	m_observer = observer;
}

int32_t WebController::OnBrowserComponentCreated(WbcBrowserComponent *component)
{
	int32_t ret = (int32_t)0x80AF0001;

	if (component != NULL) {
		m_browser_component = component;
		ret = component->SetPageClient(this);
		if (ret == 0) {
			int32_t vp_width = (int32_t)GetViewportSize().extract_x();
			int32_t vp_height = (int32_t)GetViewportSize().extract_y();

			m_browser_component->SetViewportSize(vp_width, vp_height);
			m_browser_component->SetExtensionClient(this);
		}
	}

	m_busy = false;
	LoadUrl(m_url);
	if (m_observer != NULL)
		m_observer->OnBrowserComponentReady();
	if (m_manager != NULL)
		m_manager->NotifyBrowserComponentCreated(this);
	return ret;
}

bool WebController::IsWakeup() const
{
	return m_busy || m_browser_component != NULL;
}

int32_t WebController::Sleep(bool notify_error)
{
	int32_t ret = 0;

	if (m_busy) {
		if (m_manager != NULL)
			m_manager->CancelRequestBrowserComponent(this);
		m_busy = false;
	} else if (m_browser_component != NULL) {
		m_browser_component->Close();
		m_browser_component = NULL;
		ret = 1;
		if (notify_error) {
			m_load_status = 3;
			if (m_observer != NULL) {
				common::String empty_msg("");

				m_observer->OnLoadError(18, empty_msg);
			}
			OnFinishLoad();
		}
	} else {
		ret = 1;
	}

	m_can_go_back = false;
	m_can_go_forward = false;
	m_request_tick = 0;
	return ret;
}

void WebController::OnResume()
{
}

int32_t WebController::OnSelectPopupClosed(int32_t selected_index)
{
	(void)selected_index;
	return 0;
}

int32_t WebController::OnFileChooserResult(const common::String &path,
					   const common::String &mime)
{
	(void)path;
	(void)mime;
	return 0;
}

math::v4 WebController::GetViewportSize()
{
	return math::v4(1.0f, 1.0f, 1.0f, 1.0f);
}

void WebController::SetMaxSurfaceSize(math::v2 size)
{
	(void)size;
}

int32_t WebController::GetImeTypeAndEnterLabel(int32_t *out_input_type)
{
	(void)out_input_type;
	return 0;
}

int32_t WebController::OnMultiLineImeCommit(const common::String &text)
{
	(void)text;
	return 0;
}

void WebController::OnStartLoad()
{
	if (!m_load_failed) {
		m_load_status = 0;
		if (m_observer != NULL)
			m_observer->OnStartLoad();
		OnLoadProgressChanged(30);
	}
}

void WebController::OnFinishLoad()
{
	if (m_load_failed)
		m_load_failed = false;
	OnLoadProgressChanged(100);
	if (m_observer != NULL) {
		m_observer->OnLoadResult(m_load_status);
		m_observer->OnFinishLoad();
	}
}

void WebController::OnOpenNewWindow(const char *url, uint32_t url_len,
				    const char *target, uint32_t target_len)
{
	if (m_observer != NULL) {
		string url_str;
		string target_str;

		if (url != NULL && url_len != 0 && *url != '\0')
			url_str.assign(url, url_len);
		Url parsed_url(url_str);

		if (target != NULL && target_len != 0 && *target != '\0')
			target_str.assign(target, target_len);
		common::String target_name(target_str);

		m_observer->OnOpenNewWindowWithTarget(parsed_url, target_name);
	}
}

void WebController::OnExternalUri(const char *uri, uint32_t uri_len)
{
	string uri_str;

	if (uri != NULL && uri_len != 0 && *uri != '\0')
		uri_str.assign(uri, uri_len);
	SceShellUtil_96F2D37A(uri_str.c_str(), 0);
}

void WebController::OnLocationChanged(const char *url, uint32_t url_len)
{
	Url prev_url(m_url);
	string url_str;

	if (url != NULL && url_len != 0 && *url != '\0')
		url_str.assign(url, url_len);
	m_url = url_str;

	if (m_url.GetSchemeString() == string("file")) {
		string path = m_url.GetPathString();

		if (path.find(s_app_contents_dir) == 0) {
			string root(s_app_contents_root);
			string rel_path = m_url.GetPathString().substr(root.length(), string::npos);

			if (rel_path == string(s_about_html_suffix)) {
				m_url = string("about:");
			} else {
				string file_prefix("file://");
				string rewritten = file_prefix + rel_path;

				m_url = rewritten;
			}
		}
	}

	if (m_observer != NULL) {
		m_observer->OnLocationChanged(m_url);
		if (m_observer != NULL) {
			string prev_base = prev_url.GetBaseUrlString();
			string curr_base = m_url.GetBaseUrlString();

			sce_paf_strcmp(prev_base.c_str(), curr_base.c_str());
		}
	}
}

void WebController::OnTitleChanged(const char *title, uint32_t title_len)
{
	string utf8_title;
	wstring utf16_title;

	if (title != NULL && title_len != 0 && *title != '\0')
		utf8_title.assign(title, title_len);
	common::Utf8ToUtf16(utf8_title, &utf16_title, 0);
	m_title = common::String(utf16_title);

	if (m_observer != NULL)
		m_observer->OnTitleChanged(m_title);
}

void WebController::OnHistoryChanged(bool can_go_back, bool can_go_forward)
{
	m_can_go_back = can_go_back;
	m_can_go_forward = can_go_forward;
	if (m_observer != NULL)
		m_observer->OnHistoryChanged(can_go_back, can_go_forward);
}

void WebController::OnStatusMessage(const char *text, uint32_t text_len,
				    const char *source, uint32_t source_len)
{
	wstring utf16_text;
	wstring utf16_source;
	string text_sub = string(text).substr(0, text_len);
	string source_sub = string(source).substr(0, source_len);

	common::Utf8ToUtf16(text_sub, &utf16_text, 0);
	common::Utf8ToUtf16(source_sub, &utf16_source, 0);
}

void WebController::OnSecurityStatusChanged(int32_t status)
{
	if (m_observer != NULL)
		m_observer->OnSecurityStatusChanged(status);
}

void WebController::OnJavaScriptAlert(const char *message, uint32_t len, void *request_handle)
{
	string utf8_msg;
	wstring utf16_msg;

	if (message != NULL && len != 0 && *message != '\0')
		utf8_msg.assign(message, len);
	common::Utf8ToUtf16(utf8_msg, &utf16_msg, 0);

	common::String alert_msg(utf16_msg);

	if (m_observer != NULL)
		m_observer->OnJavaScriptAlert(alert_msg);
	m_browser_component->ReplyJavaScriptAlert(request_handle);
}

void WebController::OnJavaScriptConfirm(const char *message, uint32_t len, void *request_handle)
{
	string utf8_msg;
	wstring utf16_msg;

	if (message != NULL && len != 0 && *message != '\0')
		utf8_msg.assign(message, len);
	common::Utf8ToUtf16(utf8_msg, &utf16_msg, 0);

	common::String confirm_msg(utf16_msg);

	if (m_observer != NULL)
		m_observer->OnJavaScriptConfirm(confirm_msg);
	m_browser_component->ReplyJavaScriptConfirm(false, request_handle);
}

void WebController::OnJavaScriptPrompt(const char *message, uint32_t msg_len,
				       const char *default_text, uint32_t default_len,
				       void *request_handle)
{
	(void)message;
	(void)msg_len;
	m_browser_component->ReplyJavaScriptPrompt(true, default_text, default_len, request_handle);
}

void WebController::OnNavigationPolicy(int32_t policy_type, void *request_handle)
{
	bool allow = true;

	if (policy_type > 3)
		allow = (policy_type > 5);
	if (m_browser_component != NULL)
		m_browser_component->ReplyNavigationPolicy(allow, request_handle);
	if (m_observer != NULL && !allow)
		m_observer->OnWindowCloseRequested();
}

void WebController::OnImeOpen(const char *text, uint32_t length, int32_t cursor_pos,
			      int32_t input_type, int32_t enter_label, bool multiline,
			      int32_t rect_x, int32_t rect_y, int32_t rect_w,
			      int32_t rect_h, void *request_handle)
{
	(void)text;
	(void)length;
	(void)cursor_pos;
	(void)input_type;
	(void)enter_label;
	(void)multiline;
	(void)rect_x;
	(void)rect_y;
	(void)rect_w;
	(void)rect_h;
	(void)request_handle;
}

void WebController::OnImeClose(bool force)
{
	(void)force;
}

void WebController::OnImeCursorChanged(int32_t cursor_pos)
{
	(void)cursor_pos;
}

void WebController::OnLoadProgressChanged(int32_t progress)
{
	if (m_observer != NULL)
		m_observer->OnProgressChanged(progress);
}

void WebController::OnContentBegin()
{
}

void WebController::OnContentEnd()
{
}

void WebController::OnRequestConfirmPlugin(int32_t mode, void *request_handle)
{
	(void)mode;
	if (m_browser_component != NULL)
		m_browser_component->ReplyConfirmPlugin(true, request_handle);
}

void WebController::OnRequestBadCertificate(int32_t error_mask, void *request_handle)
{
	(void)error_mask;
	if (m_browser_component != NULL)
		m_browser_component->ReplyBadCertificate(false, request_handle);
}

void WebController::OnRequestBadCertificateDetail(int32_t error_mask, const char *issuer,
						  uint32_t issuer_len, const char *subject,
						  uint32_t subject_len, double valid_from,
						  double valid_to, const char *host,
						  uint32_t host_len, void *request_handle)
{
	(void)error_mask;
	(void)issuer;
	(void)issuer_len;
	(void)subject;
	(void)subject_len;
	(void)valid_from;
	(void)valid_to;
	(void)host;
	(void)host_len;
	m_browser_component->ReplyBadCertificateDetail(false, request_handle);
}

void WebController::OnRequestNavigationWarning(int32_t reason, void *request_handle)
{
	(void)reason;
	m_browser_component->ReplyNavigationWarning(true, request_handle);
}

void WebController::OnRequestBasicAuthentication(const char *host, uint32_t host_len,
						 const char *realm, uint32_t realm_len,
						 const char *user, uint32_t user_len,
						 const char *pass, uint32_t pass_len,
						 bool is_proxy, void *request_handle)
{
	(void)host;
	(void)host_len;
	(void)realm;
	(void)realm_len;
	(void)is_proxy;
	m_browser_component->ReplyBasicAuthentication(false, user, user_len, pass, pass_len,
						      false, request_handle);
}

void WebController::OnAuthenticationCancelled()
{
}

void WebController::OnLoadError(int32_t error_code, void *request_handle)
{
	m_load_status = 1;
	m_load_failed = true;
	if (m_browser_component != NULL)
		m_browser_component->ReplyLoadError(true, request_handle);
	if (m_observer != NULL) {
		common::String empty_msg("");

		m_observer->OnLoadError(error_code, empty_msg);
	}
}

void WebController::OnCreateFaviconSurface(int32_t width, int32_t height, int32_t format,
					   const void *pixels)
{
	(void)width;
	(void)height;
	(void)format;
	(void)pixels;
}

void WebController::OnRequestFileChooser(int32_t mode, const char *accept, void *request_handle)
{
	(void)mode;
	(void)accept;
	m_browser_component->ReplyFileChooser("", 0, "", 0, request_handle);
}

void WebController::OnRequestCustomScheme(const char *scheme, uint32_t scheme_len,
					  const char *url, uint32_t url_len,
					  void *request_handle)
{
	(void)scheme;
	(void)scheme_len;
	(void)url;
	(void)url_len;
	m_browser_component->ReplyCustomScheme(request_handle);
}

void WebController::OnFocusInputStart()
{
}

void WebController::OnFocusInputEnd()
{
}

void WebController::OnHitTestResult(int32_t pos_x, int32_t pos_y, int32_t width,
				    int32_t height, const char *link_url, uint32_t link_len,
				    const char *image_url, uint32_t image_len,
				    int32_t hit_type)
{
	(void)pos_x;
	(void)pos_y;
	(void)width;
	(void)height;
	(void)link_url;
	(void)link_len;
	(void)image_url;
	(void)image_len;
	(void)hit_type;
}

void WebController::OnRequestOpenNewWindow(const char *url, uint32_t url_len, bool user_gesture)
{
	(void)user_gesture;
	if (m_observer != NULL) {
		string url_str;

		if (url != NULL && url_len != 0 && *url != '\0')
			url_str.assign(url, url_len);
		Url target_url(url_str);

		m_observer->OnOpenNewWindow(target_url);
	}
}

void WebController::OnSelectPopupOpen(const vector<string> *items, void *request_handle)
{
	(void)items;
	(void)request_handle;
}

void WebController::OnSelectPopupClose()
{
}

void WebController::OnGeolocationRequest(void *request_handle)
{
	m_browser_component->ReplyGeolocation(false, false, request_handle);
}

void WebController::GetExternalObjectList(WbcExternalObjectList *out_list)
{
	for (uint32_t idx = 0; idx < m_external_objects.size(); idx++) {
		wstring obj_name = m_external_objects[idx]->m_name;
		uint32_t buf_len = obj_name.length() + 1;
		WbcExternalObjectInfo *desc = new WbcExternalObjectInfo();

		if (desc != NULL) {
			desc->m_index = idx;
			desc->m_name = NULL;
			desc->m_name_length = buf_len;
			desc->m_user = NULL;
			desc->m_name = new wchar_t[buf_len];
			sce_paf_wcsncpy(desc->m_name, obj_name.c_str(), buf_len);
		}
		out_list->m_items[out_list->m_count] = desc;
		out_list->m_count++;
	}
}

bool WebController::HasExternalObjectProperty(uint32_t object_id, const wchar_t *name,
					      bool is_method)
{
	if (object_id >= m_external_objects.size())
		return false;

	wstring prop_name(name);

	return m_external_objects[object_id]->HasProperty(prop_name, is_method);
}

void WebController::GetExternalObjectMembers(uint32_t object_id)
{
	if (object_id < m_external_objects.size()) {
		vector<wstring> prop_names(m_external_objects[object_id]->m_property_names);
		vector<wstring> method_names(m_external_objects[object_id]->m_method_names);
		(void)prop_names;
		(void)method_names;
	}
}

void WebController::GetExternalObjectProperty(uint32_t object_id, uint32_t property_id,
					      WbcJsValue *out_value)
{
	if (object_id >= m_external_objects.size())
		return;

	vector<wstring> prop_names(m_external_objects[object_id]->m_property_names);

	if (property_id < prop_names.size()) {
		JsVariant *variant = new JsVariant(wstring(L""), 0);

		m_external_objects[object_id]->GetProperty(prop_names[property_id], variant);

		wstring value_copy(variant->m_value);
		int32_t wire_type = 0;

		switch (variant->m_type) {
		case 0:
			wire_type = 0;
			break;
		case 1:
			wire_type = 1;
			break;
		case 2:
			wire_type = 2;
			break;
		case 3:
			wire_type = 3;
			break;
		case 4:
			wire_type = 4;
			break;
		case 5:
			wire_type = 5;
			break;
		default:
			break;
		}
		out_value->m_type = wire_type;
		(void)value_copy;
	}
}

void WebController::SetExternalObjectProperty(uint32_t object_id, uint32_t property_id,
					      WbcJsValue *value)
{
	if (object_id >= m_external_objects.size())
		return;

	vector<wstring> prop_names(m_external_objects[object_id]->m_property_names);

	if (property_id < prop_names.size()) {
		wchar_t *text_buf = new wchar_t[2048];
		int32_t variant_type = 0;

		switch (value->m_type) {
		case 1:
			variant_type = 1;
			break;
		case 2:
			variant_type = 2;
			break;
		case 3:
			variant_type = 3;
			break;
		case 4:
			variant_type = 4;
			break;
		case 5:
			variant_type = 5;
			break;
		default:
			break;
		}

		JsVariant *variant = new JsVariant(wstring(text_buf), variant_type);

		m_external_objects[object_id]->SetProperty(prop_names[property_id], variant);
		delete[] text_buf;
	}
}

void WebController::OnExtensionEvent()
{
}

int32_t WebController::CreateDocumentWidget(void *ime_session, WebCoreView *view,
					    WebCoreViewport *viewport,
					    WebRenderClient **out_render_client,
					    WebCoreFrame *frame,
					    WebSurfaceHost **out_surface_host)
{
	(void)ime_session;
	DocumentWidget *doc = new DocumentWidget(NULL, NULL);

	if (doc != NULL) {
		int32_t ret = doc->Initialize(view, viewport, out_render_client, frame,
					      out_surface_host);

		m_document = doc;
		return ret;
	}
	return (int32_t)0x80AF0001;
}

int32_t WebController::DestroyDocumentWidget()
{
	return 0;
}

int32_t WebController::GetDocumentSize()
{
	return 0;
}

}
}
