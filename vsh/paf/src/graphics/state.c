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
extern uint32_t sce_paf_graphics_state_11fc;
extern uint8_t sce_paf_graphics_flag_06e8;
extern int32_t *sce_paf_graphics_active_surface_2e94;

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

/* A global graphics pointer uses the SDK's acquire/release atomic counter. */
uint32_t *sce_paf_graphics_shared_reference_0f84;

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
uint32_t sce_paf_graphics_state_11fc;
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

static inline __attribute__((always_inline))
void *InitGraphicsHeapSlot(uint8_t *self)
{
	*(void **)self = 0;
	self[4] = 1;
	*(void **)self = PafNew(16);
	return self;
}

uint32_t ScePafGraphics_52C45E43(uint32_t value)
{
	sce_paf_graphics_flag_306c = value;
	return value;
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

Mutex sce_paf_graphics_flag_mutex_1250;

uint32_t *sce_paf_graphics_current_ref_0f7c;

__attribute__((noinline))

__attribute__((noinline))

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
