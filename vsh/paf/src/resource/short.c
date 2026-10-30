/* Small standalone resource-object lifetime and readiness operations. */
#include <stdint.h>
#include "../compiler.h"
#include "../std/new.h"

/* Resource handle destruction: release the owned allocation and invalidate
 * the native three-word reference state. */
static inline __attribute__((always_inline))
uint32_t *DestroyResourceHandle(uint32_t *self)
{
	void *owned = (void *)(uintptr_t)self[4];

#if defined(FW_TYPE_CEX)
	self[0] = 0x812e1f58U;
#else
	self[0] = 0x812e2c10U;
#endif
	if (owned)
		PafDelete(owned);
	self[4] = UINT32_MAX;
	self[5] = UINT32_MAX;
	self[6] = UINT32_MAX;
	return self;
}

uint32_t *ScePafResource_71049596(uint32_t *self)
{
	return DestroyResourceHandle(self);
}

uint32_t *ScePafResource_22CE8E13(uint32_t *self)
{
	return DestroyResourceHandle(self);
}

void ScePafResource_0A3E0BBF(uint32_t *self)
{
	DestroyResourceHandle(self);
	PafDelete(self);
}

extern int ScePafResource_3B89338E(uint32_t *self, uint32_t target);

int ScePafResource_BFC7537A(uint32_t target, uint32_t *self)
{
	return ScePafResource_3B89338E(self, target);
}

int ScePafResource_CEED7338(const void *const *self)
{
	void *object = *(void *const *)self;
	if (object)
		return ((int (**)(void *))*(void **)object)[5](object);
	return 0;
}

int ScePafResource_459C4ED3(const uint8_t *self)
{
	if (*(const uint32_t *)(self + 32) &&
	    *(const uint32_t *)(self + 8))
		return 0;
	return -1;
}

int ScePafResource_205D3029(uint8_t *self, uint8_t flags)
{
	if (flags & 1) {
		void *object = *(void **)(self + 4);
		if (object)
			((void (**)(void *))*(void **)object)[1](object);
		*(void **)(self + 4) = 0;
	}
	return 0;
}

void *ScePafResource_06803F97(const uint8_t *self, uint32_t id)
{
	uint8_t **current = *(uint8_t ***)(self + 44);
	uint8_t **end = current + *(const uint32_t *)(self + 48);

	while (current != end) {
		if (*(const uint32_t *)(*current + 36) == id)
			break;
		if (current)
			++current;
	}
	return current != end ? *current : 0;
}

int ScePafResource_28783E2B(const uint8_t *self)
{
	uint8_t *current = *(uint8_t *const *)(self + 44);
	uint8_t *end = current + 56 * *(const uint32_t *)(self + 48);

	if (current != end) {
		do {
			*(uint32_t *)(current + 24) = (uint32_t)-1;
			if (current)
				current += 56;
		} while (current != end);
	}
	return 0;
}

uint32_t *ScePafResource_8689B24D(const uint8_t *self)
{
	uint32_t *current = *(uint32_t *const *)(self + 56);
	uint32_t *end = current + 4 * *(const uint32_t *)(self + 60);
	uint32_t first = **(uint32_t *const *)(self + 64);
	uint32_t second = **(uint32_t *const *)(self + 68);

	if (current >= end)
		return 0;
	while (*current != first && *current != second) {
		current += 4;
		if (current >= end)
			return 0;
	}
	return current;
}

uint32_t **ScePafResource_E2CA0FC4(uint32_t **result,
				     const uint8_t *self, uint32_t id)
{
	uint32_t *current = *(uint32_t *const *)(self + 44);
	uint32_t *end = current + *(const uint32_t *)(self + 48);
	uint32_t *cursor = current;

	*result = current;
	while (cursor != end) {
		if (*(const uint32_t *)(*(uint8_t *const *)cursor + 36) == id)
			break;
		cursor = current;
		if (current) {
			cursor = current + 1;
			*result = cursor;
			current += 1;
		}
	}
	return result;
}

int ScePafResource_946E41E9(uint8_t *self, uint32_t argument)
{
	uint32_t *begin = *(uint32_t **)(self + 44);
	uint32_t *current = begin;
	uint32_t count = *(uint32_t *)(self + 48);
	int result = 0;

	*(uint32_t *)(self + 8) = argument;
	if (begin != begin + count) {
		do {
			void *object = (void *)(uintptr_t)*current;
			result = ((int (**)(void *, uint32_t))*(void **)object)[9](
				object, argument);
			if (result)
				break;
			current = begin;
			if (begin) {
				++begin;
				++current;
			}
		} while (current != *(uint32_t **)(self + 44) +
				 *(uint32_t *)(self + 48));
	}
	return result;
}

/* Reset the three resource lists' per-entry transient state. */
int ScePafResource_BED256D0(uint32_t *self)
{
	uint32_t *firstRoot = (uint32_t *)(uintptr_t)self[21];
	uint32_t *secondRoot;
	uint32_t *thirdRoot;
	uint32_t *node;

	self[12] = 0;
	self[16] = 0;
	self[20] = 0;
	secondRoot = (uint32_t *)(uintptr_t)self[24];
	thirdRoot = (uint32_t *)(uintptr_t)self[27];
	node = firstRoot ? (uint32_t *)(uintptr_t)firstRoot[1] : 0;
	while (node != firstRoot) {
		node[7] = 0;
		if (node)
			node = (uint32_t *)(uintptr_t)node[1];
	}
	node = secondRoot ? (uint32_t *)(uintptr_t)secondRoot[1] : 0;
	while (node != secondRoot) {
		node[7] = 0;
		if (node)
			node = (uint32_t *)(uintptr_t)node[1];
	}
	node = thirdRoot ? (uint32_t *)(uintptr_t)thirdRoot[1] : 0;
	while (node != thirdRoot) {
		node[7] = 0;
		if (node)
			node = (uint32_t *)(uintptr_t)node[1];
	}
	return 0;
}

/* Propagate a resource mode to its children before optional owner release. */
int ScePafResource_B62F6AFA(uint32_t *self, int mode)
{
	uint32_t *begin = (uint32_t *)(uintptr_t)self[11];
	uint32_t *current = begin;

	if (begin != begin + self[12]) {
		do {
			void *object = (void *)(uintptr_t)*current;
			((void (*)(void *, int))(*(void ***)object)[6])(object, mode);
			current = begin;
			if (begin) {
				++begin;
				++current;
			}
		} while (current != (uint32_t *)(uintptr_t)self[11] + self[12]);
	}
	if (mode & 1) {
		void *owner = (void *)(uintptr_t)self[1];
		if (owner)
			((void (*)(void *))(*(void ***)owner)[1])(owner);
		self[1] = 0;
	}
	return 0;
}
