/*
 * paf's web view widget (the file's name is ours).
 */
#include <stddef.h>
#include <scetypes.h>
#include <memory/heap_allocator.h>
#include <rtc.h>
#include <widget/core/environment.h>
#include <widget/factory.h>
#include <widget/w_button_base.h>
#include <widget/w_text_box.h>
#include <versatileparam/resourceparam.h>
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
	m_style = 0;
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
	  m_listener(NULL), m_loadingPlane(NULL), m_appListener((WebViewDataListener *)ScePafWidget_E638AA1F()),
	  m_ipcDataLock("SceWebViewWebIpcData", 0), m_surfacePool(NULL),
	  m_surfaceData(NULL), m_ime(NULL), m_renderTarget(NULL), m_geolocationEnabled(false),
	  m_cursorType(0), m_geolocation(NULL), m_loadProgress(0), m_loadState(1),
	  m_canReload(true), m_canGoBack(false), m_canGoForward(false), m_imeType(-1),
	  m_options(0), m_imeOpen(false), m_firstLayout(true), m_contentSize(0.0f, 0.0f),
	  m_scale(1.0f), m_maxScale(5.0f), m_minScale(0.25f), m_userScalable(true), m_fitScale(0.0f),
	  m_pageInfo(0), m_scrollY(0), m_pageRect(0.0f, 0.0f, 0.0f, 0.0f),
	  m_lastPageRect(0.0f, 0.0f, 0.0f, 0.0f), m_zoomed(true),
	  m_zoomX(0.0f), m_zoomY(0.0f), m_zoomRect(0.0f, 0.0f, 0.0f, 0.0f),
	  m_contextMenuShown(false), m_imeMinDate(0, 0, 0, 0, 0, 0, 0),
	  m_imeMaxDate(0, 0, 0, 0, 0, 0, 0), m_selectCount(0), m_selectReceived(0),
	  m_selectMultiple(false), m_startWithParam(false), m_imeRect(0.0f, 0.0f, 0.0f, 0.0f),
	  m_tapScale(1.0f), m_tapping(false), m_pageHandlesInput(false),
	  m_inputMask(15), m_dragStart(0.0f, 0.0f), m_cursorMove(0.0f, 0.0f),
	  m_cursorEnabled(false), m_analogEdgeTime(0), m_cursorPlane(NULL), m_cursorMode(0), m_cursorHidden(false),
	  m_cursorOffset(0.0f, 0.0f), m_zoomOffset(0.0f, 0.0f), m_keyReady(true)
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
	m_pendingDialogs.clear();
	m_selectItems.clear();
	m_selectEnabled.clear();
	m_selectLabels.clear();
	m_scrollAnims.clear();
	m_zoomAnims.clear();
	m_tapAnims.clear();
	m_keyQueue.clear();
	m_parts.clear();
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

		param.owner = static_cast<WebViewDialogClient *>(this);
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
	RemovePart(WEB_VIEW_PART_ALL);
	m_parts.clear();
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
		ret = WEB_VIEW_ERROR_FAILED;
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
		ret = WEB_VIEW_ERROR_FAILED;
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

int WebView::SetZoomTop(const math::v2 &top, bool clip, float x, bool zoomed)
{
	math::v4 padding(0.0f, 0.0f, 0.0f, 0.0f);

	padding.set_y(top.extract_y());
	padding.set_w(m_padding.extract_w());
	SetPadding(padding);
	m_clip_children = clip;
	m_zoomX = x;
	m_zoomed = zoomed;
	if (m_zoomRect == math::v4(0.0f, 0.0f, 0.0f, 0.0f))
		m_zoomRect = m_vertical_bar_margin;

	math::v4 margin = m_zoomRect;

	if (!zoomed)
		margin.set_x(margin.extract_x() - m_padding.extract_y());
	margin.set_y(margin.extract_y() - m_padding.extract_w());
	SetVerticalBarMargin(margin);

	if (m_env != NULL) {
		graph::Surface *screen = m_env->m_screen->m_surface.get();
		math::rect layout(math::v2(0.0f, m_pageRect.extract_y() - m_padding.extract_y() -
					   m_zoomX),
			math::v2((float)screen->GetWidth(), (float)screen->GetHeight()));

		if (m_layout_rect == NULL)
			m_layout_rect = new math::rect();
		*m_layout_rect = layout;
	}
	return 0;
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
	OnImePressClose();
	return 0;
}

int WebView::SetLoadingDelay(int delay)
{
	if (m_loadingPlane == NULL)
		return WEB_VIEW_ERROR_FAILED;
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
	m_selectLabels.clear();
	m_selectCount = 0;
	m_selectReceived = 0;
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
	int ret = WEB_VIEW_ERROR_FAILED;
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
	int ret = WEB_VIEW_ERROR_FAILED;

	if (m_scrollY > 0) {
		{
			WebIpcScrollY message(y);

			ret = WebIpc::GetInstance()->Send(&message, m_viewId);
		}
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
void WebView::ResetPage()
{
	OnTitleChanged(common::String(""));
	m_scrollAnims.clear();
	m_tapAnims.clear();
	m_zoomAnims.clear();

	math::v4 pageRect = m_pageRect;

	m_firstLayout = true;
	m_tapping = false;
	m_pageRect = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_pageInfo = 0;
	m_lastPageRect = pageRect;
	m_keyQueue.clear();
	m_loadState = 0;
	m_keyReady = true;
	m_cursorType = 0;
	SetPageHandlesInput(false);
	CloseIme();
	if (m_loadingPlane != NULL)
		m_loadingPlane->Hide();
	if (m_listener != NULL)
		m_listener->OnPageReset(this);
}

void WebView::OnLoadFinished()
{
	if (m_handAltTexture.get() != NULL && m_cursorPlane != NULL)
		m_cursorPlane->SetTexture(m_arrowTexture);
	if (m_listener != NULL)
		m_listener->OnLoadFinished(this, m_loadState);
	if (m_loadState == 1)
		m_canReload = false;
	m_loadState = 0;
}

void WebView::OnLoadFailed(int error, const common::String &url)
{
	m_loadState = 1;
	if (m_listener != NULL)
		m_listener->OnLoadFailed(this, error | 0x80108000, url);
}

void WebView::OnLoadCommitted()
{
	if (m_listener != NULL)
		m_listener->OnLoadCommitted(this);
}

void WebView::OnPageLayout()
{
	SetImeText(m_imeType, m_imeText);
	SetScrollY(m_scrollY);
	if (m_url.GetUrlString().size() != 0) {
		if (m_loadRequest.m_type == WebViewLoadRequest::TYPE_DATA)
			LoadData(string(m_loadRequest.m_url), m_loadRequest.m_flag,
				 string(m_loadRequest.m_data));
		else
			LoadUrl(m_url);
		m_loadRequest = WebViewLoadRequest();
	}
	if (m_listener != NULL)
		m_listener->OnPageLayout(this);
}

void WebView::OnLoadProgress(int progress)
{
	m_loadProgress = progress;
	if (m_listener != NULL)
		m_listener->OnLoadProgress(this);
}

void WebView::OnTitleChanged(const common::String &title)
{
	m_title = common::String(title.GetString());
	if (m_listener != NULL)
		m_listener->OnTitleChanged(this, title);
}

void WebView::OnUrlChanged(const Url &url)
{
	m_url = url;
	if (m_listener != NULL)
		m_listener->OnUrlChanged(this, url);
}

void WebView::OnEvent8(int arg)
{
	if (m_listener != NULL)
		m_listener->OnEvent8(this, arg);
}

void WebView::UpdatePageRect(const math::v2 &focus)
{
	math::v2 box = m_view_box_size;
	math::v2 origin = ((box - math::v2(GetSize(0)->getImpl().getXY())) * 0.5f +
		math::v2(m_padding.getImpl().getXY())) * math::v2(1.0f, -1.0f);
	math::v2 view = GetViewFocus() * math::v2(1.0f, -1.0f);
	math::v2 viewSize = m_view_box_size;
	float offset = view.extract_y() - origin.extract_y();

	/* the page's top, moved down by as much of the top padding as the view shows */
	if (offset < 0.0f)
		offset = 0.0f;
	else if (offset > m_padding.extract_y())
		offset = m_padding.extract_y();
	m_pageRect.set_y(offset);
	if (m_zoomRect == math::v4(0.0f, 0.0f, 0.0f, 0.0f))
		m_zoomRect = m_vertical_bar_margin;

	math::v4 margin = m_zoomRect;

	if (m_zoomed)
		margin.set_x(margin.extract_x() - offset);
	else
		margin.set_x(margin.extract_x() - m_padding.extract_y());

	math::rect area(math::v2(0.0f, 0.0f), viewSize);
	float bottom = -(view + math::v2(GetSize(0)->getImpl().getXY()) * 0.5f).extract_y();
	float top = area.bottom() - bottom;

	if (top > m_padding.extract_w())
		top = m_padding.extract_w();
	else if (top < 0.0f)
		top = 0.0f;
	margin.set_y(m_zoomRect.extract_y() - m_padding.extract_w());
	SetVerticalBarMargin(margin);

	float layoutTop;

	if (top == 0.0f) {
		m_pageRect.set_w(0.0f);
		layoutTop = m_pageRect.extract_y() - m_padding.extract_y() - m_zoomX;
	} else {
		layoutTop = top + m_zoomY;
		m_pageRect.set_w(layoutTop);
	}
	if (m_env != NULL) {
		graph::Surface *screen = m_env->m_screen->m_surface.get();
		math::rect layout(math::v2(0.0f, layoutTop),
			math::v2((float)screen->GetWidth(), (float)screen->GetHeight()));

		if (m_layout_rect == NULL)
			m_layout_rect = new math::rect();
		*m_layout_rect = layout;
	}
	if (m_listener != NULL && m_loadState != 5) {
		m_listener->OnPagePos(this, math::v2(m_pageRect.getImpl().getXY()));
		m_listener->OnPageTop(this, top);
	}
}

void WebView::OnOpenWindow(const Url &url)
{
	if (m_listener != NULL) {
		WebView *window = NULL;
		bool handled = false;

		m_listener->OnCreateWindow(this, url, 0, 0, &window, &handled);
	}
}

void WebView::OnEvent11(int arg0, int arg1)
{
	if (m_listener != NULL)
		m_listener->OnEvent11(this, arg0, arg1);
}

void WebView::OnEvent12(const Url &url)
{
	if (m_listener != NULL)
		m_listener->OnEvent12(this, url);
}

void WebView::OnNavigationRequest(const Url &url, int type, bool *allow)
{
	if (m_listener != NULL)
		m_listener->OnNavigationRequest(this, url, type, allow);
}

void WebView::OnDownloadRequest(const Url &url, const string &mimeType, int64_t length)
{
	if (m_listener == NULL)
		return;

	Widget *cursor = m_cursorPlane;

	m_cursorHidden = true;
	if (cursor != NULL) {
		cursor->m_transition_complete = false;
		cursor->m_transition = 0xff;
		cursor->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
	}
	m_listener->OnDownloadRequest(this, url, mimeType, length);
}

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
		m_dialogParam.owner = static_cast<WebViewDialogClient *>(this);
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
void WebView::OnEvent15(int arg)
{
	if (m_listener != NULL)
		m_listener->OnEvent15(this, arg);
}

void WebView::OnEvent16()
{
	if (m_listener != NULL)
		m_listener->OnEvent16(this);
}

void WebView::OnHistoryChanged(bool canGoBack, bool canGoForward)
{
	if (m_canGoBack == canGoBack && m_canGoForward == canGoForward)
		return;

	m_canGoBack = canGoBack;
	m_canGoForward = canGoForward;
	if (m_listener != NULL)
		m_listener->OnHistoryChanged(this, canGoBack, canGoForward);
}

void WebView::OnEvent18(int arg0, int arg1)
{
	if (m_listener != NULL)
		m_listener->OnEvent18(this, arg0, arg1);
}

void WebView::OnEvent19(int arg0, int arg1)
{
	if (m_listener != NULL)
		m_listener->OnImeClosed(this, arg0, arg1);
}

void WebView::OnEvent20(int keycode, int down)
{
	int direction = 0;

	switch (keycode) {
	case 54:
		direction = 2;
		break;
	case 64:
		direction = 4;
		break;
	case 74:
		direction = 8;
		break;
	case 81:
		direction = 1;
		break;
	}
	if (m_listener != NULL)
		m_listener->OnCursorKey(this, direction, down == 0);
}

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

void WebView::OnPartsLayoutUpdated(bool is_dirty)
{
	if (is_dirty || m_imeOpen) {
		if (m_loadingPlane != NULL && !m_cursorEnabled)
			m_loadingPlane->Hide();
		UpdateViewport();
	}
	ScrollViewBase::OnPartsLayoutUpdated(is_dirty);
}

void WebView::OnDataEnd(bool success)
{
	if (m_appListener == NULL)
		return;

	if (success) {
		Buffer *buffer = m_receiveBuffer;

		if (buffer != NULL && buffer->m_length > 0) {
			WebViewDataEvent event(WebViewDataEvent::TYPE_DATA);

			event.m_data = buffer->m_data;
			event.m_size = buffer->m_length;
			m_appListener->OnData(event, this);

			buffer = m_receiveBuffer;
			sce_paf_memset(buffer->m_data, 0, buffer->m_size);
			buffer->m_length = 0;
		}
	}

	WebViewDataEvent end(WebViewDataEvent::TYPE_END);

	end.m_result = success ? 0 : WEB_VIEW_ERROR_DATA_FAILED;
	m_appListener->OnData(end, this);
	if (m_receiveBuffer != NULL)
		delete m_receiveBuffer;
	m_receiveBuffer = NULL;
}

void WebView::OnData(const WebIpcEvent2030 *message)
{
	Buffer *buffer = m_receiveBuffer;

	if (buffer == NULL || message == NULL)
		return;

	int size = message->m_dataSize;
	bool copied = false;

	/* the part kept, and the buffer given to the application once it is full; a part that
	   does not fit goes into the emptied buffer */
	if (buffer->m_length + size <= buffer->m_size) {
		sce_paf_memcpy(buffer->m_data + buffer->m_length, message->m_data, size);
		buffer->m_length += size;
		buffer = m_receiveBuffer;
		copied = true;
		if (buffer->m_size != buffer->m_length)
			return;
	}
	if (m_appListener != NULL) {
		WebViewDataEvent event(WebViewDataEvent::TYPE_DATA);

		event.m_data = buffer->m_data;
		event.m_size = buffer->m_length;
		m_appListener->OnData(event, this);

		buffer = m_receiveBuffer;
		sce_paf_memset(buffer->m_data, 0, buffer->m_size);
		buffer->m_length = 0;
	}
	if (!copied) {
		buffer = m_receiveBuffer;
		size = message->m_dataSize;
		if (buffer->m_length + size <= buffer->m_size) {
			sce_paf_memcpy(buffer->m_data + buffer->m_length, message->m_data, size);
			buffer->m_length += size;
		}
	}
}

void WebView::OnDataStart(int length, const string &mimeType)
{
	if (m_appListener == NULL)
		return;

	WebViewDataEvent event(WebViewDataEvent::TYPE_START);

	event.m_mimeType = mimeType;
	event.m_length = length;
	m_appListener->OnData(event, this);
}

void WebView::OnNavigationRequest(WebIpcEvent2027 *message)
{
	if (message == NULL) {
		WebIpcEvent2027 reply(string(), 0);

		WebIpc::GetInstance()->Post(&reply, m_viewId);
		return;
	}

	Url url(string(message->m_text.c_str()));
	bool allow = true;

	if (m_listener != NULL)
		m_listener->OnNavigationRequest(this, url, message->m_int, &allow);
	message->m_bool = allow;
	WebIpc::GetInstance()->Post(message, m_viewId);
}

void WebView::ScrollToPagePos(int x, int y)
{
	using namespace sce::Vectormath::Simd;

	Widget *plane = GetViewParent()->FindChild(IDParam("view_plane"), 2);

	if (m_ime_padding || plane == NULL)
		return;

	math::v2 offset(0.0f, 0.0f);

	if (y <= 0)
		offset = math::v2(m_padding.getImpl().getXY());

	Aos::Matrix3 transform = GetViewTransform();
	Aos::Vector3 world = transform.getCol0() * (float)x + transform.getCol1() * (float)y +
		Aos::mulPerElem(transform.getCol2(), Aos::Vector3(1.0f));
	math::v2 focus = math::v2(Aos::Vector2(world.getX(), world.getY())) +
		math::v2(GetSize(0)->getImpl().getXY()) * math::v2(0.5f, -0.5f) + offset;

	SetViewFocus(focus, NULL, true, true);
}

bool WebView::QueueDialog(WebIpcMessage *message)
{
	MessageDialogListener *dialog = wutil::GetMessageDialogListener();

	if (dialog == NULL || !dialog->Method8())
		return false;
	m_pendingDialogs.push_back(message);
	return true;
}

bool WebView::OnAuthRequest(WebIpcEvent2025 *message)
{
	if (message == NULL) {
		WebIpcEvent2025 reply((string()), string(), string(), string());

		WebIpc::GetInstance()->Post(&reply, m_viewId);
		return false;
	}
	if (QueueDialog(message))
		return true;

	MessageDialogListener *dialog = wutil::GetMessageDialogListener();

	if (dialog == NULL) {
		message->SetCredentials(false, string(), string());
		WebIpc::GetInstance()->Post(message, m_viewId);
		return false;
	}

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
	m_dialogParam.owner = static_cast<WebViewDialogClient *>(this);
	m_dialogParam.values[0] = 6;

	Widget *cursor = m_cursorPlane;

	m_cursorHidden = true;
	if (cursor != NULL) {
		cursor->m_transition_complete = false;
		cursor->m_transition = 0xff;
		cursor->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
	}

	MessageDialogTexts texts(string(message->m_text0.c_str()),
		string(message->m_text1.c_str()), string(message->m_user.c_str()),
		string(message->m_password.c_str()));

	dialog->Method14(&m_dialogParam, texts);
	return false;
}

/* the date the RTC gives for the page's time (name ours) */
static inline void SetDateTime(datetime::DateTime *date, const SceDateTime &time)
{
	if (date == NULL)
		return;
	date->m_year = time.year;
	date->m_month = time.month;
	date->m_day = time.day;
	date->m_hour = time.hour;
	date->m_minute = time.minute;
	date->m_second = time.second;
	date->m_microsecond = time.microsecond;
}

bool WebView::OnDateTimeRequest(WebIpcEvent2033 *message)
{
	if (message == NULL) {
		WebIpcEvent2033 reply(0, string(), string(), 0.0f, 0.0f, string());

		WebIpc::GetInstance()->Post(&reply, m_viewId);
		return false;
	}
	if (QueueDialog(message))
		return true;

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

	int inputType = message->m_inputType;
	MessageDialogListener *dialog = wutil::GetMessageDialogListener();

	if (dialog == NULL)
		goto decline;

	switch (inputType) {
	case 2: case 4: case 6: case 7: case 8: case 18: case 19: case 21: case 22: case 23:
	case 25: case 26:
		m_dialogParam.values[1] = 0;
		break;
	case 100:
		m_dialogParam.values[1] = 1;
		break;
	case 9: case 10: case 13: case 14:
		m_dialogParam.values[1] = 2;
		break;
	case 20: case 24: case 27: case 28:
		m_dialogParam.values[1] = 3;
		break;
	case 29: case 30: case 31: case 32: case 50:
		m_dialogParam.values[1] = 4;
		break;
	default:
		if (inputType <= 0)
			goto decline;
		m_dialogParam.values[1] = 4;
		break;
	}

	{
		m_imeTitle = common::String(message->m_title.c_str());
		m_imeInitialText = common::String(message->m_initialText.c_str());

		SceDateTime minDate;
		SceDateTime maxDate;

		sceRtcSetTime64_t(&minDate, (SceUInt64)message->m_minDate);
		SetDateTime(&m_imeMinDate, minDate);
		sceRtcSetTime64_t(&maxDate, (SceUInt64)message->m_maxDate);
		SetDateTime(&m_imeMaxDate, maxDate);
		m_imeDefault = common::String(message->m_defaultText.c_str());

		Widget *cursor = m_cursorPlane;

		m_cursorHidden = true;
		m_dialogParam.owner = static_cast<WebViewDialogClient *>(this);
		m_dialogParam.values[0] = 2;
		if (cursor != NULL) {
			cursor->m_transition_complete = false;
			cursor->m_transition = 0xff;
			cursor->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
		}
		if (dialog->Method12(&m_dialogParam) >= 0)
			return false;
	}

decline:
	message->m_accepted = false;
	WebIpc::GetInstance()->Post(message, m_viewId);
	return false;
}

bool WebView::OnDialogRequest(WebIpcEvent2034 *message)
{
	if (message == NULL) {
		WebIpcEvent2034 reply(0);

		WebIpc::GetInstance()->Post(&reply, m_viewId);
		return false;
	}
	if (QueueDialog(message))
		return true;

	bool open = m_dialogParam.values[1] == 1 && m_dialogParam.values[0] == 4;

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

	MessageDialogListener *dialog = wutil::GetMessageDialogListener();

	if (dialog != NULL) {
		switch (message->m_value) {
		case 0:
			m_dialogParam.values[1] = 0;
			break;
		case 1:
			m_dialogParam.values[1] = 1;
			if (open)
				goto cancel;
			break;
		default:
			goto cancel;
		}

		Widget *cursor = m_cursorPlane;

		m_dialogParam.values[0] = 4;
		m_dialogParam.owner = static_cast<WebViewDialogClient *>(this);
		m_cursorHidden = true;
		if (cursor != NULL) {
			cursor->m_transition_complete = false;
			cursor->m_transition = 0xff;
			cursor->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
		}
		if (dialog->Method10(&m_dialogParam) >= 0)
			return false;
	}

cancel:
	message->m_accepted = true;
	WebIpc::GetInstance()->Post(message, m_viewId);
	return false;
}

void WebView::ZoomToRect(const math::v2 &pos, const math::v2 &size)
{
	if (GetSize(0)->getImpl().getX().getAsFloat() < size.getImpl().getX().getAsFloat())
		return;
	if (GetSize(0)->getImpl().getY().getAsFloat() < size.getImpl().getY().getAsFloat())
		return;
	if (!CanZoom())
		return;
	if (((size.getImpl().getX() == sce::Vectormath::Simd::floatInVec(0.0f)) &
	     (size.getImpl().getY() == sce::Vectormath::Simd::floatInVec(0.0f)))
		    .getAsBool())
		return;

	float ratio = (sce::Vectormath::Simd::recipf(m_contentSize.getImpl().getX()) *
		       m_view_box_size.getImpl().getX()).getAsFloat();
	sce::Vectormath::Simd::Aos::Vector4 width = GetSize(0)->getImpl();
	sce::Vectormath::Simd::floatInVec widthScale =
		sce::Vectormath::Simd::recipf(size.getImpl().getX());
	sce::Vectormath::Simd::Aos::Vector4 height = GetSize(0)->getImpl();
	sce::Vectormath::Simd::floatInVec heightScale =
		sce::Vectormath::Simd::recipf(size.getImpl().getY());
	float scale = sce::Vectormath::Simd::fminf(widthScale * width.getX(),
		heightScale * height.getY()).getAsFloat();

	if (ratio * scale > m_maxScale)
		scale = m_maxScale / ratio;
	if (scale < 1.005f) {
		sce::Vectormath::Simd::Aos::Vector4 view = GetSize(0)->getImpl();

		scale = (sce::Vectormath::Simd::recipf(m_view_box_size.getImpl().getX()) *
			 view.getX()).getAsFloat();
	}
	if (ratio * scale < m_minScale)
		scale = m_minScale / ratio;
	if ((m_inputMask & 2) == 0)
		scale = 1.0f;
	m_scrollAnims.push_back(WebViewAnim(pos, scale));
}

inline void WebViewLoadingPlane::FadeIn(float time)
{
	Timer *timer = new Timer(time);

	if (!m_transition_complete) {
		m_transition = 0xff;
		m_transition_complete = true;
		DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(this, timer, 2, 0.0f, NULL);
}

void WebViewLoadingPlane::Show()
{
	if (m_state != 0) {
		m_state = 2;
		return;
	}
	if (m_delay == 0) {
		FadeIn(30.0f);
	} else if (m_delay == 1) {
		if (m_style == 0)
			FadeIn(30.0f);
		else if (m_style == 1)
			FadeIn(200.0f);
	}
}

inline void WebViewLoadingPlane::ShowNow()
{
	if (!m_transition_complete) {
		m_transition = 0xff;
		m_transition_complete = true;
		DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(this, NULL, 2, 0.0f, NULL);
}

inline void WebViewLoadingPlane::FadeOut(float time)
{
	Timer *timer = new Timer(time);
	float total = timer != NULL ? timer->GetTotalTime() : 0.0f;

	m_transition_complete = false;
	m_transition = 0xff;
	DoTransitionReverse_core(total, true);
	ScePafWidget_4588699E(this, timer, 2, 0.0f, NULL);
}

void WebViewLoadingPlane::Flash()
{
	if (m_state != 0) {
		m_state = 3;
		return;
	}
	if (m_delay == 0) {
		ShowNow();
		FadeOut(500.0f);
	} else if (m_delay == 1 && m_style == 0) {
		ShowNow();
		FadeOut(500.0f);
	}
}

void WebViewLoadingPlane::SetStyle(int style)
{
	switch (style) {
	case 0:
		SetColor(math::v4(0.38f, 0.4f, 0.4f, 0.41f), NULL, ANIM_COLOR, 0);
		break;
	case 1:
		SetColor(math::v4(0.1f, 0.1f, 0.8f, 0.41f), NULL, ANIM_COLOR, 0);
		break;
	}
	m_style = style;
}

void WebView::UpdateLoadingPlane(const math::v4 &rect, bool show, string first, string second)
{
	string firstText(first);
	string secondText(second);
	WebViewLoadingPlane *plane = m_loadingPlane;

	if (plane == NULL)
		goto done;
	switch (plane->m_delay) {
	case 0:
		if (firstText.size() != 0)
			show = true;
		break;
	case 1:
		if ((firstText.size() | secondText.size()) != 0)
			show = true;
		if (secondText.size() != 0) {
			plane->SetStyle(1);
			plane = m_loadingPlane;
		}
		if (firstText.size() != 0) {
			plane->SetStyle(0);
			plane = m_loadingPlane;
		}
		break;
	}

	if (show) {
		math::v4 cur = plane->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		sce::Vectormath::Simd::Aos::Vector4 pos(cur.getImpl());

		pos.setXY(rect.getImpl().getXY());
		m_loadingPlane->SetPos(math::v4(pos), NULL, Widget::ANIM_MOVE, 0);

		sce::Vectormath::Simd::Aos::Vector4 size(m_loadingPlane->GetSize(0)->getImpl());

		size.setXY(sce::Vectormath::Simd::Aos::Vector2(
			rect.getImpl().getZ() + sce::Vectormath::Simd::floatInVec(15.0f),
			rect.getImpl().getW() + sce::Vectormath::Simd::floatInVec(15.0f)));
		m_loadingPlane->SetSize(math::v4(size), NULL, Widget::ANIM_SIZE, 0);
		m_loadingPlane->RunPending();
		if (m_cursorEnabled && firstText.size() != 0)
			m_loadingPlane->Show();
	} else if (m_cursorEnabled) {
		plane->m_transition_complete = false;
		plane->m_transition = 0xff;
		plane->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(plane, NULL, 2, 0.0f, NULL);
	}
done:
	;
}

void WebView::OnHit(const WebViewHitResult *hit, int action, bool show, const string &linkUrl,
	const string &imageUrl, bool contextMenuShown)
{
	using namespace sce::Vectormath::Simd;

	if (!CanZoom() || hit == NULL)
		return;

	math::v2 hitSize((float)hit->width, (float)hit->height);
	float centerX = (float)hit->x + hitSize.getImpl().getX().getAsFloat() * 0.5f;
	float centerY = (float)hit->y + hitSize.getImpl().getY().getAsFloat() * 0.5f;
	float pointX = (float)hit->pointX;
	float pointY = (float)hit->pointY;
	Aos::Matrix3 transform = GetViewTransform();
	Aos::Vector3 center = transform.getCol0() * centerX + transform.getCol1() * centerY +
		Aos::mulPerElem(transform.getCol2(), Aos::Vector3(1.0f));
	math::v2 pos = math::v2(Aos::Vector2(center.getX(), center.getY())) - GetViewFocusTarget();
	Aos::Vector3 pressed = transform.getCol0() * pointX + transform.getCol1() * pointY +
		Aos::mulPerElem(transform.getCol2(), Aos::Vector3(1.0f));
	math::v2 point = math::v2(Aos::Vector2(pressed.getX(), pressed.getY())) -
		GetViewFocusTarget();
	math::v2 size = hitSize * math::v2(Aos::Vector2(recipf(m_contentSize.getImpl().getX()) *
		m_view_box_size.getImpl().getX()));
	string link(linkUrl);
	string image(imageUrl);
	string mimeType(hit->mimeType);
	int imageSize = hit->imageSize;
	int format;

	if (sce_paf_strcmp(mimeType.c_str(), string("image/bmp").c_str()) == 0)
		format = CONTEXT_MENU_IMAGE_BMP;
	else if (sce_paf_strcmp(mimeType.c_str(), string("image/gif").c_str()) == 0)
		format = CONTEXT_MENU_IMAGE_GIF;
	else if (sce_paf_strcmp(mimeType.c_str(), string("image/png").c_str()) == 0)
		format = CONTEXT_MENU_IMAGE_PNG;
	else if (sce_paf_strcmp(mimeType.c_str(), string("image/jpeg").c_str()) == 0)
		format = CONTEXT_MENU_IMAGE_JPEG;
	else
		format = CONTEXT_MENU_IMAGE_NONE;

	UpdateLoadingPlane(math::v4(Aos::Vector4(pos.getImpl(), size.getImpl())), show, linkUrl,
		imageUrl);
	m_contextMenuShown = contextMenuShown;

	switch (action) {
	case WEB_VIEW_HIT_ZOOM:
		if (!((size.getImpl().getX() == floatInVec(0.0f)) &
		      (size.getImpl().getY() == floatInVec(0.0f))).getAsBool()) {
			pos = pos + GetViewFocusTarget();
			ZoomToRect(math::v2(pos), math::v2(size));
		}
		break;
	case WEB_VIEW_HIT_CONTEXT_MENU:
		if ((link.size() | image.size()) != 0 && m_listener != NULL) {
			ContextMenuInfo info(math::rect(pos, size), point);

			if (link.size() != 0)
				info.AddItem(CONTEXT_MENU_ITEM_LINK, link, 0, format);
			if (image.size() != 0)
				info.AddItem(CONTEXT_MENU_ITEM_IMAGE, image, imageSize, format);

			Widget *cursor = m_cursorPlane;

			m_cursorHidden = true;
			if (cursor != NULL) {
				cursor->m_transition_complete = false;
				cursor->m_transition = 0xff;
				cursor->DoTransitionReverse_core(0.0f, true);
				ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
			}
			m_listener->OnContextMenu(this, info);
		}
		break;
	}
}

void WebView::OnEvent22()
{
	if (m_appListener == NULL)
		return;

	Widget *cursor = m_cursorPlane;

	m_cursorHidden = true;
	if (cursor != NULL) {
		cursor->m_transition_complete = false;
		cursor->m_transition = 0xff;
		cursor->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
	}
	m_appListener->OnSlot3(this);
}

void WebView::RequestDownload(const char *url, const char *mimeType)
{
	OnDownloadRequest(Url(string(url)), string(mimeType), 0);
}

void WebView::ShowSelect(const vector<string> &items)
{
	if (items.size() == 0)
		return;
	if (m_loadingPlane != NULL)
		m_loadingPlane->Hide();
	OnImePressClose();

	WebViewSelectInfo info;
	WebViewSelectGroup group;

	group.m_reserved = false;
	info.m_multipleShown = m_selectMultiple;
	info.m_multiple = m_selectMultiple;
	for (uint32_t i = 0; i < items.size(); i++) {
		WebViewSelectItem item;

		item.m_selectable = false;
		item.m_enabled = false;
		item.m_text = items[i];
		item.m_enabled = m_selectEnabled[i];
		item.m_selectable = m_selectLabels[i] == 0;
		group.m_items.push_back(item);
	}
	info.m_groups.push_back(group);
	if (m_appListener != NULL) {
		Widget *cursor = m_cursorPlane;

		m_cursorHidden = true;
		if (cursor != NULL) {
			cursor->m_transition_complete = false;
			cursor->m_transition = 0xff;
			cursor->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
		}
		m_appListener->OnSelect(this, info);
	}
}

void WebView::AddSelectItem(const string &text, bool enabled, bool label)
{
	m_selectItems.push_back(text);
	m_selectEnabled.push_back(enabled);
	m_selectLabels.push_back(label);
	if (++m_selectReceived == m_selectCount)
		ShowSelect(m_selectItems);
}

void WebView::StartSelect(int count, int reserved, bool multiple)
{
	m_selectCount = count;
	m_selectReceived = 0;
	m_selectMultiple = multiple;
	m_selectItems.clear();
	m_selectEnabled.clear();
	m_selectLabels.clear();
}

void WebView::UpdateContentSize()
{
	using namespace sce::Vectormath::Simd;

	if ((m_pageInfo & WEB_VIEW_PAGE_CONTENT_SIZE) == 0 || (m_pageInfo & WEB_VIEW_PAGE_VIEWPORT) == 0)
		return;
	if (m_contentSize.extract_x() * m_scale < GetSize(0)->extract_x())
		m_contentSize.set_x(GetSize(0)->extract_x() / m_scale);

	Widget *plane = GetViewParent()->FindChild(IDParam("view_plane"), 2);

	if (plane == NULL)
		return;

	math::v4 planeSize = *plane->GetSize(0);
	math::v4 size = *GetSize(0);
	math::v2 center = math::v2((planeSize - size).getImpl().getXY()) * 0.5f *
		math::v2(-1.0f, 1.0f) - GetViewFocusTarget();
	math::v2 viewSize(GetSize(0)->getImpl().getXY());
	math::v2 scaled = m_contentSize * m_scale;
	math::v2 newSize(Aos::maxPerElem(scaled.getImpl(), viewSize.getImpl()));
	math::v2 focus(0.0f, 0.0f);

	if (newSize.getImpl().getX() > scaled.getImpl().getX() && m_scale < 0.0f)
		m_contentSize.getImpl().setX(newSize.getImpl().getX() * recipf(floatInVec(m_scale)));
	if (m_firstLayout) {
		m_firstLayout = false;
		m_pageRect = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		focus = newSize * math::v2(-0.5f, 0.5f) -
			math::v2(GetSize(0)->getImpl().getXY()) * 0.5f;
	} else {
		if (m_contentSize.extract_x() * m_minScale <= newSize.extract_x() * m_fitScale)
			newSize = newSize * m_fitScale;
		focus = (newSize - math::v2(GetSize(0)->getImpl().getXY())) * 0.5f *
			math::v2(-1.0f, 1.0f) - center;
	}
	focus = focus + (math::v2(m_padding.getImpl().getXY()) -
		math::v2(m_pageRect.getImpl().getXY()));
	m_tapAnims.clear();
	m_zoomAnims.clear();

	Aos::Vector4 planeRect(plane->GetSize(0)->getImpl());

	planeRect.setXY(newSize.getImpl());
	plane->SetSize(math::v4(planeRect), NULL, Widget::ANIM_SIZE, 0);
	if (m_userScalable) {
		Aos::Vector4 maxSize(plane->GetSize(0)->getImpl());

		maxSize.setXY(m_contentSize.getImpl());
		plane->SetPinchLimitMax(math::v4(maxSize) * m_maxScale);

		Aos::Vector4 minSize(plane->GetSize(0)->getImpl());

		minSize.setXY(m_contentSize.getImpl());
		plane->SetPinchLimitMin(math::v4(minSize) * m_minScale);
		if (m_inputMask & 4)
			plane->SetPinchMode(0x200);
		else
			plane->SetPinchMode(0);
	} else {
		plane->SetPinchLimitMax(*plane->GetSize(0));
		plane->SetPinchLimitMin(*plane->GetSize(0));
		plane->SetPinchMode(0);
	}
	UpdateSize();
	SetViewFocus(focus, NULL, true, true);
}

bool WebView::OnPromptRequest(WebIpcEvent2015 *message)
{
	if (message != NULL) {
		if (QueueDialog(message))
			return true;

		MessageDialogListener *dialog = wutil::GetMessageDialogListener();

		if (dialog != NULL) {
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
			m_dialogParam.values[0] = 1;
			m_dialogParam.values[1] = 2;
			m_dialogParam.title = common::String(message->m_text.c_str());
			m_dialogParam.message = common::String(message->m_defaultText.c_str());

			Widget *cursor = m_cursorPlane;

			m_cursorHidden = true;
			m_dialogParam.owner = static_cast<WebViewDialogClient *>(this);
			if (cursor != NULL) {
				cursor->m_transition_complete = false;
				cursor->m_transition = 0xff;
				cursor->DoTransitionReverse_core(0.0f, true);
				ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
			}
			dialog->Method9(&m_dialogParam);
		} else {
			message->m_accepted = false;
			message->m_result = string();
			WebIpc::GetInstance()->Post(message, m_viewId);
		}
	} else {
		WebIpcEvent2015 reply((string()), string(), string());

		WebIpc::GetInstance()->Post(&reply, m_viewId);
	}
	return false;
}

bool WebView::OnConfirmRequest(WebIpcEvent2014 *message)
{
	if (message != NULL) {
		if (QueueDialog(message))
			return true;
		if (m_listener != NULL)
			m_listener->OnConfirm(this, common::String(message->m_value.c_str()));

		MessageDialogListener *dialog = wutil::GetMessageDialogListener();

		if (dialog != NULL) {
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
			m_dialogParam.values[0] = 1;
			m_dialogParam.values[1] = 1;
			m_dialogParam.title = common::String(message->m_value.c_str());

			Widget *cursor = m_cursorPlane;

			m_cursorHidden = true;
			m_dialogParam.owner = static_cast<WebViewDialogClient *>(this);
			if (cursor != NULL) {
				cursor->m_transition_complete = false;
				cursor->m_transition = 0xff;
				cursor->DoTransitionReverse_core(0.0f, true);
				ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
			}
			dialog->Method9(&m_dialogParam);
		} else {
			message->m_accepted = false;
			WebIpc::GetInstance()->Post(message, m_viewId);
		}
	} else {
		WebIpcEvent2014 reply((string()));

		WebIpc::GetInstance()->Post(&reply, m_viewId);
	}
	return false;
}

bool WebView::OnAlertRequest(WebIpcEvent2013 *message)
{
	if (message != NULL) {
		if (QueueDialog(message))
			return true;

		MessageDialogListener *dialog = wutil::GetMessageDialogListener();

		if (dialog != NULL) {
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
			m_dialogParam.values[1] = 0;
			m_dialogParam.values[0] = 1;
			m_dialogParam.title = common::String(message->m_value.c_str());

			Widget *cursor = m_cursorPlane;

			m_cursorHidden = true;
			m_dialogParam.owner = static_cast<WebViewDialogClient *>(this);
			if (cursor != NULL) {
				cursor->m_transition_complete = false;
				cursor->m_transition = 0xff;
				cursor->DoTransitionReverse_core(0.0f, true);
				ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
			}
			dialog->Method9(&m_dialogParam);
		} else {
			WebIpc::GetInstance()->Post(message, m_viewId);
		}
	} else {
		WebIpcEvent2013 reply((string("")));

		WebIpc::GetInstance()->Post(&reply, m_viewId);
	}
	return false;
}

void WebView::ProcessIpcData()
{
	if (m_loadProgress < 100 && m_tapping) {
		m_firstLayout = false;
		return;
	}
	if (m_pageState == 4) {
		m_loadState = 4;
		ClearSurface();
		return;
	}

	vector<WebIpcMessage *> messages;

	m_ipcDataLock.Lock();
	if (m_ipcData.size() == 0) {
		m_ipcDataLock.Unlock();
		return;
	}
	for (unsigned int i = 0; i < m_ipcData.size(); i++)
		messages.push_back(m_ipcData[i]);
	m_ipcData.clear();
	m_ipcDataLock.Unlock();

	for (vector<WebIpcMessage *>::iterator it = messages.begin(); it != messages.end(); ++it) {
		WebIpcMessage *message = *it;

		if (message == NULL)
			continue;
		switch (message->m_type) {
		case WEB_IPC_UPDATE_21:
			if (m_pageState == 1)
				CreatePage();
			break;
		case WEB_IPC_UPDATE_1056:
			m_loadState = 5;
			ClearSurface();
			break;
		case 0x2001:
			m_pageState = 3;
			OnPageLayout();
			break;
		case 0x2002:
			if (m_surface.get() != NULL)
				m_surface->Copy(0, m_surfaceData, ImageOrder_Linear, 0, -1);
			break;
		case WEB_IPC_EVENT_2003:
			m_imeOpen = true;
			break;
		case 0x2004:
			OnTitleChanged(common::String(
				static_cast<WebIpcEvent2004 *>(message)->m_value.c_str()));
			break;
		case 0x2005:
			OnUrlChanged(Url(string(static_cast<WebIpcEvent2005 *>(message)->m_value.c_str())));
			break;
		case 0x2006:
			ResetPage();
			break;
		case 0x2007:
			OnLoadFinished();
			break;
		case 0x2008:
			OnLoadProgress((int)(static_cast<WebIpcEvent2008 *>(message)->m_value * 100.0f));
			break;
		case 0x2009: {
			WebIpcEvent2009 *request = static_cast<WebIpcEvent2009 *>(message);

			m_ime = ImeGetInstance();
			if (m_ime == NULL) {
				sce_paf_printf_dummy();
				break;
			}

			int inputType = 0;
			int enterLabel = 0;

			ImeGetInputType(string(request->m_text1.c_str()), &inputType, &enterLabel);
			m_imeRect = math::v4((float)request->m_x, (float)request->m_y,
				(float)request->m_width, (float)request->m_height);
			ImeOpen(m_ime, static_cast<WebViewInputClient *>(this), inputType, enterLabel,
				request->m_bool);
			OnEvent18(1, 0);
			break;
		}
		case 0x2010:
			CloseIme();
			break;
		case 0x2011: {
			WebIpcEvent2011 *size = static_cast<WebIpcEvent2011 *>(message);
			float width = m_view_box_size.extract_x();
			float viewWidth = GetSize(0)->extract_x();

			if (width < viewWidth)
				width = viewWidth;
			if (m_firstLayout) {
				m_fitScale = m_scrollY > 0 ?
					GetSize(0)->extract_x() / (float)m_scrollY : 0.875f;
			} else {
				float scaled = m_contentSize.extract_x() * m_scale;

				if (scaled == 0.0f)
					m_fitScale = m_scrollY > 0 ?
						GetSize(0)->extract_x() / (float)m_scrollY : 0.875f;
				else
					m_fitScale = width / scaled;
			}
			m_pageInfo |= WEB_VIEW_PAGE_CONTENT_SIZE;
			m_contentSize = math::v2((float)size->m_first, (float)size->m_second);
			UpdateContentSize();
			break;
		}
		case 0x2012: {
			WebIpcEvent2012 *viewport = static_cast<WebIpcEvent2012 *>(message);

			m_scale = viewport->m_float[0];
			m_maxScale = viewport->m_float[1];
			m_minScale = viewport->m_float[2];
			m_userScalable = viewport->m_bool;
			m_pageInfo |= WEB_VIEW_PAGE_VIEWPORT;
			UpdateContentSize();
			break;
		}
		case 0x2013:
			if (OnAlertRequest(static_cast<WebIpcEvent2013 *>(message)))
				continue;
			break;
		case 0x2014:
			if (OnConfirmRequest(static_cast<WebIpcEvent2014 *>(message)))
				continue;
			break;
		case 0x2015:
			if (OnPromptRequest(static_cast<WebIpcEvent2015 *>(message)))
				continue;
			break;
		case 0x2016: {
			WebIpcEvent2016 *history = static_cast<WebIpcEvent2016 *>(message);

			OnHistoryChanged(history->m_first != 0, history->m_second != 0);
			break;
		}
		case 0x2019: {
			WebIpcEvent2019 *select = static_cast<WebIpcEvent2019 *>(message);

			StartSelect(select->m_int[0], select->m_int[1], select->m_bool);
			break;
		}
		case 0x2020: {
			WebIpcEvent2020 *item = static_cast<WebIpcEvent2020 *>(message);

			AddSelectItem(string(item->m_text.c_str()), item->m_bool[0], item->m_bool[1]);
			break;
		}
		case 0x2021: {
			WebIpcEvent2021 *download = static_cast<WebIpcEvent2021 *>(message);

			RequestDownload(download->m_text0.c_str(), download->m_text1.c_str());
			break;
		}
		case 0x2022:
			OnEvent22();
			break;
		case 0x2023: {
			WebIpcEvent2023 *press = static_cast<WebIpcEvent2023 *>(message);
			WebViewHitResult hit = press->m_hit;

			OnHit(&hit, press->m_action, press->m_show, string(press->m_linkUrl.c_str()),
			      string(press->m_imageUrl.c_str()), press->m_contextMenuShown);
			break;
		}
		case 0x2024:
			SetPageHandlesInput(static_cast<WebIpcEvent2024 *>(message)->m_value);
			break;
		case 0x2025:
			if (OnAuthRequest(static_cast<WebIpcEvent2025 *>(message)))
				continue;
			break;
		case 0x2026: {
			WebIpcEvent2026 *pos = static_cast<WebIpcEvent2026 *>(message);

			ScrollToPagePos(pos->m_first, pos->m_second);
			break;
		}
		case 0x2027:
			OnNavigationRequest(static_cast<WebIpcEvent2027 *>(message));
			break;
		case 0x2028:
			OnEvent12(Url(string(static_cast<WebIpcEvent2028 *>(message)->m_value.c_str())));
			break;
		case 0x2029: {
			WebIpcEvent2029 *start = static_cast<WebIpcEvent2029 *>(message);

			OnDataStart(start->m_int, string(start->m_text.c_str()));
			break;
		}
		case 0x2030:
			OnData(static_cast<WebIpcEvent2030 *>(message));
			break;
		case 0x2031:
			OnDataEnd(static_cast<WebIpcEvent2031 *>(message)->m_value);
			break;
		case 0x2032:
			OnEvent8(static_cast<WebIpcEvent2032 *>(message)->m_value);
			break;
		case 0x2033:
			if (OnDateTimeRequest(static_cast<WebIpcEvent2033 *>(message)))
				continue;
			break;
		case 0x2034:
			if (OnDialogRequest(static_cast<WebIpcEvent2034 *>(message)))
				continue;
			break;
		case 0x2035: {
			WebIpcEvent2035 *failed = static_cast<WebIpcEvent2035 *>(message);

			if (!failed->m_bool)
				OnLoadFailed(failed->m_int, common::String(failed->m_text1.c_str()));
			break;
		}
		case WEB_IPC_CREATE_PAGE_WITH_VIEW:
			OnCreateWindow(static_cast<WebIpcCreatePageWithView *>(message));
			break;
		case 0x2101:
			if (m_listener != NULL)
				m_listener->OnSlot16(this);
			break;
		case 0x2203: {
			WebIpcEvent2203 *text = static_cast<WebIpcEvent2203 *>(message);

			if (m_ime != NULL)
				ImeTextChanged(m_ime, text->m_second - text->m_first);
			else
				sce_paf_printf_dummy();
			break;
		}
		case 0x2204: {
			WebIpcEvent2204 *pos = static_cast<WebIpcEvent2204 *>(message);

			MoveCursorToPagePos(pos->m_first, pos->m_second);
			break;
		}
		case 0x2205:
			SetCursorType(static_cast<WebIpcEvent2205 *>(message)->m_value);
			break;
		case 0x2206: {
			WebIpcEvent2206 *reply = static_cast<WebIpcEvent2206 *>(message);
			bool release = true;

			m_keyReady = true;
			if (m_keyQueue.size() != 0) {
				WebViewKeyEvent key = m_keyQueue.front();

				m_keyQueue.pop_front();
				SendKey(key.m_keycode, key.m_down);
				release = false;
			}
			if (!reply->m_bool && !m_ime_padding) {
				OnEvent20(reply->m_int[0], reply->m_int[1]);
				if (reply->m_int[1] == 0 && release)
					SendKey(reply->m_int[0], false);
			}
			break;
		}
		case 0x2210:
			RequestGeolocationPermission(
				string(static_cast<WebIpcEvent2210 *>(message)->m_value.c_str()));
			break;
		case 0x2211: {
			string origin(static_cast<WebIpcEvent2211 *>(message)->m_value.c_str());

			if (m_geolocation != NULL)
				m_geolocation->AbortPermission();
			m_geolocationOrigin.assign(NULL, 0);
			KillTimeoutEvent(1);
			break;
		}
		case 0x2212:
			if (m_geolocation != NULL)
				m_geolocation->Start();
			break;
		case 0x2213:
			if (m_geolocation != NULL)
				m_geolocation->Stop();
			break;
		}
		delete message;
	}
}

void WebView::ZoomTo(const math::v2 &pos, float scale)
{
	using namespace sce::Vectormath::Simd;

	Widget *plane = GetViewParent()->FindChild(IDParam("view_plane"), 2);

	if (plane == NULL || !CanZoom())
		return;

	floatInVec ratio = recipf((m_contentSize * m_scale).getImpl().getX()) *
		m_view_box_size.getImpl().getX();
	math::v2 target = pos * recipf(ratio).getAsFloat();
	WebViewAnim tap(target, scale);

	if (__builtin_fabsf(scale - 1.0f) < 0.005f || !m_userScalable) {
		math::v4 planePos = plane->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		Aos::Vector4 to(planePos.getImpl());

		to.setXY((pos * math::v2(-1.0f, -1.0f)).getImpl());

		Widget *internal = GetInternalWidget();

		if (internal == NULL)
			return;
		if (math::v4(to) == internal->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)))
			return;
		m_tapAnims.push_back(tap);
		internal->SetPos(math::v4(to), new Timer(300.0f), Widget::ANIM_SIZE, 0, 0.0f,
			OnZoomDone, this);
	} else {
		Aos::Vector4 size(plane->GetSize(0)->getImpl());

		size.setXY(plane->GetSize(0)->getImpl().getXY() * scale);
		if (math::v4(size) == *plane->GetSize(0))
			return;
		m_tapping = true;
		m_zoomOffset = target * scale * math::v2(ratio.getAsFloat(), ratio.getAsFloat()) -
			GetViewFocus();
		m_zoomAnims.push_back(WebViewAnim(GetViewFocus(), ratio.getAsFloat()));
		m_tapAnims.push_back(tap);
		plane->SetSize(math::v4(size), new Timer(300.0f), Widget::ANIM_SIZE, 0, 0.0f,
			OnZoomDone, this);
	}
}

void WebView::Update()
{
	if (m_ime != NULL)
		ImeUpdate(m_ime);
	if (m_tapAnims.size() == 0 && m_scrollAnims.size() != 0) {
		WebViewAnim &anim = m_scrollAnims.front();
		math::v2 pos = anim.m_pos;
		float scale = anim.m_scale;

		m_scrollAnims.pop_front();
		/* the animation is read after its node is freed, as in the original */
		switch (anim.m_state) {
		case 1:
			Zoom(anim.m_state);
			break;
		case 2:
			ResetZoom(GetViewFocusTarget());
			break;
		default:
			ZoomTo(pos, scale);
			break;
		}
	}

	MessageDialogListener *dialog = wutil::GetMessageDialogListener();

	if (dialog != NULL && !dialog->Method8() && m_pendingDialogs.size() != 0) {
		m_ipcDataLock.Lock();
		for (vector<WebIpcMessage *>::iterator it = m_pendingDialogs.begin();
		     it != m_pendingDialogs.end(); ++it)
			m_ipcData.push_back(*it);
		m_pendingDialogs.clear();
		m_ipcDataLock.Unlock();
	}
	ProcessIpcData();
	ScrollView::Update();
}

int32_t WebView::HandleGestureEvent(Event *e)
{
	uint32_t detail = e->m_detail;

	if (((detail >> 20) & 0xf) == 1) {
		switch ((detail >> 8) & 0x7f) {
		case 2:
			ResetZoom(GetViewFocusTarget());
			break;
		case 14:
			m_scrollAnims.clear();
			m_tapAnims.clear();
			m_zoomAnims.clear();
			break;
		}
	}
	return ScrollView::HandleGestureEvent(e);
}

void WebView::SendTouch(const math::v2 &pos, int action, int port)
{
	using namespace sce::Vectormath::Simd;

	math::v2 &last = m_cursorPos[port];

	if (action == 67 && Aos::allElemLessThanOrEqual(Aos::absPerElem((last - pos).getImpl()),
			Aos::Vector2::zero()).getAsBool())
		return;
	last = pos;
	if (action == 66)
		last = math::v2(-1.0f, -1.0f);

	WebIpcTouch message((int)pos.getImpl().getX().getAsFloat(),
		(int)pos.getImpl().getY().getAsFloat(), action, port);

	WebIpc::GetInstance()->Send(&message, m_viewId);
}

void WebView::Touch(const math::v2 &pos, uint16_t type, uint8_t port)
{
	using namespace sce::Vectormath::Simd;

	m_dragStart = pos;
	m_scrollAnims.clear();
	m_tapAnims.clear();
	m_zoomAnims.clear();
	m_tapping = false;
	m_cursorEnabled = false;
	if (!CanZoom())
		return;

	floatInVec ratio = recipf(m_view_box_size.getImpl().getX()) * m_contentSize.getImpl().getX();
	math::v2 view = ((pos - GetFittedViewFocus(false)) * math::v2(1.0f, -1.0f) +
		math::v2(GetSize(0)->getImpl().getXY()) * 0.5f) * ratio.getAsFloat();
	Aos::Matrix3 transform = GetPageTransform();
	Aos::Vector3 page3 = transform.getCol0() * pos.getImpl().getX() +
		transform.getCol1() * pos.getImpl().getY() +
		Aos::mulPerElem(transform.getCol2(), Aos::Vector3(1.0f));
	math::v2 page(Aos::Vector2(page3.getX(), page3.getY()));

	switch (type) {
	case 0:
		if (!m_pageHandlesInput) {
			if (m_loadingPlane != NULL)
				m_loadingPlane->Flash();

			WebIpcPointerPos message((int32_t)view.extract_x(), (int32_t)view.extract_y());

			WebIpc::GetInstance()->Send(&message, m_viewId);
		}
		break;
	case 2:
		Tap(page, 2);
		break;
	case 4:
		Tap(page, 1);
		break;
	case 13:
		if (m_loadingPlane != NULL)
			m_loadingPlane->Hide();
		break;
	case 15:
		if (m_loadingPlane != NULL)
			m_loadingPlane->Hide();
		break;
	case 19:
		m_tapping = true;
		break;
	case 20:
		m_tapping = true;
		break;
	case 21:
		m_tapping = false;
		m_imeOpen = true;
		break;
	case 25:
		if (m_loadingPlane != NULL)
			m_loadingPlane->Show();
		break;
	case 29:
		m_cursorEnabled = true;
		if (!(m_cursorMove == page)) {
			Tap(page, 3);
			m_cursorMove = page;
		}
		break;
	case 65:
		Tap(page, 3);
		SendTouch(view, type, port);
		break;
	case 66:
		if (m_loadingPlane != NULL)
			m_loadingPlane->Hide();
		SendTouch(view, type, port);
		break;
	case 67:
		if (m_pageHandlesInput || m_contextMenuShown) {
			m_contextMenuShown = false;
			SendTouch(view, type, port);
		}
		break;
	case 71:
		if (m_loadingPlane != NULL)
			m_loadingPlane->Hide();
		SendTouch(view, 66, port);
		break;
	}
}

int32_t WebView::HandleKeycodeEvent(Event *e)
{
	using namespace sce::Vectormath::Simd;

	if (m_cursorPlane == NULL || m_cursorMode == 0 || m_cursorHidden ||
	    m_arrowTexture.get() == NULL)
		return ScrollViewBase::HandleKeycodeEvent(e);
	if (m_cursor_on) {
		bool down = false;
		bool repeat = false;
		bool up = false;

		switch (e->GetType()) {
		case 17:
			down = true;
			break;
		case 18:
			up = true;
			break;
		case 19:
			down = true;
			repeat = true;
			break;
		case 20:
			up = true;
			break;
		}
		switch ((uint8_t)e->m_detail) {
		case 1:
			SendKey(81, down);
			e->Consume();
			break;
		case 2:
			SendKey(54, down);
			e->Consume();
			break;
		case 3:
			SendKey(64, down);
			e->Consume();
			break;
		case 4:
			SendKey(74, down);
			e->Consume();
			break;
		case 27:
			if (!repeat) {
				math::v2 cursor = GetCursorPos(true);
				Aos::Matrix3 transform = GetPageTransform();
				Aos::Vector3 pos = transform.getCol0() * cursor.getImpl().getX() +
					transform.getCol1() * cursor.getImpl().getY() +
					Aos::mulPerElem(transform.getCol2(), Aos::Vector3(1.0f));
				int x = (int32_t)pos.getX().getAsFloat();
				int y = (int32_t)pos.getY().getAsFloat();

				if (m_cursorPlane != NULL) {
					intrusive_ptr<graph::Surface> texture;

					m_cursorPlane->GetTexture(texture);
					if (m_handAltTexture.get() != NULL && m_handTexture.get() != NULL &&
					    (m_handAltTexture.get() == texture.get() ||
					     m_handTexture.get() == texture.get())) {
						if (up)
							m_cursorPlane->SetTexture(m_handTexture);
						else
							m_cursorPlane->SetTexture(m_handAltTexture);
					} else if (m_arrowAltTexture.get() != NULL &&
						   m_arrowTexture.get() != NULL &&
						   (m_arrowAltTexture.get() == texture.get() ||
						    m_arrowTexture.get() == texture.get())) {
						if (up)
							m_cursorPlane->SetTexture(m_arrowTexture);
						else
							m_cursorPlane->SetTexture(m_arrowAltTexture);
					}
				}
				if (up) {
					WebIpcCursorRelease message(x, y);

					WebIpc::GetInstance()->Send(&message, m_viewId);
				} else {
					WebIpcCursorPress message(x, y);

					WebIpc::GetInstance()->Send(&message, m_viewId);
				}
			}
			e->Consume();
			break;
		case 29:
			if (!repeat && !up)
				Touch(GetCursorPos(true), 4, 0);
			break;
		}
	}
	return Widget::HandleKeycodeEvent(e);
}

void WebView::OnZoomDone(Timer *timer, AnimListener *listener, void *arg, void *data,
	void *reserved)
{
	WebView *view = (WebView *)data;

	if (view != NULL)
		view->m_tapAnims.clear();
}

void WebView::OnMessageDialogResult(int result)
{
	MessageDialogListener *dialog = wutil::GetMessageDialogListener();

	if (dialog != NULL && dialog->Method5()) {
		WebIpcSetting message(WEB_IPC_SETTING_1031, false);

		WebIpc::GetInstance()->Send(&message, m_viewId);
	}
	switch (m_dialogParam.values[1]) {
	case 0: {
		WebIpcEvent2013 reply((string()));

		WebIpc::GetInstance()->Post(&reply, m_viewId);
		break;
	}
	case 1: {
		WebIpcEvent2014 reply((string()));

		reply.m_accepted = result != 0;
		WebIpc::GetInstance()->Post(&reply, m_viewId);
		break;
	}
	case 2: {
		string text;

		if (result != 0 && dialog != NULL) {
			const wstring *input = dialog->Method7(0);
			string utf8;

			common::Utf16ToUtf8(*input, &utf8);
			text = utf8;
		} else {
			text = (const char *)NULL;
		}

		WebIpcEvent2015 reply((string()), string(), string());

		reply.m_result = string(text.c_str());
		reply.m_accepted = result != 0;
		WebIpc::GetInstance()->Post(&reply, m_viewId);
		break;
	}
	case 3: {
		WebIpcCreatePageWithView *request = (WebIpcCreatePageWithView *)m_dialogParam.user_data;
		bool opened = false;

		if (result != 0) {
			Url url(m_dialogParam.button.GetString());

			if (m_listener != NULL && request != NULL) {
				WebView *window = NULL;
				bool handled = false;

				m_listener->OnCreateWindow(this, url, request->m_width, request->m_height,
					&window, &handled);
				if (window != NULL) {
					window->m_startWithParam = true;
					if (!handled && window->m_pageState != 3) {
						WebViewListener *listener =
							(WebViewListener *)wutil::GetWebViewListener();

						if (listener != NULL)
							listener->Initialize(window);
						if (window->m_pageState == 0) {
							if (window->m_startWithParam)
								window->CreatePageForView();
							else
								window->CreatePage();
						}
					}
					opened = true;
				}
			}
		}
		if (!opened)
			WebIpc::GetInstance()->Post(request, m_viewId);
		delete request;
		m_dialogParam.user_data = NULL;
		break;
	}
	}
	m_cursorHidden = false;
	ShowCursor();
}

void WebView::OnDialogResult(int result)
{
	WebIpcEvent2034 reply(0);

	reply.m_accepted = result != 0;
	WebIpc::GetInstance()->Post(&reply, m_viewId);
	m_dialogParam.user_data = NULL;
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
	if (result == 0) {
		m_firstLayout = false;
		m_pageInfo = 3;
		m_pageRect = m_lastPageRect;
		m_lastPageRect = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	m_cursorHidden = false;
	ShowCursor();
}

void WebView::OnInputDialogResult(int result, int reopen)
{
	if (reopen != 0) {
		MessageDialogListener *dialog = wutil::GetMessageDialogListener();

		if (dialog != NULL) {
			m_dialogParam.values[0] = 3;
			dialog->Method13(&m_dialogParam, &m_imeTitle, 0x5cc);
		}
	} else {
		WebIpcEvent2033 reply(0, string(), string(), 0.0f, 0.0f, string());

		reply.m_accepted = result != 0;
		WebIpc::GetInstance()->Post(&reply, m_viewId);
	}
	if (result == 0) {
		m_firstLayout = false;
		m_pageInfo = 3;
		m_pageRect = m_lastPageRect;
		m_lastPageRect = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	m_cursorHidden = false;
	ShowCursor();
}

void WebView::OnAuthDialogResult(int result)
{
	string user("");
	string password("");
	MessageDialogListener *dialog = wutil::GetMessageDialogListener();

	if (dialog != NULL) {
		{
			string utf8;

			common::Utf16ToUtf8(*dialog->Method7(0), &utf8);
			user = utf8;
		}
		{
			string utf8;

			common::Utf16ToUtf8(*dialog->Method7(1), &utf8);
			password = utf8;
		}
	}
	{
		WebIpcEvent2025 reply((string()), string(), string(), string());

		reply.SetCredentials(result != 0, user, password);
		WebIpc::GetInstance()->Post(&reply, m_viewId);
		m_dialogParam.user_data = NULL;
	}
	m_cursorHidden = false;
	ShowCursor();
}

/* the use count of the location answers kept */
static uint32_t s_geolocationTime;

void WebView::OnGeolocationDialogResult(int result)
{
	if (!m_geolocationOrigin.empty()) {
		bool allowed = result != 0;

		if (m_geolocationPermissions.size() >= 3) {
			set<WebViewGeolocationPermission>::iterator it = m_geolocationPermissions.begin();
			set<WebViewGeolocationPermission>::iterator oldest = it;

			for (++it; it != m_geolocationPermissions.end(); ++it) {
				if (s_geolocationTime - oldest->m_time < s_geolocationTime - it->m_time)
					oldest = it;
			}
			sce_paf_printf_dummy();
			if (oldest.node != NULL)
				m_geolocationPermissions._Erase(oldest.node);
		}

		WebViewGeolocationPermission permission(m_geolocationOrigin.c_str());

		permission.m_state = allowed ? 1 : 2;
		permission.m_time = s_geolocationTime++;
		m_geolocationPermissions.insert(permission);

		WebIpcGeolocationPermission message(m_geolocationOrigin, allowed);

		WebIpc::GetInstance()->Send(&message, m_viewId);
		m_geolocationOrigin = (const char *)NULL;
	}
	m_cursorHidden = false;
	ShowCursor();
}

void WebView::OnImeUpdatePreedit(const string &text, int caret)
{
	WebIpcImePreedit message(text, caret);

	WebIpc::GetInstance()->Send(&message, m_viewId);
}

void WebView::OnImeCommit(const string &text, int caret)
{
	WebIpcImeCommit message(text, caret);

	WebIpc::GetInstance()->Send(&message, m_viewId);
}

void WebView::OnImeSetText(const string &text)
{
	WebIpcImeSetText message(text);

	WebIpc::GetInstance()->Send(&message, m_viewId);
}

void WebView::OnImeBackspace()
{
	SendKey(50, true);
	SendKey(50, false);
}

void WebView::OnImeDelete()
{
	SendKey(53, true);
	SendKey(53, false);
}

/* the caret moved a step by the IME, sent as the key of the direction */
void WebView::OnImeMoveCaret(int dx, int dy)
{
	int keycode;

	if (dx == 1)
		keycode = 74;
	else if (dx == -1)
		keycode = 64;
	else if (dy == 1)
		keycode = 54;
	else if (dy == -1)
		keycode = 81;
	else
		return;
	SendKey(keycode, true);
	SendKey(keycode, false);
}

void WebView::OnImeClear()
{
	WebIpcMessage message(WEB_IPC_IME_CLEAR);

	WebIpc::GetInstance()->Send(&message, m_viewId);
}

void WebView::OnImePressEnter()
{
	SendKey(56, true);
	SendKey(56, false);
	CloseIme();
}

math::v2 WebView::GetFittedFocus(const math::v2 &pos, float scale)
{
	math::v2 focus = pos;
	const math::v4 *size = GetSize(0);
	math::rect focusRect(focus, math::v2(size->extract_x(), size->extract_y()));
	math::rect viewRect(math::v2(0.0f, 0.0f), math::v2(m_view_box_size) * scale);

	viewRect.set_bottom(viewRect.bottom() - m_padding.extract_w());
	viewRect.set_bottom(viewRect.bottom() - m_ime_height);
	return FitRectCenter(viewRect, focusRect);
}

void WebView::ZoomToIme()
{
	using namespace sce::Vectormath::Simd;

	if (!CanZoom())
		return;

	math::v4 rect = m_imeRect;
	math::v2 center(rect.extract_x() + rect.extract_z() * 0.5f,
		rect.extract_y() + rect.extract_w() * 0.5f);
	Aos::Matrix3 transform = GetViewTransform();
	Aos::Vector3 pos3 = transform.getCol0() * center.getImpl().getX() +
		transform.getCol1() * center.getImpl().getY() +
		Aos::mulPerElem(transform.getCol2(), Aos::Vector3(1.0f));
	math::v2 pos(Aos::Vector2(pos3.getX(), pos3.getY()));
	math::v2 size = math::v2(rect.extract_z(), rect.extract_w()) *
		(recipf(m_contentSize.getImpl().getX()) * m_view_box_size.getImpl().getX()).getAsFloat();
	float height = GetSize(0)->extract_y() - m_ime_height;
	float ratio = (recipf(m_contentSize.getImpl().getX()) *
		m_view_box_size.getImpl().getX()).getAsFloat();
	const math::v4 *viewSize = GetSize(0);
	float scale = fminf(recipf(floatInVec(size.extract_x())) * viewSize->getImpl().getX(),
		floatInVec(height) * recipf(floatInVec(size.extract_y()))).getAsFloat();

	if (ratio * scale > 1.75f)
		scale = 1.75f / ratio;
	if (ratio * scale > m_maxScale)
		scale = m_maxScale / ratio;
	if (scale < 1.0f)
		scale = 1.0f;
	if (ratio > 1.75f)
		scale = 1.0f;
	if ((m_inputMask & 1) == 0 || !m_userScalable)
		scale = 1.0f;

	float x = GetSize(0)->extract_x() - size.extract_x() * scale;

	if (x > 0.0f)
		x = 0.0f;

	math::v2 offset(x, -m_ime_height);
	math::v2 focus = pos + offset * 0.5f * recipf(floatInVec(scale)).getAsFloat();

	if (size.extract_y() > height)
		focus.set_y(focus.extract_y() + (size.extract_y() - height) * 0.5f + 10.0f);

	math::v2 fitted = GetFittedFocus(focus, scale);

	m_tapScale = ratio;
	m_scrollAnims.push_back(WebViewAnim(fitted, scale));
}

void WebView::OnImeOpen(int height)
{
	Widget *cursor = m_cursorPlane;

	m_cursorHidden = true;
	if (cursor != NULL) {
		cursor->m_transition_complete = false;
		cursor->m_transition = 0xff;
		cursor->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
	}
	SetImePadding(true);
	SetImeRect(math::rect(0.0f, 0.0f, 0.0f, (float)(height - 30)));
	ZoomToIme();
}

void WebView::OnImePressClose()
{
	CloseIme();

	WebIpcMessage message(WEB_IPC_IME_CLOSE);

	WebIpc::GetInstance()->Send(&message, m_viewId);
}

/* the position sent from the main thread to the view's page (names ours) */
struct WebViewGeolocationPositionJob {
	int m_viewId;
	WebIpcGeolocationPosition m_position;
};

static void SendGeolocationPosition(void *arg)
{
	WebViewGeolocationPositionJob *job = (WebViewGeolocationPositionJob *)arg;
	WebIpcGeolocationPositionMessage message(job->m_position);

	WebIpc::GetInstance()->Send(&message, job->m_viewId);
	delete job;
}

void WebView::OnPosition(WebIpcGeolocationPosition position)
{
	WebViewGeolocationPositionJob *job = new WebViewGeolocationPositionJob;

	if (job == NULL)
		return;
	job->m_viewId = m_viewId;
	job->m_position.m_timestamp = position.m_timestamp;
	job->m_position.m_latitude = position.m_latitude;
	job->m_position.m_longitude = position.m_longitude;
	job->m_position.m_accuracy = position.m_accuracy;
	job->m_position.m_altitude = position.m_altitude;
	job->m_position.m_altitudeAccuracy = position.m_altitudeAccuracy;
	job->m_position.m_heading = position.m_heading;
	job->m_position.m_speed = position.m_speed;
	job->m_position.m_hasLatitude = position.m_hasLatitude;
	job->m_position.m_hasLongitude = position.m_hasLongitude;
	job->m_position.m_hasAccuracy = position.m_hasAccuracy;
	job->m_position.m_hasAltitude = position.m_hasAltitude;
	job->m_position.m_hasAltitudeAccuracy = position.m_hasAltitudeAccuracy;
	job->m_position.m_hasHeading = position.m_hasHeading;
	job->m_position.m_hasSpeed = position.m_hasSpeed;
	thread::CallQueue::main_thread_callqueue.Enqueue("sendGeolocationPositionMessage",
		SendGeolocationPosition, job);
}

void WebView::CheckGeolocationPermission()
{
	if (m_geolocationOrigin.empty())
		return;

	switch (m_geolocation->GetPermission()) {
	case 2: {
		WebIpcGeolocationPermission message(m_geolocationOrigin, false);

		m_geolocationOrigin = (const char *)NULL;
		WebIpc::GetInstance()->Send(&message, m_viewId);
		return;
	}
	case 0:
		SetTimerListener(CB_TIMEOUT | 1, 300.0f,
			new WebViewMemberListener<WebView>(this, &WebView::CheckGeolocationPermission));
		return;
	}

	/* the origin's answer of before, else the user's */
	WebViewGeolocationPermission key(m_geolocationOrigin.c_str());
	set<WebViewGeolocationPermission>::iterator it = m_geolocationPermissions.find(key);

	if (it != m_geolocationPermissions.end()) {
		it->m_time = s_geolocationTime++;

		WebIpcGeolocationPermission message(m_geolocationOrigin, it->m_state == 1);

		m_geolocationOrigin = (const char *)NULL;
		WebIpc::GetInstance()->Send(&message, m_viewId);
		return;
	}

	MessageDialogListener *dialog = wutil::GetMessageDialogListener();

	if (dialog == NULL)
		return;
	if (dialog->Method8()) {
		SetTimerListener(CB_TIMEOUT | 1, 300.0f,
			new WebViewMemberListener<WebView>(this, &WebView::CheckGeolocationPermission));
		return;
	}
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
	m_dialogParam.owner = static_cast<WebViewDialogClient *>(this);
	m_dialogParam.values[0] = 7;

	Widget *cursor = m_cursorPlane;

	m_cursorHidden = true;
	if (cursor != NULL) {
		cursor->m_transition_complete = false;
		cursor->m_transition = 0xff;
		cursor->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
	}

	common::String origin = string(m_geolocationOrigin);

	dialog->Method15(&m_dialogParam, origin);
}

void WebView::SendString1023(const string &value)
{
	WebIpcString1023 message(value);

	WebIpc::GetInstance()->Send(&message, m_viewId);
}

void WebView::UpdateZoomFocus()
{
	using namespace sce::Vectormath::Simd;

	if (m_tapAnims.size() == 0)
		return;

	Widget *plane = GetViewParent()->FindChild(IDParam("view_plane"), 2);

	if (plane == NULL || !CanZoom())
		return;

	const math::v4 *size = plane->GetSize(0);
	floatInVec ratio = recipf((m_contentSize * m_scale).getImpl().getX()) *
		size->getImpl().getX();
	const WebViewAnim &tap = *m_tapAnims.begin();
	const WebViewAnim &zoom = *m_zoomAnims.begin();
	float start = zoom.m_scale;
	floatInVec step = recipf(floatInVec(tap.m_scale * start - start));
	math::v2 focus = zoom.m_pos + m_zoomOffset * (ratio.getAsFloat() - start) *
		math::v2(step.getAsFloat(), step.getAsFloat());

	Widget *internal = GetInternalWidget();
	Aos::Vector4 to(GetInternalWidget()->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
		.getImpl());

	to.setXY(-focus.getImpl());
	internal->SetPos(math::v4(to));
}

void WebView::HandleAnalog(const inputdevice::Data *data)
{
	using namespace sce::Vectormath::Simd;

	if (m_cursorPlane == NULL || m_cursorMode == 0 || m_cursorHidden || m_arrowTexture.get() == NULL)
		return;

	const inputdevice::pad::Data *pad = data->m_pad_data;
	int lx = pad->lx;
	int ly = pad->ly;
	int rx = pad->rx;
	int ry = pad->ry;
	int dx = lx - inputdevice::pad::Data::STICK_CENTER;
	int dy = ly - inputdevice::pad::Data::STICK_CENTER;

	if (sce_paf_abs(dx) < 37 && sce_paf_abs(dy) < 37)
		return;

	bool rightHeld = false;

	if (sce_paf_abs(rx - inputdevice::pad::Data::STICK_CENTER) > 37 ||
	    sce_paf_abs(ry - inputdevice::pad::Data::STICK_CENTER) > 37)
		rightHeld = true;

	/* the cursor moves: the keys waiting are dropped, a held one released */
	m_keyReady = true;
	if (m_keyQueue.size() != 0) {
		const WebViewKeyEvent &key = *m_keyQueue.begin();

		if (!key.m_down)
			SendKey(key.m_keycode, false);
	}
	m_keyQueue.clear();

	int stepX = dx / 37;
	int stepY = dy / 37;
	bool edgeX = false;
	bool edgeY = false;

	if (lx >= 255)
		edgeX = true;
	if (lx <= 0)
		edgeX = true;
	if (ly >= 255)
		edgeY = true;
	if (ly <= 0)
		edgeY = true;

	SceUInt64 now;
	float speed = 3.0f;

	rtc::GetCurrentTick(&now);
	if (edgeX || edgeY) {
		if (m_analogEdgeTime == 0)
			m_analogEdgeTime = now;
		if (now - m_analogEdgeTime > 1200000)
			speed = 6.0f;
	} else {
		m_analogEdgeTime = 0;
	}

	math::v2 pos = MoveCursorInView(math::v2((float)stepX, (float)stepY) * speed, true,
		!rightHeld);

	MoveCursor(&pos, 0.0f);

	math::v2 cursor = GetCursorPos(true);
	Aos::Matrix3 transform = GetPageTransform();
	Aos::Vector3 page = transform.getCol0() * cursor.getImpl().getX() +
		transform.getCol1() * cursor.getImpl().getY() +
		Aos::mulPerElem(transform.getCol2(), Aos::Vector3(1.0f));

	pos = math::v2(page.getX(), page.getY());
	Tap(pos, 3);
	SendCursorPos();
}

void WebView::SetChosenFile(const common::String &path, const common::String &name)
{
	WebIpcChosenFile message(path.GetString(), name.GetString());

	WebIpc::GetInstance()->Send(&message, m_viewId);
	m_cursorHidden = false;
	ShowCursor();
}

static void OnUrlFieldEnter(int32_t type, Handler *self, Event *e, void *data);
static void OnBackPressed(int32_t type, Handler *self, Event *e, void *data);
static void OnForwardPressed(int32_t type, Handler *self, Event *e, void *data);
static void OnReloadPressed(int32_t type, Handler *self, Event *e, void *data);

int WebView::SetPart(Widget *widget, int part, bool attach)
{
	int ret = 0;

	if (widget == NULL)
		return WEB_VIEW_ERROR_FAILED;

	for (list<WebViewPart>::iterator it = m_parts.begin(); it != m_parts.end(); ++it)
		if (it->m_part == part)
			return 0;

	switch (part) {
	case WEB_VIEW_PART_TITLE:
	case WEB_VIEW_PART_PROGRESS:
		m_parts.push_back(WebViewPart(part, widget, attach));
		break;
	case WEB_VIEW_PART_URL: {
		m_parts.push_back(WebViewPart(part, widget, attach));

		/* the caret takes the field's text color */
		math::v4 color(0.0f, 0.0f, 0.0f, 0.0f);
		Text *field = (Text *)widget;

		if (field != NULL) {
			field->GetStyleAttribute(graph::TextStyleAttribute_Color, 0, color);
			field->SetCaretColor(color);
		}
		widget->AddEventListener(TextBox::CB_TEXT_BOX_ENTER_PRESSED,
			new EventCBListener(OnUrlFieldEnter, this));
		break;
	}
	case WEB_VIEW_PART_BACK:
		m_parts.push_back(WebViewPart(part, widget, attach));
		widget->AddEventListener(ButtonBase::CB_BTN_DECIDE,
			new EventCBListener(OnBackPressed, this));
		break;
	case WEB_VIEW_PART_FORWARD:
		m_parts.push_back(WebViewPart(part, widget, attach));
		widget->AddEventListener(ButtonBase::CB_BTN_DECIDE,
			new EventCBListener(OnForwardPressed, this));
		break;
	case WEB_VIEW_PART_RELOAD:
		m_parts.push_back(WebViewPart(part, widget, attach));
		widget->AddEventListener(ButtonBase::CB_BTN_DECIDE,
			new EventCBListener(OnReloadPressed, this));
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}

/* the URL field's text is loaded when its enter is pressed */
static void OnUrlFieldEnter(int32_t type, Handler *self, Event *e, void *data)
{
	WebView *view = (WebView *)data;

	if (view == NULL)
		return;

	for (list<WebViewPart>::iterator it = view->m_parts.begin(); it != view->m_parts.end();
	     ++it) {
		if (it->m_part != WEB_VIEW_PART_URL)
			continue;

		TextBox *field = (TextBox *)it->m_widget;

		if (field != NULL) {
			field->EndEdit();

			wstring text(L"");

			field->GetString(text);
			view->LoadUrl(Url(text));
		}
		return;
	}
}

static void OnBackPressed(int32_t type, Handler *self, Event *e, void *data)
{
	WebView *view = (WebView *)data;

	if (view != NULL)
		view->GoBack();
}

static void OnForwardPressed(int32_t type, Handler *self, Event *e, void *data)
{
	WebView *view = (WebView *)data;

	if (view != NULL)
		view->GoForward();
}

static void OnReloadPressed(int32_t type, Handler *self, Event *e, void *data)
{
	WebView *view = (WebView *)data;

	if (view != NULL)
		view->Reload();
}

const char *WebViewParam::GetParamName() const
{
	return m_param_name;
}

const char *WebViewParam::ParamName()
{
	return m_param_name;
}

int32_t WebViewParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret == 0 && m_serial != NULL) {
		m_serial->AddParam("home_url", &m_home_url, ResourceParam::SEMANTIC_STRING);
		if (m_serial != NULL) {
			m_serial->AddParam("title_field", &m_title_field, ResourceParam::SEMANTIC_WIDGETREF);
			if (m_serial != NULL) {
				m_serial->AddParam("url_field", &m_url_field,
					ResourceParam::SEMANTIC_WIDGETREF);
				if (m_serial != NULL) {
					m_serial->AddParam("back_btn", &m_back_btn,
						ResourceParam::SEMANTIC_WIDGETREF);
					if (m_serial != NULL) {
						m_serial->AddParam("forward_btn", &m_forward_btn,
							ResourceParam::SEMANTIC_WIDGETREF);
						if (m_serial != NULL) {
							m_serial->AddParam("reload_btn", &m_reload_btn,
								ResourceParam::SEMANTIC_WIDGETREF);
							if (m_serial != NULL)
								m_serial->AddParam("progressbar",
									&m_progressbar,
									ResourceParam::SEMANTIC_WIDGETREF);
						}
					}
				}
			}
		}
	}
	return ret;
}

/* the parts' widgets, found from the top of the view's tree, given to the view, and the home
   page loaded */
int32_t WebViewParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret != 0)
		return ret;

	WebView *view = (WebView *)m_widget;

	if (view == NULL)
		return WEB_VIEW_ERROR_FAILED;

	Widget *root = view;

	for (Widget *parent = view->m_parent; parent != NULL; parent = parent->m_parent)
		root = parent;

	if (m_title_field.GetIDHash() != 0) {
		Widget *widget = root->FindChild(m_title_field, 0);

		if (widget != NULL)
			view->SetPart(widget, WEB_VIEW_PART_TITLE, true);
	}
	if (m_url_field.GetIDHash() != 0) {
		Widget *widget = root->FindChild(m_url_field, 0);

		if (widget != NULL)
			view->SetPart(widget, WEB_VIEW_PART_URL, true);
	}
	if (m_back_btn.GetIDHash() != 0) {
		Widget *widget = root->FindChild(m_back_btn, 0);

		if (widget != NULL)
			view->SetPart(widget, WEB_VIEW_PART_BACK, true);
	}
	if (m_forward_btn.GetIDHash() != 0) {
		Widget *widget = root->FindChild(m_forward_btn, 0);

		if (widget != NULL)
			view->SetPart(widget, WEB_VIEW_PART_FORWARD, true);
	}
	if (m_reload_btn.GetIDHash() != 0) {
		Widget *widget = root->FindChild(m_reload_btn, 0);

		if (widget != NULL)
			view->SetPart(widget, WEB_VIEW_PART_RELOAD, true);
	}
	if (m_progressbar.GetIDHash() != 0) {
		Widget *widget = root->FindChild(m_progressbar, 0);

		if (widget != NULL)
			view->SetPart(widget, WEB_VIEW_PART_PROGRESS, true);
	}
	if (m_home_url != NULL)
		view->LoadUrl(Url(string(m_home_url)));
	return 0;
}

WebViewParam::~WebViewParam()
{
}

void WebView::RequestGeolocationPermission(const string &origin)
{
	WebViewGeolocation *geolocation = m_geolocation;

	if (geolocation == NULL && m_geolocationEnabled) {
		geolocation = CreateGeolocation(this);
		m_geolocation = geolocation;
	}
	if (geolocation == NULL) {
		WebIpcGeolocationPermission message(origin, false);
		WebIpc::GetInstance()->Send(&message, m_viewId);
		return;
	}
	m_geolocationOrigin = origin;
	SetTimerListener(CB_TIMEOUT | 1, 0.0f,
		new WebViewMemberListener<WebView>(this, &WebView::CheckGeolocationPermission));
}

void WebView::SendKey(int keycode, bool down)
{
	/* the keys the cursor moves with stay in the view while it shows a cursor */
	if (m_cursorMode == 1 && (keycode == 74 || keycode == 64 || keycode == 54 ||
		keycode == 81))
		return;

	if (m_keyReady) {
		bool ime = m_ime_padding;

		m_keyReady = false;
		if (down) {
			WebIpcKeyDown message(keycode, ime);
			WebIpc::GetInstance()->Send(&message, m_viewId);
		} else {
			WebIpcKeyUp message(keycode, ime);
			WebIpc::GetInstance()->Send(&message, m_viewId);
		}
		m_lastKey.m_keycode = keycode;
		m_lastKey.m_down = down;
	} else if (m_keyQueue.size() != 0) {
		const WebViewKeyEvent &last = *--m_keyQueue.end();

		/* a key held down repeats: one press waiting is enough */
		if (!down || !last.m_down || keycode != last.m_keycode)
			m_keyQueue.push_back(WebViewKeyEvent(keycode, down));
	} else {
		m_keyQueue.push_back(WebViewKeyEvent(keycode, down));
	}
}

void WebView::SetCursorType(int type)
{
	Widget *cursor = m_cursorPlane;

	if (cursor == NULL || m_cursorMode == 0 || m_cursorHidden ||
	    m_arrowTexture.get() == NULL || m_cursorType == type)
		return;

	m_cursorType = type;
	switch (type) {
	case WEB_VIEW_CURSOR_NONE:
		cursor->m_transition_complete = false;
		cursor->m_transition = 0xff;
		cursor->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
		break;
	case WEB_VIEW_CURSOR_POINTER:
		cursor->SetTexture(m_arrowTexture);
		ShowCursor();
		break;
	case WEB_VIEW_CURSOR_HAND:
		if (m_handTexture.get() != NULL)
			cursor->SetTexture(m_handTexture);
		ShowCursor();
		break;
	default:
		/* every other type shows the arrow too */
		cursor->SetTexture(m_arrowTexture);
		ShowCursor();
		break;
	}
}

__attribute__((noinline)) sce::Vectormath::Simd::Aos::Matrix3 WebView::GetViewTransform()
{
	using namespace sce::Vectormath::Simd;

	Aos::Matrix3 transform = Aos::Matrix3::identity();
	Widget *plane = GetViewParent()->FindChild(IDParam("view_plane"), 2);

	if (!(m_contentSize == math::v2(0.0f, 0.0f)) && plane != NULL) {
		const math::v4 *size = plane->GetSize(0);
		floatInVec rate = recipf(m_contentSize.getImpl().getX());
		floatInVec width = size->getImpl().getX();

		transform = Aos::Matrix3(Aos::Vector3(rate * width, floatInVec(0.0f), floatInVec(0.0f)),
					 Aos::Vector3(floatInVec(0.0f), -width * rate, floatInVec(0.0f)),
					 Aos::Vector3(size->extract_x() * -0.5f,
						      size->extract_y() * 0.5f, 1.0f));
	}
	return transform;
}

void WebView::MoveCursorToPagePos(int x, int y)
{
	using namespace sce::Vectormath::Simd;

	if (m_ime_padding || m_cursorPlane == NULL)
		return;

	if (m_keyQueue.size() != 0) {
		/* the last key sent is let go unless a release of it is queued */
		bool release = true;

		for (list<WebViewKeyEvent>::iterator it = m_keyQueue.begin(); it != m_keyQueue.end();
		     ++it) {
			if (it->m_keycode == m_lastKey.m_keycode) {
				if (!it->m_down || !m_lastKey.m_down) {
					release = true;
					break;
				}
				release = false;
			}
		}
		m_keyQueue.clear();
		if (m_lastKey.m_down && release)
			SendKey(m_lastKey.m_keycode, false);
	}

	float posX = (float)x;
	float posY = (float)y;
	math::v4 visible = GetVisiblePageRect();

	if (posX < visible.extract_x())
		posX = visible.extract_x();
	else if (visible.extract_x() + visible.extract_z() < posX)
		posX = visible.extract_x() + visible.extract_z();
	if (posY < visible.extract_y())
		posY = visible.extract_y();
	else if (visible.extract_y() + visible.extract_w() < posY)
		posY = visible.extract_y() + visible.extract_w();

	Aos::Matrix3 transform = GetViewTransform();
	Aos::Vector3 world = transform.getCol0() * posX + transform.getCol1() * posY +
		Aos::mulPerElem(transform.getCol2(), Aos::Vector3(1.0f));
	math::v2 pos = math::v2(Aos::Vector2(world.getX(), world.getY())) - GetViewFocusTarget();

	MoveCursor(&pos, 100.0f);
}

void WebView::ClearSurface()
{
	m_pageState = 0;
	m_surface.clear();

	intrusive_ptr<graph::Surface> none;

	SetTexture(none);
	OnLoadFinished();
}

void WebView::SetGeolocationEnabled(bool enabled)
{
	m_geolocationEnabled = enabled;
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
		sizeof(WebViewRenderParam), m_geolocationEnabled);

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
		sizeof(WebViewRenderParam), m_geolocationEnabled);

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

void WebView::RemovePart(int part)
{
	list<WebViewPart>::iterator it = m_parts.begin();

	if (part == WEB_VIEW_PART_ALL) {
		for (; it != m_parts.end(); ++it)
			if (it->m_attach && it->m_widget != NULL)
				it->m_widget->DestroyWidget();
		m_parts.clear();
		return;
	}
	for (; it != m_parts.end(); ++it) {
		if (it->m_part == part) {
			if (it->m_attach && it->m_widget != NULL)
				it->m_widget->DestroyWidget();
			m_parts.erase(it);
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
	if (m_geolocation != NULL) {
		DestroyGeolocation(m_geolocation);
		m_geolocation = NULL;
	}
	ClearIpcData();
	if (m_receiveBuffer != NULL)
		delete m_receiveBuffer;
	m_receiveBuffer = NULL;
}

const char WebViewStyleParam::m_param_name[] = "style_web_view";

const char *WebViewStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *WebViewStyleParam::ParamName()
{
	return m_param_name;
}

/* the page listener's defaults: it hears nothing */
WebViewPageListener::WebViewPageListener()
{
}

WebViewPageListener::~WebViewPageListener()
{
}

void WebViewPageListener::OnPageReset(WebView *view)
{
}

void WebViewPageListener::OnLoadFinished(WebView *view, int state)
{
}

void WebViewPageListener::OnLoadFailed(WebView *view, int error, const common::String &url)
{
}

void WebViewPageListener::OnLoadCommitted(WebView *view)
{
}

void WebViewPageListener::OnPageLayout(WebView *view)
{
}

void WebViewPageListener::OnLoadProgress(WebView *view)
{
}

void WebViewPageListener::OnTitleChanged(WebView *view, const common::String &title)
{
}

void WebViewPageListener::OnUrlChanged(WebView *view, const Url &url)
{
}

void WebViewPageListener::OnEvent8(WebView *view, int arg)
{
}

void WebViewPageListener::OnPagePos(WebView *view, const math::v2 &pos)
{
}

void WebViewPageListener::OnPageTop(WebView *view, const float &top)
{
}

void WebViewPageListener::OnSlot13()
{
}

void WebViewPageListener::OnContextMenu(WebView *view, const ContextMenuInfo &info)
{
}

void WebViewPageListener::OnCreateWindow(WebView *view, const Url &url, int width, int height, WebView **window, bool *handled)
{
}

void WebViewPageListener::OnSlot16(WebView *view)
{
}

void WebViewPageListener::OnEvent11(WebView *view, int arg0, int arg1)
{
}

void WebViewPageListener::OnEvent12(WebView *view, const Url &url)
{
}

void WebViewPageListener::OnNavigationRequest(WebView *view, const Url &url, int type, bool *allow)
{
}

void WebViewPageListener::OnDownloadRequest(WebView *view, const Url &url, const string &mimeType, int64_t length)
{
}

void WebViewPageListener::OnCursorKey(WebView *view, int direction, bool up)
{
}

void WebViewPageListener::OnEvent15(WebView *view, int arg)
{
}

void WebViewPageListener::OnEvent16(WebView *view)
{
}

void WebViewPageListener::OnHistoryChanged(WebView *view, bool canGoBack, bool canGoForward)
{
}

void WebViewPageListener::OnEvent18(WebView *view, int arg0, int arg1)
{
}

void WebViewPageListener::OnImeClosed(WebView *view, int arg0, int arg1)
{
}

void WebViewPageListener::OnConfirm(WebView *view, const common::String &text)
{
}

WebViewListener::WebViewListener()
{
}

WebViewListener::~WebViewListener()
{
}

WebViewDataListener::WebViewDataListener()
{
}

WebViewDataListener::~WebViewDataListener()
{
}

void WebViewAnalogListener::OnUpdate(const inputdevice::Data *data)
{
	WebView *view = (WebView *)m_binded_widget[0];

	if (view != NULL && data != NULL)
		view->HandleAnalog(data);
}

/* the web view type's factories, for paf's factory table */
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

static VersatileParam *NewWebViewFocusParam()
{
	return new WidgetFocusParam;
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

}
}

paf::ui::FactoryFuncTable g_factoryWebView = {
	paf::ui::WebView::TypeName(),
	paf::ui::NewWebView,
	paf::ui::NewWebViewParam,
	paf::ui::NewWebViewLayoutParam,
	paf::ui::NewWebViewFocusParam,
	paf::ui::NewWebViewStyleParam,
	paf::ui::NewWebViewHandlerParam,
	paf::ui::NewWebViewSoundParam,
};
