/*
 * paf's web view widget (the file's name is ours).
 */
#include <stddef.h>
#include <scetypes.h>
#include "web_view.h"

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
	  m_surfaceWidth(0), m_surfaceHeight(0), m_tileBackend(NULL), m_isSecure(false),
	  m_cursorType(0), m_popupMenu(NULL), m_loadProgress(0), m_loadState(1),
	  m_loadCanceled(true), m_canGoBack(false), m_canGoForward(false), m_imeType(-1),
	  m_imeMaxLength(0), m_imeOpen(false), m_imeEnabled(true), m_contentScale(0.0f, 0.0f),
	  m_scale(1.0f), m_maxScale(5.0f), m_minScale(0.25f), m_userScalable(true), m_scrollX(0),
	  m_imeState(0), m_scrollY(0), m_pageRect(0.0f, 0.0f, 0.0f, 0.0f),
	  m_lastPageRect(0.0f, 0.0f, 0.0f, 0.0f), m_zoomed(true),
	  m_zoomX(0.0f), m_zoomY(0.0f), m_zoomRect(0.0f, 0.0f, 0.0f, 0.0f),
	  m_contextMenuShown(false), m_imeMinDate(0, 0, 0, 0, 0, 0, 0),
	  m_imeMaxDate(0, 0, 0, 0, 0, 0, 0), m_selectCount(0), m_selectFocus(0),
	  m_selectMultiple(false), m_startWithParam(false), m_tapRect(0.0f, 0.0f, 0.0f, 0.0f),
	  m_tapScale(1.0f), m_tapping(false), m_tapButton(false),
	  m_inputMask(15), m_dragStart(0.0f, 0.0f), m_cursorMove(0.0f, 0.0f),
	  m_cursorEnabled(false), m_reserved1920(0), m_reserved1924(0), m_cursorPlane(NULL), m_cursorMode(0), m_cursorHidden(false),
	  m_cursorOffset(0.0f, 0.0f), m_reserved2008(0.0f, 0.0f), m_keyReady(true)
{
	m_ipcData.clear();
	m_dialogState = 0;
	m_dialogButtons = 0;
	m_dialogResult = 0;
	m_dialogFlags = 0;
	m_dialogMessage = common::String(L"");
	m_dialogDefault = common::String(L"");
	m_dialogType = 0;
	m_dialogId = 0;
	m_dialogInput = common::String(L"");
	m_dialogReply = 0;
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

	for (vector<IpcData *>::iterator it = m_ipcData.begin(); it != m_ipcData.end(); ++it)
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
