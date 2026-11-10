/* Standalone native widget state constructors and field operations. */
#include <stdint.h>
#include <arm_neon.h>
#include "../std/new.h"
#include "../std/string.h"

/* the screen size of each resolution, qHD, HD and full HD (Resolution's rodata; declared in
   widget/core/environment.h; until its file is C) */
const uint32_t g_screen_width_qhd = 960;
const uint32_t g_screen_height_qhd = 544;
const uint32_t g_screen_width_hd = 1280;
const uint32_t g_screen_height_hd = 725;
const uint32_t g_screen_width_full_hd = 1920;
const uint32_t g_screen_height_full_hd = 1088;

int ScePafWidget_9A7BE3B0(uint8_t *self, uint32_t value, uint8_t flag)
{
	self[205] = flag;
	*(uint32_t *)(self + 184) = value;
	return 0;
}

int ScePafWidget_E8F862FC(void *self, const void *value)
{
	return value ? 0 : -1;
}


/* paf::graph::s_wait_vblank_count (surface_display.cpp) */
extern uint32_t _ZN3paf5graph19s_wait_vblank_countE;

uint32_t *ScePafWidget_10BEC7CF(uint32_t *self)
{
	++_ZN3paf5graph19s_wait_vblank_countE;
	++*self;
	return self;
}

uint32_t *ScePafWidget_DB993F51(uint32_t *self)
{
	uint32_t count = *self;

	if (count) {
		if (_ZN3paf5graph19s_wait_vblank_countE) {
			--_ZN3paf5graph19s_wait_vblank_countE;
			count = *self;
		}
		*self = count - 1;
	}
	return self;
}
