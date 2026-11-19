/*
 * SceCtrl's button remapping, as the accessibility settings set it (the shell's
 * keyremap_data, /CONFIG/ACCESSIBILITY): each button's replacement, and whether the analog
 * sticks trade places. The header's and the structure's names are ours.
 */
#ifndef CTRL_CTRL_REMAP_H
#define CTRL_CTRL_REMAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctrlButtonRemappingInfo {
	uint8_t buttons[14];
	uint8_t reserved;
	uint8_t swapAnalogSticks;
};

/* NULL: no remapping */
int sceCtrlSetButtonRemappingInfo(const struct ctrlButtonRemappingInfo *info);

#ifdef __cplusplus
}
#endif

#endif
