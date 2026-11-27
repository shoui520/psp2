/*
 * paf::ui::MapCanvas ("map_canvas") implementation for 2014 PAF MapView.
 */
#include <widget/map_canvas.h>
#include <widget/js_document.h>

namespace paf {
namespace ui {

const char MapCanvas::m_widget_type[] = "map_canvas";

const char *MapCanvas::TypeName()
{
	return m_widget_type;
}

bool MapCanvas::IsInherit(const char *type) const
{
	if (type == m_widget_type)
		return true;
	return Widget::IsInherit(type);
}

char *MapCanvas::GetTypeName() const
{
	return const_cast<char *>(m_widget_type);
}

MapCanvas::MapCanvas(Widget *parent, CreateParam *param)
	: MapCallbackBase(),
	  Widget(parent, param),
	  WebController(),
	  m_canvas_observer(),
	  m_map_core(NULL)
{
	m_state = (m_state & ~0xFU) | 1U;

	math::v4 parent_size = *parent->GetSize(GET_CURRENT);

	SetSize(parent_size, NULL, ANIM_SIZE, 0, 0.0f, NULL);
}

MapCanvas::~MapCanvas()
{
	m_map_core = NULL;
}

int32_t MapCanvas::AttachCore(MapCore *core)
{
	m_map_core = core;
	m_canvas_observer.m_map_core = core;
	SetObserver(&m_canvas_observer);
	Wakeup();
	return 0;
}

int32_t MapCanvas::DetachCore()
{
	return 0;
}

void MapCanvas::UpdateCanvasSize(math::v4 size)
{
	SetSize(size, NULL, ANIM_SIZE, 0, 0.0f, NULL);
}

void MapCanvas::OnJavaScriptConfirm(const char *message, uint32_t len, void *request_handle)
{
	int32_t handled = m_map_core->DispatchJsResult(message, len);

	m_browser_component->ReplyJavaScriptConfirm(handled != 0, request_handle);
}

int32_t MapCanvas::OnBrowserComponentCreated(WbcBrowserComponent *component)
{
	WebController::OnBrowserComponentCreated(component);

	int32_t width = (int32_t)Widget::GetSize(GET_CURRENT)->extract_x();
	int32_t height = (int32_t)Widget::GetSize(GET_CURRENT)->extract_y();

	return component->SetViewportSize(width, height);
}

int32_t MapCanvas::GetDocumentSize()
{
	return (int32_t)0x80AF0001;
}

void MapCanvas::OnMapStartLoad()
{
	if (m_map_core != NULL) {
		MapCanvasEvent ev(2, 0);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvas::OnMapFinishLoad()
{
	if (m_map_core != NULL) {
		MapCanvasEvent ev(3, 0);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvas::OnLoadError(int32_t error_code, int32_t detail)
{
	(void)error_code;
	if (m_map_core != NULL) {
		MapCanvasEvent ev(1, detail);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvas::OnRedirect()
{
	if (m_map_core != NULL) {
		MapCanvasEvent ev(2, 0);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvas::OnBrowserComponentReady()
{
	if (m_map_core != NULL) {
		MapCanvasEvent ev(0, 0);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvas::OnSecurityStatusChanged()
{
}

void MapCanvas::OnTitleChanged()
{
}

void MapCanvas::OnLocationChanged()
{
}

void MapCanvas::OnProgressChanged()
{
}

void MapCanvas::OnScrollOffsetChanged()
{
}

void MapCanvas::OnFaviconChanged()
{
}

void MapCanvas::OnContextMenu()
{
}

void MapCanvas::OnOpenNewWindow()
{
}

void MapCanvas::OnWindowCloseRequested()
{
	if (m_map_core != NULL) {
		MapCanvasEvent ev(1, 250);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvas::OnOpenNewWindowWithTarget()
{
	if (m_map_core != NULL) {
		MapCanvasEvent ev(1, 255);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvas::OnNavigationStateChanged()
{
}

void MapCanvas::OnSelectPopupClosed()
{
}

void MapCanvas::OnHistoryChanged()
{
}

void MapCanvas::OnJsDialogResult(int32_t result)
{
	(void)result;
}

void MapCanvas::OnNavigationWarningResult(int32_t result)
{
	(void)result;
}

void MapCanvas::OnConfirmPluginResult(int32_t result)
{
	(void)result;
}

void MapCanvas::OnBadCertificateResult(int32_t result, int32_t show_detail)
{
	(void)result;
	(void)show_detail;
}

void MapCanvas::OnBasicAuthResult(int32_t result)
{
	(void)result;
}

void MapCanvas::OnNormalDialogResult(int32_t result)
{
	(void)result;
}

int32_t MapCanvas::CreateDocumentWidget(void *ime_session, WebCoreView *view,
					WebCoreViewport *viewport,
					WebRenderClient **out_render_client,
					WebCoreFrame *frame,
					WebSurfaceHost **out_surface_host)
{
	(void)ime_session;
	Widget::GetSize(GET_CURRENT);
	Widget::GetPos(GET_CURRENT, 0, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	JsDocument *doc = new JsDocument(this, NULL);

	if (doc == NULL)
		return 16;

	int32_t ret = doc->Initialize(view, viewport, out_render_client, frame,
				      out_surface_host, m_map_core);

	doc->StartRender();
	m_document = reinterpret_cast<DocumentWidget *>(doc);
	return ret;
}

}
}
