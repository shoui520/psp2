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

uint32_t sce_paf_graphics_state_11e0;
uint32_t sce_paf_graphics_state_11fc;
uint8_t sce_paf_graphics_flag_06e8;
static inline __attribute__((always_inline))
void *InitGraphicsHeapSlot(uint8_t *self)
{
	*(void **)self = 0;
	self[4] = 1;
	*(void **)self = PafNew(16);
	return self;
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


