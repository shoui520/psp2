/*
 * The wave switch's exports (wave_switch.h): declared apart, so a program that calls them does
 * not take the effects headers' globals with them. The file's name is ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_WAVE_SWITCH_API_H
#define PAF_GRAPHICS_EFFECTS_WAVE_SWITCH_API_H

#include <stdint.h>

namespace paf {
namespace graph {
class WavePreset;
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
