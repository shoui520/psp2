/* Small standalone resource-object lifetime and readiness operations. */
#include <stdint.h>
#include "../compiler.h"
#include "../std/new.h"

extern int _ZN3paf2ui16WidgetSoundParam9DelWidgetEPNS0_6WidgetE(uint32_t *self, uint32_t target);

int _ZN3paf2ui16WidgetSoundParam9delete_cbEPNS0_6WidgetEPv(uint32_t target, uint32_t *self)
{
	return _ZN3paf2ui16WidgetSoundParam9DelWidgetEPNS0_6WidgetE(self, target);
}

int ScePafResource_CEED7338(const void *const *self)
{
	void *object = *(void *const *)self;
	if (object)
		return ((int (**)(void *))*(void **)object)[5](object);
	return 0;
}

int _ZN3paf2ui18WidgetHandlerParam3GetEv(const uint8_t *self)
{
	if (*(const uint32_t *)(self + 32) &&
	    *(const uint32_t *)(self + 8))
		return 0;
	return -1;
}

int _ZN3paf2ui16WidgetSoundParam4InitEv(const uint8_t *self)
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

