/* Lightweight native PAF property/string field constructors. */
#include "../std/string.h"
#include "../std/new.h"
#include "../compiler.h"
#include <sce_atomic.h>
#include <stdint.h>

uint32_t ScePafCommon_6ED1BC31(uint32_t *self)
{
	return (uint32_t)sceAtomicIncrement32AcqRel((volatile int32_t *)&self[1]) + 1;
}

int ScePafCommon_DAC99BB6(const PafString *text, const PafString *suffix)
{
	int length = (int)text->count;
	int matched = 0;
	int suffixLength;
	int offset;

	if (!length)
		return 0;
	suffixLength = (int)suffix->count;
	offset = length - suffixLength;
	if (suffixLength <= 0)
		return 1;
	while (offset >= 0 && offset < length &&
	       text->ptr[offset] == suffix->ptr[matched]) {
		++matched;
		++offset;
		if (matched >= suffixLength)
			return 1;
	}
	return 0;
}

typedef struct PafCommonReference {
	void **methods;
	volatile int32_t count;
} PafCommonReference;

uint32_t ScePafCommon_5221EB33(PafCommonReference *self)
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

uint32_t *ScePafCommon_2CDB40AC(uint32_t *self)
{
	return ConstructCommonTagPair(self);
}

uint32_t *ScePafCommon_97C2B0FE(uint32_t *self)
{
	return ConstructCommonTagPair(self);
}

void ScePafCommon_38964597(uint32_t *self)
{
	self[0] = (uint32_t)-2127688360;
	PafDelete(self);
}

/* Convert the native property representation class to its float bit pattern. */
uint32_t ScePafCommon_DE5322EB(uint32_t kind)
{
	switch (kind) {
	case 0: case 6: case 7: case 8: case 9: case 10: case 11: case 13:
		return 1125515264u;
	case 1: case 2: case 3: case 4: case 5: case 12:
	case 14: case 15: case 18: case 19:
		return 1132068864u;
	case 16: case 17: case 20: case 21:
		return 1138819072u;
	case 22: case 23: case 24: case 25:
		return 1140457472u;
	default:
		return kind;
	}
}
