/* Small standalone resource-object lifetime and readiness operations. */
#include <stdint.h>
#include "../compiler.h"
#include "../std/new.h"

int ScePafResource_CEED7338(const void *const *self)
{
	void *object = *(void *const *)self;
	if (object)
		return ((int (**)(void *))*(void **)object)[5](object);
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

