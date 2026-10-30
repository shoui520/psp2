/* Native toplevel state and transition parameter helpers. */
#include <stdint.h>
#include <stddef.h>
#include "../compiler.h"
#include "../std/string.h"

uint32_t sce_paf_toplevel_value_1194;

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
