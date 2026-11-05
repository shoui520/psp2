/*
 * paf::graph::PageTurnObj: the page-turn draw object. Each draw puts the object's world position,
 * rotation, scale, alpha and page size into its parameter block and queues the page in the
 * scene's draw layer; the queued draw has the page-turn renderer draw it. The renderer is made
 * with the first page and lets go of its textures when the last one goes (the file's name ours,
 * after the class).
 */
#include <stdint.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/color.h>
#include <graphics/drawobj/page_turn_obj.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/effects/page_turn_renderer.h>
#include <graphics/surface/surface.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>

namespace paf {
namespace graph {

const char PageTurnObj::m_drawobj_type[] = "PageTurnObj";
bool PageTurnObj::s_renderer_made = false;
int32_t PageTurnObj::s_count = 0;
int32_t PageTurnObj::s_draw_count = 0;
intrusive_ptr<Surface> PageTurnObj::s_bg_texture;

bool PageTurnObj::IsInherit(const char *type) const
{
	if (type == m_drawobj_type)
		return true;
	return DrawObj::IsInherit(type);
}

const char *PageTurnObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

const char *PageTurnObj::DrawObjType()
{
	return m_drawobj_type;
}

PageTurnObj::PageTurnObj(const DrawStyle &style)
	: DrawObj(style), m_turn_changed(false), m_turn(0.0f, 0.0f), m_turn_offset(0.0f, 0.0f),
	  m_margin(0.0f, 0.0f)
{
	m_param = new PageTurnParam;
	m_mesh = new PageTurnMesh;

	if (!s_renderer_made)
		ScePafGraphics_4E038C05()->m_ptr->GetPageTurnRenderer(true);
	s_renderer_made = true;

	if (s_bg_texture.get() != NULL)
		ScePafGraphics_4E038C05()->m_ptr->GetPageTurnRenderer(false)->m_background.m_surface = s_bg_texture.get();

	m_param->m_renderer_param = math::v2(0.0f, 0.0f);
	s_count++;
}

PageTurnObj::~PageTurnObj()
{
	delete m_param;
	m_param = NULL;
	delete m_mesh;
	m_mesh = NULL;

	if (--s_count == 0) {
		ScePafGraphics_4E038C05()->m_ptr->GetPageTurnRenderer(false)->m_background.m_surface.clear();
		ScePafGraphics_4E038C05()->m_ptr->GetPageTurnRenderer(false)->ReleaseTextures();
		s_count = 0;
	}
}

}
}

using namespace paf;
using namespace paf::graph;

void ScePafGraphics_1612A18D()
{
	if (PageTurnObj::s_count != 0 && PageTurnObj::s_draw_count == 0)
		ScePafGraphics_4E038C05()->m_ptr->GetPageTurnRenderer(false)->ReleaseTextures();
	PageTurnObj::s_draw_count = 0;
}

namespace paf {
namespace graph {

void PageTurnObj::SetBgTexture(intrusive_ptr<Surface> texture)
{
	s_bg_texture = texture.get();
	ScePafGraphics_4E038C05()->m_ptr->GetPageTurnRenderer(false)->m_background.m_surface = s_bg_texture.get();
}

/* the page is the object's size less its margin on each side; when the turn changed, its curl
   is the turn scaled to the page's aspect */
void PageTurnObj::OnDraw(const math::matrix &parent, uint32_t color, float alpha, DrawInfo &info)
{
	if (!m_flags.m_visible)
		return;

	ScePafGraphics_B7EB9246(this, &color, &alpha);

	PageTurnParam *param = m_param;
	float32x4_t size = m_size.getImpl().get128();

	param->m_margin = m_margin;
	param->m_page_size = math::v2(sce::Vectormath::Simd::Aos::Vector2(vget_low_f32(size))) -
		m_margin * math::v2(2.0f, 2.0f);

	if (m_turn_changed) {
		math::v2 page = param->m_page_size;
		math::v2 curl = (m_turn + m_turn_offset) * (page / page.extract_y());

		if (!((curl.getImpl().getX() == param->m_curl.getImpl().getX()) &
		      (curl.getImpl().getY() == param->m_curl.getImpl().getY())).getAsBool()) {
			param->m_curl = curl;
			param->m_curl_changed = true;
		}
		m_turn_changed = false;
	}

	param->m_alpha = alpha;

	math::v4 pos = parent.w();
	math::quaternion rot(parent);

	param->m_rot = math::v4(sce::Vectormath::Simd::Aos::Vector4(rot.getImpl())).unit();
	param->m_pos = pos;
	m_param->m_scale = math::v4(sce::Vectormath::Simd::Aos::Vector4(parent.x().len(),
		parent.y().len(), parent.z().len(), sce::Vectormath::Simd::floatInVec(0.0f)));

	if (alpha > 0.0f) {
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
}

void PageTurnObj::DrawCallback(const DrawCallbackArg &arg)
{
	arg.self->Draw(*arg.world_mat, arg.color, arg.alpha);
}

void PageTurnObj::Draw(const math::matrix &world, uint32_t color, float alpha)
{
	ScePafGraphics_4E038C05()->m_ptr->GetPageTurnRenderer(false)->Draw(m_param, m_mesh);
	s_draw_count++;
}

void PageTurnObj::ReleaseAll()
{
	m_param->m_texture.clear();
}

void PageTurnObj::SetTexture(const intrusive_ptr<Surface> &texture, uint32_t index)
{
	m_param->m_texture = texture.get();
}

void PageTurnObj::GetTexture(intrusive_ptr<Surface> &texture, uint32_t index)
{
	texture = m_param->m_texture.get();
}

}
}

void ScePafGraphics_2B9FB1E5()
{
}

namespace paf {
namespace graph {

const char PageTurnObjParam::m_param_name[] = "planeobj";

const char *PageTurnObjParam::GetParamName() const
{
	return m_param_name;
}

const char *PageTurnObjParam::ParamName()
{
	return m_param_name;
}

PageTurnObjParam::PageTurnObjParam()
{
}

PageTurnObjParam::~PageTurnObjParam()
{
}

int32_t PageTurnObjParam::Create(int32_t mode)
{
	return DrawObjParam::Create(mode);
}

}
}
