/*
 * 2014 PAF EarthRenderer / map layer registration state and frame hooks
 * (UpdateMapLayers at 0x810a0c44, FlushMapLayers at 0x810a0c9e).
 */
#include <stdint.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/drawobj/draw_env_flush.h>
#include <graphics/effects/earth_renderer.h>

namespace paf {
namespace graph {

bool g_earth_objs_ready;
int32_t g_earth_count;
int32_t g_draws_earth;

static inline Renderer *MapLayerRenderer()
{
	return ScePafGraphics_4E038C05()->m_ptr;
}

void UpdateMapLayers()
{
	if (!g_earth_objs_ready)
		return;

	int32_t mode = 2;

	if (g_earth_count > 0)
		mode = 1;
	if (g_draws_earth == 0)
		mode = 2;
	g_draws_earth = 0;

	Renderer *renderer = MapLayerRenderer();
	DrawLayer *earth_layer = ScePafGraphics_71E2CB77(renderer, false)->m_rs_earth;

	renderer->SetDrawMode(earth_layer, mode);
}

void FlushMapLayers()
{
	if (!g_earth_objs_ready || g_earth_count <= 0)
		return;

	Renderer *renderer = MapLayerRenderer();
	DrawLayer *earth_layer = ScePafGraphics_71E2CB77(renderer, false)->m_rs_earth;

	if (earth_layer->m_state == 1)
		renderer->m_draw_env->FlushLayers(earth_layer);
}

}
}
