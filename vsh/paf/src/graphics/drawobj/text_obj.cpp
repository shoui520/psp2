/*
 * paf::graph::TextObj: the text draw object. Its layout is laid out by the text threads: a draw
 * or a change queues it with the text queue, an update takes the size it was laid out at, and
 * a draw draws the layout's graphics under the object's matrix, whose origin is the layout's
 * top left corner. And TextObjParam, which hands the layout and its first style to the object
 * (the file's name ours, after the class).
 */
#include <stdint.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/text/text_graphics.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include <system/system.h>
#include <versatileparam/resourceparam.h>

namespace paf {
namespace graph {

extern "C" const uint32_t ScePafGraphics_5C7E6693 = 0x41;

/* what a text object returns without a layout (name ours) */
static const int32_t TEXT_OBJ_ERROR_NO_LAYOUT = -2136014814;	/* 0x80AF2322 */

const char TextObj::m_drawobj_type[] = "TextObj";

bool TextObj::IsInherit(const char *type) const
{
	if (type == m_drawobj_type)
		return true;
	return DrawObj::IsInherit(type);
}

const char *TextObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

const char *TextObj::DrawObjType()
{
	return m_drawobj_type;
}

/* the system's text style 4 deepens the colors */
TextObj::TextObj(const DrawStyle &style)
	: DrawObj(style),
	  m_layout(new TextLayout(new TextGraphicsImpl(GetTextGraphicsMutex()), GetTextLayoutMutex()))
{
	if (system::SystemParam::GetTextStyle() & 4)
		m_draw_style.deepen_color = 1;
}

TextObj::~TextObj()
{
}

/* the size the layout was laid out at becomes the object's, and the graphics are made again */
void TextObj::Update()
{
	math::v2 size;

	if (m_layout.get() != NULL && m_layout->GetDrawSize(size) == 0) {
		TextGraphics *graphics;
		float *size_xy = (float *)&m_size;

		size_xy[0] = size.extract_x();
		size_xy[1] = size.extract_y();
		m_layout->GetGraphics(graphics);
		if (graphics != NULL)
			graphics->Update();
		if (m_flags.m_prepare)
			m_flags.m_prepare = false;
	}
}

/* an object to prepare or dirty queues its layout */
void TextObj::UpdatePost(const math::matrix &world_mat)
{
	if (m_flags.m_prepare || m_flags.m_dirty != 0) {
		if (m_layout.get() != NULL) {
			TextQueue *queue = GetTextQueue();

			if (queue != NULL)
				queue->Request(m_layout);
		}
	}
	DrawObj::UpdatePost(world_mat);
}

void TextObj::ReleaseAll()
{
	if (m_layout.get() != NULL) {
		TextQueue *queue = GetTextQueue();

		if (queue != NULL)
			queue->Cancel(m_layout);
		m_layout->Release();
	}
}

void TextObj::OnDraw(const math::matrix &parent, uint32_t color, float alpha, DrawInfo &info)
{
	if (!m_flags.m_visible)
		return;

	ScePafGraphics_B7EB9246(this, &color, &alpha);
	if (!(alpha > 0.0f))
		return;

	if (m_layout.get() != NULL) {
		TextQueue *queue = GetTextQueue();

		if (queue != NULL)
			queue->Request(m_layout);
	}

	uint32_t key = (info.m_layer << 16) + (info.m_sub_layer << 8) + info.m_order;
	DrawCallbackArg arg;

	arg.self = this;
	arg.world_mat = &parent;
	arg.color = color;
	arg.alpha = alpha;
	if (m_flags.m_draw_list == 0)
		g_scene_draw_layer.m_ptr->Push(key, DrawCallback, arg);
	else if (m_flags.m_draw_list == 1 && g_scene_draw_layer2.m_ptr != NULL)
		g_scene_draw_layer2.m_ptr->Push(key, DrawCallback, arg);
	info.m_order++;
}

int32_t TextObj::SetLayoutAttribute(TextLayoutAttribute attr, const math::v2 &val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetLayoutAttribute(attr, val));
}

/* the layout's box is the object's size */
void TextObj::SetSize(math::v3 size)
{
	math::v2 box(sce::Vectormath::Simd::Aos::Vector2(vget_low_f32(size.getImpl().get128())));

	if (m_layout.get() != NULL)
		LayoutChanged(m_layout->SetLayoutAttribute(TextLayoutAttribute_Size, box));
}

/* the position, and the rotation about it, unscaled: the layout's top left corner is half the
   size left of and above the position */
void TextObj::CalcWorldMatrix(math::matrix &world_mat, const math::matrix &parent, bool unscaled)
{
	using sce::Vectormath::Simd::Aos::Matrix4;
	using sce::Vectormath::Simd::Aos::Vector3;
	using sce::Vectormath::Simd::Aos::Vector4;

	if (m_rot == NULL) {
		Vector3 corner = m_pos.getImpl() + mulPerElem(m_size.getImpl(), Vector3(-0.5f, 0.5f, 0.0f));

		world_mat = parent * math::matrix(math::v4(1.0f, 0.0f, 0.0f, 0.0f),
						  math::v4(0.0f, 1.0f, 0.0f, 0.0f),
						  math::v4(0.0f, 0.0f, 1.0f, 0.0f),
						  math::v4(math::v3(corner), 1.0f));
	} else {
		math::matrix rot = math::quaternion::FromEulerAnglesYXZ(*m_rot).toMatrix(1.0e-4f);
		Vector3 offset = mulPerElem(m_size.getImpl(), Vector3(-0.5f, 0.5f, 0.0f));
		Vector4 corner = Matrix4(rot.x().getImpl(), rot.y().getImpl(), rot.z().getImpl(),
					 Vector4(m_pos.getImpl(), 1.0f)) *
				 Vector4(offset, 1.0f);

		world_mat = parent * math::matrix(rot.x(), rot.y(), rot.z(), math::v4(corner));
	}
}

int32_t TextObj::SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len, bool val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetStyleAttribute(attr, index, len, val));
}

/* the font flags into the layout's first style, unless 128, and the surface draw bit */
void TextObj::SetSurfaceDraw(const SurfaceDrawParam &param)
{
	int32_t flags = param.m_font_flags;

	if (flags != 128) {
		bool enlarge_font_size = (flags & 1) ? true : false;
		bool force_bold = (flags & 2) != 0;
		bool deepen_color = (flags & 4) != 0;

		SetStyleAttribute(TextStyleAttribute_EnlargeFontSize, 0, 0, enlarge_font_size);
		SetStyleAttribute(TextStyleAttribute_ForceBold, 0, 0, force_bold);
		m_draw_style.deepen_color = deepen_color;
	}
	m_draw_style.surface_draw = param.m_surface_draw;
}

void TextObj::DrawCallback(const DrawCallbackArg &arg)
{
	arg.self->Draw(*arg.world_mat, arg.color, arg.alpha);
}

void TextObj::Draw(const math::matrix &world, uint32_t color, float alpha)
{
	if (m_layout.get() != NULL) {
		TextGraphics *graphics;

		m_layout->GetGraphics(graphics);
		if (graphics != NULL) {
			math::matrix world_mat;

			BindDrawStyle();
			CalcWorldMatrix(world_mat, world, true);

			uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(color)));
			math::v4 draw_color(sce::Vectormath::Simd::Aos::Vector4(vmulq_f32(
				vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))), vdupq_n_f32(1.0f / 255.0f))));

			draw_color.set_w(alpha);
			graphics->Draw(world_mat, draw_color, m_shader.get());
		}
	}
}

int32_t TextObj::SetLayoutAttribute(TextLayoutAttribute attr, int32_t val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetLayoutAttribute(attr, val));
}

int32_t TextObj::GetLayoutAttribute(TextLayoutAttribute attr, int32_t &val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetLayoutAttribute(attr, val);
}

int32_t TextObj::SetLayoutAttribute(TextLayoutAttribute attr, float val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetLayoutAttribute(attr, val));
}

int32_t TextObj::GetLayoutAttribute(TextLayoutAttribute attr, float &val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetLayoutAttribute(attr, val);
}

int32_t TextObj::GetLayoutAttribute(TextLayoutAttribute attr, math::v2 &val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetLayoutAttribute(attr, val);
}

int32_t TextObj::SetLayoutAttribute(TextLayoutAttribute attr, bool val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetLayoutAttribute(attr, val));
}

int32_t TextObj::GetLayoutAttribute(TextLayoutAttribute attr, bool &val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetLayoutAttribute(attr, val);
}

int32_t TextObj::SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				   int32_t val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetStyleAttribute(attr, index, len, val));
}

int32_t TextObj::GetStyleAttribute(TextStyleAttribute attr, uint32_t index, int32_t &val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetStyleAttribute(attr, index, val);
}

int32_t TextObj::SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				   float val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetStyleAttribute(attr, index, len, val));
}

int32_t TextObj::GetStyleAttribute(TextStyleAttribute attr, uint32_t index, float &val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetStyleAttribute(attr, index, val);
}

int32_t TextObj::SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				   const math::v2 &val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetStyleAttribute(attr, index, len, val));
}

int32_t TextObj::GetStyleAttribute(TextStyleAttribute attr, uint32_t index, math::v2 &val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetStyleAttribute(attr, index, val);
}

int32_t TextObj::SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				   const math::v4 &val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetStyleAttribute(attr, index, len, val));
}

int32_t TextObj::GetStyleAttribute(TextStyleAttribute attr, uint32_t index, math::v4 &val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetStyleAttribute(attr, index, val);
}

int32_t TextObj::GetStyleAttribute(TextStyleAttribute attr, uint32_t index, bool &val)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetStyleAttribute(attr, index, val);
}

int32_t TextObj::SetStyle(uint32_t index, uint32_t len, const TextStyle &style)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetStyle(index, len, style));
}

int32_t TextObj::GetStyle(uint32_t index, TextStyle &style)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetStyle(index, style);
}

int32_t TextObj::SetString(const wstring &str)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetString(str));
}

int32_t TextObj::ReplaceString(uint32_t index, uint32_t len, const wstring &str)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->ReplaceString(index, len, str));
}

int32_t TextObj::GetString(wstring &str)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetString(str);
}

int32_t TextObj::GetLength(uint32_t &len)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetLength(len);
}

int32_t TextObj::GetStringBuffer(const wchar_t *&buf, uint32_t &len)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetStringBuffer(buf, len);
}

int32_t TextObj::SetAttributedString(const AttributedString &str)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetAttributedString(str));
}

int32_t TextObj::GetAttributedString(AttributedString &str)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetAttributedString(str);
}

int32_t TextObj::SetPlaceholderStyle(const TextStyle &style)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetPlaceholderStyle(style));
}

int32_t TextObj::SetPlaceholderString(const wstring &str)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->SetPlaceholderString(str));
}

int32_t TextObj::GetPlaceholderString(wstring &str)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetPlaceholderString(str);
}

int32_t TextObj::GetBounds(math::v2 &bounds)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetBounds(bounds);
}

int32_t TextObj::GetLineCount(uint32_t &count)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetLineCount(count);
}

int32_t TextObj::SetCharPos(uint32_t index, const math::v2 &pos)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->SetCharPos(index, pos);
}

int32_t TextObj::GetCharInfo(uint32_t index, TextLayoutCharInfo &info)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetCharInfo(index, info);
}

int32_t TextObj::GetLineInfo(uint32_t line, TextLayoutLineInfo &info)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetLineInfo(line, info);
}

int32_t TextObj::GetNextUpHit(uint32_t index, uint32_t &next)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetNextUpHit(index, next);
}

int32_t TextObj::GetNextDownHit(uint32_t index, uint32_t &next)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetNextDownHit(index, next);
}

int32_t TextObj::GetNextLeftHit(uint32_t index, uint32_t &next)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetNextLeftHit(index, next);
}

int32_t TextObj::GetNextRightHit(uint32_t index, uint32_t &next)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetNextRightHit(index, next);
}

int32_t TextObj::HitTestChar(const math::v2 &pos, bool inside, uint32_t &index)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->HitTestChar(pos, inside, index);
}

int32_t TextObj::MoveCaretUp()
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->MoveCaretUp();
}

int32_t TextObj::MoveCaretDown()
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->MoveCaretDown();
}

int32_t TextObj::MoveCaretLeft()
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->MoveCaretLeft();
}

int32_t TextObj::MoveCaretRight()
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->MoveCaretRight();
}

int32_t TextObj::SetCaretPos(const math::v2 &pos)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->SetCaretPos(pos);
}

int32_t TextObj::SetSelectionPos(const math::v2 &start, const math::v2 &end)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->SetSelectionPos(start, end);
}

int32_t TextObj::SetCaretIndex(uint32_t index)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->SetCaretIndex(index);
}

int32_t TextObj::GetCaretIndex(uint32_t &index)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetCaretIndex(index);
}

int32_t TextObj::SetSelectionRange(uint32_t index, uint32_t len)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->SetSelectionRange(index, len);
}

int32_t TextObj::GetSelectionRange(uint32_t &index, uint32_t &len)
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return m_layout->GetSelectionRange(index, len);
}

int32_t TextObj::SetCaretVisible(bool visible)
{
	int32_t ret = TEXT_OBJ_ERROR_NO_LAYOUT;

	if (m_layout.get() != NULL) {
		TextGraphics *graphics;

		m_layout->GetGraphics(graphics);
		if (graphics != NULL)
			ret = graphics->SetCaretVisible(visible);
	}
	return ret;
}

int32_t TextObj::GetCaretVisible(bool &visible)
{
	int32_t ret = TEXT_OBJ_ERROR_NO_LAYOUT;

	if (m_layout.get() != NULL) {
		TextGraphics *graphics;

		m_layout->GetGraphics(graphics);
		if (graphics != NULL)
			ret = graphics->GetCaretVisible(visible);
	}
	return ret;
}

int32_t TextObj::SetCaretColor(const math::v4 &color)
{
	int32_t ret = TEXT_OBJ_ERROR_NO_LAYOUT;

	if (m_layout.get() != NULL) {
		TextGraphics *graphics;

		m_layout->GetGraphics(graphics);
		if (graphics != NULL)
			ret = graphics->SetCaretColor(color);
	}
	return ret;
}

int32_t TextObj::GetCaretColor(math::v4 &color)
{
	int32_t ret = TEXT_OBJ_ERROR_NO_LAYOUT;

	if (m_layout.get() != NULL) {
		TextGraphics *graphics;

		m_layout->GetGraphics(graphics);
		if (graphics != NULL)
			ret = graphics->GetCaretColor(color);
	}
	return ret;
}

int32_t TextObj::SetSelectionColor(const math::v4 &color)
{
	int32_t ret = TEXT_OBJ_ERROR_NO_LAYOUT;

	if (m_layout.get() != NULL) {
		TextGraphics *graphics;

		m_layout->GetGraphics(graphics);
		if (graphics != NULL)
			ret = graphics->SetSelectionColor(color);
	}
	return ret;
}

int32_t TextObj::GetSelectionColor(math::v4 &color)
{
	int32_t ret = TEXT_OBJ_ERROR_NO_LAYOUT;

	if (m_layout.get() != NULL) {
		TextGraphics *graphics;

		m_layout->GetGraphics(graphics);
		if (graphics != NULL)
			ret = graphics->GetSelectionColor(color);
	}
	return ret;
}

int32_t TextObj::Invalidate()
{
	if (m_layout.get() == NULL)
		return TEXT_OBJ_ERROR_NO_LAYOUT;
	return LayoutChanged(m_layout->Invalidate());
}

const char TextObjParam::m_param_name[] = "textobj";

const char *TextObjParam::GetParamName() const
{
	return m_param_name;
}

const char *TextObjParam::ParamName()
{
	return m_param_name;
}

/* white text with a 40% black shadow, kinsoku shori on */
TextObjParam::TextObjParam()
	: m_pos(0.0f, 0.0f, 0.0f), m_size(0.0f, 0.0f, 0.0f), m_align_x(0), m_align_y(0),
	  m_text_align(0), m_max_line_count(0), m_line_space(0.0f), m_line_pitch(0.0f),
	  m_visible_pos(0.0f, 0.0f), m_visible_size(0.0f, 0.0f), m_adjust_x(false),
	  m_adjust_y(false), m_condense_x(false), m_condense_y(false), m_word_wrap(false),
	  m_kinsoku(true), m_ellipsis(false), m_enlarge_line_pitch(-1), m_family(0),
	  m_font_size(0.0f), m_font_color(1.0f, 1.0f, 1.0f, 1.0f),
	  m_shadow_color(0.0f, 0.0f, 0.0f, 0.4f), m_back_color(0.0f, 0.0f, 0.0f, 0.0f),
	  m_underline_color(0.0f, 0.0f, 0.0f, 0.0f), m_strike_color(0.0f, 0.0f, 0.0f, 0.0f),
	  m_bold(false), m_oblique(false), m_underline(false), m_strike(false), m_mask(false),
	  m_shadow(false), m_emboss(false), m_enlarge_font_size(-1), m_force_bold(-1)
{
}

TextObjParam::~TextObjParam()
{
}

int32_t TextObjParam::Create(int32_t mode)
{
	int32_t ret = DrawObjParam::Create(mode);

	if (ret != 0)
		return ret;

	if (m_serial != NULL)
		m_serial->AddParam("pos", &m_pos, ResourceParam::SEMANTIC_VEC3);
	if (m_serial != NULL)
		m_serial->AddParam("size", &m_size, ResourceParam::SEMANTIC_VEC3);
	if (m_serial != NULL)
		m_serial->AddParam("align_x", &m_align_x, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("align_y", &m_align_y, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("text_align", &m_text_align, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("max_line_count", &m_max_line_count, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("line_space", &m_line_space, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL)
		m_serial->AddParam("line_pitch", &m_line_pitch, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL)
		m_serial->AddParam("visible_pos", &m_visible_pos, ResourceParam::SEMANTIC_VEC2);
	if (m_serial != NULL)
		m_serial->AddParam("visible_size", &m_visible_size, ResourceParam::SEMANTIC_VEC2);
	if (m_serial != NULL)
		m_serial->AddParam("adjust_x", &m_adjust_x, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("adjust_y", &m_adjust_y, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("condense_x", &m_condense_x, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("condense_y", &m_condense_y, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("word_wrap", &m_word_wrap, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("kinsoku", &m_kinsoku, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("ellipsis", &m_ellipsis, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("enlarge_line_pitch", &m_enlarge_line_pitch,
				   ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("family", &m_family, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("font_size", &m_font_size, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL)
		m_serial->AddParam("font_color", &m_font_color, ResourceParam::SEMANTIC_VEC4);
	if (m_serial != NULL)
		m_serial->AddParam("shadow_color", &m_shadow_color, ResourceParam::SEMANTIC_VEC4);
	if (m_serial != NULL)
		m_serial->AddParam("back_color", &m_back_color, ResourceParam::SEMANTIC_VEC4);
	if (m_serial != NULL)
		m_serial->AddParam("underline_color", &m_underline_color,
				   ResourceParam::SEMANTIC_VEC4);
	if (m_serial != NULL)
		m_serial->AddParam("strike_color", &m_strike_color, ResourceParam::SEMANTIC_VEC4);
	if (m_serial != NULL)
		m_serial->AddParam("bold", &m_bold, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("oblique", &m_oblique, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("underline", &m_underline, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("strike", &m_strike, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("mask", &m_mask, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("shadow", &m_shadow, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("emboss", &m_emboss, ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("enlarge_font_size", &m_enlarge_font_size,
				   ResourceParam::SEMANTIC_BOOL);
	if (m_serial != NULL)
		m_serial->AddParam("force_bold", &m_force_bold, ResourceParam::SEMANTIC_BOOL);
	return ret;
}

/* the position and size through the object, the layout attributes into its layout (the
   enlarged line pitch unless -1) and the style attributes into its first style (the enlarged
   font size and forced bold unless -1) */
int32_t TextObjParam::SetEach(DrawObj *obj)
{
	int32_t ret = DrawObjParam::SetEach(obj);

	if (ret != 0)
		return ret;

	TextObj *text = (TextObj *)obj;

	text->SetPos(m_pos);
	text->SetSize(m_size);
	text->SetLayoutAttribute(TextLayoutAttribute_AlignX, m_align_x);
	text->SetLayoutAttribute(TextLayoutAttribute_AlignY, m_align_y);
	text->SetLayoutAttribute(TextLayoutAttribute_TextAlign, m_text_align);
	text->SetLayoutAttribute(TextLayoutAttribute_MaxLineCount, m_max_line_count);
	text->SetLayoutAttribute(TextLayoutAttribute_LineSpace, m_line_space);
	text->SetLayoutAttribute(TextLayoutAttribute_LinePitch, m_line_pitch);
	text->SetLayoutAttribute(TextLayoutAttribute_VisiblePos, m_visible_pos);
	text->SetLayoutAttribute(TextLayoutAttribute_VisibleSize, m_visible_size);
	text->SetLayoutAttribute(TextLayoutAttribute_AdjustX, m_adjust_x);
	text->SetLayoutAttribute(TextLayoutAttribute_AdjustY, m_adjust_y);
	text->SetLayoutAttribute(TextLayoutAttribute_CondenseX, m_condense_x);
	text->SetLayoutAttribute(TextLayoutAttribute_CondenseY, m_condense_y);
	text->SetLayoutAttribute(TextLayoutAttribute_WordWrap, m_word_wrap);
	text->SetLayoutAttribute(TextLayoutAttribute_Kinsoku, m_kinsoku);
	text->SetLayoutAttribute(TextLayoutAttribute_Ellipsis, m_ellipsis);
	if (m_enlarge_line_pitch != -1)
		text->SetLayoutAttribute(TextLayoutAttribute_EnlargeLinePitch,
					 m_enlarge_line_pitch != 0);

	TextStyle style;

	text->GetStyle(0, style);
	style.SetStyleAttribute(TextStyleAttribute_Family, m_family);
	style.SetStyleAttribute(TextStyleAttribute_Point, m_font_size);
	style.SetStyleAttribute(TextStyleAttribute_Color, m_font_color);
	style.SetStyleAttribute(TextStyleAttribute_ShadowColor, m_shadow_color);
	style.SetStyleAttribute(TextStyleAttribute_BackColor, m_back_color);
	style.SetStyleAttribute(TextStyleAttribute_UnderlineColor, m_underline_color);
	style.SetStyleAttribute(TextStyleAttribute_StrikeColor, m_strike_color);
	style.SetStyleAttribute(TextStyleAttribute_Bold, m_bold);
	style.SetStyleAttribute(TextStyleAttribute_Oblique, m_oblique);
	style.SetStyleAttribute(TextStyleAttribute_Underline, m_underline);
	style.SetStyleAttribute(TextStyleAttribute_Strike, m_strike);
	style.SetStyleAttribute(TextStyleAttribute_Mask, m_mask);
	style.SetStyleAttribute(TextStyleAttribute_Shadow, m_shadow);
	style.SetStyleAttribute(TextStyleAttribute_Emboss, m_emboss);
	if (m_enlarge_font_size != -1)
		style.SetStyleAttribute(TextStyleAttribute_EnlargeFontSize, m_enlarge_font_size != 0);
	if (m_force_bold != -1)
		style.SetStyleAttribute(TextStyleAttribute_ForceBold, m_force_bold != 0);
	text->SetStyle(0, 0, style);
	return ret;
}

int32_t TextObjParam::GetEach(DrawObj *obj)
{
	int32_t ret = DrawObjParam::GetEach(obj);

	if (ret != 0)
		return ret;

	TextObj *text = (TextObj *)obj;

	m_pos = text->m_pos;
	m_size = text->m_size;
	text->GetLayoutAttribute(TextLayoutAttribute_AlignX, m_align_x);
	text->GetLayoutAttribute(TextLayoutAttribute_AlignY, m_align_y);
	text->GetLayoutAttribute(TextLayoutAttribute_TextAlign, m_text_align);
	text->GetLayoutAttribute(TextLayoutAttribute_MaxLineCount, m_max_line_count);
	text->GetLayoutAttribute(TextLayoutAttribute_LineSpace, m_line_space);
	text->GetLayoutAttribute(TextLayoutAttribute_LinePitch, m_line_pitch);
	text->GetLayoutAttribute(TextLayoutAttribute_VisiblePos, m_visible_pos);
	text->GetLayoutAttribute(TextLayoutAttribute_VisibleSize, m_visible_size);
	text->GetLayoutAttribute(TextLayoutAttribute_AdjustX, m_adjust_x);
	text->GetLayoutAttribute(TextLayoutAttribute_AdjustY, m_adjust_y);
	text->GetLayoutAttribute(TextLayoutAttribute_CondenseX, m_condense_x);
	text->GetLayoutAttribute(TextLayoutAttribute_CondenseY, m_condense_y);
	text->GetLayoutAttribute(TextLayoutAttribute_WordWrap, m_word_wrap);
	text->GetLayoutAttribute(TextLayoutAttribute_Kinsoku, m_kinsoku);
	text->GetLayoutAttribute(TextLayoutAttribute_Ellipsis, m_ellipsis);
	text->GetLayoutAttribute(TextLayoutAttribute_EnlargeLinePitch, (bool &)m_enlarge_line_pitch);

	TextStyle style;

	text->GetStyle(0, style);
	style.GetStyleAttribute(TextStyleAttribute_Family, m_family);
	style.GetStyleAttribute(TextStyleAttribute_Point, m_font_size);
	style.GetStyleAttribute(TextStyleAttribute_Color, m_font_color);
	style.GetStyleAttribute(TextStyleAttribute_ShadowColor, m_shadow_color);
	style.GetStyleAttribute(TextStyleAttribute_BackColor, m_back_color);
	style.GetStyleAttribute(TextStyleAttribute_UnderlineColor, m_underline_color);
	style.GetStyleAttribute(TextStyleAttribute_StrikeColor, m_strike_color);
	style.GetStyleAttribute(TextStyleAttribute_Bold, m_bold);
	style.GetStyleAttribute(TextStyleAttribute_Oblique, m_oblique);
	style.GetStyleAttribute(TextStyleAttribute_Underline, m_underline);
	style.GetStyleAttribute(TextStyleAttribute_Strike, m_strike);
	style.GetStyleAttribute(TextStyleAttribute_Mask, m_mask);
	style.GetStyleAttribute(TextStyleAttribute_Shadow, m_shadow);
	style.GetStyleAttribute(TextStyleAttribute_Emboss, m_emboss);
	style.GetStyleAttribute(TextStyleAttribute_EnlargeFontSize, (bool &)m_enlarge_font_size);
	style.GetStyleAttribute(TextStyleAttribute_ForceBold, (bool &)m_force_bold);
	return ret;
}

}
}
