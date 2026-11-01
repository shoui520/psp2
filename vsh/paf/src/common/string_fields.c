/* Lightweight native PAF property/string field constructors. */
#include "../std/string.h"
#include "../std/new.h"
#include "../compiler.h"
#include <sce_atomic.h>
#include <stdint.h>

uint32_t _ZN3paf6common6RefObj6AddRefEv(uint32_t *self)
{
	return (uint32_t)sceAtomicIncrement32AcqRel((volatile int32_t *)&self[1]) + 1;
}

typedef struct PafCommonReference {
	void **methods;
	volatile int32_t count;
} PafCommonReference;

uint32_t _ZN3paf6common6RefObj7ReleaseEv(PafCommonReference *self)
{
	uint32_t remaining = (uint32_t)sceAtomicDecrement32AcqRel(&self->count) - 1;

	if (!remaining && self) {
		void *method = self->methods[1];
		((void (*)(PafCommonReference *, void *, int, int))
		 method)(self, method, 0, 0);
	}
	return remaining;
}

/* Native fixed-tag constructors and deleting destructor aliases. */
static inline __attribute__((always_inline))
uint32_t *ConstructCommonTag(uint32_t *self)
{
	self[0] = (uint32_t)-2127688360;
	return self;
}

uint32_t *ScePafCommon_0691F7F7(uint32_t *self)
{
	return ConstructCommonTag(self);
}

uint32_t *ScePafCommon_31766A0F(uint32_t *self)
{
	return ConstructCommonTag(self);
}

uint32_t *ScePafCommon_559CDA63(uint32_t *self)
{
	return ConstructCommonTag(self);
}

uint32_t *ScePafCommon_D4CE120A(uint32_t *self)
{
	return ConstructCommonTag(self);
}

static inline __attribute__((always_inline))
uint32_t *ConstructCommonTagPair(uint32_t *self)
{
#if defined(FW_TYPE_CEX)
	self[0] = 0x812e0e20U;
#else
	self[0] = 0x812e1ad8U;
#endif
	self[1] = 0;
	return self;
}

uint32_t *_ZN3paf6common6RefObjC2Ev(uint32_t *self)
{
	return ConstructCommonTagPair(self);
}

uint32_t *_ZN3paf6common6RefObjC1Ev(uint32_t *self)
{
	return ConstructCommonTagPair(self);
}

void ScePafCommon_38964597(uint32_t *self)
{
	self[0] = (uint32_t)-2127688360;
	PafDelete(self);
}
