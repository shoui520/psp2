/*
 * paf::ui::WebRenderer implementation for 2014 PAF WebView/MapView.
 */
#include <widget/web_renderer.h>
#include <graphics/drawobj/web_bg_obj.h>
#include <graphics/drawobj/web_obj.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <std/math.h>

namespace paf {
namespace ui {

using sce::Vectormath::Simd::Aos::Matrix3;
using sce::Vectormath::Simd::Aos::Vector3;

WebRenderer::WebRenderer()
	: m_widget_to_doc(Matrix3::identity()),
	  m_doc_to_widget(Matrix3::identity()),
	  m_doc_scale(Matrix3::identity()),
	  m_doc_size(0.0f, 0.0f),
	  m_widget_size(0.0f, 0.0f),
	  m_document_client(NULL),
	  m_web_obj(NULL),
	  m_bg_obj(NULL),
	  m_viewport(NULL),
	  m_frame(NULL),
	  m_view(NULL),
	  m_max_surface_size(0.0f, 0.0f),
	  m_last_surface_rect()
{
	m_last_surface_rect.Set(0.0f, 0.0f, 0.0f, 0.0f);
	m_bg_obj = new graph::WebBGObj(graph::PlaneObjStyle());
}

WebRenderer::~WebRenderer()
{
}

int32_t WebRenderer::UpdateTransform()
{
	float doc_w = m_doc_size.extract_x();
	float doc_h = m_doc_size.extract_y();
	float widget_w = m_widget_size.extract_x();
	float widget_h = m_widget_size.extract_y();

	if (doc_w != 0.0f && doc_h != 0.0f && widget_w != 0.0f && widget_h != 0.0f) {
		float scale_w2d_x = doc_w / widget_w;
		float scale_w2d_y = -doc_h / widget_h;
		float scale_d2w_x = widget_w / doc_w;
		float scale_d2w_y = widget_h / doc_h;

		m_widget_to_doc.setCol0(Vector3(scale_w2d_x, 0.0f, 0.0f));
		m_widget_to_doc.setCol1(Vector3(0.0f, scale_w2d_y, 0.0f));
		m_widget_to_doc.setCol2(Vector3(doc_w * 0.5f, doc_h * 0.5f, 1.0f));

		m_doc_to_widget.setCol0(Vector3(scale_d2w_x, 0.0f, 0.0f));
		m_doc_to_widget.setCol1(Vector3(0.0f, -scale_d2w_y, 0.0f));
		m_doc_to_widget.setCol2(Vector3(widget_w * -0.5f, widget_h * 0.5f, 1.0f));

		m_doc_scale.setCol0(Vector3(scale_d2w_x, 0.0f, 0.0f));
		m_doc_scale.setCol1(Vector3(0.0f, scale_d2w_y, 0.0f));
		m_doc_scale.setCol2(Vector3(0.0f, 0.0f, 1.0f));
	} else {
		m_widget_to_doc = Matrix3::identity();
		m_doc_to_widget = Matrix3::identity();
	}

	return 0;
}

int32_t WebRenderer::SetWidgetSize(math::v2 size)
{
	m_widget_size = size;
	if (size.extract_x() > 0.0f && size.extract_y() > 0.0f)
		return UpdateTransform();
	return (int32_t)0x80AF008E;
}

int32_t WebRenderer::Initialize(DocumentSurfaceClient *document_client, math::v2 init_size,
				WebRect surface_rect, WebRect view_rect,
				int32_t tex_width, int32_t tex_height,
				WebCoreView *view, WebCoreViewport *viewport,
				WebRenderClient **out_render_client, WebCoreFrame *frame,
				WebSurfaceHost **out_surface_host, graph::WebObj *web_obj)
{
	int32_t ret = (int32_t)0x80AF0001;

	if (document_client == NULL || view == NULL || viewport == NULL || frame == NULL)
		return ret;

	m_document_client = document_client;
	SetWidgetSize(init_size);

	Vector3 doc_extent = m_widget_to_doc *
			     Vector3(init_size.extract_x(), init_size.extract_y(), 1.0f);

	m_view = view;
	m_viewport = viewport;
	m_frame = frame;
	m_web_obj = web_obj;
	m_doc_size = math::v2(doc_extent.getX(), doc_extent.getY());
	m_max_surface_size = math::v2((float)tex_width, (float)tex_height);

	if (web_obj == NULL)
		return (int32_t)0x80AF008E;

	math::v2 initial_origin(0.0f, 0.0f);
	ret = web_obj->Initialize(&initial_origin, tex_width, tex_height,
				  &surface_rect, &view_rect);
	if (ret == 0) {
		*out_render_client = this;
		*out_surface_host = this;
		return document_client->OnDocumentSizeChanged(m_doc_size);
	}
	return ret;
}

void WebRenderer::Finalize()
{
	if (m_view != NULL)
		m_view = NULL;
	if (m_viewport != NULL) {
		m_viewport->Release();
		m_viewport = NULL;
	}
	if (m_frame != NULL) {
		m_frame->Release();
		m_frame = NULL;
	}
	if (m_document_client != NULL)
		m_document_client = NULL;
	if (m_web_obj != NULL)
		m_web_obj = NULL;
	if (m_bg_obj != NULL)
		m_bg_obj = NULL;

	m_widget_to_doc = Matrix3::identity();
	m_doc_to_widget = Matrix3::identity();
	m_doc_size = math::v2(0.0f, 0.0f);
	m_widget_size = math::v2(0.0f, 0.0f);
}

math::v2 WebRenderer::ClampZoomPlaneCenter(math::v2 center, math::v2 size)
{
	math::rect zoom_rect(center, size);
	float half_w = m_widget_size.extract_x() * 0.5f;
	float half_h = m_widget_size.extract_y() * 0.5f;
	math::v3 obj_size = m_web_obj->m_size;

	if (zoom_rect.width() < obj_size.extract_x() &&
	    zoom_rect.height() < obj_size.extract_y()) {
		if (zoom_rect.left() < -half_w)
			zoom_rect.align_left(-half_w);
		if (zoom_rect.top() > half_h)
			zoom_rect.align_top(half_h);
		if (zoom_rect.right() > half_w)
			zoom_rect.align_right(half_w);
		if (zoom_rect.bottom() < -half_h)
			zoom_rect.align_bottom(-half_h);
	}
	return zoom_rect.center();
}

void WebRenderer::UpdateZoomPlane(const WebRect &view_doc_rect)
{
	graph::PlaneObj *zoom_plane = m_web_obj->GetZoomPlane();
	WebRect origin_rect = m_web_obj->GetZoomOriginRect();
	WebRect surface_rect = m_web_obj->GetSurfaceRect();

	float zoom_scale = origin_rect.Width() / view_doc_rect.Width();
	uint32_t tex_width = 0;
	uint32_t tex_height = 0;

	m_web_obj->GetTextureSize(&tex_width, &tex_height);

	math::v2 plane_size((float)tex_width * zoom_scale, (float)tex_height * zoom_scale);
	zoom_plane->SetSize(math::v3(plane_size.extract_x(), plane_size.extract_y(), 1.0f));

	float surface_w = surface_rect.Width();
	float surface_h = surface_rect.Height();
	float view_center_x = view_doc_rect.m_left + view_doc_rect.Width() * 0.5f;
	float view_center_y = view_doc_rect.m_bottom + view_doc_rect.Height() * 0.5f;

	float rel_x = (1.0f - (view_center_x - surface_rect.m_left) / surface_w) - 0.5f;
	float rel_y = (1.0f - (view_center_y - surface_rect.m_bottom) / surface_h) - 0.5f;

	math::v3 web_pos = m_web_obj->m_pos;
	math::v2 desired_pos(web_pos.extract_x() + rel_x * plane_size.extract_x(),
			     web_pos.extract_y() - rel_y * plane_size.extract_y());
	math::v2 clamped_pos = ClampZoomPlaneCenter(desired_pos, plane_size);

	zoom_plane->SetPos(math::v3(clamped_pos.extract_x(), clamped_pos.extract_y(), 1.0f));
}

int32_t WebRenderer::SetVisibleRect(const WebRect &widget_rect, WebRect *out_doc_rect, bool commit)
{
	int32_t ret = (int32_t)0x80AF008E;

	Vector3 top_left = m_widget_to_doc *
			   Vector3(widget_rect.m_left, widget_rect.m_bottom, 1.0f);
	Vector3 bottom_right = m_widget_to_doc *
			       Vector3(widget_rect.m_right, widget_rect.m_top, 1.0f);

	float doc_left = top_left.getX();
	float doc_top = top_left.getY();

	if (doc_left < 0.0f)
		doc_left = 0.0f;
	if (doc_top < 0.0f)
		doc_top = 0.0f;

	WebRect doc_rect;
	doc_rect.Set(doc_left, doc_top, bottom_right.getX(), bottom_right.getY());
	*out_doc_rect = doc_rect;

	if (doc_rect.Width() > 0.0f && doc_rect.Height() > 0.0f) {
		if (m_web_obj != NULL) {
			uint32_t tex_width = 0;
			uint32_t tex_height = 0;

			m_web_obj->GetTextureSize(&tex_width, &tex_height);

			float tex_w = (float)tex_width;
			if (doc_rect.Width() + 0.005f > tex_w)
				doc_rect.m_right = doc_rect.m_left + tex_w;

			math::v3 obj_size = m_web_obj->m_size;
			float max_h = (tex_w / obj_size.extract_x()) * obj_size.extract_y();
			if (doc_rect.Height() + 0.005f > max_h)
				doc_rect.m_top = doc_rect.m_bottom + max_h;

			m_bg_obj->UpdateTexRegion(doc_rect);
			ret = m_web_obj->SetViewRect(doc_rect, !commit);
			if (m_web_obj->GetZoomPlane() != NULL)
				UpdateZoomPlane(doc_rect);

			if (ret == 0 && m_view != NULL && commit) {
				ret = m_view->SetVisibleRect(doc_rect.m_left, doc_rect.m_bottom,
							     doc_rect.Width(), doc_rect.Height());
			}
		} else {
			ret = 0;
		}
	}

	return ret;
}

int32_t WebRenderer::SetRenderedRect(const WebRect &widget_rect, WebRect *out_doc_rect,
				     bool commit, bool skip_clear)
{
	int32_t ret = (int32_t)0x80AF008E;

	Vector3 top_left = m_widget_to_doc *
			   Vector3(widget_rect.m_left, widget_rect.m_bottom, 1.0f);
	Vector3 bottom_right = m_widget_to_doc *
			       Vector3(widget_rect.m_right, widget_rect.m_top, 1.0f);

	float doc_left = top_left.getX();
	float doc_top = top_left.getY();

	if (doc_left < 0.0f)
		doc_left = 0.0f;
	if (doc_top < 0.0f)
		doc_top = 0.0f;

	WebRect doc_rect;
	doc_rect.Set(doc_left, doc_top, bottom_right.getX(), bottom_right.getY());
	*out_doc_rect = doc_rect;

	if (doc_rect.Width() > 0.0f && doc_rect.Height() > 0.0f &&
	    m_web_obj != NULL && m_view != NULL) {
		if (doc_rect.ApproxEquals(m_last_surface_rect)) {
			m_web_obj->SetZoomState(0);
			ret = 0;
		} else {
			uint32_t tex_width = 0;
			uint32_t tex_height = 0;

			m_web_obj->GetTextureSize(&tex_width, &tex_height);

			float tex_w = (float)tex_width;
			float tex_h = (float)tex_height;
			math::v3 obj_size = m_web_obj->m_size;
			float scale_x = tex_w / obj_size.extract_x();

			if (doc_rect.Width() + 0.005f > scale_x * tex_w)
				doc_rect.m_right = doc_rect.m_left + scale_x * tex_w;

			scale_x = tex_w / m_web_obj->m_size.extract_x();
			if (doc_rect.Height() + 0.005f > scale_x * tex_h)
				doc_rect.m_top = doc_rect.m_bottom + scale_x * tex_h;

			if (commit)
				m_web_obj->SetZoomState(3);

			ret = m_web_obj->SetSurfaceRect(doc_rect, skip_clear);
			if (ret == 0 && commit) {
				m_last_surface_rect = doc_rect;
				ret = m_view->SetRenderedRect(tex_width, tex_height,
							      doc_rect.m_left, doc_rect.m_bottom,
							      doc_rect.Width(), doc_rect.Height());
			}
		}
	}

	return ret;
}

int32_t WebRenderer::SuspendSurface()
{
	int32_t ret = (int32_t)0x80AF0001;

	if (m_view != NULL) {
		ret = m_view->SetRenderedRect(0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
		if (ret == 0)
			m_web_obj->DetachSurface();
	}
	return ret;
}

int32_t WebRenderer::ResumeSurface()
{
	int32_t ret = (int32_t)0x80AF0001;

	if (m_web_obj != NULL) {
		int32_t width = 0;
		int32_t height = 0;

		m_web_obj->EnsureSurface(&width, &height);
		if (m_view != NULL)
			return m_view->ResumeSurface(width, height);
	}
	return ret;
}

int32_t WebRenderer::HandleMouseEvent(int32_t event_type, int32_t pos_x, int32_t pos_y,
				      int32_t button, int32_t click_count,
				      int32_t modifiers, int32_t extra)
{
	int32_t ret = (int32_t)0x80AF0001;

	if (m_view != NULL) {
		Vector3 doc_pos = m_widget_to_doc * Vector3((float)pos_x, (float)pos_y, 1.0f);
		int32_t doc_x = (int32_t)sce_paf_roundf(doc_pos.getX());
		int32_t doc_y = (int32_t)sce_paf_roundf(doc_pos.getY());

		ret = m_view->HandleMouseEvent(event_type, doc_x, doc_y,
					       button, click_count, modifiers, extra);
	}
	return ret;
}

int32_t WebRenderer::HandleKeyEvent(int32_t key_code)
{
	int32_t char_code = (key_code == 13) ? 13 : 0;

	m_view->HandleKeyEvent(0, key_code, char_code, 0);
	m_view->HandleKeyEvent(2, key_code, char_code, 0);
	return m_view->HandleKeyEvent(1, key_code, char_code, 0);
}

graph::WebObj *WebRenderer::ResizeWebObj(math::v2 size)
{
	if (m_web_obj != NULL) {
		math::v3 cur_size = m_web_obj->m_size;
		m_web_obj->SetSize(math::v3(size.extract_x(), size.extract_y(),
					    cur_size.extract_z()));
	}
	return m_web_obj;
}

int32_t WebRenderer::CreateRenderSurface(int32_t width, int32_t height,
					 WebTileObserver **out_observer)
{
	int32_t ret = m_document_client->AttachDrawObj(m_bg_obj, 2);
	graph::WebObj *web_obj = ResizeWebObj(math::v2((float)width, (float)height));

	if (web_obj != NULL && m_document_client != NULL) {
		ret = m_document_client->AttachDrawObj(web_obj, 1);
		if (ret == 0)
			*out_observer = static_cast<WebTileObserver *>(m_web_obj);
	}
	return ret;
}

int32_t WebRenderer::OnDocumentSizeChanged(int32_t width, int32_t height)
{
	int32_t ret = (int32_t)0x80AF0001;
	math::v2 doc_size((float)width, (float)height);
	math::v2 clamped(doc_size.extract_x() < m_max_surface_size.extract_x()
				 ? doc_size.extract_x()
				 : m_max_surface_size.extract_x(),
			 doc_size.extract_y() < m_max_surface_size.extract_y()
				 ? doc_size.extract_y()
				 : m_max_surface_size.extract_y());

	m_doc_size = doc_size;
	if (m_web_obj != NULL)
		m_web_obj->ResizeSurface((uint32_t)clamped.extract_x(),
					 (uint32_t)clamped.extract_y());

	if (m_document_client != NULL) {
		ret = m_document_client->OnDocumentSizeChanged(m_doc_size);
		UpdateTransform();
	}
	return ret;
}

int32_t WebRenderer::ResetSurface()
{
	m_last_surface_rect.Set(0.0f, 0.0f, 0.0f, 0.0f);
	m_web_obj->ClearSurfaceWhite();
	if (m_document_client != NULL)
		return m_document_client->OnSurfaceReset();
	return (int32_t)0x80AF0001;
}

void WebRenderer::SetMaxSurfaceSize(math::v2 max_size)
{
	m_max_surface_size = max_size;
}

}
}
