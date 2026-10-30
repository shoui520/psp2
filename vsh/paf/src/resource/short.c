/* Small standalone resource-object lifetime and readiness operations. */
#include <stdint.h>
#include "../compiler.h"

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
