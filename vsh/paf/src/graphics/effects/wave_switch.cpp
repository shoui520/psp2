/*
 * The wave switch (DEBUGGER 0x810d5580..0x810d5a9e): the waves drawn while a switch is made and
 * on, and their presets moved from wave to wave. The preset names and
 * scePafGraphicsUpdateCurrentWave are Sony's; the class, the file's and the other names ours.
 * The effects headers come first: Sony's file constructs their globals (the resource folders, the
 * colours, the tile size, the FFT waves' defaults) in that order.
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/color.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/effects/fft_wave.h>
#include <graphics/drawobj/draw_env_flush.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/effects/wave_renderer.h>
#include <graphics/effects/wave_switch.h>
#include <std/string>

namespace paf {
namespace graph {

const char *s_wave_names[WAVE_COUNT] = {
	"effects_default_0512_01",
	"iboot",
	"effects_0406_ruby",
	"effects_0407_purple_01",
	"effects_0408_01_orange",
	"effects_0411_purple_02",
	"effects_0415_A5",
	"effects_0518_dalia",
	"effects_0518_gray_FLAT",
	"effects_0518_ruby",
	"effects_A1",
	"effects_black",
	"effects_blue_FLAT",
	"effects_blueTB_0513_01",
	"effects_bluewhite_05",
	"effects_casis",
	"effects_green_0512_01",
	"effects_koiao_0512_01",
	"effects_maintosh",
	"effects_marrygold",
	"effects_peach_0512_001",
	"effects_purple_01",
	"effects_purplered",
	"effects_yellow_0512_01",
	"effects_redTB_0513_01",
	"effects_limegreen_0513_02",
	"effects_turquoise_0513_01",
	"effects_color_vari_2g_01",
	"effects_color_vari_2g_02",
	"effects_color_vari_2g_03",
	"effects_color_vari_2g_04",
	"effects_original_01"
};

/* the wave the presets last moved to (Sony's name); switches disabled; the switch; the two holds
   (names ours) */
extern "C" {
uint32_t scePafGraphicsCurrentWave;
}
bool s_wave_switch_disabled;
WaveSwitch *s_wave_switch;
bool s_wave_hold;
bool s_wave_hold2;

/* the waves' renderer (name ours) */
static inline WaveRenderer *Waves(bool create)
{
	return ScePafGraphics_4E038C05()->m_ptr->GetWaveRenderer(create).get();
}

__attribute__((always_inline)) WaveSwitch::WaveSwitch() : m_on(true)
{
	s_wave_switch = this;

	DrawLayer *layer = Waves(true)->m_rs_wave;

	if (layer != NULL)
		DrawCalls()->SetPriority(layer, 1001);
	layer = Waves(true)->m_rs_wave_debug;
	if (layer != NULL)
		DrawCalls()->SetPriority(layer, 900);
}

}
}

using namespace paf;
using namespace paf::graph;

void ScePafGraphics_3EB90427(void)
{
	if (!s_wave_switch_disabled)
		new WaveSwitch;
}

__attribute__((always_inline)) WaveSwitch::~WaveSwitch()
{
	s_wave_switch = NULL;
	DrawCalls()->SetDrawMode(Waves(false)->m_rs_wave, 2);
	DrawCalls()->SetDrawMode(Waves(false)->m_rs_wave_debug, 2);
}

void ScePafGraphics_2665FBAA(void)
{
	WaveSwitch *wave_switch = s_wave_switch;

	if (wave_switch != NULL)
		delete wave_switch;
}

void ScePafGraphics_52C45E43(bool disabled)
{
	s_wave_switch_disabled = disabled;
}

void ScePafGraphics_4EF30D15(void)
{
	if (s_wave_switch == NULL)
		return;

	int32_t mode = 1;

	if (!s_wave_switch->m_on)
		mode = 2;
	ScePafGraphics_4E038C05()->m_ptr->SetDrawMode(Waves(false)->m_rs_wave, mode);
	ScePafGraphics_4E038C05()->m_ptr->SetDrawMode(Waves(false)->m_rs_wave_debug, mode);
}

void ScePafGraphics_3AFA1144(void)
{
	if (s_wave_switch != NULL && s_wave_switch->m_on)
		Waves(false)->Update();
}

void ScePafGraphics_BE4C5403(void)
{
	if (s_wave_switch == NULL)
		return;
	if (Waves(false)->m_rs_wave->m_state == 1)
		ScePafGraphics_4E038C05()->m_ptr->m_draw_env->FlushLayers(Waves(false)->m_rs_wave);
}

void ScePafGraphics_47F42806(void)
{
	if (s_wave_switch == NULL)
		return;
	if (Waves(false)->m_rs_wave_debug->m_state == 1)
		ScePafGraphics_4E038C05()->m_ptr->m_draw_env->FlushLayers(
			Waves(false)->m_rs_wave_debug);
}

void scePafGraphicsUpdateCurrentWave(uint32_t index, float update_interval)
{
	if (s_wave_switch == NULL || scePafGraphicsCurrentWave == index)
		return;

	scePafGraphicsCurrentWave = index;
	if (index <= WAVE_COUNT - 1)
		Waves(false)->m_param_keys.StartTransition(s_wave_names[index], update_interval, 0,
							    0.09f);
	else
		Waves(false)->m_param_keys.StartTransition("effects_default_0512_01",
							    update_interval, 0, 0.09f);
}

void ScePafGraphics_906BE45C(void)
{
	if (s_wave_switch == NULL)
		return;
	s_wave_hold = false;
	if (!s_wave_hold2)
		s_wave_switch->m_on = true;
}

void ScePafGraphics_00B9D8CC(void)
{
	if (s_wave_switch == NULL)
		return;
	s_wave_hold = true;
	s_wave_switch->m_on = false;
}

void paf::graph::ReleaseWaveHold()
{
	if (s_wave_switch == NULL)
		return;
	s_wave_hold2 = false;
	if (!s_wave_hold)
		s_wave_switch->m_on = true;
}

void paf::graph::SetWaveHold()
{
	if (s_wave_switch == NULL)
		return;
	s_wave_hold2 = true;
	s_wave_switch->m_on = false;
}

bool ScePafGraphics_DD2002B4(void)
{
	if (s_wave_switch != NULL)
		return s_wave_switch->m_on;
	return false;
}

int32_t ScePafGraphics_45A01FA1(const WavePreset *preset)
{
	if (preset->m_index != WAVE_COUNT - 1)
		return SCE_PAF_ERROR_WAVE_INVALID_PRESET;
	return Waves(false)->SetPreset(s_wave_names[preset->m_index], *preset);
}

/* 0x810d5a9e: nothing, called by nothing (name ours) */
void paf::graph::WaveSwitchNothing()
{
}
