/*
 * The panel objects alive (DEBUGGER 0x810d5f88..0x810d6934): the bubbles (kinds 0 and 2) and the
 * panels (kind 1) register themselves here, each frame's draws of them are counted, and the
 * layers the panel renderer draws them in are drawn by the scene only while there are some. No
 * record names the file or anything in it: every name is ours.
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/color.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/drawobj/bubble_obj.h>
#include <graphics/drawobj/draw_env_flush.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/panel_obj.h>
#include <graphics/effects/panel_renderer.h>
#include <graphics/effects/wave_renderer.h>
#include <std/map>

namespace paf {
namespace graph {

/* the scene draws the refractions' and the glass bubbles' layers while there are no panels */
bool s_panel_refractions = true;

/* the panel renderer made; the bubbles and the panels alive; the bubbles, nothing and the
   panels drawn this frame; the objects alive, by their kinds */
bool s_panel_objs_ready;
int32_t s_bubble_count;
int32_t s_panel_count;
int32_t s_draws_other;
int32_t s_draws_bubble;
int32_t s_draws_panel;
map<DrawObj *, int32_t> s_panel_objs;

/* the renderer the panel objects are drawn with */
static inline Renderer *PanelObjRenderer()
{
	return ScePafGraphics_4E038C05()->m_ptr;
}

void RegisterPanelObj(DrawObj *obj, int32_t kind)
{
	if (!s_panel_objs_ready) {
		PanelObjRenderer()->GetPanelRenderer(true);
		s_panel_objs_ready = true;
	}
	if (s_panel_objs.find(obj) != s_panel_objs.end())
		return;

	s_panel_objs[obj] = kind;
	if (kind == 0 || kind == 2)
		s_bubble_count++;
	else if (kind == 1)
		s_panel_count++;
}

void UnregisterPanelObj(DrawObj *obj)
{
	map<DrawObj *, int32_t>::iterator it = s_panel_objs.find(obj);

	if (it == s_panel_objs.end())
		return;

	int32_t kind = it->second;

	if ((kind == 0 || kind == 2) && s_bubble_count > 0)
		s_bubble_count--;
	else if (kind == 1 && s_panel_count > 0)
		s_panel_count--;
	s_panel_objs.erase(it);
}

/* the panel objects' layers drawn by the scene this frame (1) while any were drawn, else by
   their owners (2), and the frame's draws counted from 0 again */
void UpdatePanelLayers()
{
	if (!s_panel_objs_ready)
		return;

	int32_t mode = 1;

	if (s_panel_objs.size() == 0 || (s_draws_bubble | s_draws_other | s_draws_panel) == 0)
		mode = 2;
	s_draws_bubble = 0;
	s_draws_other = 0;
	s_draws_panel = 0;

	PanelObjRenderer()->SetDrawMode(PanelObjRenderer()->m_rs_capture_reflections, mode);
	if (!s_panel_refractions || s_panel_count != 0) {
		PanelObjRenderer()->SetDrawMode(PanelObjRenderer()->m_rs_copy_screen_for_refractions,
						mode);
		PanelObjRenderer()->SetDrawMode(
			PanelObjRenderer()->GetPanelRenderer(false)->m_rs_bubble_glass, mode);
	} else {
		PanelObjRenderer()->SetDrawMode(PanelObjRenderer()->m_rs_copy_screen_for_refractions,
						2);
		PanelObjRenderer()->SetDrawMode(
			PanelObjRenderer()->GetPanelRenderer(false)->m_rs_bubble_glass, 2);
	}
	PanelObjRenderer()->SetDrawMode(
		PanelObjRenderer()->GetPanelRenderer(false)->m_rs_solid_3x3_panel,
		s_panel_count > 0 ? mode : 2);
	PanelObjRenderer()->SetDrawMode(
		PanelObjRenderer()->GetPanelRenderer(false)->m_rs_bubble_opaque, mode);
}

/* the refractions', the glass and opaque bubbles' and the panels' layers drawn now, those that
   take draws this frame */
void FlushPanelLayers()
{
	if (!s_panel_objs_ready)
		return;

	if (PanelObjRenderer()->m_rs_copy_screen_for_refractions->m_state == 1)
		PanelObjRenderer()->m_draw_env->FlushLayers(
			PanelObjRenderer()->m_rs_copy_screen_for_refractions);
	if (PanelObjRenderer()->GetPanelRenderer(false)->m_rs_bubble_glass->m_state == 1)
		PanelObjRenderer()->m_draw_env->FlushLayers(
			PanelObjRenderer()->GetPanelRenderer(false)->m_rs_bubble_glass);
	if (PanelObjRenderer()->GetPanelRenderer(false)->m_rs_bubble_opaque->m_state == 1)
		PanelObjRenderer()->m_draw_env->FlushLayers(
			PanelObjRenderer()->GetPanelRenderer(false)->m_rs_bubble_opaque);
	if (PanelObjRenderer()->GetPanelRenderer(false)->m_rs_solid_3x3_panel->m_state == 1)
		PanelObjRenderer()->m_draw_env->FlushLayers(
			PanelObjRenderer()->GetPanelRenderer(false)->m_rs_solid_3x3_panel);
}

/* the reflections' layer drawn now, when it takes draws this frame */
void FlushPanelReflections()
{
	if (!s_panel_objs_ready)
		return;

	if (PanelObjRenderer()->m_rs_capture_reflections->m_state == 1)
		PanelObjRenderer()->m_draw_env->FlushLayers(
			PanelObjRenderer()->m_rs_capture_reflections);
}

void DrawPanelObj(DrawObj *obj, int32_t kind, int32_t flags)
{
	if (!s_panel_objs_ready || s_panel_objs.size() == 0)
		return;

	if (kind == 0) {
		BubbleParam *param = static_cast<BubbleObj *>(obj)->m_param;

		PanelObjRenderer()->GetPanelRenderer(false)->AddBubbleObj(
			param, 0, PanelObjRenderer()->GetWaveRenderer(false), NULL);
		s_draws_bubble++;
	} else if (kind == 2) {
		BubbleParam *param = static_cast<BubbleObj *>(obj)->m_param;

		PanelObjRenderer()->GetPanelRenderer(false)->AddBubbleObj(
			param, 1, PanelObjRenderer()->GetWaveRenderer(false), NULL);
		s_draws_bubble++;
	} else if (kind == 1) {
		PanelParam *param = static_cast<PanelObj *>(obj)->m_param;

		PanelObjRenderer()->GetPanelRenderer(false)->AddSolid3x3Panel(
			*param, param->m_revolution.m_axis[1] > 0.0f, 1);
		s_draws_panel++;
	}
}

}
}
