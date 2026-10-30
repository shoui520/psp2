/* Lightweight native PAF property/string field constructors. */
#include "../std/string.h"
#include "../compiler.h"
#include <sce_atomic.h>
#include <stdint.h>

typedef struct PafCommonStringField {
	PafString string;
	void *context;
} PafCommonStringField;

_Static_assert(offsetof(PafCommonStringField, context) == 12,
	       "common string context offset");

static inline __attribute__((always_inline))
void InitCommonStringField(PafCommonStringField *self)
{
	self->string.ptr = &PafStringEmpty;
	self->string.count = 0;
	self->context = 0;
}

PafCommonStringField *ScePafCommon_50C5FA24(PafCommonStringField *self)
{
	InitCommonStringField(self);
	return self;
}

PafCommonStringField *ScePafCommon_CA19F75C(PafCommonStringField *self)
{
	InitCommonStringField(self);
	return self;
}

static inline __attribute__((always_inline))
PafCommonStringField *CopyCommonStringField(PafCommonStringField *self,
					  const PafString *source)
{
	InitCommonStringField(self);
	PafStringCopy(&self->string, source);
	return self;
}

PafCommonStringField *ScePafCommon_01F82D1B(PafCommonStringField *self,
					     const PafString *source)
{
	return CopyCommonStringField(self, source);
}

PafCommonStringField *ScePafCommon_BFB672E6(PafCommonStringField *self,
					     const PafString *source)
{
	return CopyCommonStringField(self, source);
}

PafCommonStringField *ScePafCommon_CC229501(PafCommonStringField *self,
					     const PafString *source)
{
	return CopyCommonStringField(self, source);
}

PafCommonStringField *ScePafCommon_F98D9F4F(PafCommonStringField *self,
					     const PafString *source)
{
	return CopyCommonStringField(self, source);
}

static inline __attribute__((always_inline))
PafCommonStringField *AssignCommonStringField(PafCommonStringField *self,
					    const char *source, size_t length)
{
	InitCommonStringField(self);
	PafStringAssign(&self->string, source, length);
	return self;
}

PafCommonStringField *ScePafCommon_1F1C3F8D(PafCommonStringField *self,
					     const char *source, size_t length)
{
	return AssignCommonStringField(self, source, length);
}

PafCommonStringField *ScePafCommon_5EEAEBAE(PafCommonStringField *self,
					     const char *source, size_t length)
{
	return AssignCommonStringField(self, source, length);
}

static inline __attribute__((always_inline))
PafCommonStringField *FillCommonStringField(PafCommonStringField *self,
					  size_t length, char value)
{
	InitCommonStringField(self);
	PafStringAppend(&self->string, length, value);
	return self;
}

PafCommonStringField *ScePafCommon_6CD38929(PafCommonStringField *self,
					     size_t length, char value)
{
	return FillCommonStringField(self, length, value);
}

PafCommonStringField *ScePafCommon_74D8D5D7(PafCommonStringField *self,
					     size_t length, char value)
{
	return FillCommonStringField(self, length, value);
}

uint32_t ScePafCommon_6ED1BC31(uint32_t *self)
{
	return (uint32_t)sceAtomicIncrement32AcqRel((volatile int32_t *)&self[1]) + 1;
}

uint32_t ScePafCommon_A704EEE6(uint32_t *self)
{
	return (uint32_t)sceAtomicIncrement32AcqRel((volatile int32_t *)&self[3]) + 1;
}

int ScePafCommon_ED0D1711(const void *self, const char *tag)
{
	return tag == "property_element";
}
