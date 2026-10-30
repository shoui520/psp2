/* Lightweight native PAF property/string field constructors. */
#include "../std/string.h"
#include "../std/new.h"
#include "../compiler.h"
#include "property_list.h"
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

int ScePafCommon_B8FC8757(const void *self, const char *tag)
{
	return tag == "string_element" || tag == "property_element";
}

int ScePafCommon_CF5AC562(const void *self, const char *tag)
{
	return tag == "wstring_element" || tag == "property_element";
}

PafCommonStringField *ScePafCommon_25E1C8C4(PafCommonStringField *self,
					    const char *source)
{
	size_t length = 0;

	InitCommonStringField(self);
	if (source)
		length = sce_paf_strlen(source);
	PafStringAssign(&self->string, source, length);
	return self;
}

PafCommonStringField *ScePafCommon_AED94915(PafCommonStringField *self,
					    const char *source)
{
	size_t length = 0;

	InitCommonStringField(self);
	if (source)
		length = sce_paf_strlen(source);
	PafStringAssign(&self->string, source, length);
	return self;
}

typedef struct PafCommonElement {
	int (**methods)(struct PafCommonElement *self, const char *tag);
	uint8_t contents[];
} PafCommonElement;

PafString *ScePafCommon_FE3900DF(PafCommonElement **self)
{
	PafCommonElement *element = *self;

	if (element && element->methods[0](element, "string_element") &&
	    (element = *self) != 0)
		return (PafString *)element->contents;
	return &StringElementEmptyString;
}

PafWString *ScePafCommon_215DA541(PafCommonElement **self)
{
	PafCommonElement *element = *self;

	if (element && element->methods[0](element, "wstring_element") &&
	    (element = *self) != 0)
		return (PafWString *)element->contents;
	return &WStringElementEmptyString;
}

typedef struct PafCommonSharedPointer {
	void *object;
	void *control;
} PafCommonSharedPointer;

typedef struct PafCommonPointerList {
	uint8_t reserved[28];
	PafCommonSharedPointer *items;
	uint32_t count;
} PafCommonPointerList;

_Static_assert(offsetof(PafCommonPointerList, items) == 28,
	       "common pointer list offset");
_Static_assert(offsetof(PafCommonPointerList, count) == 32,
	       "common pointer list count offset");

PafCommonSharedPointer *ScePafCommon_EA03EA22(
		PafCommonSharedPointer *result, PafCommonPointerList *self,
		uint32_t index)
{
	if (index >= self->count) {
		result->object = 0;
		result->control = 0;
	} else {
		*(uint64_t *)result = *(uint64_t *)&self->items[index];
	}
	return result;
}

typedef struct PafCommonSharedPointerRange {
	PafCommonSharedPointer *items;
	uint32_t firstIndex;
} PafCommonSharedPointerRange;

PafCommonSharedPointer *ScePafCommon_4A6AF5C3(
		PafCommonSharedPointer *result,
		const PafCommonSharedPointerRange *range, uint32_t index)
{
	if (range->firstIndex >= index) {
		result->object = 0;
		result->control = 0;
	} else {
		PafCommonSharedPointer *source = &range->items[index];

		result->object = source->object;
		result->control = source->control;
		if (result->object)
			return (PafCommonSharedPointer *)(uintptr_t)(
				(uint32_t)sceAtomicIncrement32AcqRel(
					(volatile int32_t *)result->control + 1) + 1);
	}
	return result;
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

typedef struct PafCommonReferenceString {
	char *ptr;
	uint32_t count;
	uint32_t allocator;
	volatile int32_t references;
} PafCommonReferenceString;

uint32_t ScePafCommon_3728D46C(PafCommonReferenceString *self)
{
	uint32_t remaining = (uint32_t)sceAtomicDecrement32AcqRel(
		&self->references) - 1;

	if (!remaining && self) {
		char *ptr = self->ptr;

		if (ptr != &PafStringEmpty)
			PafDelete(ptr);
		self->ptr = (char *)-1;
		self->count = UINT32_MAX;
		PafDelete(self);
	}
	return remaining;
}

/* Find a keyed string-property pair in the native node tree. */
PafCommonSharedPointer *ScePafCommon_91CDF7D0(
		PafCommonSharedPointer *result, const uint8_t *self,
		const uint8_t *key)
{
	uint32_t *sentinel = *(uint32_t *const *)(self + 44);
	uint32_t *node;

	if (sentinel) {
		node = (uint32_t *)*sentinel;
		if (!((uint8_t *)node)[41]) {
			uint32_t wanted = *(const uint32_t *)(key + 12);

			do {
				uint32_t actual = node[7];

				if (wanted >= actual) {
					if (actual >= wanted)
						break;
					node = (uint32_t *)node[2];
				} else {
					node = (uint32_t *)node[1];
				}
			} while (!((uint8_t *)node)[41]);
		}
	} else {
		node = 0;
	}
	if (node == sentinel) {
		result->object = 0;
		result->control = 0;
	} else {
		*(uint64_t *)result = *(uint64_t *)(node + 8);
	}
	return result;
}

/* Find a keyed shared resource and retain its reference-counted owner. */
PafCommonSharedPointer *ScePafCommon_97478EC4(
		PafCommonSharedPointer *result, const uint8_t *self,
		const uint8_t *key)
{
	uint32_t *sentinel = *(uint32_t *const *)(self + 16);
	uint32_t *node;

	if (sentinel) {
		node = (uint32_t *)*sentinel;
		if (!((uint8_t *)node)[37]) {
			uint32_t wanted = *(const uint32_t *)(key + 12);

			do {
				uint32_t actual = node[6];

				if (wanted >= actual) {
					if (actual >= wanted)
						break;
					node = (uint32_t *)node[2];
				} else {
					node = (uint32_t *)node[1];
				}
			} while (!((uint8_t *)node)[37]);
		}
	} else {
		node = 0;
	}
	if (node == sentinel) {
		result->object = 0;
		result->control = 0;
	} else {
		void *object = (void *)(uintptr_t)node[7];
		uint32_t control = node[8];

		result->object = object;
		result->control = (void *)(uintptr_t)control;
		if (object)
			return (PafCommonSharedPointer *)(uintptr_t)(
				(uint32_t)sceAtomicIncrement32AcqRel(
					(volatile int32_t *)(uintptr_t)(control + 4)) + 1);
	}
	return result;
}
