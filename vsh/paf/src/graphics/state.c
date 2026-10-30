/* Native ScePafGraphics globals, field accessors and GXM context forwarding. */
#include <stdint.h>
#include "../compiler.h"
#include "../std/string.h"
#include "../std/new.h"
#include <gxm.h>

uint8_t sce_paf_graphics_flag_306c;
uint32_t sce_paf_graphics_state_11e0;
uint32_t sce_paf_graphics_state_12ac;
uint32_t sce_paf_graphics_state_10fe4;
uint32_t sce_paf_graphics_state_11fc;
uint32_t sce_paf_graphics_state_12b0;
uint8_t sce_paf_graphics_flag_06e8;
int32_t *sce_paf_graphics_active_surface_2e94;

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

int ScePafGraphics_1AD606BC(uint8_t *self, uint16_t bits)
{
	*(uint16_t *)(self + 112) |= bits;
	return 0;
}

int32_t ScePafGraphics_22AB24A0(const uint8_t *self, int32_t value)
{
	return ((value << *(const uint32_t *)(self + 12)) + 7) >> 3;
}

int32_t ScePafGraphics_3B5327DF(const uint8_t *self)
{
	return *(const int32_t *)(self + 56) *
		(3 - *(const int32_t *)(self + 32));
}

uint8_t *ScePafGraphics_4C4593FF(void)
{
	sce_paf_graphics_flag_06e8 = 0;
	return &sce_paf_graphics_flag_06e8;
}

uint8_t *ScePafGraphics_A05227FA(void)
{
	sce_paf_graphics_flag_06e8 = 1;
	return &sce_paf_graphics_flag_06e8;
}

int32_t *ScePafGraphics_8624501D(int32_t *result)
{
	*result = -2127676444;
	return result;
}

int32_t *ScePafGraphics_AAB453CB(int32_t *result)
{
	*result = -2127676444;
	return result;
}

void *ScePafGraphics_AE20B115(void *self, void *destination,
			      const void *source, uint32_t count)
{
	return sce_paf_memcpy(destination, source, count);
}

int32_t ScePafGraphics_BEF37718(void)
{
	return *sce_paf_graphics_active_surface_2e94;
}

int ScePafGraphics_E61D19A9(const uint8_t *self,
			     uint32_t *first, uint32_t *second)
{
	*first = *(const uint32_t *)(self + 4);
	*second = *(const uint32_t *)(self + 8);
	return 0;
}

int32_t *sce_paf_graphics_buffer_11e4;
uint8_t *sce_paf_graphics_display_06f8;

uint8_t ScePafGraphics_505CD74E(uint8_t value)
{
	sce_paf_graphics_display_06f8[1432] = value;
	return value;
}

uint32_t *ScePafGraphics_57664903(const void *self, uint32_t index)
{
	return sce_paf_graphics_buffer_11e4 + index;
}

int ScePafGraphics_59EC6C24(const uint8_t *self, PafWString *result)
{
	PafWStringCopy(result, (const PafWString *)(self + 4));
	return 0;
}

int32_t ScePafGraphics_59FF472E(const uint8_t *self)
{
	return ((*(const int16_t *)(self + 28) <<
		  *(const uint32_t *)(self + 12)) + 7) >> 3;
}

uint8_t *ScePafGraphics_5B6A4EA2(const uint8_t *self, uint32_t index)
{
	return *(uint8_t *const *)(self + 148) + index * 48;
}

uint32_t *ScePafGraphics_7E87FE3F(uint32_t *self, uint32_t value)
{
	self[2] = value;
	self[0] = 0;
	self[1] = 0;
	self[3] = (uint32_t)-2;
	return self;
}

int ScePafGraphics_83ABDFF5(const uint8_t *self, uint8_t argument)
{
	void *object = *(void *const *)(self + 4);

	if (object)
		return ((int (**)(void *, uint8_t))*(void **)object)[7](
			object, argument);
	return 0;
}

uint8_t *ScePafGraphics_87657577(const uint8_t *self, int32_t index)
{
	uint8_t *base = *(uint8_t *const *)(self + 108);

	if (base)
		return base + *(const int32_t *)(self + 56) * index;
	return 0;
}

uint32_t *ScePafGraphics_E79F111A(uint32_t *self, uint32_t value)
{
	self[2] = value;
	self[0] = 0;
	self[1] = 0;
	self[3] = (uint32_t)-2;
	return self;
}

static inline __attribute__((always_inline))
uint32_t *InitializeGraphicsFlags(uint32_t *self)
{
	self[0] = 0;
	self[1] = 0;
	self[5] = 0x100000;
	self[7] = 0;
	self[6] = 0;
	return self;
}

uint32_t *ScePafGraphics_212605A1(uint32_t *self)
{
	return InitializeGraphicsFlags(self);
}

uint32_t *ScePafGraphics_2F46C2B9(uint32_t *self)
{
	return InitializeGraphicsFlags(self);
}

int32_t ScePafGraphics_133EC137(void *self, SceGxmTexture **texture,
			       void *palette)
{
	int32_t result = sceGxmTextureSetPalette(*texture, palette);
	return result >= 0 ? 0 : result;
}

int ScePafGraphics_D53DC736(const uint8_t *self)
{
	void *object = *(void *const *)(self + 4);

	if (object)
		return ((int (**)(void *))*(void **)object)[4](object);
	return 0;
}

int ScePafGraphics_B000FA14(const uint8_t *self)
{
	int result = *(const int16_t *)(self + 102) - 1;
	if (result < 0)
		return *(const int16_t *)(self + 100) - 1;
	return result;
}

static inline __attribute__((always_inline))
uint32_t *InitializeGraphicsZeros(uint32_t *self)
{
	self[0] = 0;
	self[1] = 0;
	self[2] = 0;
	self[3] = 0;
	self[4] = 0;
	self[5] = 0;
	self[7] = 0;
	self[6] = 0;
	return self;
}

uint32_t *ScePafGraphics_19163245(uint32_t *self)
{
	return InitializeGraphicsZeros(self);
}

uint32_t *ScePafGraphics_BF863FEE(uint32_t *self)
{
	return InitializeGraphicsZeros(self);
}

int ScePafGraphics_404AC3D7(const uint8_t *self)
{
	void *object = *(void *const *)(self + 4);

	if (object)
		return ((int (**)(void *))*(void **)object)[8](object);
	return 1;
}

static inline __attribute__((always_inline))
int GraphicsInvokeOffset20(const uint32_t *self)
{
	void *object = (void *)(uintptr_t)self[0];
	return ((int (**)(void *, uint32_t, uint32_t, uint32_t))
		*(void **)object)[5](object, self[1], self[2], self[3]);
}

int ScePafGraphics_4423922B(const uint32_t *self)
{
	return GraphicsInvokeOffset20(self);
}

int ScePafGraphics_7C402472(const uint32_t *self)
{
	return GraphicsInvokeOffset20(self);
}

void ScePafGraphics_722A4987(uint8_t *self, SceGxmCullMode mode)
{
	SceGxmContext *context =
		*(SceGxmContext *const *)(self + 1676);
	*(SceGxmCullMode *)(self + 1896) = mode;
	sceGxmSetCullMode(context, mode);
}

__attribute__((noinline))
int ScePafGraphics_8D25DDE9(const void *self, const char *tag)
{
	return tag == "DrawObj";
}

uint8_t *sce_paf_graphics_indicator_3070;

int ScePafGraphics_BD1F2F7D(uint8_t *self, uint32_t value)
{
	if (value > 0x3f)
		return -1;
	*(uint32_t *)(self + 92) = value;
	return 0;
}

int ScePafGraphics_DD2002B4(void)
{
	if (sce_paf_graphics_indicator_3070)
		return *sce_paf_graphics_indicator_3070;
	return 0;
}

void ScePafGraphics_EA96F738(int32_t *self)
{
	*self = -2127676444;
	PafDelete(self);
}

int ScePafGraphics_FBC9302A(const uint32_t *self)
{
	return GraphicsInvokeOffset20(self);
}

uint32_t *ScePafGraphics_570E7907(uint32_t *self, uint32_t second,
				    uint32_t third, uint32_t fourth,
				    uint32_t fifth)
{
	self[88] = third;
	self[89] = second;
	self[90] = fourth;
	self[91] = fifth;
	return self;
}

int ScePafGraphics_0FD022D5(uint8_t *self, int mode, uint8_t value)
{
	if (mode != 0)
		return -2136005886;
	self[4] = value;
	return 0;
}

static inline __attribute__((always_inline))
int GraphicsIndexedValue(const uint8_t *self, uint32_t index)
{
	if (index < 4) {
		uint8_t *item = *(uint8_t *const *)(self + 196 + 4 * index);
		if (item)
			return *(const int32_t *)(item + 72);
	}
	return -1;
}

int ScePafGraphics_1D53BDC2(const uint8_t *self, uint32_t index)
{
	return GraphicsIndexedValue(self, index);
}

int ScePafGraphics_9DCF66E4(const uint8_t *self, uint32_t index)
{
	return GraphicsIndexedValue(self, index);
}

int ScePafGraphics_5FDB925A(const void *self, const char *tag)
{
	if (tag == "PlaneObj")
		return 1;
	return ScePafGraphics_8D25DDE9(self, tag);
}

int ScePafGraphics_88561C1B(const void *self, const char *tag)
{
	if (tag == "PanelObj")
		return 1;
	return ScePafGraphics_8D25DDE9(self, tag);
}

int ScePafGraphics_8D9E23C2(const void *self, const char *tag)
{
	if (tag == "PageTurnObj")
		return 1;
	return ScePafGraphics_8D25DDE9(self, tag);
}

int ScePafGraphics_930F72BF(const void *self, const char *tag)
{
	if (tag == "TextObj")
		return 1;
	return ScePafGraphics_8D25DDE9(self, tag);
}

void *ScePafGraphics_0C968320(const uint8_t *self, const uint64_t *data)
{
	uint64_t second = data[1];
	uint8_t *target = *(uint8_t *const *)(self + 80);
	*(uint64_t *)(target + 92) = data[0];
	*(uint64_t *)(target + 100) = second;
	return target;
}
