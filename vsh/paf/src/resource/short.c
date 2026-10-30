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
