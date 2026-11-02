/* Native ScePafGraphics globals, field accessors and GXM context forwarding. */
#include <stdint.h>
#include "../compiler.h"
#include "../std/string.h"
#include "../std/new.h"
#include "../thread/mutex.h"
#include <gxm.h>
#include <arm_neon.h>
#include <sce_atomic.h>
#include <kernel/sysmem.h>
#include <display.h>

extern int sceSharedFbGetShellRenderPort(void);
extern int _ZN3paf6thread4Cond4WaitEi(void *condition, int milliseconds);
extern const SceGxmProgramParameter *sceGxmProgramFindParameterBySemantic(
	const SceGxmProgram *program, SceGxmParameterSemantic semantic,
	unsigned int index);
uint32_t _ZN3paf5graph7Surface6AddRefEv(uint8_t *self);
uint32_t _ZN3paf5graph7Surface7ReleaseEv(uint8_t *self);
uint32_t _ZN3paf5graph3arl2GlEv(void);
extern uint32_t sce_paf_graphics_state_12b0;
extern uint32_t sce_paf_graphics_state_11fc;
extern uint8_t sce_paf_graphics_flag_06e8;
extern int32_t *sce_paf_graphics_active_surface_2e94;

/* Initialize a graphics descriptor, preserving the native field-copy order. */
uint8_t *_ZN3paf5graph9TextStyleC1ERKS1_(uint8_t *self, const uint8_t *source)
{
	uint64_t packed;
	uint32_t value;
	uint16_t flags;

	*(uint32_t *)(self + 0) = (uint32_t)-2127676444;
	*(uint64_t *)(self + 8) = 0;
	self[4] = 0;
	*(uint16_t *)(self + 6) = 0;
	*(uint32_t *)(self + 16) = UINT32_MAX;
	*(uint32_t *)(self + 20) = 1711276032u;
	*(uint32_t *)(self + 24) = 0;
	*(uint32_t *)(self + 28) = 0;
	*(uint32_t *)(self + 32) = 0;

	self[4] = source[4];
	packed = *(const uint64_t *)(source + 8);
	*(uint32_t *)(self + 16) = *(const uint32_t *)(source + 16);
	value = *(const uint32_t *)(source + 20);
	*(uint64_t *)(self + 8) = packed;
	*(uint32_t *)(self + 20) = value;
	flags = *(const uint16_t *)(source + 6);
	*(uint32_t *)(self + 24) = *(const uint32_t *)(source + 24);
	*(uint16_t *)(self + 6) = flags;
	*(uint32_t *)(self + 28) = *(const uint32_t *)(source + 28);
	*(uint32_t *)(self + 32) = *(const uint32_t *)(source + 32);
	return self;
}

int _ZN3paf5graph9TextStyle14EqualFontStyleERKS1_(const uint8_t *left, const uint8_t *right)
{
	float32x2_t first, second;
	uint32x2_t lowEqual, highEqual;
	int result = 0;

	if (*(const int8_t *)(left + 4) == *(const int8_t *)(right + 4) &&
	    !((*(const int16_t *)(left + 6) ^
	       *(const int16_t *)(right + 6)) & 0x1A3)) {
		first = *(const volatile float32x2_t *)(right + 8);
		second = *(const volatile float32x2_t *)(left + 8);
		lowEqual = vceq_f32(vdup_lane_f32(first, 0),
				    vdup_lane_f32(second, 0));
		highEqual = vceq_f32(vdup_lane_f32(first, 1),
				     vdup_lane_f32(second, 1));
		if (vget_lane_u32(vand_u32(lowEqual, highEqual), 0))
			result = 1;
	}
	return result;
}

/* Compare the native packed graphics descriptor, including IEEE float lanes. */
static inline __attribute__((always_inline))
int GraphicsDescriptorEqual(const uint8_t *left, const uint8_t *right)
{
	float32x2_t lvalues, rvalues;
	uint32x2_t equality;

	if (*(const int8_t *)(left + 4) != *(const int8_t *)(right + 4) ||
	    *(const int16_t *)(left + 6) != *(const int16_t *)(right + 6))
		return 0;
	rvalues = *(const volatile float32x2_t *)(right + 8);
	lvalues = *(const volatile float32x2_t *)(left + 8);
	equality = vand_u32(
		vceq_f32(vdup_lane_f32(rvalues, 0), vdup_lane_f32(lvalues, 0)),
		vceq_f32(vdup_lane_f32(rvalues, 1), vdup_lane_f32(lvalues, 1)));
	if (!vget_lane_u32(equality, 0))
		return 0;
	return *(const uint32_t *)(left + 16) == *(const uint32_t *)(right + 16) &&
	       *(const uint32_t *)(left + 20) == *(const uint32_t *)(right + 20) &&
	       *(const uint32_t *)(left + 24) == *(const uint32_t *)(right + 24) &&
	       *(const uint32_t *)(left + 28) == *(const uint32_t *)(right + 28) &&
	       *(const uint32_t *)(left + 32) == *(const uint32_t *)(right + 32);
}

int _ZN3paf5graph9TextStyleeqERKS1_(const uint8_t *left, const uint8_t *right)
{
	return GraphicsDescriptorEqual(left, right);
}

int _ZN3paf5graph9TextStyleneERKS1_(const uint8_t *left, const uint8_t *right)
{
	return !GraphicsDescriptorEqual(left, right);
}

/* Advance an indexed graphics entry across the native grouped ranges. */
int _ZN3paf5graph10TextLayout15GetNextRightHitEjRj(const uint8_t *self, uint32_t index,
			     uint32_t *output)
{
	int result = -2136005885;

	if (!(*(const uint16_t *)(self + 112) & 0x22)) {
		result = -2136005886;
		if (index < *(const uint32_t *)(self + 120)) {
			uint32_t count = *(const uint32_t *)(self + 128);
			const uint8_t *entry =
				*(const uint8_t *const *)(self + 116) + index * 12;
			uint32_t groupIndex = *(const uint32_t *)(entry + 8);

			if (groupIndex < count) {
				const uint32_t *groups =
					*(const uint32_t *const *)(self + 124);
				const uint8_t *group =
					(const uint8_t *)(uintptr_t)groups[groupIndex];
				uint32_t size = *(const uint32_t *)(group + 32);
				uint32_t last = UINT32_MAX;

				if (size) {
					const uint32_t *children = *(const uint32_t *const *)(group + 28);
					const uint8_t *child =
						(const uint8_t *)(uintptr_t)children[size - 1];
					last = *(const uint32_t *)(child + 28) +
						*(const uint32_t *)(child + 32) - 1;
				}
				if (index >= last) {
					if (groupIndex >= count - 1) {
						*output = index;
					} else {
						const uint8_t *next =
							(const uint8_t *)(uintptr_t)groups[groupIndex + 1];
						uint32_t first = 0;
						if (*(const uint32_t *)(next + 32)) {
							const uint32_t *children =
								*(const uint32_t *const *)(next + 28);
							const uint8_t *child =
								(const uint8_t *)(uintptr_t)children[0];
							first = *(const uint32_t *)(child + 28);
						}
						*output = first;
					}
				} else {
					*output = index + 1;
				}
				return 0;
			}
		}
	}
	return result;
}

int32_t _ZN3paf5graph10TextLayout11GetCharInfoEjRNS0_18TextLayoutCharInfoE(const uint8_t *self, uint32_t index,
				uint8_t *output)
{
	const uint8_t *entry;
	const uint8_t *const *lookup;
	uint32_t entryIndex;
	int32_t result = -2136005885;

	if (!(*(const int16_t *)(self + 112) & 0x22)) {
		result = -2136005886;
		if (index < *(const uint32_t *)(self + 120)) {
			entry = *(const uint8_t *const *)(self + 116) + 12 * index;
			entryIndex = *(const uint32_t *)(entry + 8);
			if (entryIndex < *(const uint32_t *)(self + 128)) {
				float32x4_t values;

				lookup = *(const uint8_t *const *const *)(self + 124);
				values = *(const volatile float32x4_t *)(lookup[entryIndex] + 8);
				result = 0;
				*(uint32_t *)(output + 8) = entryIndex;
				*(float *)(output + 4) =
					vgetq_lane_f32(values, 0) +
					*(const float *)(entry + 4);
			}
		}
	}
	return result;
}

void *ScePafGraphics_12BDEBFF(void)
{
	uint32_t *state = *(uint32_t *volatile *)
		&sce_paf_graphics_active_surface_2e94;
	void (*callback)(void *, uintptr_t, uintptr_t, uintptr_t) =
		(void (*)(void *, uintptr_t, uintptr_t, uintptr_t))
		(uintptr_t)state[7];
	void *argument = (void *)(uintptr_t)state[8];

	state[7] = 0;
	state = *(uint32_t *volatile *)&sce_paf_graphics_active_surface_2e94;
	state[8] = 0;
	if (callback)
		callback(argument, (uintptr_t)callback, (uintptr_t)state, 0);
	state = *(uint32_t *volatile *)&sce_paf_graphics_active_surface_2e94;
	state[7] = 0;
	state[0] = 3;
	uint32_t *result = *(uint32_t *volatile *)
		&sce_paf_graphics_active_surface_2e94;
	state[1] = 0;
	result[8] = 0;
	return result;
}

static inline __attribute__((always_inline))
void GraphicsSetPointLineWidth(uint32_t *self, uint32_t flags, float width)
{
	uint32_t integerWidth = (uint32_t)width;

	if (flags & 1) {
		self[475] = integerWidth;
		sceGxmSetFrontPointLineWidth((SceGxmContext *)self[419],
					     integerWidth);
	}
	if (flags & 2) {
		self[476] = integerWidth;
		sceGxmSetBackPointLineWidth((SceGxmContext *)self[419],
					    integerWidth);
	}
}

int ScePafGraphics_6EA0C6D7(uint8_t *self, int value)
{
	int result = value % *(int16_t *)(self + 100);

	*(int16_t *)(self + 104) = (int16_t)result;
	return result;
}

static inline __attribute__((always_inline))
uint8_t *DestroyGraphicsOwnedPointer(uint8_t *self)
{
	if (self[4])
		PafDelete(*(void **)self);
	*(void **)self = NULL;
	return self;
}

static inline __attribute__((always_inline))
__attribute__((noinline))
uint8_t *_ZN3paf5graph7Surface4SwapEv(uint8_t *self)
{
	int16_t next = *(int16_t *)(self + 102) + 1;
	int16_t maximum = *(int16_t *)(self + 100);
	uint8_t flags;

	*(int16_t *)(self + 102) = next;
	flags = self[64];
	if (next >= maximum)
		*(int16_t *)(self + 102) = 0;
	self[64] = flags & ~0x10;
	return self;
}

uint8_t *_ZN3paf5graph14DisplaySurface4SwapEv(uint8_t *self)
{
	int32_t mode = *(int32_t *)(self + 368);

	if (mode > 1) {
		if (mode <= 3)
			return _ZN3paf5graph7Surface4SwapEv(self);
	} else if (mode >= 0 && self[292]) {
		return _ZN3paf5graph7Surface4SwapEv(self);
	}
	return self;
}

int _ZN3paf5graph9TextStyle13EqualFontTypeERKS1_(const uint8_t *first,
			     const uint8_t *second)
{
	if (first[4] != second[4])
		return 0;
	return ((*(const int16_t *)(first + 6) ^
		  *(const int16_t *)(second + 6)) & 0x123) == 0;
}

/* A global graphics pointer uses the SDK's acquire/release atomic counter. */
uint32_t *sce_paf_graphics_shared_reference_0f84;

int ScePafGraphics_D5E30C64(const uint8_t *self,
			     SceGxmNotification *vertexNotification,
			     SceGxmNotification *fragmentNotification)
{
	SceGxmContext *context =
		(SceGxmContext *)(uintptr_t)sce_paf_graphics_state_12b0;

	if (!context)
		context = *(SceGxmContext **)(uintptr_t)
			(_ZN3paf5graph3arl2GlEv() + 1676);
	if (!context)
		return -2136006139;
	int result = sceGxmEndScene(context, vertexNotification,
				    fragmentNotification);
	if (result >= 0)
		return 0;
	return result;
}

extern uint8_t *sce_paf_graphics_indicator_3070;
uint8_t sce_paf_graphics_reset_flag_3074;
uint8_t sce_paf_graphics_reset_gate_3075;

uint8_t *ScePafGraphics_00B9D8CC(void)
{
	uint8_t *result = sce_paf_graphics_indicator_3070;

	if (result) {
		sce_paf_graphics_reset_flag_3074 = 1;
		*result = 0;
	}
	return result;
}

uint8_t *ScePafGraphics_906BE45C(void)
{
	uint8_t *result = sce_paf_graphics_indicator_3070;

	if (result) {
		sce_paf_graphics_reset_flag_3074 = 0;
		if (!sce_paf_graphics_reset_gate_3075)
			*result = 1;
	}
	return result;
}

uint8_t sce_paf_graphics_flag_306c;
uint32_t sce_paf_graphics_state_11e0;
uint32_t sce_paf_graphics_state_12ac;
uint32_t sce_paf_graphics_state_11fc;
uint32_t sce_paf_graphics_state_12b0;
uint8_t sce_paf_graphics_flag_06e8;
int32_t *sce_paf_graphics_active_surface_2e94;
uint8_t sce_paf_graphics_flag_3518;
uint8_t sce_paf_graphics_flag_136c;
uint8_t sce_paf_graphics_flag_351a;

char *ScePafGraphics_0EC5770E(void)
{
	char *result = (char *)(uintptr_t)sce_paf_graphics_flag_3518;

	if (sce_paf_graphics_flag_3518) {
		sce_paf_graphics_flag_136c = 0;
		sce_paf_graphics_flag_351a = 0;
		return (char *)&sce_paf_graphics_flag_351a;
	}
	return result;
}

__attribute__((noinline))
int _ZN3paf5graph10TextLayout9GetBoundsERNS_4math2v2E(const uint8_t *self, uint64_t *output)
{
	if (*(const uint16_t *)(self + 112) & 0x22)
		return -2136005885;
	*output = *(const uint64_t *)(self + 104);
	return 0;
}

static inline __attribute__((always_inline))
uint32_t *DestroyGraphicsRefPair(uint32_t *self)
{
	uint32_t second = self[1];

	if (second)
		_ZN3paf5graph7Surface7ReleaseEv((uint8_t *)(uintptr_t)second);
	if (self[0])
		_ZN3paf5graph7Surface7ReleaseEv((uint8_t *)(uintptr_t)self[0]);
	return self;
}

uint32_t *ScePafGraphics_025C548E(uint32_t *self)
{
	return DestroyGraphicsRefPair(self);
}

uint32_t *ScePafGraphics_9E20360C(uint32_t *self)
{
	return DestroyGraphicsRefPair(self);
}

static inline __attribute__((always_inline))
int GraphicsSetSamplerMode(uint8_t *self, uint32_t fieldOffset, int mode)
{
	int mapped = 2;

	if (mode) {
		int selected = 0;
		if (mode != 1) {
			if (mode != 2) {
				mapped = 1;
				if (mode != 4) {
					mapped = 3;
					if (mode != 5) {
						mapped = 4;
						if (mode != 6) {
							mapped = 6;
							if (mode != 7) {
								mapped = 5;
								if (mode != 8) {
									mapped = -1;
									if (mode == 9)
										mapped = 7;
								}
							}
						}
					}
				}
			}
			selected = mapped;
		}
		mapped = selected;
		if (selected == -1)
			return -2136006142;
	}
	if (mapped != *(int32_t *)(self + fieldOffset)) {
		*(int32_t *)(self + fieldOffset) = mapped;
		self[96] = 1;
	}
	return 0;
}

int _ZN3paf5graph7Surface8SetWrapSENS0_11TextureWrapE(uint8_t *self, int mode)
{
	return GraphicsSetSamplerMode(self, 84, mode);
}

int _ZN3paf5graph7Surface8SetWrapTENS0_11TextureWrapE(uint8_t *self, int mode)
{
	return GraphicsSetSamplerMode(self, 88, mode);
}

__attribute__((noinline))
int _ZN3paf5graph7Surface12SetMinFilterENS0_13TextureFilterE(uint8_t *self, int mode)
{
	int filter;
	int wrapping;

	switch (mode) {
	case 0:
	case 2:
		filter = 0;
		wrapping = 0;
		break;
	case 1:
	case 3:
		filter = 1;
		wrapping = 0;
		break;
	case 4:
		filter = 0;
		wrapping = 512;
		break;
	case 5:
		filter = 1;
		wrapping = 512;
		break;
	default:
		return -2136006142;
	}
	if (filter != *(int32_t *)(self + 76) ||
	    wrapping != *(int32_t *)(self + 80)) {
		*(int32_t *)(self + 76) = filter;
		*(int32_t *)(self + 80) = wrapping;
		self[96] = 1;
	}
	return 0;
}

__attribute__((noinline))
int _ZN3paf5graph7Surface12SetMagFilterENS0_13TextureFilterE(uint8_t *self, int mode)
{
	int mapped = 0;

	if (mode)
		mapped = mode == 1 ? 1 : -1;
	if (mapped != *(int32_t *)(self + 72)) {
		if (mapped == -1)
			return -2136006142;
		*(int32_t *)(self + 72) = mapped;
		self[96] = 1;
	}
	return 0;
}

int _ZN3paf5graph14DisplaySurface13GetOutputPortEv(const uint8_t *self)
{
	int result = 0;

	if (*(const uint32_t *)(self + 368) == 1) {
		result = sceSharedFbGetShellRenderPort();
		if (result < 0)
			return 0;
	}
	return result;
}

static inline __attribute__((always_inline))
uint32_t *InitGraphicsRefParameter(uint32_t *result,
				   const uint32_t *source, uint32_t value)
{
	uint32_t object = source[0];

	result[0] = source[0];
	if (object)
		_ZN3paf5graph7Surface6AddRefEv((uint8_t *)(uintptr_t)object);
	result[2] = value;
	result[1] = 0;
	return result;
}

uint32_t *ScePafGraphics_3D0309E3(uint32_t *result,
				   const uint32_t *source, uint32_t value)
{
	return InitGraphicsRefParameter(result, source, value);
}

uint32_t *ScePafGraphics_A078C447(uint32_t *result,
				   const uint32_t *source, uint32_t value)
{
	return InitGraphicsRefParameter(result, source, value);
}

int _ZN3paf5graph14DisplaySurface18SetFrameBufferModeENS_5gutil15FrameBufferModeE(uint8_t *self, int32_t mode)
{
	if (mode != 2 && mode != 3)
		return -2136006138;
	*(int32_t *)(self + 368) = mode;
	return 0;
}

__attribute__((noinline))
int _ZN3paf5graph10TextLayout12GetLineCountERj(const uint8_t *self, uint32_t *output)
{
	if (*(const uint16_t *)(self + 112) & 0x11)
		return -2136005885;
	*output = *(const uint32_t *)(self + 128);
	return 0;
}

int ScePafGraphics_D1081027(uint8_t *self, int32_t index,
			     const uint64_t *value)
{
	if (index != 1)
		return -2136005886;
	*(uint64_t *)(self + 8) = *value;
	return 0;
}

uint32_t _ZN3paf5graph13ScreenSurface13GetRawContextEv(void)
{
	uint32_t result = sce_paf_graphics_state_12b0;

	if (!result)
		return *(uint32_t *)(uintptr_t)(_ZN3paf5graph3arl2GlEv() + 1676);
	return result;
}

static inline __attribute__((always_inline))
void *InitGraphicsHeapSlot(uint8_t *self)
{
	*(void **)self = 0;
	self[4] = 1;
	*(void **)self = PafNew(16);
	return self;
}

uint32_t _ZN3paf5graph14DisplaySurface7GetFbIdEv(void)
{
	return sce_paf_graphics_state_12ac;
}

uint32_t ScePafGraphics_52C45E43(uint32_t value)
{
	sce_paf_graphics_flag_306c = value;
	return value;
}

uint32_t _ZN3paf5graph13ScreenSurface13SetRawContextEP13SceGxmContext(uint32_t value)
{
	sce_paf_graphics_state_12b0 = value;
	return value;
}

static inline __attribute__((always_inline))
__attribute__((noinline))
int _ZN3paf5graph10TextLayout17GetSelectionRangeERjS2_(const uint8_t *self,
			     uint32_t *first, uint32_t *second)
{
	*first = *(const uint32_t *)(self + 96);
	*second = *(const uint32_t *)(self + 100);
	return 0;
}

int _ZN3paf5graph10TextLayout8SetStateEi(uint8_t *self, uint16_t bits)
{
	*(uint16_t *)(self + 112) |= bits;
	return 0;
}

int _ZN3paf5graph10TextLayout10ClearStateEi(uint8_t *self, uint16_t bits)
{
	*(uint16_t *)(self + 112) &= (uint16_t)~bits;
	return 0;
}

int _ZN3paf5graph16AttributedString12GetStyleRunsERNS_14_list_iteratorIPNS0_12TextStyleRunENS_9allocatorIS4_EEEES8_(const uint8_t *self,
			     uint32_t *first, uint32_t *second)
{
	const uint8_t *item = *(uint8_t *const *)(self + 16);
	uint32_t value = 0;
	if (item)
		value = *(const uint32_t *)(item + 4);
	*first = value;
	*second = (uint32_t)(uintptr_t)item;
	return 0;
}

int32_t _ZNK3paf5graph7Surface9GetStrideEi(const uint8_t *self, int32_t shiftCount)
{
	int32_t result = *(const int32_t *)(self + 60);
	int32_t iteration;

	for (iteration = shiftCount - 1; ; --iteration) {
		int32_t oldCount = shiftCount;
		shiftCount = iteration;
		if (oldCount <= 0)
			break;
		if (result > 1)
			result >>= 1;
	}
	return result;
}

int32_t _ZN3paf5graph7Surface13GetCopyHeightEii(const uint8_t *self,
				 int32_t unused, int32_t shiftCount)
{
	int32_t result = *(const int16_t *)(self + 30);
	int32_t iteration;

	for (iteration = shiftCount - 1; ; --iteration) {
		int32_t oldCount = shiftCount;
		shiftCount = iteration;
		if (oldCount <= 0)
			break;
		if (result > 1)
			result >>= 1;
	}
	return result;
}

int _ZNK3paf5graph9TextStyle17GetStyleAttributeENS0_18TextStyleAttributeERi(const uint8_t *self,
			    int32_t index, int32_t *output)
{
	if (index)
		return -2136005886;
	*output = (int8_t)self[4];
	return 0;
}

int _ZN3paf5graph10TextLayout10InvalidateEv(uint8_t *self)
{
	RMutex *mutex = *(RMutex **)(self + 140);

	if (mutex) {
		_ZN3paf6thread6RMutex4LockEv(mutex);
		mutex = *(RMutex **)(self + 140);
	}
	*(uint16_t *)(self + 112) |= 7;
	if (mutex)
		_ZN3paf6thread6RMutex6UnlockEv(mutex);
	return 7;
}

int32_t _ZNK3paf5graph7Surface8GetScoreEv(const uint8_t *self)
{
	return *(const int32_t *)(self + 56) *
		(3 - *(const int32_t *)(self + 32));
}

int32_t *_ZN3paf5graph9TextStyleD1Ev(int32_t *result)
{
	*result = -2127676444;
	return result;
}

int32_t *_ZN3paf5graph9TextStyleD2Ev(int32_t *result)
{
	*result = -2127676444;
	return result;
}

void *_ZN3paf5graph7Surface10FastMemcpyEPvPKvi(void *self, void *destination,
			      const void *source, uint32_t count)
{
	return sce_paf_memcpy(destination, source, count);
}

int32_t ScePafGraphics_BEF37718(void)
{
	return *sce_paf_graphics_active_surface_2e94;
}

void *ScePafGraphics_04F03872(void)
{
	uint8_t *state = (uint8_t *)sce_paf_graphics_active_surface_2e94;

	if (*(uint32_t *)state) {
		void *callbackAddress = *(void **)(state + 28);
		/* Preserve the native BLX register arguments in ordinary C. */
		void (*callback)(void *, void *, void *, void *) = callbackAddress;
		void *argument = *(void **)(state + 32);

		*(void **)(state + 28) = NULL;
		void *resetState = *(void *volatile *)&sce_paf_graphics_active_surface_2e94;
		*(void **)((uint8_t *)resetState + 32) = NULL;
		if (callback)
			callback(argument, callbackAddress, resetState, NULL);
		state = (uint8_t *)sce_paf_graphics_active_surface_2e94;
		*(void **)(state + 28) = NULL;
		*(uint32_t *)state = 0;
		uint8_t *lastState =
			*(uint8_t *volatile *)&sce_paf_graphics_active_surface_2e94;
		*(uint32_t *)(state + 4) = 0;
		*(uint32_t *)(lastState + 32) = 0;
		return lastState;
	}
	return &sce_paf_graphics_active_surface_2e94;
}

RMutex sce_paf_graphics_rmutex_1230;
void *sce_paf_graphics_selected_object_12a4;

typedef struct PafGraphicsRegistryAnchor {
	void *previous;
	void *next;
} PafGraphicsRegistryAnchor;

PafGraphicsRegistryAnchor sce_paf_graphics_registry_1270;
Mutex sce_paf_graphics_registry_mutex_1278;

typedef struct PafGraphicsItemNode {
	struct PafGraphicsItemNode *previous;
	struct PafGraphicsItemNode *next;
	void *value;
} PafGraphicsItemNode;

PafGraphicsItemNode *sce_paf_graphics_items_12a0;
Mutex sce_paf_graphics_items_mutex_12b0;

int ScePafGraphics_A842275A(const uint8_t *self,
			     SceGxmRenderTarget *target)
{
	SceUID driverMemBlock;
	int result;

	if (!target)
		return -2136006139;
	if (!self[160])
		return 0;
	sceGxmRenderTargetGetDriverMemBlock(target, &driverMemBlock);
	result = sceGxmDestroyRenderTarget(target);
	sceKernelFreeMemBlock(driverMemBlock);
	return result;
}

Mutex sce_paf_graphics_flag_mutex_1250;

uint32_t *sce_paf_graphics_current_ref_0f7c;

static const int32_t graphicsFormatMap[4] = {
	4, 0,
	-1, -1
};

static const int32_t graphicsUsageMap[8] = {
	1, 0,
	2, 0x08000000,
	3, 0x04000000,
	0, -1
};

static const int32_t graphicsMemoryMap[6] = {
	100, 0x02444000,
	101, 0x00044000,
	-1, -1
};

int _ZN3paf5graph13ScreenSurface18ConvertDepthFormatE9ImageMode(int value)
{
	int i;

	for (i = 0; i < 3; ++i)
		if (graphicsMemoryMap[2 * i] == value)
			return graphicsMemoryMap[2 * i + 1];
	return -1;
}

int _ZN3paf5graph13ScreenSurface14GetDepthFormatEv(const uint8_t *self)
{
	int i;
	const uint8_t *resource = *(const uint8_t *const *)(self + 120);
	int value = *(const int32_t *)(resource + 4);

	for (i = 0; i < 3; ++i)
		if (graphicsMemoryMap[2 * i] == value)
			return graphicsMemoryMap[2 * i + 1];
	return -1;
}

uint32_t _ZN3paf5graph7Surface13GetRawTextureEv(const uint8_t *self)
{
	const uint8_t *items = *(const uint8_t *const *)(self + 68);
	int index;
	int offset;

	if (!items)
		return 0;
	index = *(const int16_t *)(self + 102);
	offset = *(const int16_t *)(self + 104);
	if (offset) {
		int total = *(const int16_t *)(self + 100);

		index += offset;
		if (index < 0)
			index = total - 1;
		else if (total < index)
			index = 0;
	}
	return *(const uint32_t *)(items + 8 * index);
}

uint8_t *_ZN3paf5graph9TextStyleaSERKS1_(uint8_t *self, const uint8_t *source)
{
	uint64_t values;
	uint32_t value20;
	uint16_t value6;

	self[4] = source[4];
	values = *(const uint64_t *)(source + 8);
	*(uint32_t *)(self + 16) = *(const uint32_t *)(source + 16);
	value20 = *(const uint32_t *)(source + 20);
	*(uint64_t *)(self + 8) = values;
	*(uint32_t *)(self + 20) = value20;
	value6 = *(const uint16_t *)(source + 6);
	*(uint32_t *)(self + 24) = *(const uint32_t *)(source + 24);
	*(uint16_t *)(self + 6) = value6;
	*(uint32_t *)(self + 28) = *(const uint32_t *)(source + 28);
	*(uint32_t *)(self + 32) = *(const uint32_t *)(source + 32);
	return self;
}

void ScePafGraphics_B63D3B14(uint8_t *self)
{
	int mode = *(int32_t *)(self + 368);

	if (mode > 1) {
		if (mode > 3)
			return;
		sceDisplayWaitVblankStart();
		return;
	}
	if (mode < 0)
		return;
	if (!self[293]) {
		sceDisplayWaitVblankStart();
		return;
	}
	MutexLock((Mutex *)(self + 296));
	if (self[293])
		_ZN3paf6thread4Cond4WaitEi(self + 328, 0);
	MutexUnlock((Mutex *)(self + 296));
}

__attribute__((noinline))
int _ZN3paf5graph10TextLayout18GetLayoutAttributeENS0_19TextLayoutAttributeERNS_4math2v2E(const uint8_t *self, int index,
			     uint64_t *result)
{
	int status = 0;

	if (index > 6) {
		if (index <= 7) {
			const volatile double *values =
				(const volatile double *)(self + 40);
			double first = values[0];

			(void)values[1];
			*(double *)result = first;
			goto done;
		}
		if (index <= 8) {
			*(double *)result = *(const double *)(self + 48);
			goto done;
		}
		status = -2136005886;
		goto done;
	}
	if (index <= 5) {
		status = -2136005886;
		goto done;
	}
	*(double *)result = *(const double *)(self + 28);
done:
	return status;
}

__attribute__((noinline))
int _ZN3paf5graph10TextLayout18GetLayoutAttributeENS0_19TextLayoutAttributeERi(const uint8_t *self, int index,
			    uint32_t *result)
{
	if (index > 1) {
		if (index <= 2) {
			*result = (int32_t)(int8_t)self[14];
			return 0;
		}
		if (index <= 3) {
			*result = *(const uint16_t *)(self + 18);
			return 0;
		}
		return -2136005886;
	}
	if (index < 0)
		return -2136005886;
	if (index == 0)
		*result = (int32_t)(int8_t)self[12];
	else
		*result = (int32_t)(int8_t)self[13];
	return 0;
}

int _ZN3paf5graph13ScreenSurface24ConvertColorSurfaceOrderE10ImageOrder(const uint8_t *self, int value)
{
	int i;

	for (i = 0; i < 4; ++i)
		if (graphicsUsageMap[2 * i] == value)
			return graphicsUsageMap[2 * i + 1];
	return -1;
}

int _ZN3paf5graph13ScreenSurface20GetColorSurfaceOrderEv(const uint8_t *self)
{
	int i;
	int value = *(const int32_t *)(self + 32);

	for (i = 0; i < 4; ++i)
		if (graphicsUsageMap[2 * i] == value)
			return graphicsUsageMap[2 * i + 1];
	return -1;
}

int _ZN3paf5graph13ScreenSurface25ConvertColorSurfaceFormatE9ImageMode(const uint8_t *self, int value)
{
	int i;

	for (i = 0; i < 2; ++i)
		if (graphicsFormatMap[2 * i] == value)
			return graphicsFormatMap[2 * i + 1];
	return -1;
}

int _ZN3paf5graph13ScreenSurface21GetColorSurfaceFormatEv(const uint8_t *self)
{
	int i;
	int value = *(const int32_t *)(self + 4);

	for (i = 0; i < 2; ++i)
		if (graphicsFormatMap[2 * i] == value)
			return graphicsFormatMap[2 * i + 1];
	return -1;
}

int _ZN3paf5graph16AttributedString15GetStringBufferERPKwRj(const uint8_t *self,
			     uint32_t *first, uint32_t *second)
{
	*first = *(const uint32_t *)(self + 4);
	*second = *(const uint32_t *)(self + 8);
	return 0;
}

int32_t *sce_paf_graphics_buffer_11e4;
int _ZN3paf5graph16AttributedString9GetStringERNS_12basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEEE(const uint8_t *self, PafWString *result)
{
	PafWStringCopy(result, (const PafWString *)(self + 4));
	return 0;
}

int32_t _ZNK3paf5graph11SurfaceCLUT11GetCLUTSizeEv(const uint8_t *self)
{
	return ((*(const int16_t *)(self + 28) <<
		  *(const uint32_t *)(self + 12)) + 7) >> 3;
}

uint8_t *_ZN3paf5graph13ScreenSurface18GetRawColorSurfaceEi(const uint8_t *self, uint32_t index)
{
	return *(uint8_t *const *)(self + 148) + index * 48;
}

int32_t _ZN3paf5graph7Surface10SetPaletteEPNS1_7TextureEPKv(void *self, SceGxmTexture **texture,
			       void *palette)
{
	int32_t result = sceGxmTextureSetPalette(*texture, palette);
	return result >= 0 ? 0 : result;
}

static inline __attribute__((always_inline))
int GraphicsInvokeOffset20(const uint32_t *self)
{
	void *object = (void *)(uintptr_t)self[0];
	return ((int (**)(void *, uint32_t, uint32_t, uint32_t))
		*(void **)object)[5](object, self[1], self[2], self[3]);
}

uint8_t *sce_paf_graphics_indicator_3070;

int ScePafGraphics_DD2002B4(void)
{
	if (sce_paf_graphics_indicator_3070)
		return *sce_paf_graphics_indicator_3070;
	return 0;
}

void _ZN3paf5graph9TextStyleD0Ev(int32_t *self)
{
	*self = -2127676444;
	PafDelete(self);
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

int _ZN3paf5graph9TextStyle17SetStyleAttributeENS0_18TextStyleAttributeEi(uint8_t *self, int mode, uint8_t value)
{
	if (mode != 0)
		return -2136005886;
	self[4] = value;
	return 0;
}

/* Advance through the native mip-chain layout for a selected buffer. */
uint8_t *_ZNK3paf5graph7Surface10GetPagePtrEii(const uint8_t *self, int32_t index,
				 int32_t levels)
{
	uint8_t *result = _ZNK3paf5graph7Surface10GetPagePtrEi(self, index);

	if (!result)
		return 0;
	if (levels > 0) {
		int32_t stride = *(const int32_t *)(self + 60);
		int32_t height = *(const int16_t *)(self + 46);
		int32_t count = levels - 1;
		int32_t next = levels - 2;
		int32_t offset;

		for (offset = stride * height; ; offset += stride * height) {
			int32_t check = count;
			count = next;
			if (check <= 0)
				break;
			--next;
			if (stride > 1)
				stride >>= 1;
			if (height > 1)
				height >>= 1;
		}
		result += offset;
	}
	return result;
}
