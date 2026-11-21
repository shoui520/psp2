/*
 * paf's web view widget (the file's name is ours).
 */
#include <stddef.h>
#include <scetypes.h>
#include <memory/heap_allocator.h>
#include "web_view.h"

/* GetGlobalHeapAllocator, exported by its C name. */
extern "C" paf::memory::HeapAllocator *GetGlobalHeapAllocator(void);

namespace paf {
namespace ui {

const char WebView::m_widget_type[] = "webview";
const char WebViewParam::m_param_name[] = "web_view_base";

void WebView::MoveCursor(math::v2 *pos, float time)
{
	if (m_cursorPlane != NULL) {
		*pos = *pos + (math::v2(m_cursorPlane->GetSize(0)->getImpl().getXY()) *
			math::v2(0.5f, -0.5f) + m_cursorOffset);

		Widget *plane = m_cursorPlane;
		math::v4 cur = plane->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		sce::Vectormath::Simd::Aos::Vector4 to(cur.getImpl());

		to.setXY(pos->getImpl());
		plane->SetPos(math::v4(to), new Timer(time), Widget::ANIM_MOVE, 1);
	}
}

WebViewLoadingPlane::WebViewLoadingPlane(Widget *parent) : Plane(parent, NULL)
{
	m_state = 0;
	m_delay = 0;
	m_reserved = 0;
	m_transition_complete = false;
	m_transition = 0xff;
	/* through the vtable, as Sony's */
	Widget *self = this;

	self->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(this, NULL, 2, 0.0f, NULL);
	SetPos(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	SetColor(math::v4(0.38f, 0.4f, 0.4f, 0.41f));
}

WebView::WebView(Widget *parent, CreateParam *param)
	: ScrollView(parent, param), m_receiveBuffer(NULL), m_viewId(-1), m_pageState(0),
	  m_listener(NULL), m_loadingPlane(NULL), m_appListener(ScePafWidget_E638AA1F()),
	  m_ipcDataLock("SceWebViewWebIpcData", 0), m_surfacePool(NULL),
	  m_surfaceWidth(0), m_ime(NULL), m_renderTarget(NULL), m_isSecure(false),
	  m_cursorType(0), m_popupMenu(NULL), m_loadProgress(0), m_loadState(1),
	  m_canReload(true), m_canGoBack(false), m_canGoForward(false), m_imeType(-1),
	  m_options(0), m_imeOpen(false), m_imeEnabled(true), m_contentSize(0.0f, 0.0f),
	  m_scale(1.0f), m_maxScale(5.0f), m_minScale(0.25f), m_userScalable(true), m_scrollX(0),
	  m_imeState(0), m_scrollY(0), m_pageRect(0.0f, 0.0f, 0.0f, 0.0f),
	  m_lastPageRect(0.0f, 0.0f, 0.0f, 0.0f), m_zoomed(true),
	  m_zoomX(0.0f), m_zoomY(0.0f), m_zoomRect(0.0f, 0.0f, 0.0f, 0.0f),
	  m_contextMenuShown(false), m_imeMinDate(0, 0, 0, 0, 0, 0, 0),
	  m_imeMaxDate(0, 0, 0, 0, 0, 0, 0), m_selectCount(0), m_selectFocus(0),
	  m_selectMultiple(false), m_startWithParam(false), m_tapRect(0.0f, 0.0f, 0.0f, 0.0f),
	  m_tapScale(1.0f), m_tapping(false), m_pageHandlesInput(false),
	  m_inputMask(15), m_dragStart(0.0f, 0.0f), m_cursorMove(0.0f, 0.0f),
	  m_cursorEnabled(false), m_reserved1920(0), m_reserved1924(0), m_cursorPlane(NULL), m_cursorMode(0), m_cursorHidden(false),
	  m_cursorOffset(0.0f, 0.0f), m_reserved2008(0.0f, 0.0f), m_keyReady(true)
{
	m_ipcData.clear();
	m_dialogParam.values[0] = 0;
	m_dialogParam.values[1] = 0;
	m_dialogParam.values[2] = 0;
	m_dialogParam.values[3] = 0;
	m_dialogParam.title = common::String(L"");
	m_dialogParam.message = common::String(L"");
	m_dialogParam.owner = NULL;
	m_dialogParam.values[4] = 0;
	m_dialogParam.button = common::String(L"");
	m_dialogParam.user_data = NULL;
	m_selectIndices.clear();
	m_selectItems.clear();
	m_selectEnabled.clear();
	m_selectChecked.clear();
	m_scrollAnims.clear();
	m_zoomAnims.clear();
	m_tapAnims.clear();
	m_keyQueue.clear();
	m_heldKeys.clear();
	for (int i = 0; i < 8; i++)
		m_cursorPos[i] = math::v2(-1.0f, -1.0f);

	m_clip_mode = 1;
	ChangeState(0, -1);

	WebViewViewPlane *view = new WebViewViewPlane(GetViewParent(), NULL);

	if (view != NULL) {
		view->m_view = this;
		view->SetGestureMask(view->m_touch_gesture_mask | 1, true);
		view->SetGestureMask(view->m_touch_gesture_mask | 4, true);
		view->SetGestureMask(view->m_touch_gesture_mask | 2, true);
	}
	m_loadingPlane = new WebViewLoadingPlane(this);
	m_cursorPlane = new Plane(this, NULL);
	if (m_cursorPlane != NULL) {
		if (m_arrowTexture.get() != NULL) {
			m_cursorPlane->SetTexture(m_arrowTexture);

			Widget *plane = m_cursorPlane;
			math::v4 size = *GetSize(0);
			graph::Surface *texture = m_arrowTexture.get();

			plane->SetSize(math::v4(texture->GetWidth(), texture->GetHeight(),
				size.extract_z(), size.extract_w()));
		}

		math::v2 pos(GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)).getImpl().getXY());

		MoveCursor(&pos, 0.0f);

		Widget *cursor = m_cursorPlane;

		cursor->m_transition_complete = false;
		cursor->m_transition = 0xff;
		cursor->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
	}

	WebViewAnalogListener *listener = new WebViewAnalogListener(m_env);

	if (listener != NULL) {
		/* the view it moves, as its first touch's widget */
		listener->m_binded_widget[0] = this;
		m_analogListener = common::SharedPtr<inputdevice::InputListener>(listener);
		inputdevice::AddInputListener(m_analogListener);
	} else {
		m_analogListener = common::SharedPtr<inputdevice::InputListener>();
	}
	m_viewId = WebIpc::GetInstance()->RegisterView(this);
}

void WebView::DestroyWidget()
{
	if (m_hold_count > 0)
		return;

	MessageDialogListener *dialog = wutil::GetMessageDialogListener();

	if (dialog != NULL && dialog->Method8()) {
		MessageDialogParam param = {};

		param.owner = static_cast<WebViewPageClient *>(this);
		dialog->Method17(&param);
	}

	WebViewListener *listener = (WebViewListener *)wutil::GetWebViewListener();

	if (listener != NULL)
		listener->Terminate(this);
	inputdevice::DelInputListener(m_keyListener);
	m_keyListener.reset();
	inputdevice::DelInputListener(m_analogListener);
	m_analogListener.reset();
	if (m_loadingPlane != NULL) {
		m_loadingPlane->DestroyWidget();
		m_loadingPlane = NULL;
	}
	CloseIme();
	m_listener = NULL;

	WebIpcMessage message(WEB_IPC_CLOSE_PAGE);

	WebIpc::GetInstance()->Send(&message, m_viewId);
	WebIpc::GetInstance()->UnregisterView(m_viewId);
	m_viewId = -1;
	ReleaseHeldKey(WEB_VIEW_KEY_ALL);
	m_heldKeys.clear();
	ScrollViewBase::DestroyWidget();
	if (m_renderTarget != NULL) {
		DestroyRenderTarget(m_renderTarget);
		m_renderTarget = NULL;
	}
	m_arrowTexture.clear();
	m_arrowAltTexture.clear();
	m_handTexture.clear();
	m_handAltTexture.clear();
	m_surface.clear();
}

int WebView::LoadUrl(const Url &url)
{
	m_loadRequest.m_type = WebViewLoadRequest::TYPE_URL;
	Start();
	m_url = url;
	m_title = common::String("");

	WebIpcLoadUrlRequest message(m_url.GetUrlString());

	return WebIpc::GetInstance()->Send(&message, m_viewId);
}

int WebView::LoadData(const string &source, bool flag, const string &type)
{
	int ret;

	m_loadRequest.m_flag = flag;
	m_loadRequest.m_type = WebViewLoadRequest::TYPE_DATA;
	m_loadRequest.m_url = source;
	m_loadRequest.m_data = type;
	Start();

	string data;

	if (source.length() != 0) {
		ret = WEB_VIEW_ERROR_NO_WIDGET;
		{
			string file("file:///");
			Url url(source);
			string path = source.substr(url.GetSchemeString().length() + 1,
						    source.length());

			m_url = file + path;
		}
		{
			Url url(source);

			data = source.substr(url.GetSchemeString().length() + 1, source.length());
		}
		m_title = common::String("");
		if (data.length() != 0) {
			WebIpcLoadData message(data, m_url.GetUrlString(), type, flag);

			ret = WebIpc::GetInstance()->Send(&message, m_viewId);
		}
	} else {
		m_url = string("about:blank");
		ret = WEB_VIEW_ERROR_NO_WIDGET;
	}
	return ret;
}

int WebView::GoBack()
{
	Start();

	WebIpcMessage message(WEB_IPC_GO_BACK);

	return WebIpc::GetInstance()->Send(&message, m_viewId);
}

int WebView::GoForward()
{
	Start();

	WebIpcMessage message(WEB_IPC_GO_FORWARD);

	return WebIpc::GetInstance()->Send(&message, m_viewId);
}

int WebView::Reload()
{
	Start();
	if (!m_canReload) {
		m_canReload = true;
		return LoadUrl(m_url);
	}

	WebIpcMessage message(WEB_IPC_RELOAD);

	return WebIpc::GetInstance()->Send(&message, m_viewId);
}

int WebView::Stop()
{
	WebIpcMessage message(WEB_IPC_STOP);
	int ret = WebIpc::GetInstance()->Send(&message, m_viewId);

	m_loadState = 2;
	return ret;
}

int WebView::Request1050()
{
	WebIpcMessage message(WEB_IPC_REQUEST_1050);

	return WebIpc::GetInstance()->Send(&message, m_viewId);
}

int WebView::SetZoomOffset(float bottom, float y)
{
	math::v4 padding(0.0f, 0.0f, 0.0f, 0.0f);

	padding.set_w(bottom);
	padding.set_y(m_padding.extract_y());
	SetPadding(padding);
	m_zoomY = y;
	return 0;
}

int WebView::SetSetting1031(bool value)
{
	WebIpcSetting message(WEB_IPC_SETTING_1031, value);

	return WebIpc::GetInstance()->Send(&message, m_viewId);
}

int WebView::SetSetting1048(bool value)
{
	WebIpcSetting message(WEB_IPC_SETTING_1048, value);

	return WebIpc::GetInstance()->Send(&message, m_viewId);
}

int WebView::SetSetting1053(bool value)
{
	WebIpcSetting message(WEB_IPC_SETTING_1053, value);

	return WebIpc::GetInstance()->Send(&message, m_viewId);
}

void WebViewLoadingPlane::Hide()
{
	Timer *timer = new Timer(50.0f);
	float time = timer != NULL ? timer->GetTotalTime() : 0.0f;

	m_transition_complete = false;
	m_transition = 0xff;
	DoTransitionReverse_core(time, true);
	ScePafWidget_4588699E(this, timer, 2, 0.0f, NULL);
	if (m_state == 2 || m_state == 3)
		m_state = 1;
}

int WebView::HideLoading()
{
	if (m_loadingPlane != NULL)
		m_loadingPlane->Hide();
	OnInput9();
	return 0;
}

int WebView::SetLoadingDelay(int delay)
{
	if (m_loadingPlane == NULL)
		return WEB_VIEW_ERROR_NO_WIDGET;
	m_loadingPlane->m_delay = delay;
	return 0;
}

int WebView::SetOptions(int options)
{
	m_options = options;
	return 0;
}

void WebView::SetVerticalScrollBar(ScrollBar *scroll_bar)
{
	ScrollViewBase::SetVerticalScrollBar(scroll_bar);
}

void WebView::SetHorizontalScrollBar(ScrollBar *scroll_bar)
{
	ScrollViewBase::SetHorizontalScrollBar(scroll_bar);
}

int32_t WebView::EnableTouchRearEvent(bool on_off)
{
	Widget::EnableTouchRearEvent(on_off);
	/* Sony's returns nothing: r0 is what SetMovableDirOff leaves */
	SetMovableDirOff(1);
}

__attribute__((noinline)) void WebView::SetInputMask(int mask)
{
	m_inputMask = mask;

	Widget *parent = GetViewParent();
	Widget *plane = parent->FindChild(IDParam("view_plane"), 2);

	if (plane != NULL) {
		if (m_inputMask & 4)
			plane->SetPinchMode(0x200);
		else
			plane->SetPinchMode(0);
	}
}

void WebView::AddInputMask(int mask)
{
	SetInputMask(m_inputMask | mask);
}

void WebView::RemoveInputMask(int mask)
{
	SetInputMask(m_inputMask & ~mask);
}

__attribute__((noinline)) bool WebView::CanZoom()
{
	if (((m_view_box_size.getImpl().getX() == sce::Vectormath::Simd::floatInVec(0.0f)) &
	     (m_view_box_size.getImpl().getY() == sce::Vectormath::Simd::floatInVec(0.0f)))
		    .getAsBool())
		return false;
	if (((m_contentSize.getImpl().getX() == sce::Vectormath::Simd::floatInVec(0.0f)) &
	     (m_contentSize.getImpl().getY() == sce::Vectormath::Simd::floatInVec(0.0f)))
		    .getAsBool())
		return false;
	return m_scale != 0.0f;
}

__attribute__((noinline)) void WebView::ZoomBy(float factor)
{
	if (!CanZoom() || m_ime_padding)
		return;

	math::v2 target = GetViewFocusTarget();
	float ratio = (sce::Vectormath::Simd::recipf(m_contentSize.getImpl().getX()) *
		       m_view_box_size.getImpl().getX()).getAsFloat();

	if (ratio > 0.0f) {
		if (ratio * factor > m_maxScale)
			factor = m_maxScale / ratio;
		else if (ratio * factor < m_minScale)
			factor = m_minScale / ratio;
		if (m_scrollAnims.size() == 0 && m_tapAnims.size() == 0)
			m_scrollAnims.push_back(WebViewAnim(target, factor));
	}
}

void WebView::Zoom(int action)
{
	if (!CanZoom() || m_ime_padding)
		return;

	switch (action) {
	case 1: {
		math::v2 target = m_view_box_size;
		float scale = m_contentSize.extract_x() * m_scale / target.extract_x();

		target = GetViewFocusTarget();
		const math::v4 *size = GetSize(0);
		math::v2 pos(sce::Vectormath::Simd::Aos::Vector2(target.getImpl().getX(),
			(m_view_box_size.getImpl().getY() - size->getImpl().getY()) *
				sce::Vectormath::Simd::floatInVec(0.5f) +
			m_padding.getImpl().getY()));

		if (scale <= 1.05f && scale >= 0.95f)
			m_scrollAnims.push_back(WebViewAnim(pos, 1.0f, 0));
		else
			m_scrollAnims.push_back(WebViewAnim(pos, 1.0f, 1));
		break;
	}
	case 2: {
		math::v2 target = GetViewFocusTarget();
		const math::v4 *size = GetSize(0);
		math::v2 pos(sce::Vectormath::Simd::Aos::Vector2(target.getImpl().getX(),
			-((m_view_box_size.getImpl().getY() - size->getImpl().getY()) *
			  sce::Vectormath::Simd::floatInVec(0.5f)) -
			m_padding.getImpl().getY()));

		m_scrollAnims.push_back(WebViewAnim(pos, 1.0f));
		break;
	}
	case 4:
		ZoomBy(1.25f);
		break;
	case 8:
		ZoomBy(0.75f);
		break;
	}
}

__attribute__((noinline)) void WebView::ResetZoom(const math::v2 &pos)
{
	if (!CanZoom())
		return;

	float scale = m_contentSize.extract_x() * m_scale / m_view_box_size.extract_x();

	if (scale != 1.0f) {
		if (m_tapAnims.size() != 0)
			m_scrollAnims.push_back(WebViewAnim(pos, scale, 2));
		else
			m_scrollAnims.push_back(WebViewAnim(pos, scale, 0));
	}
}

void WebView::ResetZoom()
{
	ResetZoom(GetViewFocusTarget());
}

__attribute__((noinline)) void WebView::Tap(const math::v2 &pos, int button)
{
	if (m_loadingPlane != NULL)
		m_loadingPlane->m_state = 1;

	WebIpcTap message((int32_t)pos.extract_x(), (int32_t)pos.extract_y(), button);

	WebIpc::GetInstance()->Send(&message, m_viewId);
}

__attribute__((noinline)) sce::Vectormath::Simd::Aos::Matrix3 WebView::GetPageTransform()
{
	using namespace sce::Vectormath::Simd;

	Aos::Matrix3 transform = Aos::Matrix3::identity();
	Widget *plane = GetViewParent()->FindChild(IDParam("view_plane"), 2);

	if (plane != NULL && !(*plane->GetSize(0) == math::v4(0.0f, 0.0f, 0.0f, 0.0f))) {
		floatInVec rate = recipf(plane->GetSize(0)->getImpl().getX());
		floatInVec width = m_contentSize.getImpl().getX();

		transform = Aos::Matrix3(Aos::Vector3(rate * width, floatInVec(0.0f), floatInVec(0.0f)),
					 Aos::Vector3(floatInVec(0.0f), -width * rate, floatInVec(0.0f)),
					 Aos::Vector3(m_contentSize.extract_x() * 0.5f,
						      m_contentSize.extract_y() * 0.5f, 1.0f));
	}
	return transform;
}

void WebView::TapAtDragStart()
{
	using namespace sce::Vectormath::Simd;

	Aos::Matrix3 transform = GetPageTransform();
	Aos::Vector3 pos = transform.getCol0() * m_dragStart.getImpl().getX() +
		transform.getCol1() * m_dragStart.getImpl().getY() +
		Aos::mulPerElem(transform.getCol2(), Aos::Vector3(1.0f));

	Tap(math::v2(Aos::Vector2(pos.getX(), pos.getY())), 0);
}

void WebView::Request1070()
{
	WebIpcMessage message(WEB_IPC_REQUEST_1070);

	WebIpc::GetInstance()->Send(&message, m_viewId);
}

void WebView::RequestData(int64_t id, int64_t size)
{
	Buffer *buffer = m_receiveBuffer;

	if (buffer != NULL) {
		if (buffer->m_size != size) {
			delete buffer;
			m_receiveBuffer = new Buffer(size);
		} else {
			sce_paf_memset(buffer->m_data, 0, buffer->m_size);
			buffer->m_length = 0;
		}
	} else {
		m_receiveBuffer = new Buffer(size);
	}

	WebIpcRequestData message(id, size);

	WebIpc::GetInstance()->Send(&message, m_viewId);
}

inline void WebView::ClearSelect()
{
	m_selectItems.clear();
	m_selectEnabled.clear();
	m_selectChecked.clear();
	m_selectCount = 0;
	m_selectFocus = 0;
}

int WebView::CloseSelect(int index)
{
	ClearSelect();

	WebIpcSelectReply message(index, false);
	int ret = WebIpc::GetInstance()->Send(&message, m_viewId);

	m_cursorHidden = false;
	ShowCursor();
	return ret;
}

int WebView::ChooseSelect(uint32_t index, bool force, bool close)
{
	int ret = WEB_VIEW_ERROR_NO_WIDGET;
	bool choose = force;

	if (!force && !close && m_selectEnabled.at(index))
		choose = true;
	if (close) {
		ClearSelect();
		m_cursorHidden = false;
		ShowCursor();
	} else if (!choose) {
		return ret;
	}

	WebIpcSelectReply message(index, close);

	ret = WebIpc::GetInstance()->Send(&message, m_viewId);
	return ret;
}

int WebView::SendStringPair(const string &first, const string &second)
{
	WebIpcStringPair message(first, second);

	return WebIpc::GetInstance()->Send(&message, m_viewId);
}

int WebView::SetImeText(int type, const string &text)
{
	int mode = 0;

	m_imeType = type;
	m_imeText = text;
	switch (m_imeType) {
	case 0:
		mode = 1;
		break;
	case 1:
		mode = 2;
		break;
	}

	WebIpcImeText message(mode, text);

	return WebIpc::GetInstance()->Send(&message, m_viewId);
}

int WebView::Request1051()
{
	WebIpcMessage message(WEB_IPC_REQUEST_1051);

	return WebIpc::GetInstance()->Send(&message, m_viewId);
}

int WebView::SetScrollY(int y)
{
	int ret = WEB_VIEW_ERROR_NO_WIDGET;

	if (m_scrollY > 0) {
		WebIpcScrollY message(y);

		ret = WebIpc::GetInstance()->Send(&message, m_viewId);
		if (ret == 0)
			m_scrollY = y;
	}
	return ret;
}

/* the rectangle's centre, the rectangle moved into the bound where it is not in it */
static inline math::v2 FitRectCenter(const math::rect &boundRect, const math::rect &rect)
{
	math::rect bound(boundRect);
	math::rect fitted(rect);

	if (!fitted.is_in_rect(bound)) {
		if (fitted.left() < bound.left()) {
			float width = fitted.width();

			fitted.set_left(bound.left());
			fitted.set_right(bound.left() + width);
		} else if (bound.right() < fitted.right()) {
			float width = fitted.width();

			fitted.set_left(bound.right() - width);
			fitted.set_right(bound.right());
		}
		if (fitted.bottom() < bound.bottom())
			fitted.align_bottom(bound.bottom());
		else if (bound.top() < fitted.top())
			fitted.align_top(bound.top());
	}
	return fitted.center();
}

math::v2 WebView::GetFittedViewFocus(bool offset)
{
	math::v2 focus = GetViewFocus();
	const math::v4 *size = GetSize(0);
	math::rect focusRect(focus, math::v2(size->extract_x(), size->extract_y()));
	math::rect viewRect(math::v2(0.0f, 0.0f), math::v2(m_view_box_size));

	viewRect.set_bottom(viewRect.bottom() - m_padding.extract_w());
	focus = FitRectCenter(viewRect, focusRect);
	if (offset)
		focus.set_y(focus.extract_y() + (m_padding.extract_y() - m_pageRect.extract_y()));
	return focus;
}

void WebView::SetViewFocus(const math::v2 &pos, Timer *timer, bool flag_a, bool flag_b)
{
	Widget *plane = GetViewParent()->FindChild(IDParam("view_plane"), 2);
	math::v2 focus = pos;
	math::rect area;

	if (plane != NULL) {
		const math::v4 *size = plane->GetSize(0);

		area = math::rect(math::v2(0.0f, 0.0f), math::v2(size->extract_x(), size->extract_y()));
	}
	area.set_top(area.top() + m_padding.extract_y());
	area.set_bottom(area.bottom() - m_padding.extract_w());
	if (m_ime_padding)
		area.set_bottom(area.bottom() - m_ime_height);

	const math::v4 *size = GetSize(0);
	math::rect focusRect(pos, math::v2(size->extract_x(), size->extract_y()));
	math::v2 center = FitRectCenter(area, focusRect);

	if (m_tapAnims.size() == 0) {
		focus = center;
		ScrollViewBase::SetViewFocus(focus, timer, flag_a, flag_b);
	}
}

void WebView::QueueMessage21()
{
	WebIpcMessage *message = new WebIpcMessage(WEB_IPC_UPDATE_21);

	if (message == NULL)
		return;

	thread::RMutexLock lock(&m_ipcDataLock);

	m_ipcData.push_back(message);
}

void WebView::QueueMessage1056()
{
	WebIpcMessage *message = new WebIpcMessage(WEB_IPC_UPDATE_1056);

	if (message == NULL)
		return;

	thread::RMutexLock lock(&m_ipcDataLock);

	m_ipcData.push_back(message);
}

/* the message the page sent, read from its packet and queued for the view's update; a type the
   view does not know is dropped */
void WebView::ReceiveMessage(const WebIpcPacket *packet)
{
	WebIpcMessage *message = NULL;

	if (packet->m_type == WEB_IPC_EVENT_2003 && !CanZoom())
		return;

	switch (packet->m_type) {
	case WEB_IPC_EVENT_2003:
	case 0x2006:
	case 0x2007:
	case 0x2010:
	case 0x2022:
	case 0x2101:
	case 0x2201:
	case 0x2202:
	case 0x2212:
	case 0x2213:
		message = new WebIpcMessage(packet->m_type);
		break;
	case 0x2001:
		message = new WebIpcEvent2001(packet);
		break;
	case 0x2004:
		message = new WebIpcEvent2004(packet);
		break;
	case 0x2005:
		message = new WebIpcEvent2005(packet);
		break;
	case 0x2008:
		message = new WebIpcEvent2008(packet);
		break;
	case 0x2009:
		message = new WebIpcEvent2009(packet);
		break;
	case 0x2011:
		message = new WebIpcEvent2011(packet);
		break;
	case 0x2012:
		message = new WebIpcEvent2012(packet);
		break;
	case 0x2013:
		message = new WebIpcEvent2013(packet);
		break;
	case 0x2014:
		message = new WebIpcEvent2014(packet);
		break;
	case 0x2015:
		message = new WebIpcEvent2015(packet);
		break;
	case 0x2016:
		message = new WebIpcEvent2016(packet);
		break;
	case 0x2019:
		message = new WebIpcEvent2019(packet);
		break;
	case 0x2020:
		message = new WebIpcEvent2020(packet);
		break;
	case 0x2021:
		message = new WebIpcEvent2021(packet);
		break;
	case 0x2023:
		message = new WebIpcEvent2023(packet);
		break;
	case 0x2024:
		message = new WebIpcEvent2024(packet);
		break;
	case 0x2025:
		message = new WebIpcEvent2025(packet);
		break;
	case 0x2026:
		message = new WebIpcEvent2026(packet);
		break;
	case 0x2027:
		message = new WebIpcEvent2027(packet);
		break;
	case 0x2028:
		message = new WebIpcEvent2028(packet);
		break;
	case 0x2029:
		message = new WebIpcEvent2029(packet);
		break;
	case 0x2030:
		message = new WebIpcEvent2030(packet);
		break;
	case 0x2031:
		message = new WebIpcEvent2031(packet);
		break;
	case 0x2032:
		message = new WebIpcEvent2032(packet);
		break;
	case 0x2033:
		message = new WebIpcEvent2033(packet);
		break;
	case 0x2034:
		message = new WebIpcEvent2034(packet);
		break;
	case 0x2035:
		message = new WebIpcEvent2035(packet);
		break;
	case WEB_IPC_CREATE_PAGE_WITH_VIEW:
		message = new WebIpcCreatePageWithView(packet);
		break;
	case 0x2203:
		message = new WebIpcEvent2203(packet);
		break;
	case 0x2204:
		message = new WebIpcEvent2204(packet);
		break;
	case 0x2205:
		message = new WebIpcEvent2205(packet);
		break;
	case 0x2206:
		message = new WebIpcEvent2206(packet);
		break;
	case 0x2210:
		message = new WebIpcEvent2210(packet);
		break;
	case 0x2211:
		message = new WebIpcEvent2211(packet);
		break;
	default:
		break;
	}

	if (message != NULL) {
		thread::RMutexLock lock(&m_ipcDataLock);

		m_ipcData.push_back(message);
		return;
	}

	string url = m_url.GetUrlString();
	memory::HeapAllocator *heap = GetGlobalHeapAllocator();

	if (heap != NULL) {
		heap->GetFreeSize();
		sce_paf_printf_dummy();
	}
}

/* the web process gone: a page made or loading is lost, and the messages it sent with it */
void WebView::OnProcessExit()
{
	if (m_pageState != 0)
		m_pageState = 4;
	ClearIpcData();
}

/* the page takes the view's input, the view's plane neither scrolling, pinching nor taking the
   gestures, while the input mask passes any; otherwise the plane has them back */
void WebView::SetPageHandlesInput(bool on)
{
	if (m_pageHandlesInput == on)
		return;

	Widget *parent = GetViewParent();
	Widget *plane = parent->FindChild(IDParam("view_plane"), 2);

	if (on && (m_inputMask & 0xf) != 0) {
		if (plane != NULL) {
			if (m_tapping)
				return;
			SetMovableDirOn(3);
			plane->SetPinchMode(0);
			plane->SetGestureMask(plane->m_touch_gesture_mask & ~1, true);
			plane->SetGestureMask(plane->m_touch_gesture_mask & ~4, true);
			plane->SetGestureMask(plane->m_touch_gesture_mask & ~2, true);
		}
	} else if (plane != NULL) {
		SetMovableDirOn(0);
		if (m_userScalable)
			plane->SetPinchMode(0x200);
		if ((m_inputMask & 4) == 0)
			plane->SetPinchMode(0);
		plane->SetGestureMask(plane->m_touch_gesture_mask | 1, true);
		plane->SetGestureMask(plane->m_touch_gesture_mask | 4, true);
		plane->SetGestureMask(plane->m_touch_gesture_mask | 2, true);
	}
	m_pageHandlesInput = on;
}

/* the page asks for a new window: the user is asked first unless the view's options say not to,
   or else the listener makes the window's view, which then makes its page; with no view for it,
   the request goes back to the page */
void WebView::OnCreateWindow(const WebIpcCreatePageWithView *message)
{
	if (message == NULL) {
		WebIpcCreatePageWithView reply(0, 0, -1, NULL, 0, false);

		WebIpc::GetInstance()->Post(&reply, m_viewId);
		return;
	}

	CloseIme();

	bool ask = message->m_flag & ((m_options & 1) == 0);
	MessageDialogListener *dialog = wutil::GetMessageDialogListener();

	if (dialog != NULL && ask) {
		m_dialogParam.values[0] = 1;
		m_dialogParam.values[1] = 3;
		m_dialogParam.button = common::String(message->m_url.c_str());
		m_dialogParam.title = common::String(L"");
		m_dialogParam.owner = static_cast<WebViewPageClient *>(this);
		m_dialogParam.user_data = new WebIpcCreatePageWithView(message->m_url.c_str(),
			message->m_width, message->m_height, message->m_flag);

		Widget *cursor = m_cursorPlane;

		m_cursorHidden = true;
		if (cursor != NULL) {
			cursor->m_transition_complete = false;
			cursor->m_transition = 0xff;
			cursor->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
		}
		dialog->Method9(&m_dialogParam);
		return;
	}

	if (m_listener == NULL)
		return;

	WebView *window = NULL;
	bool handled = false;

	m_listener->OnCreateWindow(this, Url(string(message->m_url.c_str())), message->m_width,
		message->m_height, &window, &handled);
	if (window == NULL) {
		WebIpc::GetInstance()->Post(message, m_viewId);
		return;
	}

	window->m_startWithParam = true;
	if (handled || window->m_pageState == 3)
		return;

	WebViewListener *listener = (WebViewListener *)wutil::GetWebViewListener();

	if (listener != NULL)
		listener->Initialize(window);
	if (window->m_pageState == 0) {
		if (window->m_startWithParam)
			window->CreatePageForView();
		else
			window->CreatePage();
	}
}

/* the cursor moved by the step, kept in the view (less the IME's height where it pads the view,
   with ime): where it would leave the view, the focus moves by the step instead, and with
   scroll the view follows it */
math::v2 WebView::MoveCursorInView(const math::v2 &step, bool ime, bool scroll)
{
	math::v2 focus = GetViewFocusTarget();
	math::v2 pos = GetCursorPos(false) + step * math::v2(1.0f, -1.0f);
	float padding = m_padding.extract_y();
	float top = padding - m_pageRect.extract_y();
	float margin = m_loadProgress < 100 ? padding : top;

	if (m_pageRect.extract_y() > 0.0f && m_padding.extract_w() > 0.0f)
		margin = top;

	const math::v4 *size = GetSize(0);
	math::rect area(math::v2(0.0f, 0.0f), math::v2(size->extract_x(), size->extract_y()));

	area.set_top(area.top() - margin);
	area.set_bottom(area.bottom() + m_pageRect.extract_w());
	if (m_ime_padding && ime)
		area.set_bottom(area.bottom() - m_ime_height);

	if (!area.is_point_in_rect(pos)) {
		if (pos.extract_x() < area.left()) {
			pos.set_x(area.left());
			focus.set_x(focus.extract_x() + step.extract_x());
		} else if (area.right() < pos.extract_x()) {
			pos.set_x(area.right());
			focus.set_x(focus.extract_x() + step.extract_x());
		}
		if (pos.extract_y() < area.bottom()) {
			pos.set_y(area.bottom());
			focus.set_y(focus.extract_y() - step.extract_y());
		} else if (area.top() < pos.extract_y()) {
			pos.set_y(area.top());
			focus.set_y(focus.extract_y() - step.extract_y());
		}
	}
	if (scroll)
		SetViewFocus(focus, NULL, true, true);
	return pos;
}

/* the page's texture moved up past the top padding, or, while the IME pads the view, by as much
   of the view as it covers (up to 30 past its height) */
void WebView::UpdateTexRegion()
{
	graph::PlaneObj *plane = (graph::PlaneObj *)GetDrawObj(0);

	if (plane == NULL || plane->m_size == math::v3(0.0f, 0.0f, 0.0f))
		return;

	float offset = -((m_padding.extract_y() - m_pageRect.extract_y()) /
		plane->m_size.extract_y());

	if (m_ime_padding) {
		math::v2 focus = GetViewFocus();
		math::v2 box = m_view_box_size;
		float covered = -(focus + (box - math::v2(GetSize(0)->getImpl().getXY())) *
			0.5f).extract_y();

		if (covered > 0.0f) {
			if (covered > m_ime_height + 30.0f)
				covered = m_ime_height + 30.0f;
			offset = covered / plane->m_size.extract_y();
		}
	}
	plane->SetTexRegion(math::v4(0.0f, offset, 1.0f, 1.0f));
}

/* the view moved or zoomed: the page told what part of it the view shows (with the render
   target's state when it draws into the view's surface), the cursor kept in the view, and a zoom
   left over from a finished tap dropped */
void WebView::UpdateViewport()
{
	if (!CanZoom() || GetViewParent() == NULL)
		return;

	UpdatePageRect(GetViewFocus());
	UpdateTexRegion();

	math::v4 visible = GetVisiblePageRect();
	int x = (int32_t)visible.extract_x();
	int y = (int32_t)visible.extract_y();
	int width = (int32_t)visible.extract_z();
	int height = (int32_t)visible.extract_w();
	WebIpcViewport viewport(x, y, width, height, m_tapping);

	WebIpc::GetInstance()->Send(&viewport, m_viewId);
	if (m_surface.get() != NULL && m_renderTarget != NULL) {
		WebIpcRenderRect render(x, y, width, height, m_renderTarget, 28);

		WebIpc::GetInstance()->Send(&render, m_viewId);
		ProcessRenderCommands(m_renderTarget);
	}

	math::v2 pos = MoveCursorInView(math::v2(0.0f, 0.0f), false, false);

	MoveCursor(&pos, 0.0f);
	if (m_imeOpen)
		m_imeOpen = false;
	else
		SendCursorPos();

	if (m_zoomAnims.size() != 0 && m_tapAnims.size() == 0) {
		m_tapping = false;
		m_zoomAnims.clear();
		UpdateViewport();
	}
}

void WebView::SetSecure(bool secure)
{
	m_isSecure = secure;
}

int WebView::Reserved3C88A96E()
{
	return 0;
}

math::v4 WebView::GetVisiblePageRect()
{
	using namespace sce::Vectormath::Simd;

	if (!CanZoom())
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);

	math::v2 focus = GetViewFocus();

	focus = GetFittedViewFocus(false);

	const math::v4 *parentSize = GetViewParent()->GetSize(0);

	focus = focus + math::v2(parentSize->extract_x(), parentSize->extract_y()) *
		math::v2(-0.5f, 0.5f);

	floatInVec scale = recipf(m_view_box_size.getImpl().getX()) *
		m_contentSize.getImpl().getX();
	Aos::Matrix3 transform = GetPageTransform();
	Aos::Vector3 pos = transform.getCol0() * focus.getImpl().getX() +
		transform.getCol1() * focus.getImpl().getY() +
		Aos::mulPerElem(transform.getCol2(), Aos::Vector3(1.0f));

	if (pos.getX().getAsFloat() < 0.0f)
		pos.setX(0.0f);
	if (pos.getY().getAsFloat() < 0.0f)
		pos.setY(0.0f);

	float x = floorf(pos.getX()).getAsFloat();
	float y = floorf(pos.getY()).getAsFloat();
	float width = floorf(scale * GetSize(0)->getImpl().getX()).getAsFloat() + 1.0f;
	float height = floorf(scale * GetSize(0)->getImpl().getY()).getAsFloat();

	if (m_contentSize.extract_x() < width + x)
		width = m_contentSize.extract_x() - x;
	return math::v4(x, y, width, height);
}

__attribute__((noinline)) math::v2 WebView::GetCursorPos(bool offset)
{
	math::v2 pos(0.0f, 0.0f);

	if (m_cursorPlane != NULL) {
		math::v4 origin(0.0f, 0.0f, 0.0f, 0.0f);
		math::v4 world = m_cursorPlane->GetPos(1, NULL, origin);
		const math::v4 *size = m_cursorPlane->GetSize(0);

		pos = math::v2(world.extract_x(), world.extract_y()) +
			math::v2(size->extract_x(), size->extract_y()) * math::v2(-0.5f, 0.5f) -
			m_cursorOffset;
		if (offset)
			pos = pos + GetFittedViewFocus(true);
	}
	return pos;
}

void WebView::SendCursorPos()
{
	using namespace sce::Vectormath::Simd;

	if (m_cursorPlane == NULL || m_cursorMode == 0 || m_cursorHidden ||
	    m_arrowTexture.get() == NULL)
		return;

	math::v2 cursor = GetCursorPos(true);
	Aos::Matrix3 transform = GetPageTransform();
	Aos::Vector3 pos = transform.getCol0() * cursor.getImpl().getX() +
		transform.getCol1() * cursor.getImpl().getY() +
		Aos::mulPerElem(transform.getCol2(), Aos::Vector3(1.0f));
	math::v4 visible = GetVisiblePageRect();
	int right = (int32_t)(visible.extract_x() + visible.extract_z());
	int bottom = (int32_t)(visible.extract_y() + visible.extract_w());

	if (right > (int32_t)pos.getX().getAsFloat()) {
		if (pos.getX().getAsFloat() < visible.extract_x())
			pos.setX(visible.extract_x());
	} else {
		pos.setX((float)right);
	}
	if (bottom > (int32_t)pos.getY().getAsFloat()) {
		if (pos.getY().getAsFloat() < visible.extract_y())
			pos.setY(visible.extract_y());
	} else {
		pos.setY((float)bottom);
	}

	WebIpcCursorPos message((int32_t)pos.getX().getAsFloat(), (int32_t)pos.getY().getAsFloat());

	WebIpc::GetInstance()->Send(&message, m_viewId);
}

void WebView::SetCursorMode(int mode)
{
	Widget *cursor = m_cursorPlane;

	m_cursorMode = mode;
	if (cursor == NULL)
		return;
	if (mode != 0) {
		SetFocus(NULL);
		ShowCursor();
		SendCursorPos();
	} else {
		cursor->m_transition_complete = false;
		cursor->m_transition = 0xff;
		cursor->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
	}
}

int WebView::SetCursorTextures(const intrusive_ptr<graph::Surface> &arrow,
			       const intrusive_ptr<graph::Surface> &arrowAlt,
			       const intrusive_ptr<graph::Surface> &hand,
			       const intrusive_ptr<graph::Surface> &handAlt, const math::v2 &offset)
{
	if (arrow.get() == NULL)
		return WEB_VIEW_ERROR_NO_TEXTURE;
	m_arrowTexture = arrow;
	m_arrowAltTexture = arrowAlt;
	m_handTexture = hand;
	m_handAltTexture = handAlt;
	m_cursorOffset = offset;
	if (m_cursorPlane != NULL && m_arrowTexture.get() != NULL) {
		m_cursorPlane->SetTexture(m_arrowTexture);

		Widget *cursor = m_cursorPlane;
		const math::v4 *size = GetSize(0);
		graph::Surface *texture = m_arrowTexture.get();

		cursor->SetSize(math::v4(texture->GetWidth(), texture->GetHeight(), size->extract_z(),
					 size->extract_w()));
	}
	return 0;
}

int WebView::CreateSurface()
{
	if (m_surface.get() != NULL)
		return 0;

	graph::SurfacePool *pool = m_surfacePool;

	if (pool == NULL)
		pool = gutil::GetDefaultSurfacePool();

	math::v4 size = *GetSize(0);

	if (size == math::v4(0.0f, 0.0f, 0.0f, 0.0f))
		return WEB_VIEW_ERROR_NO_SIZE;

	m_surface.clear();
	m_surface = intrusive_ptr<graph::Surface>(new graph::Surface(pool,
		(int32_t)size.extract_x(), (int32_t)size.extract_y(), ImageMode_BGRA8888,
		ImageOrder_Linear, 1, 1, false));

	{
		intrusive_ptr<graph::Surface> surface = m_surface;

		if (m_renderTarget != NULL) {
			DestroyRenderTarget(m_renderTarget);
			m_renderTarget = NULL;
		}
		if (surface.get() != NULL) {
			void *base = surface->Lock(0);

			CreateRenderTarget(&m_renderTarget, surface->GetWidth(),
				surface->GetHeight(), surface->GetStride(), base);
			surface->Unlock();
		}
	}
	if (m_surface.get() == NULL || m_surface->GetMemory() == NULL) {
		sce_paf_printf_dummy();
		return WEB_VIEW_ERROR_NO_MEMORY;
	}
	sce_paf_memset(m_surface->Lock(0), 0xff, m_surface->GetPageSize());
	m_surface->Unlock();
	SetTexture(m_surface);
	return 0;
}

void WebView::CreatePageForView()
{
	if (CreateSurface() != 0)
		return;

	math::v4 size = *GetSize(0);
	WebIpcCreatePageWithView message((int32_t)(float)size.getImpl().getX(),
		(int32_t)(float)size.getImpl().getY(), m_viewId, GetRenderParam(),
		sizeof(WebViewRenderParam), m_isSecure);

	WebIpc::GetInstance()->Post(&message, m_viewId);
	m_startWithParam = false;
	m_pageState = 2;
}

int WebView::CreatePage()
{
	int ret = CreateSurface();

	if (ret != 0 || (m_pageState != 0 && m_pageState != 1))
		return ret;

	math::v4 size = *GetSize(0);
	WebIpcCreatePage message((int32_t)(float)size.getImpl().getX(),
		(int32_t)(float)size.getImpl().getY(), GetRenderParam(),
		sizeof(WebViewRenderParam), m_isSecure);

	ret = WebIpc::GetInstance()->Send(&message, m_viewId);
	m_pageState = ret != 0 ? 1 : 2;
	return ret;
}

void WebView::Start()
{
	if (m_pageState == 3)
		return;

	WebViewListener *listener = (WebViewListener *)wutil::GetWebViewListener();

	if (listener != NULL)
		listener->Initialize(this);
	if (m_pageState == 0) {
		if (m_startWithParam)
			CreatePageForView();
		else
			CreatePage();
	}
}

void WebView::ReleaseHeldKey(int keycode)
{
	list<WebViewHeldKey>::iterator it = m_heldKeys.begin();

	if (keycode == WEB_VIEW_KEY_ALL) {
		for (; it != m_heldKeys.end(); ++it)
			if (it->m_sent && it->m_widget != NULL)
				it->m_widget->DestroyWidget();
		m_heldKeys.clear();
		return;
	}
	for (; it != m_heldKeys.end(); ++it) {
		if (it->m_keycode == keycode) {
			if (it->m_sent && it->m_widget != NULL)
				it->m_widget->DestroyWidget();
			m_heldKeys.erase(it);
			return;
		}
	}
}

__attribute__((noinline)) void WebView::ShowCursor()
{
	Widget *cursor = m_cursorPlane;

	if (cursor == NULL || m_cursorMode == 0 || m_cursorHidden ||
	    m_arrowTexture.get() == NULL || m_cursorType == WEB_VIEW_CURSOR_NONE)
		return;
	if (!cursor->m_transition_complete) {
		cursor->m_transition = 0xff;
		cursor->m_transition_complete = true;
		cursor->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(cursor, NULL, 2, 0.0f, NULL);
}

void WebView::UnhideCursor()
{
	m_cursorHidden = false;
	ShowCursor();
}

void WebView::CloseIme()
{
	if (m_ime_padding &&
	    !((m_contentSize.getImpl().getX() == sce::Vectormath::Simd::floatInVec(0.0f)) &
	      (m_contentSize.getImpl().getY() == sce::Vectormath::Simd::floatInVec(0.0f)))
		     .getAsBool()) {
		m_cursorHidden = false;
		ShowCursor();
		SetImePadding(false);

		math::v2 target = GetViewFocusTarget();
		float scale = m_tapScale /
			(sce::Vectormath::Simd::recipf(m_contentSize.getImpl().getX()) *
			 m_view_box_size.getImpl().getX()).getAsFloat();

		if (m_tapScale >= 1.75f)
			scale = 1.0f;
		m_scrollAnims.push_back(WebViewAnim(target, scale));
	}
	if (m_ime != NULL) {
		if (m_listener != NULL)
			m_listener->OnImeClosed(this, 1, 0);
		ImeClose(m_ime);
		m_ime = NULL;
	}
}

WebViewParam::WebViewParam() : m_home_url(NULL)
{
}

bool WebView::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return ScrollView::IsInherit(type);
}

char *WebView::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *WebView::TypeName()
{
	return m_widget_type;
}

void WebView::ClearIpcData()
{
	thread::RMutexLock lock(&m_ipcDataLock);

	for (vector<WebIpcMessage *>::iterator it = m_ipcData.begin(); it != m_ipcData.end(); ++it)
		if (*it != NULL)
			delete *it;
	m_ipcData.clear();
}

WebView::~WebView()
{
	if (m_popupMenu != NULL) {
		DestroyPopupMenu(m_popupMenu);
		m_popupMenu = NULL;
	}
	ClearIpcData();
	if (m_receiveBuffer != NULL)
		delete m_receiveBuffer;
	m_receiveBuffer = NULL;
}

}
}
