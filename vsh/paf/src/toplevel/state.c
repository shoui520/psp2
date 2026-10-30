/* Native toplevel state and transition parameter helpers. */
#include <stdint.h>
#include <stddef.h>
#include "../compiler.h"
#include "../std/string.h"
#include "../std/new.h"
#include "../thread/thread.h"

uint32_t sce_paf_toplevel_value_1194;
volatile int32_t sce_paf_toplevel_pending_4694;

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

int ScePafToplevel_D198980D(uint8_t *self, int32_t key)
{
	return RemoveTopLevelListKey(self, 244, key);
}

int ScePafToplevel_E3F5A4B9(uint8_t *self, int32_t key)
{
	return RemoveTopLevelListKey(self, 232, key);
}

int ScePafToplevel_1F15D1D7(uint8_t *self)
{
	int result = *(const uint32_t *)(self + 228) & 0x10;
	int (*callback)(void *);

	if (result) {
		for (result = sce_paf_toplevel_pending_4694;
		     sce_paf_toplevel_pending_4694 > 0;
		     result = sce_paf_toplevel_pending_4694)
			ScePafThread_651F4F81(17);
	}
	callback = *(int (**)(void *))(self + 44);
	if (callback)
		return callback(self);
	return result;
}

int32_t ScePafToplevel_1DF2C6FD(const uint8_t *self, int32_t value)
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

int32_t ScePafToplevel_9EA96CE0(const uint8_t *self, const uint8_t *key)
{
	return LookupTopLevelValue(self, 124, key);
}

int32_t ScePafToplevel_E1DC7DF5(const uint8_t *self, const uint8_t *key)
{
	return LookupTopLevelValue(self, 100, key);
}

uint8_t *ScePafToplevel_820E3DBD(const uint8_t *self, const uint8_t *key)
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

uint32_t ScePafToplevel_F72FC59E(void)
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

PafTopLevelTransition *ScePafToplevel_185EC9F9(PafTopLevelTransition *self)
{
	return InitTopLevelTransition(self);
}

PafTopLevelTransition *ScePafToplevel_4CBEB06F(PafTopLevelTransition *self)
{
	return InitTopLevelTransition(self);
}

int ScePafToplevel_CFF44F67(uint32_t value)
{
	return value != 0 && value <= 1;
}

int32_t ScePafToplevel_CE0BFCFE(const uint8_t *self)
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

uint32_t *ScePafToplevel_7C8CF5E8(uint32_t *self)
{
	return InitTopLevelRequest(self);
}

uint32_t *ScePafToplevel_99DD7850(uint32_t *self)
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

int ScePafToplevel_9974EC7E(void *unused0, void *unused1,
			      uint8_t *object)
{
	if (object && !(object[405] & 8))
		return ((int (**)(void *))*(void **)object)[27](object);
	return (int)(uintptr_t)object;
}
