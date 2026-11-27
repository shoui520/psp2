/*
 * paf::graph::WebBGObj implementation for 2014 PAF WebRenderer.
 */
#include <graphics/drawobj/web_bg_obj.h>
#include <vectormath.h>

namespace paf {
namespace graph {

using sce::Vectormath::Simd::Aos::Matrix3;
using sce::Vectormath::Simd::Aos::Vector3;

const char WebBGObj::m_drawobj_type[] = "WebBGObj";

const char *WebBGObj::DrawObjType()
{
	return m_drawobj_type;
}

bool WebBGObj::IsInherit(const char *type) const
{
	if (type == m_drawobj_type)
		return true;
	return PlaneObj::IsInherit(type);
}

const char *WebBGObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

WebBGObj::WebBGObj(const DrawStyle &style)
	: PlaneObj(style)
{
	DrawObj::SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
}

WebBGObj::~WebBGObj()
{
	ReleaseAll();
}

int32_t WebBGObj::UpdateTexRegion(ui::WebRect rect)
{
	intrusive_ptr<Surface> surface;

	GetTexture(surface, 0);

	float rect_width = rect.Width();
	float rect_height = rect.Height();
	Matrix3 uv_matrix;

	if (rect_width > 0.0f && rect_height > 0.0f && surface.get() != NULL) {
		uv_matrix.setCol0(Vector3(1.0f / (float)surface->GetWidth(), 0.0f, 0.0f));
		uv_matrix.setCol1(Vector3(0.0f, 1.0f / (float)surface->GetHeight(), 0.0f));
		uv_matrix.setCol2(Vector3(0.0f, 1.0f, 1.0f));
	} else {
		uv_matrix = Matrix3::identity();
	}

	if (rect_width > 0.0f && rect_height > 0.0f && surface.get() != NULL) {
		Vector3 top_left =
			uv_matrix.getCol0() * rect.m_left + uv_matrix.getCol1() * rect.m_top + uv_matrix.getCol2();
		Vector3 bottom_right =
			uv_matrix.getCol0() * rect.m_right + uv_matrix.getCol1() * rect.m_bottom + uv_matrix.getCol2();

		math::v4 region(top_left.getX(), bottom_right.getY(),
				rect_width / (float)surface->GetWidth(),
				rect_height / (float)surface->GetHeight());
		SetTexRegion(region);
	}

	return 0;
}

void WebBGObj::SetSize(math::v3 size)
{
	if (size.extract_x() * size.extract_y() != 0.0f) {
		PlaneObj::SetSize(size);
		SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		SetWrapS(REPEAT, 0);
		SetWrapT(REPEAT, 0);

		ui::WebRect rect;
		rect.Set(0.0f, 0.0f, size.extract_x(), size.extract_y());
		UpdateTexRegion(rect);
	}
}

}
}
