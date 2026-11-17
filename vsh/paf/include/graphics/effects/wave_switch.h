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
#include <graphics/effects/wave_switch_api.h>

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
	__declspec(dllexport) WaveSwitch();
	/* ScePafGraphics_2A2F68EF (D1), ScePafGraphics_2FB40E94 (D2): the layers drawn by their
	   owner again */
	__declspec(dllexport) ~WaveSwitch();

	/* the waves drawn and moved on */
	bool m_on;
};

/* the presets' variant names, by the waves' numbers (Sony's names, the table's ours) */
extern const char *s_wave_names[WAVE_COUNT];

/* the switch turned on, unless the other hold keeps it off, and off (names ours; called by
   ScePafWidget_DC1B19DB) */
void ReleaseWaveHold();
void SetWaveHold();
/* an empty function after them (name ours) */
void WaveSwitchNothing();

}
}

#endif
