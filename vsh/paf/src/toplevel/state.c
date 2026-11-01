/* Native toplevel state and transition parameter helpers. */
#include <stdint.h>
#include <stddef.h>
#include "../compiler.h"
#include "../std/string.h"
#include "../std/new.h"
#include "../thread/thread.h"
#include "../memory/heap_allocator.h"

extern HeapAllocator *ScePafResource_D60FF214(uint8_t *self,
					      HeapAllocator *allocator);

extern uint32_t _ZN3paf5graph7Surface6AddRefEv(uint8_t *self);
extern uint32_t _ZN3paf5graph7Surface7ReleaseEv(uint8_t *self);
extern int _ZN3paf5graph14DisplaySurface18SetFrameBufferModeENS_5gutil15FrameBufferModeE(uint8_t *self, int32_t mode);

uint32_t sce_paf_toplevel_value_1194;
volatile int32_t sce_paf_toplevel_pending_4694;

int ScePafToplevel_1AEDE4E0(int value)
{
	switch (value) {
	case 0: case 1: case 2: case 3: case 4:
	case 5: case 6: case 7: case 8: case 9:
	case 10: case 11: case 12: case 13: case 14:
	case 15: case 16: case 17: case 18: case 19:
		return value;
	default:
		return 18;
	}
}

int _ZN3paf9Framework20GetMode_ApplicationCEv(const uint8_t *self)
{
	uint8_t *graphics;
	uint8_t *frame;

	if (*(const uint32_t *)(self + 84) != 4)
		return -1;
	graphics = *(uint8_t *const *)(self + 76);
	graphics = *(uint8_t **)(graphics + 812);
	if (!graphics)
		return -1;
	frame = *(uint8_t **)(graphics + 64);
	if (!frame)
		return 2;
	_ZN3paf5graph7Surface6AddRefEv(frame);
	_ZN3paf5graph7Surface7ReleaseEv(frame);
	return *(uint32_t *)(frame + 368) == 2 ? 2 : 3;
}

int _ZN3paf9Framework7SetModeEj(const uint8_t *self, int32_t requestedMode)
{
	uint8_t *graphics;
	uint8_t *frame;
	int32_t mode;

	if (*(const uint32_t *)(self + 84) != 4)
		return -1;
	graphics = *(uint8_t *const *)(self + 76);
	graphics = *(uint8_t **)(graphics + 812);
	if (!graphics)
		return -1;
	frame = *(uint8_t **)(graphics + 64);
	if (!frame)
		return -1;
	_ZN3paf5graph7Surface6AddRefEv(*(uint8_t **)(graphics + 64));
	_ZN3paf5graph7Surface7ReleaseEv(frame);
	mode = requestedMode == 3 ? 3 : 2;
	_ZN3paf5graph14DisplaySurface18SetFrameBufferModeENS_5gutil15FrameBufferModeE(frame, mode);
	return 0;
}

static inline __attribute__((always_inline))
int RemoveTopLevelListKey(uint8_t *self, size_t listOffset, int32_t key)
{
	int result = (int)(uintptr_t)self;
	int32_t *root = *(int32_t **)(self + listOffset);
	int32_t *next = NULL;
	int32_t *node;

	if (root)
		next = (int32_t *)root[1];
	node = next;
	while (node != root) {
		result = node[2];
		if (result == key) {
			int32_t previous = *node;

			*(int32_t *)node[1] = previous;
			*(int32_t *)(previous + 4) = node[1];
			PafDelete(node);
			node = next;
			root = *(int32_t **)(self + listOffset);
			result = *(int32_t *)(self + listOffset + 4) - 1;
			*(int32_t *)(self + listOffset + 4) = result;
		}
		if (node) {
			next = (int32_t *)node[1];
			node = next;
		}
	}
	return result;
}

int _ZN3paf6Plugin20RemoveUnloadCallbackEPFvPvE(uint8_t *self, int32_t key)
{
	return RemoveTopLevelListKey(self, 244, key);
}

int _ZN3paf6Plugin20RemoveNotifyCallbackEPFviPvE(uint8_t *self, int32_t key)
{
	return RemoveTopLevelListKey(self, 232, key);
}

int _ZN3paf6Plugin4ExitEv(uint8_t *self)
{
	int result = *(const uint32_t *)(self + 228) & 0x10;
	int (*callback)(void *);

	if (result) {
		for (result = sce_paf_toplevel_pending_4694;
		     sce_paf_toplevel_pending_4694 > 0;
		     result = sce_paf_toplevel_pending_4694)
			_ZN3paf6thread5SleepEi(17);
	}
	callback = *(int (**)(void *))(self + 44);
	if (callback)
		return callback(self);
	return result;
}

int32_t _ZNK3paf6Plugin12GetInterfaceEi(const uint8_t *self, int32_t value)
{
	int32_t *root = *(int32_t *const *)(self + 88);
	int32_t *node;

	if (root) {
		node = (int32_t *)(uintptr_t)*root;
		if (!*((uint8_t *)node + 21)) {
			int32_t sentinel;

			do {
				int32_t key = node[3];

				if (value >= key) {
					if (key >= value)
						break;
					node = (int32_t *)(uintptr_t)node[2];
					sentinel = *((char *)node + 21);
				} else {
					node = (int32_t *)(uintptr_t)node[1];
					sentinel = *((char *)node + 21);
				}
			} while (!sentinel);
		}
	} else {
		node = 0;
	}
	if (node == root)
		return 0;
	return node[4];
}

static inline __attribute__((always_inline))
int32_t LookupTopLevelValue(const uint8_t *self, size_t treeOffset,
			    const uint8_t *key)
{
	int32_t *root = *(int32_t *const *)(self + treeOffset);
	int32_t *node;
	uint32_t wanted = *(const uint32_t *)(key + 12);

	if (root) {
		node = (int32_t *)(uintptr_t)*root;
		if (!*((uint8_t *)node + 21)) {
			int32_t sentinel;

			do {
				uint32_t actual = (uint32_t)node[3];

				if (wanted >= actual) {
					if (actual >= wanted)
						break;
					node = (int32_t *)(uintptr_t)node[2];
					sentinel = *((char *)node + 21);
				} else {
					node = (int32_t *)(uintptr_t)node[1];
					sentinel = *((char *)node + 21);
				}
			} while (!sentinel);
		}
	} else {
		node = 0;
	}
	return node == root ? 0 : node[4];
}

int32_t _ZNK3paf6Plugin21GetNodeEntity_SurfaceERKNS_7IDParamE(const uint8_t *self, const uint8_t *key)
{
	return LookupTopLevelValue(self, 124, key);
}

int32_t _ZNK3paf6Plugin20GetNodeEntity_WidgetERKNS_7IDParamE(const uint8_t *self, const uint8_t *key)
{
	return LookupTopLevelValue(self, 100, key);
}

uint8_t *_ZNK3paf6Plugin24GetNodeEntity_AddTextureERKNS_7IDParamE(const uint8_t *self, const uint8_t *key)
{
	uint32_t *root = *(uint32_t *const *)(self + 136);
	uint32_t *node;
	uint32_t wanted = *(const uint32_t *)(key + 12);

	if (root) {
		node = (uint32_t *)(uintptr_t)*root;
		if (!*((uint8_t *)node + 49)) {
			int32_t sentinel;

			do {
				uint32_t actual = node[3];

				if (wanted >= actual) {
					if (actual >= wanted)
						break;
					node = (uint32_t *)(uintptr_t)node[2];
					sentinel = *((char *)node + 49);
				} else {
					node = (uint32_t *)(uintptr_t)node[1];
					sentinel = *((char *)node + 49);
				}
			} while (!sentinel);
		}
	} else {
		node = 0;
	}
	return node == root ? 0 : (uint8_t *)node + 16;
}

uint32_t _ZN3paf9Framework8InstanceEv(void)
{
	return sce_paf_toplevel_value_1194;
}

typedef struct PafTopLevelTransition {
	uint8_t active;
	uint8_t reserved[3];
	float duration;
	uint32_t phase;
	uint32_t timer;
} PafTopLevelTransition;

_Static_assert(sizeof(PafTopLevelTransition) == 16,
	       "toplevel transition size");

static inline __attribute__((always_inline))
PafTopLevelTransition *InitTopLevelTransition(PafTopLevelTransition *self)
{
	self->active = 0;
	self->timer = 0;
	self->duration = 200.0f;
	self->phase = 0;
	return self;
}

PafTopLevelTransition *_ZN3paf6Plugin14PageCloseParamC1Ev(PafTopLevelTransition *self)
{
	return InitTopLevelTransition(self);
}

PafTopLevelTransition *_ZN3paf6Plugin14PageCloseParamC2Ev(PafTopLevelTransition *self)
{
	return InitTopLevelTransition(self);
}

int ScePafToplevel_CFF44F67(uint32_t value)
{
	return value != 0 && value <= 1;
}

int32_t _ZNK3paf9Framework23GetCommonResourceLocaleEv(const uint8_t *self)
{
	const uint8_t *nested = *(const uint8_t *const *)(self + 28);

	if (nested)
		return *(const int32_t *)(nested + 268);
	return *(const int32_t *)(self + 92);
}

static inline __attribute__((always_inline))
uint32_t *InitTopLevelRequest(uint32_t *self)
{
	self[0] = (uint32_t)(uintptr_t)&PafStringEmpty;
	self[1] = 0;
	self[3] = 0;
	self[4] = 0;
	self[5] = 0;
	self[6] = 0;
	self[8] = (uint32_t)-1;
	self[7] = 0;
	return self;
}

uint32_t *_ZN3paf9Framework23CommonResourceInitParamC1Ev(uint32_t *self)
{
	return InitTopLevelRequest(self);
}

uint32_t *_ZN3paf9Framework23CommonResourceInitParamC2Ev(uint32_t *self)
{
	return InitTopLevelRequest(self);
}

int ScePafToplevel_9A4B0DC4(uint8_t *self)
{
	if (!self[112]) {
		void (*callback)(uint32_t) =
			*(void (**)(uint32_t))(self + 40);

		if (callback)
			callback(*(uint32_t *)(self + 44));
	}
	self[112] = 1;
	return 1;
}

int _ZN3paf6Plugin17OnAnimPageHideFinEPNS_5TimerEPNS_2ui12AnimListenerEPvS6_S6_(void *unused0, void *unused1,
			      uint8_t *object)
{
	if (object && !(object[405] & 8))
		return ((int (**)(void *))*(void **)object)[27](object);
	return (int)(uintptr_t)object;
}

/* Find a page's indexed object by its native tree key. */
int _ZNK3paf6Plugin19GetInitEventHandlerERKNS_7IDParamE(const uint8_t *self, const uint8_t *key)
{
	uint32_t wanted = *(const uint32_t *)(key + 12);
	uint32_t *root = *(uint32_t *const *)(self + 60);
	uint32_t *node;

	if (root) {
		node = (uint32_t *)(uintptr_t)root[0];
		if (*((uint8_t *)node + 21) == 0) {
			int flag;

			do {
				uint32_t actual = node[3];

				if (wanted >= actual) {
					if (actual >= wanted)
						break;
					node = (uint32_t *)(uintptr_t)node[2];
					flag = *(char *)((uint8_t *)node + 21);
				} else {
					node = (uint32_t *)(uintptr_t)node[1];
					flag = *(char *)((uint8_t *)node + 21);
				}
			} while (flag == 0);
		}
	} else {
		node = 0;
	}
	if (node == *(uint32_t *const *)(self + 60))
		return 0;
	return *(const uint32_t *)(
		*(const uint8_t *const *)(self + 52) + 20 * node[4] + 16);
}

/* Release the page's resource buffer once its pending-state flag clears. */
int _ZN3paf6Plugin27UnloadResourceTable_TextureEv(uint8_t *self)
{
	int status = *(const int32_t *)(self + 292);

	if (!status) {
		ScePafResource_D60FF214(
			*(uint8_t **)(self + 176),
			*(HeapAllocator **)(self + 200));
		self[272] = 0;
		return 0;
	}
	return status;
}
