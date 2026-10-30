/* Native ScePafGraphics globals, field accessors and GXM context forwarding. */
#include <stdint.h>
#include "../compiler.h"
#include <gxm.h>

uint8_t sce_paf_graphics_flag_306c;
uint32_t sce_paf_graphics_state_11e0;
uint32_t sce_paf_graphics_state_12ac;
uint32_t sce_paf_graphics_state_10fe4;
uint32_t sce_paf_graphics_state_11fc;
uint32_t sce_paf_graphics_state_12b0;

uint32_t ScePafGraphics_6C448991(void)
{
	return sce_paf_graphics_state_11e0;
}

uint32_t ScePafGraphics_77872076(void)
{
	return sce_paf_graphics_state_12ac;
}

uint32_t ScePafGraphics_CDE30B64(void)
{
	return sce_paf_graphics_state_10fe4;
}

uint32_t ScePafGraphics_52C45E43(uint32_t value)
{
	sce_paf_graphics_flag_306c = value;
	return value;
}

uint32_t ScePafGraphics_D666D983(uint32_t value)
{
	sce_paf_graphics_state_11fc = value;
	return value;
}

uint32_t ScePafGraphics_E4AB21CC(uint32_t value)
{
	sce_paf_graphics_state_12b0 = value;
	return value;
}

static inline __attribute__((always_inline))
uint32_t *InitializeGraphicsState(uint32_t *self)
{
	self[24] = 0;
	self[29] = 0;
	self[31] = 0;
	self[30] = 0;
	return self;
}

uint32_t *ScePafGraphics_13DC10AE(uint32_t *self)
{
	return InitializeGraphicsState(self);
}

uint32_t *ScePafGraphics_A5727A5A(uint32_t *self)
{
	return InitializeGraphicsState(self);
}

int ScePafGraphics_428855D1(const uint8_t *self,
			     uint32_t *first, uint32_t *second)
{
	*first = *(const uint32_t *)(self + 96);
	*second = *(const uint32_t *)(self + 100);
	return 0;
}

void *ScePafGraphics_AEBC68E5(const uint8_t *self, const uint64_t *value)
{
	uint8_t *target = *(uint8_t *const *)(self + 80);

	*(uint64_t *)(target + 48) = *value;
	return target;
}

void ScePafGraphics_6E7B77AC(const uint8_t *self, float value)
{
	sceGxmSetWClampValue(*(SceGxmContext *const *)(self + 1676), value);
}

void ScePafGraphics_E4B3B1D2(const uint8_t *self)
{
	sceGxmFinish(*(SceGxmContext *const *)(self + 1676));
}

int32_t ScePafGraphics_F3CD22A0(void)
{
	return -2127491096;
}
