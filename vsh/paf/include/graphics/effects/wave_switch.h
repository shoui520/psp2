/*
 * The wave switch: while one is made the waves are drawn, in the layers' priorities 1001 and
 * 900, their presets moved to the current wave's and held off by either of two holds. The
 * exports' names are ScePafGraphics_<NID> (no NID hashes as a name tried) except
 * scePafGraphicsUpdateCurrentWave; the class, the file's and every other name are ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_WAVE_SWITCH_H
#define PAF_GRAPHICS_EFFECTS_WAVE_SWITCH_H

#include <stdint.h>
#include <graphics/effects/wave_renderer.h>

/* WaveSwitch_SetPreset's error for a preset of another wave than the last (name ours) */
#define SCE_PAF_ERROR_WAVE_INVALID_PRESET	(-2136014720)	/* 0x80AF0080 */

namespace paf {
namespace graph {

/* the number of the waves the system has presets of; the last is the user's own (name ours) */
#define WAVE_COUNT	32

/* 1 byte */
class WaveSwitch {
public:
	/* ScePafGraphics_C0487D0A (C1), ScePafGraphics_ACD27A89 (C2): the switch made on, the
	   waves' layers given their priorities */
	WaveSwitch();
	/* ScePafGraphics_2A2F68EF (D1), ScePafGraphics_2FB40E94 (D2): the layers drawn by their
	   owner again */
	~WaveSwitch();

	/* the waves drawn and moved on */
	bool m_on;
};

/* the presets' variant names, by the waves' numbers (Sony's names, the table's ours) */
extern const char *s_wave_names[WAVE_COUNT];

/* the switch turned on, unless the other hold keeps it off, and off (names ours; called by
   ScePafWidget_DC1B19DB) */
void ReleaseWaveHold();
void SetWaveHold();

}
}

extern "C" {
/* the switch made, unless switches are disabled (vitasdk: none) */
void ScePafGraphics_3EB90427(void);
/* the switch deleted (vitasdk: none) */
void ScePafGraphics_2665FBAA(void);
/* switches disabled or not (vitasdk: none) */
void ScePafGraphics_52C45E43(bool disabled);
/* the waves' layers drawn by the scene while the switch is on, by their owner while it is off
   (vitasdk: none) */
void ScePafGraphics_4EF30D15(void);
/* the waves moved on while the switch is on (vitasdk: none) */
void ScePafGraphics_3AFA1144(void);
/* the waves' layer, and their debug layer, drawn now (vitasdk: none) */
void ScePafGraphics_BE4C5403(void);
void ScePafGraphics_47F42806(void);
/* the waves' presets moving to the index-th wave's, over update_interval seconds; the default
   wave's past the last */
void scePafGraphicsUpdateCurrentWave(uint32_t index, float update_interval);
/* the first hold released and set (vitasdk: none) */
void ScePafGraphics_906BE45C(void);
void ScePafGraphics_00B9D8CC(void);
/* whether a switch is on (vitasdk: none) */
bool ScePafGraphics_DD2002B4(void);
/* the user's own wave's presets set, the preset's index the last wave's (vitasdk: none) */
int32_t ScePafGraphics_45A01FA1(const paf::graph::WavePreset *preset);
}

#endif
