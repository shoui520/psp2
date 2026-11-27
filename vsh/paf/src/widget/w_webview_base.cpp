/*
 * paf::ui::WebViewBase ("webview_base") implementation for 2014 PAF.
 */
#include <widget/w_webview_base.h>
#include <common/char_encoding.h>
#include <graphics/gutil.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <image/image_types.h>
#include <inputdevice/inputdevice.h>
#include <math/rect.h>
#include <rtc.h>
#include <std/math.h>
#include <std/string.h>
#include <timer/timer.h>
#include <url/url.h>
#include <versatileparam/idparam.h>
#include <widget/core/event.h>
#include <widget/web_renderer.h>

extern "C" {
int32_t sceRegMgrSrvGetRegion(void);
}

namespace paf {
namespace ui {

static uint8_t s_analog_state_front[192];
static uint8_t s_analog_state_rear[192];

const char WebViewBase::m_widget_type[] = "webview_base";

const char *WebViewBase::TypeName()
{
	return m_widget_type;
}

bool WebViewBase::IsInherit(const char *type) const
{
	if (type == m_widget_type)
		return true;
	return ScrollView::IsInherit(type);
}

char *WebViewBase::GetTypeName() const
{
	return const_cast<char *>(m_widget_type);
}

WebViewAnalogListener::WebViewAnalogListener(Environment *env)
	: UIInputGestureListener(env), m_deadzone(256.0f), m_sensitivity(80.0f),
	  m_active_port(0)
{
	sce_paf_memset(s_analog_state_front, 0, sizeof(s_analog_state_front));
	sce_paf_memset(s_analog_state_rear, 0, sizeof(s_analog_state_rear));
	m_type = inputdevice::DEVICE_TYPE_PAD;
	for (int32_t i = 0; i < BIND_NUM; ++i)
		m_binded_widget[i] = NULL;
}

WebViewAnalogListener::~WebViewAnalogListener()
{
}

void WebViewAnalogListener::OnUpdate(const inputdevice::Data *data)
{
	(void)data;
}

WebViewLoadingPlane::WebViewLoadingPlane(Widget *parent)
	: Plane(parent, NULL), m_state(0), m_delay(0), m_style(0)
{
	m_transition_complete = false;
	DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(this, NULL, 2, 0.0f, NULL);
	SetPos(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ANIM_MOVE, 0, 0.0f, NULL);
	SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ANIM_SIZE, 0, 0.0f, NULL);
	SetColor(math::v4(0.38f, 0.4f, 0.4f, 0.41f), NULL, ANIM_COLOR, 0, 0.0f, NULL);
}

void WebViewLoadingPlane::Flash()
{
	if (m_state != 0) {
		m_state = 3;
		return;
	}

	if (m_delay == 0 || (m_delay == 1 && m_style == 0)) {
		if (!m_transition_complete) {
			m_transition_complete = true;
			DoTransition_core(0.0f, true);
		}
		ScePafWidget_FF5BC957(this, NULL, 2, 0.0f, NULL);
		Timer *timer = new Timer(500.0f, 0, 0, -1, NULL);

		m_transition_complete = false;
		DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(this, timer, 2, 0.0f, NULL);
	}
}

void WebViewLoadingPlane::Show()
{
	if (m_state != 0) {
		m_state = 2;
		return;
	}

	float fade_ms = 0.0f;

	if (m_delay == 0) {
		fade_ms = 30.0f;
	} else if (m_delay == 1) {
		if (m_style == 0)
			fade_ms = 30.0f;
		else if (m_style == 1)
			fade_ms = 200.0f;
		else
			return;
	} else {
		return;
	}

	Timer *timer = new Timer(fade_ms, 0, 0, -1, NULL);

	if (!m_transition_complete) {
		m_transition_complete = true;
		DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(this, timer, 2, 0.0f, NULL);
}

void WebViewLoadingPlane::Hide()
{
	Timer *timer = new Timer(500.0f, 0, 0, -1, NULL);

	m_transition_complete = false;
	DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(this, timer, 2, 0.0f, NULL);

	if (m_state == 2 || m_state == 3)
		m_state = 1;
}

void WebViewLoadingPlane::SetStyle(int32_t style)
{
	if (style == 0) {
		SetColor(math::v4(0.38f, 0.4f, 0.4f, 0.41f), NULL, ANIM_COLOR, 0, 0.0f, NULL);
	} else if (style == 1) {
		SetColor(math::v4(0.1f, 0.1f, 0.8f, 0.41f), NULL, ANIM_COLOR, 0, 0.0f, NULL);
	}
	m_style = style;
}

WebViewBase::WebViewBase(Widget *parent, CreateParam *param)
	: ScrollView(parent, param),
	  WebController(),
	  m_top_offset(0.0f, 0.0f),
	  m_saved_margin(0.0f, 0.0f, 0.0f, 0.0f),
	  m_zoom_scale(1.0f, 1.0f),
	  m_scroll_origin(0.0f, 0.0f),
	  m_web_edit_listener(static_cast<WebEditListener *>(wutil::GetWebEditListener())),
	  m_message_dialog_listener(wutil::GetMessageDialogListener()),
	  m_ime_request(NULL),
	  m_dialog_request(NULL),
	  m_ime_text(),
	  m_edit_state(0),
	  m_dialog_param(),
	  m_ssl_cert_info(),
	  m_debug_enabled(false),
	  m_ime_type(0),
	  m_ime_enter_label(0),
	  m_ime_multiline(false),
	  m_pending_max_surface_size(0.0f, 0.0f),
	  m_ime_session(NULL),
	  m_analog_listener(),
	  m_ime_open(false),
	  m_geolocation_policy(-1),
	  m_ime_cursor_pos(0),
	  m_loading_plane(NULL),
	  m_hit_pending(false),
	  m_hit_center(0.0f, 0.0f),
	  m_hit_size(0.0f, 0.0f),
	  m_tap_pos(0.0f, 0.0f),
	  m_gesture_active(false),
	  m_pending_requests()
{
	m_state = (m_state & ~0xFU) | 1U;
	m_pending_requests.clear();
	m_loading_plane = new WebViewLoadingPlane(this);

	common::SharedPtr<inputdevice::InputListener> listener(new WebViewAnalogListener(m_env));

	m_analog_listener = listener;
	inputdevice::AddInputListener(m_analog_listener);

	if (sceRegMgrSrvGetRegion() != 2)
		m_geolocation_policy = 3;
}

WebViewBase::~WebViewBase()
{
}

DocumentWidget *WebViewBase::FindDocumentWidget()
{
	Widget *internal = GetInternalWidget();
	IDParam doc_id(string("document_widget"));
	Widget *found = internal->FindChild(doc_id, 2);

	if (found != NULL)
		return static_cast<DocumentWidget *>(found);
	return NULL;
}

int32_t WebViewBase::Wakeup()
{
	int32_t ret = WebController::Wakeup();
	DocumentWidget *doc = FindDocumentWidget();

	if (doc != NULL)
		doc->ResumeSurface();
	return ret;
}

void WebViewBase::OnResume()
{
	DocumentWidget *doc = FindDocumentWidget();

	if (doc != NULL)
		doc->SuspendSurface();
}

int32_t WebViewBase::Sleep(bool notify_error)
{
	int32_t ret = WebController::Sleep(notify_error);

	if (ret != 0)
		DestroyDocumentWidget();
	return ret;
}

math::v4 WebViewBase::GetViewportSize()
{
	return *GetSize(GET_CURRENT);
}

void WebViewBase::SetMaxSurfaceSize(math::v2 size)
{
	DocumentWidget *doc = FindDocumentWidget();

	if (doc != NULL) {
		m_pending_max_surface_size = math::v2(0.0f, 0.0f);
		doc->SetMaxSurfaceSize(size);
	} else {
		m_pending_max_surface_size = size;
	}
}

int32_t WebViewBase::OnBrowserComponentCreated(WbcBrowserComponent *component)
{
	int32_t ret = WebController::OnBrowserComponentCreated(component);
	math::v4 cur_size = *GetSize(GET_CURRENT);

	component->SetViewportSize((int32_t)cur_size.extract_x(),
				   (int32_t)cur_size.extract_y());
	return ret;
}

void WebViewBase::OnNormalDialogResult(int32_t result)
{
	int32_t reason = m_dialog_param.values[3];

	if (reason == 2) {
		if (m_browser_component != NULL)
			m_browser_component->ReplyConfirmPlugin(true, m_dialog_request);
	} else if (reason == 1) {
		m_geolocation_policy = (result != 0) ? 1 : 2;
		if (m_browser_component != NULL)
			m_browser_component->ReplyGeolocation(result != 0, false,
							      m_dialog_request);

		for (list<WebPendingRequest>::iterator it = m_pending_requests.begin();
		     it != m_pending_requests.end();) {
			if (it->m_kind == 1) {
				if (m_browser_component != NULL)
					m_browser_component->ReplyGeolocation(
						result != 0, false, it->m_request_handle);
				it = m_pending_requests.erase(it);
			} else {
				++it;
			}
		}
	}
	m_dialog_request = NULL;
}

void WebViewBase::OnJsDialogResult(int32_t result)
{
	if (m_message_dialog_listener != NULL && m_message_dialog_listener->Method5())
		SetJavaScriptEnabled(false);

	string prompt_utf8;

	switch (m_dialog_param.values[1]) {
	case 0:
		if (m_browser_component != NULL)
			m_browser_component->ReplyJavaScriptAlert(m_dialog_request);
		break;
	case 1:
		if (m_browser_component != NULL)
			m_browser_component->ReplyJavaScriptConfirm(result != 0,
								    m_dialog_request);
		break;
	case 2: {
		const wstring *input = m_message_dialog_listener->Method7(0);
		string converted;

		common::Utf16ToUtf8(*input, &converted);
		prompt_utf8 = converted;
		if (m_browser_component != NULL)
			m_browser_component->ReplyJavaScriptPrompt(result != 0,
								   prompt_utf8.c_str(),
								   prompt_utf8.length(),
								   m_dialog_request);
		break;
	}
	case 3:
		if (result == 1 && m_observer != NULL) {
			Url target_url(m_dialog_param.button.GetString());

			m_observer->OnOpenNewWindow(target_url);
		}
		break;
	case 4:
		if (m_observer != NULL)
			m_observer->OnWindowCloseRequested();
		break;
	case 5:
		if (m_browser_component != NULL)
			m_browser_component->ReplyConfirmPlugin(result != 0, m_dialog_request);
		break;
	default:
		break;
	}

	m_dialog_request = NULL;
}

void WebViewBase::OnNavigationWarningResult(int32_t result)
{
	if (m_browser_component != NULL)
		m_browser_component->ReplyNavigationWarning(result != 0, m_dialog_request);
	m_dialog_request = NULL;
}

void WebViewBase::OnConfirmPluginResult(int32_t result)
{
	if (m_browser_component != NULL)
		m_browser_component->ReplyPluginAction(result != 0, m_dialog_request);
	m_dialog_request = NULL;
}

void WebViewBase::OnBadCertificateResult(int32_t result, int32_t show_detail)
{
	if (show_detail != 0) {
		if (m_message_dialog_listener != NULL) {
			m_dialog_param.values[0] = 3;
			m_message_dialog_listener->OpenBadCertificateDetailDialog(
				&m_dialog_param, &m_ssl_cert_info);
		}
	} else {
		if (m_browser_component != NULL)
			m_browser_component->ReplyBadCertificateDetail(result != 0,
								       m_dialog_request);
		m_dialog_request = NULL;
	}
}

void WebViewBase::OnBasicAuthResult(int32_t result)
{
	string user_utf8("");
	string pass_utf8("");

	if (m_message_dialog_listener != NULL) {
		const wstring *user_w = m_message_dialog_listener->Method7(0);
		string converted_user;

		common::Utf16ToUtf8(*user_w, &converted_user);
		user_utf8 = converted_user;

		const wstring *pass_w = m_message_dialog_listener->Method7(1);
		string converted_pass;

		common::Utf16ToUtf8(*pass_w, &converted_pass);
		pass_utf8 = converted_pass;
	}

	if (m_browser_component != NULL)
		m_browser_component->ReplyBasicAuthentication(result != 0,
							      user_utf8.c_str(),
							      user_utf8.length(),
							      pass_utf8.c_str(),
							      pass_utf8.length(),
							      result != 0,
							      m_dialog_request);
	m_dialog_request = NULL;
}

void WebViewBase::EndTextEdit(bool send_enter)
{
	if (((m_edit_state & 2U) != 0 || (m_edit_state & 4U) != 0) &&
	    m_web_edit_listener != NULL) {
		DocumentWidget *doc = FindDocumentWidget();

		if (doc != NULL && send_enter && doc->m_renderer != NULL)
			doc->m_renderer->HandleKeyEvent(13);

		SetImePadding(false);

		Widget *internal = GetInternalWidget();
		IDParam area_id(string("textarea_widget"));
		Widget *area = internal->FindChild(area_id, 2);

		if (area != NULL)
			area->DestroyWidget();

		m_web_edit_listener->EndEdit(false, this);
		if (m_ime_session != NULL)
			m_ime_session->Close(m_ime_request);

		m_edit_state = (m_edit_state & ~0xFU) | 1U;
		m_ime_text = common::String("");
		m_ime_request = NULL;
	}

	if (m_observer != NULL) {
		m_ime_open = false;
		m_observer->OnImeClosed(1, 0);
	}
}

void WebViewBase::DestroyWidget()
{
	inputdevice::DelInputListener(m_analog_listener);
	m_analog_listener = common::SharedPtr<inputdevice::InputListener>();

	if (m_loading_plane != NULL) {
		m_loading_plane->DestroyWidget();
		m_loading_plane = NULL;
	}

	EndTextEdit(false);
	DestroyDocumentWidget();
	WebController::Finalize();
	ScrollView::DestroyWidget();
}

void WebViewBase::Update()
{
	if (m_web_edit_listener != NULL && (m_edit_state & 2U) != 0) {
		m_edit_state = (m_edit_state & ~0xFU) | 4U;
		m_web_edit_listener->BeginEdit(this);
	}
	Widget::Update();
}

void WebViewBase::UpdateScrollOffset()
{
	math::v2 top_offset(0.0f, 0.0f);
	DocumentWidget *doc = FindDocumentWidget();

	if (doc != NULL) {
		math::v4 size_diff = *doc->GetSize(GET_CURRENT) - *GetSize(GET_CURRENT);
		math::v2 base_offset(
			(m_view_size.extract_x() + size_diff.extract_x() * 0.5f),
			-(m_view_size.extract_y() + size_diff.extract_y() * 0.5f));
		math::v2 focus = GetViewFocus();
		float offset_y = -focus.extract_y() - base_offset.extract_y();

		top_offset = math::v2(0.0f, offset_y);
	}

	if (top_offset.extract_y() < 0.0f)
		top_offset = math::v2(top_offset.extract_x(), 0.0f);
	else if (top_offset.extract_y() > m_view_size.extract_y())
		top_offset = math::v2(top_offset.extract_x(), m_view_size.extract_y());

	m_top_offset = math::v2(m_top_offset.extract_x(), top_offset.extract_y());

	if (m_saved_margin == math::v4(0.0f, 0.0f, 0.0f, 0.0f))
		m_saved_margin = m_padding;

	math::v4 adjusted_margin(m_saved_margin.extract_x() - top_offset.extract_y(),
				 m_saved_margin.extract_y(),
				 m_saved_margin.extract_z(),
				 m_saved_margin.extract_w());

	SetVerticalBarMargin(adjusted_margin);
	if (m_observer != NULL)
		m_observer->OnScrollOffsetChanged(top_offset);
}

void WebViewBase::OnPartsLayoutUpdated(bool is_dirty)
{
	if (is_dirty && GetInternalWidget() != NULL) {
		UpdateScrollOffset();
		DocumentWidget *doc = FindDocumentWidget();

		if (doc != NULL)
			doc->SyncScrollPosition(m_velocity);
	}
	ScrollViewBase::OnPartsLayoutUpdated(is_dirty);
}

int32_t WebViewBase::OnDraw(graph::DrawInfo &info)
{
	if (m_web_edit_listener != NULL && (m_edit_state & 2U) != 0) {
		m_edit_state = (m_edit_state & ~0xFU) | 4U;
		m_web_edit_listener->Update(this);
	}
	return Widget::OnDraw(info);
}

void WebViewBase::OnImeAreaActivated()
{
	if ((m_edit_state & 4U) != 0) {
		SetImePadding(true);

		math::rect ime_rect;

		SetImeRect(ime_rect);
	}
}

void WebViewBase::SetImeCursorMode(bool enabled)
{
	(void)enabled;
}

int32_t WebViewBase::GetImeCursorPos() const
{
	return m_ime_cursor_pos;
}

common::String WebViewBase::GetImeText() const
{
	return m_ime_text;
}

void WebViewBase::SendKeyLeft()
{
	DocumentWidget *doc = FindDocumentWidget();

	if (doc != NULL && doc->m_renderer != NULL)
		doc->m_renderer->HandleKeyEvent(37);
}

void WebViewBase::SendKeyRight()
{
	DocumentWidget *doc = FindDocumentWidget();

	if (doc != NULL && doc->m_renderer != NULL)
		doc->m_renderer->HandleKeyEvent(39);
}

void WebViewBase::SendKeyBackspace()
{
	DocumentWidget *doc = FindDocumentWidget();

	if (doc != NULL && doc->m_renderer != NULL)
		doc->m_renderer->HandleKeyEvent(8);
}

int32_t WebViewBase::CommitImeText(const wstring &text)
{
	string utf8_text;
	int32_t ret = 0;

	common::Utf16ToUtf8(text, &utf8_text);
	if (text.length() == 0 && m_ime_session != NULL)
		m_ime_session->SetComposition(utf8_text.c_str(), 0, NULL, 0, NULL, 0, 0,
					      m_ime_request);
	if (m_ime_session != NULL)
		ret = m_ime_session->Commit(m_ime_request);
	return ret;
}

int32_t WebViewBase::UpdateImePreedit(int32_t caret_start, int32_t caret_end,
				      const wstring &text)
{
	string utf8_text;
	int32_t ret = 0;

	common::Utf16ToUtf8(text, &utf8_text);

	int32_t clause_types[2];
	int32_t clause_ranges[2];

	clause_types[0] = 1;
	clause_types[1] = 2;
	clause_ranges[0] = caret_start;
	clause_ranges[1] = caret_end - caret_start;

	if (utf8_text.length() != 0 && m_ime_session != NULL) {
		ret = m_ime_session->SetComposition(utf8_text.c_str(), utf8_text.length(),
						    clause_types, 2, clause_ranges, 2,
						    -1, m_ime_request);
	}
	return ret;
}

int32_t WebViewBase::ReplaceImeText(const wstring &text)
{
	wstring copied(text);
	string utf8_text;
	int32_t ret = 0;

	common::Utf16ToUtf8(copied, &utf8_text);
	if (m_ime_session == NULL ||
	    (ret = m_ime_session->SetComposition(utf8_text.c_str(), utf8_text.length(),
						 NULL, 0, NULL, 0, -1, m_ime_request)) == 0) {
		if (m_ime_session != NULL)
			ret = m_ime_session->Commit(m_ime_request);
	}
	return ret;
}

int32_t WebViewBase::GetImeTypeAndEnterLabel(int32_t *out_input_type)
{
	*out_input_type = m_ime_type;
	return m_ime_enter_label;
}

int32_t WebViewBase::OnMultiLineImeCommit(const common::String &text)
{
	if (m_ime_session != NULL) {
		string utf8 = text.GetString();

		m_ime_session->SetComposition(utf8.c_str(), utf8.length(),
					      NULL, 0, NULL, 0, 0, m_ime_request);
		m_ime_session->Commit(m_ime_request);
		m_ime_session->Close(m_ime_request);
	}
	m_ime_request = NULL;
	return 0;
}

int32_t WebViewBase::OnSelectPopupClosed(int32_t selected_index)
{
	if (m_browser_component != NULL)
		m_browser_component->ReplySelectPopup(selected_index, m_dialog_request);
	m_dialog_request = NULL;
	return 0;
}

int32_t WebViewBase::OnFileChooserResult(const common::String &path,
					 const common::String &mime)
{
	if (m_browser_component != NULL) {
		string path_str = path.GetString();
		string mime_str = mime.GetString();

		m_browser_component->ReplyFileChooser(path_str.c_str(), path_str.length(),
						      mime_str.c_str(), mime_str.length(),
						      m_dialog_request);
	}
	m_dialog_request = NULL;
	return 0;
}

void WebViewBase::RequestHitTest(const math::v2 *pos, int32_t hit_type)
{
	if (m_browser_component == NULL || m_hit_pending)
		return;

	if (m_loading_plane != NULL)
		m_loading_plane->m_state = 1;

	m_tap_pos = *pos - GetViewFocus();

	math::v2 doc_pts[2];

	doc_pts[0] = math::v2(0.0f, 0.0f);
	doc_pts[1] = math::v2(0.0f, 0.0f);

	DocumentWidget *doc = FindDocumentWidget();

	if (doc != NULL)
		doc->WidgetRectToDocRect(*pos, *pos, doc_pts);

	m_hit_pending = true;
	m_browser_component->HitTest((int32_t)doc_pts[0].extract_x(),
				     (int32_t)doc_pts[0].extract_y(), hit_type);
}

int32_t WebViewBase::HandleTouchEvent(Event *e)
{
	uint32_t touch_action = (e->m_type >> 20) & 0xFU;

	if (touch_action == 0)
		m_edit_state = (m_edit_state & ~0xF00U) | 0x100U;
	else if (touch_action == 1)
		m_edit_state |= 0xF00U;
	return ScrollViewBase::HandleTouchEvent(e);
}

int32_t WebViewBase::HandleGestureEvent(Event *e)
{
	uint32_t ev_type = e->m_type;

	if (((ev_type >> 20) & 0xFU) == 0 && ((ev_type >> 8) & 0xFFU) == 12) {
		DocumentWidget *doc = FindDocumentWidget();

		if (doc != NULL)
			doc->HandleGestureEvent(e);
	}
	return ScrollView::HandleGestureEvent(e);
}

void WebViewBase::HandleDebugMagic(const string &command)
{
	if (command.find("requestBadCertificate") == 0) {
		datetime::DateTime sample_date;

		sample_date = datetime::DateTime::UTC();
		sample_date = datetime::DateTime::UTC();
		OnRequestBadCertificateDetail(2, "aaaaaaaaaaaa", 12,
					      "bbbbbbbbbbbb", 12,
					      0.0, 0.0,
					      "cccccccccccc", 12,
					      m_dialog_request);
		return;
	}
	if (command.find("requestBasicAuthentication") == 0) {
		OnRequestBasicAuthentication("https://http://www.yahoo.co.jp/", 31,
					     "Hdjeotmeo39038djkr930sETFGHTYRWwer", 34,
					     "ninonino", 8,
					     "0617", 4,
					     false, m_dialog_request);
		return;
	}
	if (command.find("requestConfirmPlugin") == 0) {
		OnGeolocationRequest(m_dialog_request);
		return;
	}
	if (command.find("DialogOpen") == 0) {
		m_dialog_param.Clear();
		m_dialog_param.owner = static_cast<WebDialogCallback *>(this);
		m_dialog_param.values[0] = 0;

		if (command.find("Type_CANCEL") != string::npos) {
			m_dialog_param.title = common::String(L"Nomal Dialog Type_Cancel");
			m_dialog_param.values[1] = 1;
		} else if (command.find("Type_OK_CANCEL") != string::npos) {
			m_dialog_param.title = common::String(L"Nomal Dialog Type_OK_Cancel");
			m_dialog_param.values[1] = 2;
		} else if (command.find("Type_OK") != string::npos) {
			m_dialog_param.title = common::String(L"Nomal Dialog Type_OK");
			m_dialog_param.values[1] = 0;
		} else if (command.find("Type_YES_NO") != string::npos) {
			m_dialog_param.title = common::String(L"Nomal Dialog Type_Yes_No");
			m_dialog_param.values[1] = 3;
		} else {
			return;
		}

		if (m_message_dialog_listener != NULL)
			m_message_dialog_listener->OpenNormalDialog(&m_dialog_param);
		return;
	}
	if (command.find("requestNavigationWarning") == 0) {
		OnRequestNavigationWarning(10, m_dialog_request);
	}
}

void WebViewBase::OnStartLoad()
{
	if (m_geolocation_policy != 3)
		m_geolocation_policy = -1;
	WebController::OnStartLoad();
}

void WebViewBase::OnFinishLoad()
{
	WebController::OnFinishLoad();
}

void WebViewBase::OnLocationChanged(const char *url, uint32_t url_len)
{
	WebController::OnLocationChanged(url, url_len);

	common::String cur_url(GetUrl().GetUrlString());
	bool has_url = (cur_url.GetString().length() != 0);
	DocumentWidget *doc = FindDocumentWidget();

	if (doc != NULL)
		doc->SetBlitEnabled(has_url);
}

void WebViewBase::OnJavaScriptAlert(const char *message, uint32_t len, void *request_handle)
{
	string msg_str;

	if (len != 0 && message != NULL && *message != '\0')
		msg_str.assign(message, len);

	if (msg_str.find("___web_debug_magic__") == 0) {
		if (m_browser_component != NULL)
			m_browser_component->ReplyJavaScriptAlert(request_handle);
		m_dialog_request = request_handle;
		HandleDebugMagic(msg_str.substr(20, string::npos));
		return;
	}

	bool opened = false;

	if (m_message_dialog_listener != NULL) {
		common::String msg_text(msg_str);

		m_dialog_request = request_handle;
		m_dialog_param.values[0] = 1;
		m_dialog_param.values[1] = 0;
		m_dialog_param.title = msg_text;
		m_dialog_param.owner = static_cast<WebDialogCallback *>(this);
		opened = (m_message_dialog_listener->OpenJsDialog(&m_dialog_param) >= 0);
	}

	if (!opened && m_browser_component != NULL) {
		m_browser_component->ReplyJavaScriptAlert(request_handle);
		m_dialog_request = NULL;
	}
}

void WebViewBase::OnJavaScriptConfirm(const char *message, uint32_t len,
				      void *request_handle)
{
	m_dialog_param.Clear();
	m_dialog_param.owner = static_cast<WebDialogCallback *>(this);

	bool opened = false;

	if (m_message_dialog_listener != NULL) {
		string msg_str;

		if (len != 0 && message != NULL && *message != '\0')
			msg_str.assign(message, len);

		common::String msg_text(msg_str);

		m_dialog_request = request_handle;
		m_dialog_param.values[0] = 1;
		m_dialog_param.values[1] = 1;
		m_dialog_param.title = msg_text;
		opened = (m_message_dialog_listener->OpenJsDialog(&m_dialog_param) >= 0);
	}

	if (!opened && m_browser_component != NULL) {
		m_browser_component->ReplyJavaScriptConfirm(false, request_handle);
		m_dialog_request = NULL;
	}
}

void WebViewBase::OnJavaScriptPrompt(const char *message, uint32_t msg_len,
				     const char *default_text, uint32_t default_len,
				     void *request_handle)
{
	bool opened = false;

	if (m_message_dialog_listener != NULL) {
		string msg_str;
		string def_str;

		if (msg_len != 0 && message != NULL && *message != '\0')
			msg_str.assign(message, msg_len);
		if (default_len != 0 && default_text != NULL && *default_text != '\0')
			def_str.assign(default_text, default_len);

		common::String msg_text(msg_str);
		common::String def_text(def_str);

		m_dialog_request = request_handle;
		m_dialog_param.values[0] = 1;
		m_dialog_param.values[1] = 2;
		m_dialog_param.title = msg_text;
		m_dialog_param.owner = static_cast<WebDialogCallback *>(this);
		m_dialog_param.message = def_text;
		opened = (m_message_dialog_listener->OpenJsDialog(&m_dialog_param) >= 0);
	}

	if (!opened && m_browser_component != NULL) {
		m_browser_component->ReplyJavaScriptPrompt(true, default_text, default_len,
							   request_handle);
		m_dialog_request = NULL;
	}
}

void WebViewBase::OnNavigationPolicy(int32_t policy_type, void *request_handle)
{
	bool allow = true;

	if (policy_type > 3)
		allow = (policy_type > 5);

	if (m_browser_component != NULL)
		m_browser_component->ReplyNavigationPolicy(allow, request_handle);

	if (m_observer != NULL && !allow)
		m_observer->OnWindowCloseRequested();
}

void WebViewBase::OnImeOpen(const char *text, uint32_t length, int32_t cursor_pos,
			    int32_t input_type, int32_t enter_label, bool multiline,
			    int32_t rect_x, int32_t rect_y, int32_t rect_w, int32_t rect_h,
			    void *request_handle)
{
	(void)cursor_pos;
	m_ime_type = input_type;
	m_ime_enter_label = enter_label;
	m_ime_multiline = multiline;
	m_ime_request = request_handle;

	string utf8_str;

	if (length != 0 && text != NULL && *text != '\0')
		utf8_str.assign(text, length);

	wstring utf16_str;

	common::Utf8ToUtf16(utf8_str, &utf16_str, 0);
	m_ime_text = common::String(utf16_str);

	if (m_ime_multiline) {
		if (m_manager != NULL)
			m_manager->OpenMultiLineIme(this);
		return;
	}

	Widget *internal = GetInternalWidget();
	IDParam area_id(string("textarea_widget"));
	Widget *existing = internal->FindChild(area_id, 2);

	if (existing != NULL)
		existing->DestroyWidget();

	Widget *textarea = new Widget(GetInternalWidget(), NULL);
	DocumentWidget *doc = FindDocumentWidget();

	if (doc != NULL && textarea != NULL) {
		math::v2 pts[2];

		pts[0] = math::v2(0.0f, 0.0f);
		pts[1] = math::v2(0.0f, 0.0f);
		doc->DocRectToWidgetRect(math::v2((float)rect_x, (float)rect_y),
					 math::v2((float)(rect_x + rect_w),
						  (float)(rect_y + rect_h)),
					 pts);

		math::v2 box_size(__builtin_fabsf(pts[0].extract_x()) - __builtin_fabsf(pts[1].extract_x()),
				  __builtin_fabsf(pts[0].extract_y()) - __builtin_fabsf(pts[1].extract_y()));
		math::v4 cur_size = *textarea->GetSize(GET_CURRENT);
		math::v4 new_size(box_size.extract_x(), box_size.extract_y(),
				  cur_size.extract_z(), cur_size.extract_w());
		math::v2 box_pos(pts[0].extract_x() + box_size.extract_x() * 0.5f,
				 pts[1].extract_y() + box_size.extract_y() * 0.5f);

		textarea->m_name.SetID(string("textarea_widget"));
		textarea->SetSize(new_size, NULL, ANIM_SIZE, 0, 0.0f, NULL);

		math::v4 cur_pos = textarea->GetPos(GET_CURRENT, 0,
						    math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		math::v4 new_pos(box_pos.extract_x(), box_pos.extract_y(),
				 cur_pos.extract_z(), cur_pos.extract_w());

		textarea->SetPos(new_pos, NULL, ANIM_MOVE, 0, 0.0f, NULL);
	}

	if (m_observer != NULL) {
		m_ime_open = true;
		m_observer->OnImeOpened(1, 0);
	}

	if (m_web_edit_listener != NULL)
		m_edit_state = (m_edit_state & ~0xFU) | 2U;
}

void WebViewBase::OnImeCursorChanged(int32_t cursor_pos)
{
	m_ime_cursor_pos = cursor_pos;
}

void WebViewBase::OnImeClose(bool force)
{
	if (m_ime_multiline) {
		m_ime_request = NULL;
		if (m_manager != NULL)
			m_manager->CloseMultiLineIme();
		return;
	}

	Widget *internal = GetInternalWidget();
	IDParam area_id(string("textarea_widget"));
	Widget *area = internal->FindChild(area_id, 2);

	if (area != NULL) {
		if (force) {
			if (m_web_edit_listener != NULL)
				m_web_edit_listener->EndEdit(true, this);
		} else {
			EndTextEdit(false);
		}
	}
}

void WebViewBase::OnRequestConfirmPlugin(int32_t mode, void *request_handle)
{
	(void)mode;
	m_dialog_param.Clear();
	m_dialog_request = request_handle;

	if (m_message_dialog_listener != NULL) {
		m_dialog_param.Clear();
		m_dialog_param.owner = static_cast<WebDialogCallback *>(this);
		m_dialog_param.values[0] = 0;
		m_dialog_param.values[1] = 0;
		m_dialog_param.values[3] = 2;
		if (m_message_dialog_listener->OpenNormalDialog(&m_dialog_param) >= 0)
			return;
	}

	if (m_browser_component != NULL)
		m_browser_component->ReplyConfirmPlugin(true, request_handle);
	m_dialog_request = NULL;
}

static bool MapSslErrorCode(int32_t error_mask, int32_t *out_code)
{
	switch (error_mask) {
	case 1:
		*out_code = 4;
		return true;
	case 2:
		*out_code = 0;
		return true;
	case 4:
		*out_code = 1;
		return true;
	case 8:
	case 16:
		*out_code = 2;
		return true;
	case 32:
		*out_code = 3;
		return true;
	default:
		return false;
	}
}

void WebViewBase::OnRequestBadCertificate(int32_t error_mask, void *request_handle)
{
	m_dialog_param.Clear();
	m_dialog_request = request_handle;

	int32_t mapped_code = 0;

	if (m_message_dialog_listener != NULL && MapSslErrorCode(error_mask, &mapped_code)) {
		m_dialog_param.values[1] = mapped_code;
		m_dialog_param.owner = static_cast<WebDialogCallback *>(this);
		m_dialog_param.values[0] = 2;
		if (m_message_dialog_listener->OpenBadCertificateDialog(&m_dialog_param) >= 0)
			return;
	}

	if (m_browser_component != NULL)
		m_browser_component->ReplyBadCertificate(false, request_handle);
	m_dialog_request = NULL;
}

static void SetDateTimeFromEpoch(double epoch_sec, datetime::DateTime *out_dt)
{
	SceDateTime rtc_time;
	SceUInt64 secs = (SceUInt64)epoch_sec;

	sceRtcSetTime64_t(&rtc_time, secs);
	out_dt->m_year = rtc_time.year;
	out_dt->m_month = rtc_time.month;
	out_dt->m_day = rtc_time.day;
	out_dt->m_hour = rtc_time.hour;
	out_dt->m_minute = rtc_time.minute;
	out_dt->m_second = rtc_time.second;
	out_dt->m_microsecond = rtc_time.microsecond;
}

void WebViewBase::OnRequestBadCertificateDetail(int32_t error_mask, const char *issuer,
						uint32_t issuer_len, const char *subject,
						uint32_t subject_len, double valid_from,
						double valid_to, const char *host,
						uint32_t host_len, void *request_handle)
{
	m_dialog_param.Clear();
	m_dialog_request = request_handle;

	int32_t mapped_code = 0;

	if (m_message_dialog_listener != NULL && MapSslErrorCode(error_mask, &mapped_code)) {
		m_dialog_param.values[1] = mapped_code;

		string issuer_str(issuer);
		m_ssl_cert_info.m_issuer = common::String(issuer_str.substr(0, issuer_len));

		string subject_str(subject);
		m_ssl_cert_info.m_subject = common::String(subject_str.substr(0, subject_len));

		SetDateTimeFromEpoch(valid_from, &m_ssl_cert_info.m_valid_from);
		SetDateTimeFromEpoch(valid_to, &m_ssl_cert_info.m_valid_to);

		string host_str(host);
		m_ssl_cert_info.m_host = common::String(host_str.substr(0, host_len));

		m_dialog_param.owner = static_cast<WebDialogCallback *>(this);
		m_dialog_param.values[0] = 2;
		if (m_message_dialog_listener->OpenBadCertificateDialog(&m_dialog_param) >= 0)
			return;
	}

	if (m_browser_component != NULL)
		m_browser_component->ReplyBadCertificate(false, request_handle);
	m_dialog_request = NULL;
}

void WebViewBase::OnRequestNavigationWarning(int32_t reason, void *request_handle)
{
	m_dialog_param.Clear();

	if (m_message_dialog_listener != NULL) {
		int32_t mapped = -1;

		if (reason == 9)
			mapped = 0;
		else if (reason == 10)
			mapped = 1;

		if (mapped >= 0) {
			m_dialog_request = request_handle;
			m_dialog_param.values[1] = mapped;
			m_dialog_param.values[0] = 4;
			m_dialog_param.owner = static_cast<WebDialogCallback *>(this);
			if (m_message_dialog_listener->OpenNavigationWarningDialog(
				    &m_dialog_param) >= 0) {
				return;
			}
		}
	}

	if (m_browser_component != NULL)
		m_browser_component->ReplyNavigationWarning(true, request_handle);
	m_dialog_request = NULL;
}

void WebViewBase::OnRequestBasicAuthentication(const char *host, uint32_t host_len,
					       const char *realm, uint32_t realm_len,
					       const char *user, uint32_t user_len,
					       const char *pass, uint32_t pass_len,
					       bool is_proxy, void *request_handle)
{
	(void)is_proxy;
	m_dialog_param.Clear();

	bool opened = false;

	if (m_message_dialog_listener != NULL) {
		m_dialog_request = request_handle;
		m_dialog_param.Clear();
		m_dialog_param.owner = static_cast<WebDialogCallback *>(this);
		m_dialog_param.values[0] = 6;

		string host_str;
		string realm_str;
		string user_str;
		string pass_str;

		if (host_len != 0 && host != NULL && *host != '\0')
			host_str.assign(host, host_len);
		if (realm_len != 0 && realm != NULL && *realm != '\0')
			realm_str.assign(realm, realm_len);
		if (user_len != 0 && user != NULL && *user != '\0')
			user_str.assign(user, user_len);
		if (pass_len != 0 && pass != NULL && *pass != '\0')
			pass_str.assign(pass, pass_len);

		MessageDialogTexts texts(host_str, realm_str, user_str, pass_str);

		opened = (m_message_dialog_listener->OpenBasicAuthDialog(
				  &m_dialog_param, texts) >= 0);
	}

	if (!opened) {
		if (m_browser_component != NULL)
			m_browser_component->ReplyBasicAuthentication(
				false, user, user_len, pass, pass_len, false, request_handle);
		m_dialog_request = NULL;
	}
}

void WebViewBase::OnCreateFaviconSurface(int32_t width, int32_t height, int32_t format,
					 const void *pixels)
{
	(void)format;
	graph::SurfacePool *pool = gutil::GetDefaultSurfacePool();
	intrusive_ptr<graph::Surface> surf(new graph::Surface(pool, width, height,
							      ImageMode_RGBA8888,
							      ImageOrder_Linear,
							      1, 1, true));

	if (surf.get() != NULL)
		surf->Copy(0, pixels, ImageOrder_Linear, 0, -1);
}

void WebViewBase::OnRequestFileChooser(int32_t mode, const char *accept,
				       void *request_handle)
{
	(void)mode;
	(void)accept;
	if (m_manager != NULL) {
		m_dialog_request = request_handle;
		m_manager->OpenFileChooser(this);
	} else if (m_browser_component != NULL) {
		m_browser_component->ReplyFileChooser("", 0, "", 0, request_handle);
	}
}

void WebViewBase::OnHitTestResult(int32_t pos_x, int32_t pos_y, int32_t width,
				  int32_t height, const char *link_url, uint32_t link_len,
				  const char *image_url, uint32_t image_len,
				  int32_t hit_type)
{
	math::v2 top_left((float)pos_x, (float)pos_y);
	math::v2 bottom_right((float)(pos_x + width), (float)(pos_y + height));
	math::v2 pts[2];

	pts[0] = math::v2(0.0f, 0.0f);
	pts[1] = math::v2(0.0f, 0.0f);

	DocumentWidget *doc = FindDocumentWidget();

	if (doc != NULL)
		doc->DocRectToWidgetRect(top_left, bottom_right, pts);

	m_hit_center = (pts[0] + pts[1]) * 0.5f - GetViewFocus();
	m_hit_pending = false;
	m_hit_size = math::v2(__builtin_fabsf(pts[1].extract_x() - pts[0].extract_x()),
			      __builtin_fabsf(pts[1].extract_y() - pts[0].extract_y()));

	if (m_loading_plane != NULL) {
		bool show_plane = false;

		if (m_loading_plane->m_delay == 0) {
			show_plane = (link_len != 0);
		} else if (m_loading_plane->m_delay == 1) {
			if (image_len != 0)
				m_loading_plane->SetStyle(1);
			if (link_len != 0)
				m_loading_plane->SetStyle(0);
			show_plane = (link_len != 0 || image_len != 0);
		}

		if (show_plane) {
			math::v4 old_pos = m_loading_plane->GetPos(
				GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			math::v4 new_pos(m_hit_center.extract_x(), m_hit_center.extract_y(),
					 old_pos.extract_z(), old_pos.extract_w());

			m_loading_plane->SetPos(new_pos, NULL, ANIM_MOVE, 0, 0.0f, NULL);

			math::v4 old_size = *m_loading_plane->GetSize(GET_CURRENT);
			math::v4 new_size(m_hit_size.extract_x() + 15.0f,
					  m_hit_size.extract_y() + 15.0f,
					  old_size.extract_z(), old_size.extract_w());

			m_loading_plane->SetSize(new_size, NULL, ANIM_SIZE, 0, 0.0f, NULL);

			int32_t pending_state = m_loading_plane->m_state;

			m_loading_plane->m_state = 0;
			if (pending_state == 2)
				m_loading_plane->Show();
			else if (pending_state == 3)
				m_loading_plane->Flash();
		}
	}

	if (hit_type == 4) {
		if ((link_len | image_len) != 0 && m_observer != NULL) {
			math::rect hit_rect(m_hit_center, m_hit_size);
			ContextMenuInfo menu_info(hit_rect, m_tap_pos);

			if (link_url != NULL) {
				string url_str;

				if (link_len != 0 && *link_url != '\0')
					url_str.assign(link_url, link_len);
				menu_info.AddItem(1, url_str);
			}
			if (image_url != NULL) {
				string img_str;

				if (image_len != 0 && *image_url != '\0')
					img_str.assign(image_url, image_len);
				menu_info.AddItem(2, img_str);
			}
			m_observer->OnContextMenu(menu_info);
		}
		return;
	}

	if (hit_type == 2 && width * height != 0) {
		math::v4 self_size = *GetSize(GET_CURRENT);
		float scale_x = self_size.extract_x() / m_hit_size.extract_x();
		float scale_y = self_size.extract_y() / m_hit_size.extract_y();
		float factor = (scale_x < scale_y) ? scale_x : scale_y;

		if (factor < 1.0f)
			factor = 1.0f;

		float reset_factor = 1.0f / m_zoom_scale.extract_x();

		if ((link_len | image_len) == 0 || (factor > 0.998f && factor < 1.002f))
			factor = reset_factor;

		if (factor != 1.0f) {
			DocumentWidget *target_doc = FindDocumentWidget();

			if (target_doc != NULL) {
				m_zoom_scale = math::v2(m_zoom_scale.extract_x() * factor,
							m_zoom_scale.extract_y() * factor);
				target_doc->SetZoomState(0);

				math::v4 doc_size = *target_doc->GetSize(GET_CURRENT);
				math::v4 scaled_size(doc_size.extract_x() * factor,
						     doc_size.extract_y() * factor,
						     doc_size.extract_z(),
						     doc_size.extract_w());

				target_doc->SetSize(scaled_size, NULL, ANIM_SIZE, 0, 0.0f,
						    NULL);

				math::v2 zoomed_pts[2];

				zoomed_pts[0] = math::v2(0.0f, 0.0f);
				zoomed_pts[1] = math::v2(0.0f, 0.0f);
				target_doc->DocRectToWidgetRect(top_left, bottom_right,
								zoomed_pts);

				math::v2 new_focus =
					(zoomed_pts[0] + zoomed_pts[1] - m_view_size) * 0.5f;

				SetViewFocus(new_focus, NULL, true, true);
			}

			Widget *internal = GetInternalWidget();
			IDParam area_id(string("textarea_widget"));
			Widget *area = internal->FindChild(area_id, 2);

			if (area != NULL) {
				float cur_zoom = m_zoom_scale.extract_x();
				math::v4 area_size = *area->GetSize(GET_CURRENT);
				math::v4 new_area_size(area_size.extract_x() * cur_zoom,
						       area_size.extract_y() * cur_zoom,
						       area_size.extract_z(),
						       area_size.extract_w());

				area->SetSize(new_area_size, NULL, ANIM_SIZE, 0, 0.0f, NULL);

				math::v4 area_pos = area->GetPos(
					GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
				math::v4 new_area_pos(area_pos.extract_x() * cur_zoom,
						      area_pos.extract_y() * cur_zoom,
						      cur_zoom, cur_zoom);

				area->SetPos(new_area_pos, NULL, ANIM_MOVE, 0, 0.0f, NULL);
			}
		}
	}
}

void WebViewBase::OnRequestOpenNewWindow(const char *url, uint32_t url_len, bool user_gesture)
{
	int32_t popup_policy = (m_web_core != NULL) ? m_web_core->GetPopupPolicy() : 0;

	if (popup_policy == 1)
		return;

	bool opened = false;

	if (user_gesture && popup_policy == 2 && m_observer != NULL &&
	    m_message_dialog_listener != NULL) {
		string url_str;

		if (url_len != 0 && url != NULL && *url != '\0')
			url_str.assign(url, url_len);

		common::String url_text(url_str);

		m_dialog_param.values[0] = 1;
		m_dialog_request = NULL;
		m_dialog_param.values[1] = 3;
		m_dialog_param.button = url_text;
		m_dialog_param.owner = static_cast<WebDialogCallback *>(this);
		opened = (m_message_dialog_listener->OpenJsDialog(&m_dialog_param) >= 0);
	}

	if (!opened && m_observer != NULL) {
		string url_str;

		if (url_len != 0 && url != NULL && *url != '\0')
			url_str.assign(url, url_len);

		Url target_url(url_str);

		m_observer->OnOpenNewWindow(target_url);
	}
}

void WebViewBase::OnSelectPopupOpen(const vector<string> *items, void *request_handle)
{
	if (items->size() == 0) {
		OnSelectPopupClosed(0);
		return;
	}

	if (m_observer == NULL)
		return;

	OnImeClose(false);
	m_dialog_request = request_handle;

	WebViewSelectInfo info;
	WebViewSelectGroup group;

	for (size_t i = 0; i < items->size(); ++i) {
		WebViewSelectItem item;

		item.m_text = (*items)[i];
		group.m_items.push_back(item);
	}
	info.m_groups.push_back(group);

	if (m_manager != NULL)
		m_manager->OpenSelectPopup(this, &info);
}

void WebViewBase::OnSelectPopupClose()
{
	if (m_observer != NULL)
		m_observer->OnSelectPopupClosed();
	m_dialog_request = NULL;
}

void WebViewBase::OnGeolocationRequest(void *request_handle)
{
	bool allow = true;

	if (m_geolocation_policy == -1) {
		m_dialog_param.Clear();
		m_dialog_param.owner = static_cast<WebDialogCallback *>(this);
		if (m_message_dialog_listener != NULL) {
			m_dialog_request = request_handle;
			m_dialog_param.values[0] = 0;
			m_dialog_param.values[3] = 1;
			m_dialog_param.values[1] = 3;
			bool opened = (m_message_dialog_listener->OpenNormalDialog(
					       &m_dialog_param) >= 0);

			m_geolocation_policy = 0;
			if (opened)
				return;
		}
	} else if (m_geolocation_policy == 0) {
		m_pending_requests.push_back(WebPendingRequest(1, request_handle));
		return;
	} else {
		allow = (m_geolocation_policy != 2);
	}

	if (m_browser_component != NULL) {
		m_browser_component->ReplyGeolocation(allow, false, request_handle);
		m_dialog_request = NULL;
	}
}

int32_t WebViewBase::CreateDocumentWidget(void *ime_session, WebCoreView *view,
					  WebCoreViewport *viewport,
					  WebRenderClient **out_render_client,
					  WebCoreFrame *frame,
					  WebSurfaceHost **out_surface_host)
{
	int32_t ret = (int32_t)0x80AF0001;

	GetSize(GET_CURRENT);
	GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	DocumentWidget *doc = FindDocumentWidget();

	if (doc == NULL)
		doc = new DocumentWidget(GetInternalWidget(), NULL);

	if (doc != NULL) {
		m_ime_session = static_cast<WebImeSession *>(ime_session);
		ret = doc->Initialize(view, viewport, out_render_client, frame,
				      out_surface_host);
		doc->m_observer = this;
		doc->m_view_padding = m_view_size;

		if (m_pending_max_surface_size.extract_x() != 0.0f ||
		    m_pending_max_surface_size.extract_y() != 0.0f) {
			doc->SetMaxSurfaceSize(m_pending_max_surface_size);
			m_pending_max_surface_size = math::v2(0.0f, 0.0f);
		}
	}
	return ret;
}

int32_t WebViewBase::DestroyDocumentWidget()
{
	DocumentWidget *doc = FindDocumentWidget();

	if (doc != NULL)
		doc->DestroyWidget();

	if (m_ime_session != NULL) {
		m_ime_session->Release();
		m_ime_session = NULL;
	}
	return (int32_t)0x80AF0001;
}

int32_t WebViewBase::GetDocumentSize()
{
	if (IsWakeup()) {
		DocumentWidget *doc = FindDocumentWidget();

		if (doc != NULL) {
			math::v2 vp_size(0.0f, 0.0f);

			return doc->GetViewportSize(&vp_size);
		}
	}
	return (int32_t)0x80AF0001;
}

void WebViewBase::OnDocumentEvent(int32_t event_type, math::v2 touch_pos)
{
	if (event_type == 0) {
		if (m_loading_plane != NULL)
			m_loading_plane->Flash();
	} else if (event_type == 2 || event_type == 4 || event_type == 65) {
		RequestHitTest(&touch_pos, event_type);
	} else if (event_type == 13 || event_type == 66) {
		if (m_loading_plane != NULL)
			m_loading_plane->Hide();
	} else if (event_type == 25) {
		if (m_loading_plane != NULL)
			m_loading_plane->Show();
	}
}

}
}
