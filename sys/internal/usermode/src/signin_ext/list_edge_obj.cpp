/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * ListEdgeObj — PlaneObj subclass applying edge alpha gradients and custom
 * 9-slice mesh texcoords relative to a target background plane.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <graphics/color.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include "signin_ext_internal.h"

using namespace paf;
using namespace paf::graph;

const char ListEdgeObj::m_drawobj_type[] = "ListEdgeObj";

bool ListEdgeObj::IsInherit(const char *type) const
{
	if (m_drawobj_type == type)
		return true;
	return PlaneObj::IsInherit(type);
}

const char *ListEdgeObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

ListEdgeObj::ListEdgeObj(const DrawStyle &style)
	: PlaneObj(style),
	  m_edgeSide(EDGE_UPPER),
	  m_gradient(1.0f),
	  m_intercept(0.0f),
	  m_basePos(0.0f, 0.0f),
	  m_baseSize(0.0f, 0.0f),
	  m_customTexRegion(0.0f, 0.0f, 1.0f, 1.0f)
{
	SetShader(ListEdgeShaderGet());
}

ListEdgeObj::~ListEdgeObj()
{
}

void ListEdgeObj::SetBasePlane(const math::v2 &pos, const math::v2 &size)
{
	m_basePos = pos;
	m_baseSize = size;
	m_plane_flags.m_texcoord_dirty = true;
	m_plane_flags.m_layout_dirty = true;
}

void ListEdgeObj::OnDraw(const math::matrix &parent, uint32_t color, float alpha, DrawInfo &info)
{
	if (!m_flags.m_visible)
		return;

	if (m_plane_flags.m_texcoord_dirty) {
		uint32_t divMode = m_divide != NULL ? m_divide->m_div_mode
						    : m_tex_surface[0]->GetDivideMode();

		if (divMode == 6 || divMode == 7 || divMode == 8 || divMode == 9) {
			math::v2 ratioH;
			math::v2 ratioV;
			math::v2 stretchRatio;

			if (m_divide != NULL) {
				ratioH = m_divide->m_div_border_ratio_h;
				ratioV = m_divide->m_div_border_ratio_v;
				stretchRatio = m_divide->m_div_ratio_stretch;
			} else {
				math::v2 unused;
				ScePafGraphics_AFAE7807(m_tex_surface[0].get(), &ratioH, &unused);
				ScePafGraphics_AFAE7807(m_tex_surface[0].get(), &unused, &ratioV);
				m_tex_surface[0]->GetDivideStretchRatio(stretchRatio);
			}

			uint32_t mappedMode;
			if (divMode > 7)
				mappedMode = (divMode <= 8) ? 1U : 2U;
			else
				mappedMode = 3U;

			if (m_divide == NULL) {
				m_divide = new PlaneDivide();
				if (m_divide != NULL) {
					m_divide->m_div_mode = 0;
					m_divide->m_div_border_ratio_h = math::v2(0.0f, 0.0f);
					m_divide->m_div_border_ratio_v = math::v2(0.0f, 0.0f);
					m_divide->m_div_ratio_stretch = math::v2(0.0f, 0.0f);
				}
			}
			m_divide->m_div_mode = mappedMode;
			m_divide->m_div_border_ratio_h = ratioH;
			m_divide->m_div_border_ratio_v = ratioV;
			m_divide->m_div_ratio_stretch = stretchRatio;
			m_plane_flags.m_texcoord_dirty = true;
			m_plane_flags.m_mesh_dirty = true;
		}
	}

	PlaneObj::OnDraw(parent, color, alpha, info);
}

namespace {
class SurfaceMarginAccess : public Surface {
public:
	void SaveAndClearMargins(uint16_t &l, uint16_t &b, uint16_t &r, uint16_t &t)
	{
		l = m_margin_left;
		b = m_margin_bottom;
		r = m_margin_right;
		t = m_margin_top;
		m_margin_left = 0;
		m_margin_bottom = 0;
		m_margin_right = 0;
		m_margin_top = 0;
	}

	void RestoreMargins(uint16_t l, uint16_t b, uint16_t r, uint16_t t)
	{
		m_margin_left = l;
		m_margin_bottom = b;
		m_margin_right = r;
		m_margin_top = t;
	}
};
}

void ListEdgeObj::UpdateLayout()
{
	Surface *surf = m_tex_surface[0].get();
	if (surf == NULL) {
		PlaneObj::UpdateLayout();
		return;
	}

	uint16_t savedL, savedB, savedR, savedT;
	static_cast<SurfaceMarginAccess *>(surf)->SaveAndClearMargins(
		savedL, savedB, savedR, savedT);

	PlaneObj::UpdateLayout();

	surf = m_tex_surface[0].get();
	static_cast<SurfaceMarginAccess *>(surf)->RestoreMargins(
		savedL, savedB, savedR, savedT);

	if (m_baseSize.extract_x() < 1.0f)
		return;

	uint32_t divMode = 0;
	if (m_divide != NULL)
		divMode = m_divide->m_div_mode;
	else if (surf != NULL)
		divMode = surf->GetDivideMode();

	math::v2 regOffset(0.0f, 0.0f);
	math::v2 regScale(1.0f, 1.0f);
	if (m_tex_region != NULL) {
		regOffset = math::v2(m_tex_region->extract_x(), m_tex_region->extract_y());
		regScale = math::v2(m_tex_region->extract_z(), m_tex_region->extract_w());
	}

	if (divMode == 0) {
		float invBaseX = 1.0f / m_baseSize.extract_x();
		float invBaseY = 1.0f / m_baseSize.extract_y();
		float ox = (m_baseSize.extract_x() - m_size.extract_x()) * 0.5f -
			   (m_basePos.extract_x() - m_pos.extract_x());
		float oy = (m_baseSize.extract_y() - m_size.extract_y()) * 0.5f +
			   (m_basePos.extract_y() - m_pos.extract_y());
		float u0 = regOffset.extract_x() + regScale.extract_x() * (ox * invBaseX);
		float v0 = regOffset.extract_y() + regScale.extract_y() * (oy * invBaseY);
		float us = regScale.extract_x() * (m_size.extract_x() * invBaseX);
		float vs = regScale.extract_y() * (m_size.extract_y() * invBaseY);
		m_customTexRegion = math::v4(u0, v0, us, vs);
	} else {
		m_customTexRegion = math::v4(regOffset.extract_x(), regOffset.extract_y(),
					     regScale.extract_x(), regScale.extract_y());
	}
}

float ListEdgeObj::InterpolateTexCoord(float pos, const float *positions, const float *texCoords,
				       int32_t count) const
{
	for (int32_t i = 1; i < count; ++i) {
		if (pos <= positions[i]) {
			if (positions[i] == positions[i - 1])
				return texCoords[i];
			return (pos - positions[i - 1]) / (positions[i] - positions[i - 1]) *
				       (texCoords[i] - texCoords[i - 1]) +
			       texCoords[i - 1];
		}
	}
	return 1.0f;
}

void ListEdgeObj::UpdateDivVerticesPosition()
{
	uint32_t divMode = 0;
	if (m_divide != NULL) {
		divMode = m_divide->m_div_mode;
	} else {
		if (m_tex_surface[0].get() == NULL)
			return;
		divMode = m_tex_surface[0]->GetDivideMode();
	}

	if (divMode == 0)
		return;

	if (m_baseSize.extract_x() < 1.0f || m_baseSize.extract_y() < 1.0f)
		return;

	float halfW = m_plane_size.extract_x() * 0.5f;
	float halfH = m_plane_size.extract_y() * 0.5f;
	float minX = -halfW;
	float maxX = halfW;
	float maxY = halfH;
	float minY = -halfH;

	int32_t countX = 2;
	int32_t countY = 2;
	float outPosX[4] = { minX, maxX, maxX, maxX };
	float outPosY[4] = { maxY, minY, minY, minY };
	float outTexU[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
	float outTexV[4] = { 0.0f, 1.0f, 1.0f, 1.0f };

	Surface *surf = m_tex_surface[0].get();
	if (surf != NULL) {
		math::v2 ratioH;
		math::v2 ratioV;

		if (m_divide != NULL) {
			ratioH = m_divide->m_div_border_ratio_h;
			ratioV = m_divide->m_div_border_ratio_v;
		} else {
			ScePafGraphics_AFAE7807(surf, &ratioH, &ratioV);
			surf = m_tex_surface[0].get();
		}

		float relX = m_basePos.extract_x() - m_pos.extract_x();
		float baseW = m_baseSize.extract_x();
		float refPosX[4] = { relX - baseW * 0.5f, relX + baseW * 0.5f,
				     relX + baseW * 0.5f, relX + baseW * 0.5f };
		float refTexU[4] = { 0.0f, 1.0f, 1.0f, 1.0f };

		if (divMode != 1) {
			float bLeft = ratioH.extract_x() * (m_div_border_scale * (float)surf->GetWidth());
			float bRight = ratioH.extract_y() * (m_div_border_scale * (float)surf->GetWidth());
			if (baseW < bLeft + bRight) {
				float s = baseW / (bLeft + bRight);
				bLeft *= s;
				bRight *= s;
			}
			float innerLeft = (bLeft - baseW * 0.5f) + relX;
			float innerRight = (baseW * 0.5f - bRight) + relX;
			countX = 4;
			outTexU[1] = 0.0f;
			outPosX[1] = minX;
			refPosX[1] = innerLeft;
			refPosX[2] = innerRight;
			if (innerLeft <= maxX)
				outPosX[1] = (minX < innerLeft) ? innerLeft : minX;
			if (innerRight >= minX)
				outPosX[2] = (maxX < innerRight) ? maxX : innerRight;
			refTexU[1] = ratioH.extract_x();
			refTexU[2] = 1.0f - ratioH.extract_y();
		}

		for (int32_t i = 0; i < countX; ++i) {
			float p = outPosX[i];
			outTexU[i] = InterpolateTexCoord(p, refPosX, refTexU, countX);
			outPosX[i] = (m_plane_size.extract_x() > 1.0f) ? (p / m_plane_size.extract_x()) : 0.0f;
		}

		float relY = m_basePos.extract_y() - m_pos.extract_y();
		float baseH = m_baseSize.extract_y();
		float refPosY[4] = { relY - baseH * 0.5f, relY + baseH * 0.5f,
				     relY + baseH * 0.5f, relY + baseH * 0.5f };
		float refTexV[4] = { 1.0f, 0.0f, 0.0f, 0.0f };

		if (divMode != 2) {
			float bTop = ratioV.extract_x() * (m_div_border_scale * (float)surf->GetHeight());
			float bBottom = ratioV.extract_y() * (m_div_border_scale * (float)surf->GetHeight());
			if (baseH < bTop + bBottom) {
				float s = baseH / (bTop + bBottom);
				bTop *= s;
				bBottom *= s;
			}
			float innerBottom = relY + (bBottom - baseH * 0.5f);
			float innerTop = relY + (baseH * 0.5f - bTop);
			countY = 4;
			outPosY[1] = maxY;
			outTexV[1] = 0.0f;
			refPosY[1] = innerBottom;
			refPosY[2] = innerTop;
			if (innerTop >= minY)
				outPosY[1] = (maxY < innerTop) ? maxY : innerTop;
			if (innerBottom <= maxY)
				outPosY[2] = (minY < innerBottom) ? innerBottom : minY;
			refTexV[1] = 1.0f - ratioV.extract_y();
			refTexV[2] = ratioV.extract_x();
		}

		for (int32_t i = 0; i < countY; ++i) {
			float p = outPosY[i];
			outTexV[i] = InterpolateTexCoord(p, refPosY, refTexV, countY);
			outPosY[i] = (m_plane_size.extract_y() > 1.0f) ? (p / m_plane_size.extract_y()) : 0.0f;
		}
	}

	struct DivVertex {
		float x;
		float y;
		float z;
		float u;
		float v;
	};

	DivVertex *verts = (DivVertex *)m_div_vertices->m_buffer->m_memory->m_addr;
	int32_t rowBase = 0;
	for (int32_t y = 0; y < countY; ++y) {
		DivVertex *row = verts + rowBase;
		for (int32_t x = 0; x < countX; ++x) {
			row[x].x = outPosX[x];
			row[x].y = outPosY[y];
			row[x].u = outTexU[x];
			row[x].v = outTexV[y];
		}
		rowBase += countX;
	}
}

void ListEdgeObj::UpdateDivVerticesTexcoord()
{
}

void ListEdgeObj::Draw(const math::matrix &world, uint32_t color, float alpha)
{
	if (m_baseSize.extract_x() < 1.0f)
		return;

	math::v4 *savedRegion = m_tex_region;
	m_tex_region = &m_customTexRegion;

	ListEdgeShader *shader = static_cast<ListEdgeShader *>(m_shader.get());
	if (m_edgeSide == EDGE_UPPER) {
		shader->m_coefficient = math::v2(m_gradient, m_intercept);
	} else {
		shader->m_coefficient = math::v2(-m_gradient, m_gradient + m_intercept);
	}

	PlaneObj::Draw(world, color, alpha);
	m_tex_region = savedRegion;
}
