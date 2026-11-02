/*
 * paf::graph::TextGraphics and TextGraphicsImpl: a text layout's glyphs, caret and selection
 * (the file's name ours). The text threads change them under the graphics mutex, marking what
 * changed; Update clips what changed to the box for drawing, and Draw draws the characters'
 * backgrounds, the selection, the glyphs with their shadows, underlines and strikes, and the
 * caret, each a quad of one vertex array the graphics share.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <graphics/text/text_graphics.h>
#include <graphics/text/text_style.h>
#include <graphics/text/text_surface_cache.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/list>
#include <std/vector>
#include <thread/thread.h>
#include "../../paf_types.h"

namespace paf {
namespace graph {

using arl::ArlInstance;
using arl::VertexArray;
using arl::VertexArraySpec;

/* a vertex of the quad: its position and texture coordinates (names ours) */
class TextVertex {
public:
	float x;
	float y;
	float z;
	float u;
	float v;
};

/* the quad from (0, 0) to (1, -1), scaled to each rectangle drawn */
static const TextVertex s_vertex_data[4] = {
	{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, -1.0f, 0.0f, 0.0f, 1.0f },
	{ 1.0f, 0.0f, 0.0f, 1.0f, 0.0f },
	{ 1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
};

/* the graphics alive, and the quad made for the first one drawn and kept while any is */
static int32_t s_count;
static VertexArray *s_vertices;

/* the color as RGBA bytes, each saturated, and back */
static inline uint32_t PackColor(const math::v4 &color)
{
	uint16x4_t rgba = vqmovn_u32(vcvtq_u32_f32(
		vmulq_f32(color.getImpl().get128(), vdupq_n_f32(255.0f))));

	return vget_lane_u32(vreinterpret_u32_u8(vqmovn_u16(vcombine_u16(rgba, rgba))), 0);
}

static inline math::v4 UnpackColor(uint32_t color)
{
	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(color)));

	return math::v4(sce::Vectormath::Simd::Aos::Vector4(vmulq_f32(
		vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))), vdupq_n_f32(1.0f / 255.0f))));
}

/* the matrix that scales by scale, then moves by pos (name ours) */
inline math::matrix QuadMatrix(const math::v3 &pos, const math::v3 &scale)
{
	return math::matrix(sce::Vectormath::Simd::Aos::Matrix4(
		sce::Vectormath::Simd::Aos::Matrix3::scale(scale.getImpl()), pos.getImpl()));
}

TextGraphics::TextGraphics(thread::RMutex *mutex)
	: m_rect(0.0f, 0.0f, 0.0f, 0.0f), m_scale(1.0f, 1.0f), m_caret_pos(0.0f, 0.0f),
	  m_caret_height(0.0f), m_dirty(0), m_caret_visible(false), m_caret_color(0xFF00F78C),
	  m_selection_color(0x4D999999), m_mutex(mutex)
{
}

TextGraphics::~TextGraphics()
{
	for (list<Char *>::iterator it = m_chars.begin(); it != m_chars.end(); ++it)
		delete *it;
}

/* Sony's build calls these from TextGraphicsImpl rather than inlining them; TextGraphics's own
   methods lock the mutex themselves */
__attribute__((noinline)) void TextGraphics::Unlock()
{
	if (m_mutex != NULL)
		m_mutex->Unlock();
}

__attribute__((noinline)) void TextGraphics::Lock()
{
	if (m_mutex != NULL)
		m_mutex->Lock();
}

int32_t TextGraphics::RemoveChar(Char *c)
{
	if (!c->m_removed) {
		if (m_mutex != NULL)
			m_mutex->Lock();
		m_dirty |= Dirty_Chars | Dirty_CharRects;
		c->m_removed = true;
		if (m_mutex != NULL)
			m_mutex->Unlock();
	}
	return SCE_PAF_OK;
}

int32_t TextGraphics::RemoveAllChars()
{
	if (m_chars.size() != 0) {
		if (m_mutex != NULL)
			m_mutex->Lock();
		for (list<Char *>::iterator it = m_chars.begin(); it != m_chars.end(); ++it) {
			Char *c = *it;

			if (!c->m_removed) {
				m_dirty |= Dirty_Chars | Dirty_CharRects;
				c->m_removed = true;
			}
		}
		if (m_mutex != NULL)
			m_mutex->Unlock();
	}
	return SCE_PAF_OK;
}

int32_t TextGraphics::SetPos(const math::v2 &pos)
{
	if (!((pos.getImpl().getX() == m_rect.getImpl().getX()) &
	      (pos.getImpl().getY() == m_rect.getImpl().getY())).getAsBool()) {
		if (m_mutex != NULL)
			m_mutex->Lock();
		m_dirty |= Dirty_Pos | Dirty_CharRects | Dirty_Caret | Dirty_Selection;
		math::v4 rect = m_rect;

		rect.getImpl().setX(pos.getImpl().getX());
		rect.getImpl().setY(pos.getImpl().getY());
		m_rect = rect;
		if (m_mutex != NULL)
			m_mutex->Unlock();
	}
	return SCE_PAF_OK;
}

int32_t TextGraphics::SetSize(const math::v2 &size)
{
	if (!((size.getImpl().getX() == m_rect.getImpl().getZ()) &
	      (size.getImpl().getY() == m_rect.getImpl().getW())).getAsBool()) {
		if (m_mutex != NULL)
			m_mutex->Lock();
		m_dirty |= Dirty_Size | Dirty_CharRects | Dirty_Caret | Dirty_Selection;
		math::v4 rect = m_rect;

		rect.getImpl().setZ(size.getImpl().getX());
		rect.getImpl().setW(size.getImpl().getY());
		m_rect = rect;
		if (m_mutex != NULL)
			m_mutex->Unlock();
	}
	return SCE_PAF_OK;
}

int32_t TextGraphics::SetScale(const math::v2 &scale)
{
	if (!(scale == m_scale)) {
		if (m_mutex != NULL)
			m_mutex->Lock();
		m_dirty |= Dirty_Scale;
		m_scale = scale;
		if (m_mutex != NULL)
			m_mutex->Unlock();
	}
	return SCE_PAF_OK;
}

int32_t TextGraphics::SetCaret(const math::v2 &pos, float height)
{
	if (!(pos == m_caret_pos) || height != m_caret_height) {
		if (m_mutex != NULL)
			m_mutex->Lock();
		m_dirty |= Dirty_Caret;
		m_caret_height = height;
		m_caret_pos = pos;
		if (m_mutex != NULL)
			m_mutex->Unlock();
	}
	return SCE_PAF_OK;
}

int32_t TextGraphics::SetSelection(const vector<math::v4> &rects)
{
	if (rects.size() == m_selection.size()) {
		for (uint32_t i = 0; i < rects.size(); i++) {
			if (!(rects[i] == m_selection[i])) {
				if (m_mutex != NULL)
					m_mutex->Lock();
				m_dirty |= Dirty_Selection;
				m_selection = rects;
				if (m_mutex != NULL)
					m_mutex->Unlock();
				break;
			}
		}
	} else {
		if (m_mutex != NULL)
			m_mutex->Lock();
		m_dirty |= Dirty_Selection;
		m_selection = rects;
		if (m_mutex != NULL)
			m_mutex->Unlock();
	}
	return SCE_PAF_OK;
}

int32_t TextGraphics::SetCaretVisible(bool visible)
{
	m_caret_visible = visible;
	return SCE_PAF_OK;
}

int32_t TextGraphics::GetCaretVisible(bool &visible)
{
	visible = m_caret_visible;
	return SCE_PAF_OK;
}

int32_t TextGraphics::SetCaretColor(const math::v4 &color)
{
	m_caret_color = PackColor(color);
	return SCE_PAF_OK;
}

int32_t TextGraphics::GetCaretColor(math::v4 &color)
{
	color = UnpackColor(m_caret_color);
	return SCE_PAF_OK;
}

int32_t TextGraphics::SetSelectionColor(const math::v4 &color)
{
	m_selection_color = PackColor(color);
	return SCE_PAF_OK;
}

int32_t TextGraphics::GetSelectionColor(math::v4 &color)
{
	color = UnpackColor(m_selection_color);
	return SCE_PAF_OK;
}

TextGraphicsImpl::TextGraphicsImpl(thread::RMutex *mutex)
	: TextGraphics(mutex), m_draw_rect(0.0f, 0.0f, 0.0f, 0.0f), m_draw_scale(1.0f, 1.0f),
	  m_draw_caret_pos(0.0f, 0.0f), m_draw_caret_height(0.0f)
{
	s_count++;
}

TextGraphicsImpl::~TextGraphicsImpl()
{
	if (--s_count == 0 && s_vertices != NULL) {
		delete s_vertices;
		s_vertices = NULL;
	}
}

int32_t TextGraphicsImpl::UpdateCharRects()
{
	for (list<TextGraphics::Char *>::iterator it = m_draw_chars.begin();
	     it != m_draw_chars.end(); ++it) {
		Char *c = (Char *)*it;
		math::v4 glyph = c->m_rect;

		glyph.set_x(glyph.extract_x() + c->m_offset);
		glyph.set_z(c->m_width);
		IntersectRect(m_draw_rect, glyph, c->m_draw_glyph_rect);
		IntersectRect(m_draw_rect, c->m_rect, c->m_draw_rect);
	}
	return SCE_PAF_OK;
}

int32_t TextGraphicsImpl::UpdateChars()
{
	list<TextGraphics::Char *>::iterator it = m_chars.begin();

	while (it != m_chars.end()) {
		TextGraphics::Char *c = *it;

		if (c->m_removed) {
			it = m_chars.erase(it);
			delete c;
		} else {
			++it;
		}
	}
	m_draw_chars = m_chars;
	return SCE_PAF_OK;
}

int32_t TextGraphicsImpl::Update()
{
	if (m_dirty != 0) {
		Lock();
		if (m_dirty & Dirty_Chars)
			UpdateChars();
		if (m_dirty & Dirty_Pos) {
			m_draw_rect.set_x(m_rect.extract_x());
			m_draw_rect.set_y(m_rect.extract_y());
		}
		if (m_dirty & Dirty_Size) {
			m_draw_rect.set_z(m_rect.extract_z());
			m_draw_rect.set_w(m_rect.extract_w());
		}
		if (m_dirty & Dirty_Scale)
			m_draw_scale = m_scale;
		if (m_dirty & Dirty_CharRects)
			UpdateCharRects();
		if (m_dirty & Dirty_Caret) {
			math::v4 caret;

			IntersectRect(m_draw_rect,
				      math::v4(m_caret_pos.extract_x(), m_caret_pos.extract_y(), 3.0f,
					       m_caret_height),
				      caret);
			m_draw_caret_pos = math::v2(caret.extract_x(), caret.extract_y());
			if (caret.extract_z() > 0.0f)
				m_draw_caret_height = caret.extract_w();
			else
				m_draw_caret_height = 0.0f;
		}
		if (m_dirty & Dirty_Selection) {
			m_draw_selection.resize(m_selection.size());
			for (uint32_t i = 0; i < m_selection.size(); i++)
				IntersectRect(m_draw_rect, m_selection[i], m_draw_selection[i]);
		}
		m_dirty = 0;
		Unlock();
	}
	return SCE_PAF_OK;
}

/* Each rectangle is the quad scaled to it under the box's matrix, in the color times the
   rectangle's. A glyph clipped by the box draws the part of its surface left; one of an image
   (an emoji) keeps its colors, under the text color's alpha. */
int32_t TextGraphicsImpl::Draw(const math::matrix &world, const math::v4 &color, Shader *shader)
{
	ArlInstance *gl = arl::Gl();

	if (s_vertices == NULL)
		s_vertices = new VertexArray(gl->m_default_allocator,
					     ScePafGraphics_4D1E289A(gl,
						VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2,
						SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
					     s_vertex_data, 4);

	PlaneObjDrawParams params(NULL, NULL);

	params.m_vertices = s_vertices;
	params.m_count = 4;
	params.m_instances = 1;
	params.m_num_textures = 0;

	math::matrix base = world * QuadMatrix(math::v3(-m_draw_rect.extract_x(),
							m_draw_rect.extract_y(), 0.0f),
					       math::v3(m_draw_scale.extract_x(),
							m_draw_scale.extract_y(), 0.0f));
	for (list<TextGraphics::Char *>::iterator it = m_draw_chars.begin();
	     it != m_draw_chars.end(); ++it) {
		Char *c = (Char *)*it;
		math::v4 back_color;

		c->m_style->GetStyleAttribute(TextStyleAttribute_BackColor, back_color);
		if (back_color.extract_w() > 0.0f && c->m_draw_rect.extract_z() > 0.0f &&
		    c->m_draw_rect.extract_w() > 0.0f) {
			gl->Color(math::v4(mulPerElem(color.getImpl(), back_color.getImpl())));
			gl->m_model.Load(base * QuadMatrix(math::v3(c->m_draw_rect.extract_x(),
								    -c->m_draw_rect.extract_y(), 0.0f),
							   math::v3(c->m_draw_rect.extract_z(),
								    c->m_draw_rect.extract_w(), 1.0f)));
			params.m_effect = PlaneObjDrawParams::Effect_None;
			params.m_textures = NULL;
			params.m_num_textures = 0;
			params.m_tex_region = NULL;
			shader->Draw(params);
		}
	}

	math::v4 selection_color = UnpackColor(m_selection_color);

	if (selection_color.extract_w() > 0.0f) {
		for (uint32_t i = 0; i < m_draw_selection.size(); i++) {
			math::v4 &rect = m_draw_selection[i];

			if (rect.extract_z() > 0.0f && rect.extract_w() > 0.0f) {
				gl->Color(math::v4(mulPerElem(color.getImpl(),
							      selection_color.getImpl())));
				gl->m_model.Load(base * QuadMatrix(math::v3(rect.extract_x(),
									    -rect.extract_y(), 0.0f),
								   math::v3(rect.extract_z(),
									    rect.extract_w(), 1.0f)));
				params.m_effect = PlaneObjDrawParams::Effect_None;
				params.m_textures = NULL;
				params.m_num_textures = 0;
				params.m_tex_region = NULL;
				shader->Draw(params);
			}
		}
	}

	for (list<TextGraphics::Char *>::iterator it = m_draw_chars.begin();
	     it != m_draw_chars.end(); ++it) {
		Char *c = (Char *)*it;
		math::v4 text_color;

		c->m_style->GetStyleAttribute(TextStyleAttribute_Color, text_color);
		if (text_color.extract_w() > 0.0f && c->m_draw_glyph_rect.extract_z() > 0.0f &&
		    c->m_draw_glyph_rect.extract_w() > 0.0f) {
			TextSurface *surface = (TextSurface *)c->m_surface.get();
			math::v4 region;
			math::v4 shadow_color;
			bool shadow;
			bool emboss;

			if (surface->IsAlphaMask())
				gl->Color(math::v4(mulPerElem(color.getImpl(), text_color.getImpl())));
			else
				gl->Color(math::v4(mulPerElem(color.getImpl(),
					math::v4(1.0f, 1.0f, 1.0f, text_color.extract_w()).getImpl())));
			gl->m_model.Load(base * QuadMatrix(
				math::v3(c->m_draw_glyph_rect.extract_x(),
					 -c->m_draw_glyph_rect.extract_y(), 0.0f),
				math::v3(c->m_draw_glyph_rect.extract_z(),
					 c->m_draw_glyph_rect.extract_w(), 1.0f)));
			surface->Prepare();
			params.m_textures = &c->m_surface;
			params.m_num_textures = 1;

			math::v4 glyph(c->m_rect.extract_x() + c->m_offset, c->m_rect.extract_y(),
				       c->m_width, c->m_rect.extract_w());

			params.m_tex_region = NULL;
			if (!(glyph == c->m_draw_glyph_rect)) {
				math::v4 size(c->m_width, c->m_rect.extract_w(), c->m_width,
					      c->m_rect.extract_w());

				/* the part drawn: where it starts in the glyph, and how much of it */
				region = math::v4(divPerElem((c->m_draw_glyph_rect -
							      math::v4(glyph.extract_x(),
								       glyph.extract_y(), 0.0f,
								       0.0f)).getImpl(),
							     size.getImpl()));
				params.m_tex_region = &region;
			}
			c->m_style->GetStyleAttribute(TextStyleAttribute_Shadow, shadow);
			if (shadow) {
				c->m_style->GetStyleAttribute(TextStyleAttribute_ShadowColor,
							      shadow_color);
				if (shadow_color.extract_w() > 0.0f) {
					params.m_effect_color = &shadow_color;
					params.m_effect_color_valid = 1;
					shadow_color = math::v4(mulPerElem(shadow_color.getImpl(),
						mulPerElem(color.getImpl(),
							   math::v4(1.0f, 1.0f, 1.0f,
								    text_color.extract_w()).getImpl())));
				} else {
					shadow = false;
				}
			}
			c->m_style->GetStyleAttribute(TextStyleAttribute_Emboss, emboss);
			if (shadow) {
				if (emboss)
					params.m_effect = PlaneObjDrawParams::Effect_ShadowEmboss;
				else
					params.m_effect = PlaneObjDrawParams::Effect_Shadow;
			} else if (emboss) {
				params.m_effect = PlaneObjDrawParams::Effect_Emboss;
			} else {
				params.m_effect = PlaneObjDrawParams::Effect_None;
			}
			shader->Draw(params);
		}

		bool underline;

		c->m_style->GetStyleAttribute(TextStyleAttribute_Underline, underline);
		if (underline && c->m_draw_rect.extract_z() > 0.0f) {
			float y = (c->m_rect.extract_y() + c->m_rect.extract_w()) - 2.0f;

			if (m_draw_rect.extract_y() <= y &&
			    y <= m_draw_rect.extract_y() + m_draw_rect.extract_w()) {
				math::v4 line_color;

				c->m_style->GetStyleAttribute(TextStyleAttribute_UnderlineColor,
							      line_color);
				if (line_color.extract_w() > 0.0f) {
					gl->Color(math::v4(mulPerElem(color.getImpl(),
								      line_color.getImpl())));
					gl->m_model.Load(base * QuadMatrix(
						math::v3(c->m_draw_rect.extract_x(), -y, 0.0f),
						math::v3(c->m_draw_rect.extract_z(), 2.0f, 1.0f)));
					params.m_effect = PlaneObjDrawParams::Effect_None;
					params.m_textures = NULL;
					params.m_num_textures = 0;
					params.m_tex_region = NULL;
					shader->Draw(params);
				}
			}
		}

		bool strike;

		c->m_style->GetStyleAttribute(TextStyleAttribute_Strike, strike);
		if (strike && c->m_draw_rect.extract_z() > 0.0f) {
			float y = c->m_rect.extract_y() + (c->m_rect.extract_w() - 2.0f) * 0.5f;

			if (m_draw_rect.extract_y() <= y &&
			    y <= m_draw_rect.extract_y() + m_draw_rect.extract_w()) {
				math::v4 line_color;

				c->m_style->GetStyleAttribute(TextStyleAttribute_StrikeColor,
							      line_color);
				if (line_color.extract_w() > 0.0f) {
					gl->Color(math::v4(mulPerElem(color.getImpl(),
								      line_color.getImpl())));
					gl->m_model.Load(base * QuadMatrix(
						math::v3(c->m_draw_rect.extract_x(),
							 -((c->m_rect.extract_w() - 2.0f) * 0.5f) -
								 c->m_rect.extract_y(),
							 0.0f),
						math::v3(c->m_draw_rect.extract_z(), 2.0f, 1.0f)));
					params.m_effect = PlaneObjDrawParams::Effect_None;
					params.m_textures = NULL;
					params.m_num_textures = 0;
					params.m_tex_region = NULL;
					shader->Draw(params);
				}
			}
		}
	}

	math::v4 caret_color = UnpackColor(m_caret_color);

	if (m_caret_visible && caret_color.extract_w() > 0.0f && m_draw_caret_height > 0.0f) {
		gl->Color(math::v4(mulPerElem(color.getImpl(), caret_color.getImpl())));
		gl->m_model.Load(base * QuadMatrix(math::v3(m_draw_caret_pos.extract_x(),
							    -m_draw_caret_pos.extract_y(), 0.0f),
						   math::v3(3.0f, m_draw_caret_height, 1.0f)));
		params.m_effect = PlaneObjDrawParams::Effect_None;
		params.m_textures = NULL;
		params.m_num_textures = 0;
		params.m_tex_region = NULL;
		shader->Draw(params);
	}
	return SCE_PAF_OK;
}

int32_t TextGraphicsImpl::AddChar(const math::v4 &rect, float offset, float width,
				  const common::SharedPtr<TextStyle> &style,
				  const intrusive_ptr<Surface> &surface, TextGraphics::Char *&c)
{
	Char *new_char = new Char();

	if (new_char == NULL)
		return SCE_PAF_ERROR_TEXT_NO_MEMORY;
	new_char->m_rect = rect;
	new_char->m_offset = offset;
	new_char->m_width = width;
	new_char->m_style = style;
	new_char->m_surface = surface;
	Lock();
	m_dirty |= Dirty_Chars | Dirty_CharRects;
	m_chars.push_back(new_char);
	Unlock();
	c = new_char;
	return SCE_PAF_OK;
}

int32_t TextGraphicsImpl::MoveChar(TextGraphics::Char *c, const math::v4 &rect)
{
	Char *draw_char = (Char *)c;

	draw_char->m_rect = rect;

	math::v4 glyph = draw_char->m_rect;

	glyph.set_x(glyph.extract_x() + draw_char->m_offset);
	glyph.set_z(draw_char->m_width);
	IntersectRect(m_draw_rect, glyph, draw_char->m_draw_glyph_rect);
	IntersectRect(m_draw_rect, draw_char->m_rect, draw_char->m_draw_rect);
	return SCE_PAF_OK;
}

}
}
