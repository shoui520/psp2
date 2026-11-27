/*
 * paf::ui::MapCanvas ("map_canvas"): bridges MapCore with WebController and
 * JsDocument for 2014 PAF MapView.
 */
#ifndef PAF_WIDGET_MAP_CANVAS_H
#define PAF_WIDGET_MAP_CANVAS_H

#include <stdint.h>
#include <math/ngp/v4.h>
#include <widget/core/widget.h>
#include <widget/map_core.h>
#include <widget/web_controller.h>

namespace paf {
namespace ui {

class JsDocument;

class MapCanvas : public MapCallbackBase, public Widget, public WebController {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	MapCanvas(Widget *parent, CreateParam *param);
	virtual ~MapCanvas();

	/* MapCallbackBase overrides. */
	virtual void OnStartLoad();
	virtual void OnFinishLoad();
	virtual void OnLoadError(int32_t error_code, int32_t detail);
	virtual void OnRedirect();
	virtual void OnBrowserComponentReady();
	virtual void OnSecurityStatusChanged();
	virtual void OnTitleChanged();
	virtual void OnLocationChanged();
	virtual void OnProgressChanged();
	virtual void OnScrollOffsetChanged();
	virtual void OnFaviconChanged();
	virtual void OnContextMenu();
	virtual void OnOpenNewWindow();
	virtual void OnWindowCloseRequested();
	virtual void OnOpenNewWindowWithTarget();
	virtual void OnNavigationStateChanged();
	virtual void OnSelectPopupClosed();
	virtual void OnHistoryChanged();

	/* Widget overrides. */
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;

	/* WebController / WebCorePageClient / WebCoreDocumentHost overrides. */
	virtual void OnJavaScriptConfirm(const char *message, uint32_t len,
					 void *request_handle);
	virtual int32_t OnBrowserComponentCreated(WbcBrowserComponent *component);
	virtual int32_t GetDocumentSize();

	virtual void OnJsDialogResult(int32_t result);
	virtual void OnNavigationWarningResult(int32_t result);
	virtual void OnConfirmPluginResult(int32_t result);
	virtual void OnBadCertificateResult(int32_t result, int32_t show_detail);
	virtual void OnBasicAuthResult(int32_t result);
	virtual void OnNormalDialogResult(int32_t result);

	virtual int32_t CreateDocumentWidget(void *ime_session, WebCoreView *view,
					     WebCoreViewport *viewport,
					     WebRenderClient **out_render_client,
					     WebCoreFrame *frame,
					     WebSurfaceHost **out_surface_host);

	int32_t AttachCore(MapCore *core);
	int32_t DetachCore();
	void UpdateCanvasSize(math::v4 size);

	MapCanvasObserver m_canvas_observer;
	MapCore *m_map_core;
};

}
}

#endif
