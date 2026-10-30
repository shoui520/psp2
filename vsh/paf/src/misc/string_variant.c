/* Ownership operations for PAF's narrow/wide dynamic-string variants. */
#include <stdint.h>
#include "../std/string.h"
#include "../std/new.h"
#include <ces/libces_ucs.h>

typedef struct PafStringPair {
	PafString *narrow;
	PafWString *wide;
} PafStringPair;

typedef struct PafStringVariant {
	uint32_t kind;
	PafString *narrow;
	PafWString *wide;
} PafStringVariant;

static void DeleteNarrowString(PafString *value)
{
	if (value) {
		PafStringDestroy(value);
		PafDelete(value);
	}
}

static void DeleteWideString(PafWString *value)
{
	if (value) {
		PafWStringDestroy(value);
		PafDelete(value);
	}
}

__attribute__((noinline))
PafStringPair *ScePafMisc_8FDF9295(PafStringPair *self)
{
	DeleteNarrowString(self->narrow);
	DeleteWideString(self->wide);
	self->narrow = 0;
	self->wide = 0;
	return self;
}

__attribute__((noinline))
PafStringVariant *ScePafMisc_AF142E26(PafStringVariant *self)
{
	PafString *narrow = self->narrow;

	self->kind = 0;
	DeleteNarrowString(narrow);
	DeleteWideString(self->wide);
	self->narrow = 0;
	self->wide = 0;
	return self;
}

PafStringPair *ScePafMisc_4B6BCFAF(PafStringPair *self)
{
	ScePafMisc_8FDF9295(self);
	return self;
}

PafStringPair *ScePafMisc_AD3D3047(PafStringPair *self)
{
	ScePafMisc_8FDF9295(self);
	return self;
}

PafStringVariant *ScePafMisc_B45A7521(PafStringVariant *self)
{
	ScePafMisc_AF142E26(self);
	return self;
}

PafStringVariant *ScePafMisc_B9C99123(PafStringVariant *self)
{
	ScePafMisc_AF142E26(self);
	return self;
}

/* Convert a UTF-8 byte string into an owned UTF-16 PAF string. */
PafWString *ScePafMisc_A6ABC0B2(const uint8_t *src, PafWString *destination)
{
	uint32_t sourceLength;
	uint32_t convertedLength;
	uint32_t size = 0;
	SceCesUcsContext context;

	sceCesUcsContextInit(&context);
	if (sceCesUtf8StrGetUtf16Len(&context, src, 0,
					&sourceLength, &convertedLength) == 0)
		size = convertedLength;
	if (destination) {
		if (size < destination->count)
			PafWStringErase(destination, size, UINT32_MAX);
		else if (size > destination->count)
			PafWStringAppend(destination, size - destination->count, 0);
	} else {
		destination = PafNew(sizeof(*destination));
		if (destination) {
			PafWStringInitialize(destination);
			PafWStringAppend(destination, size, 0);
		}
	}
	if (size)
		sceCesUtf8StrToUtf16Str(&context, src, 0, &sourceLength,
					  destination->ptr, size + 1, &convertedLength);
	return destination;
}

/* Reverse conversion, preserving the native resize-before-convert order. */
PafString *ScePafMisc_E690F751(const uint16_t *src, PafString *destination)
{
	uint32_t sourceLength;
	uint32_t convertedLength;
	uint32_t size = 0;
	SceCesUcsContext context;

	sceCesUcsContextInit(&context);
	if (sceCesUtf16StrGetUtf8Len(&context, src, 0,
					&sourceLength, &convertedLength) == 0)
		size = convertedLength;
	if (destination) {
		if (size < destination->count)
			PafStringErase(destination, size, UINT32_MAX);
		else if (size > destination->count)
			PafStringAppend(destination, size - destination->count, 0);
	} else {
		destination = PafNew(sizeof(*destination));
		if (destination) {
			PafStringInitialize(destination);
			PafStringAppend(destination, size, 0);
		}
	}
	if (size)
		sceCesUtf16StrToUtf8Str(&context, src, 0, &sourceLength,
					 destination->ptr, size + 1, &convertedLength);
	return destination;
}

/* Initialise a tagged string variant from a UTF-8 source. */
__attribute__((noinline))
PafStringVariant *ScePafMisc_86181366(PafStringVariant *self,
				    const char *src, int mode)
{
	ScePafMisc_AF142E26(self);
	if (mode == 2) {
		if (src) {
			self->wide = ScePafMisc_A6ABC0B2((const uint8_t *)src,
							   self->wide);
			self->kind = 2;
		}
	} else {
		PafString *narrow = self->narrow;

		if (narrow) {
			size_t length = src ? sce_paf_strlen(src) : 0;
			PafStringAssign(narrow, src, length);
		} else {
			PafString *value = PafNew(sizeof(*value));

			if (value) {
				size_t length;

				PafStringInitialize(value);
				length = src ? sce_paf_strlen(src) : 0;
				PafStringAssign(value, src, length);
			}
			self->narrow = value;
		}
		self->kind = 1;
	}
	return self;
}

/* Initialise a tagged string variant from a UTF-16 source. */
__attribute__((noinline))
PafStringVariant *ScePafMisc_6DBCED34(PafStringVariant *self,
				    const uint16_t *src, int mode)
{
	ScePafMisc_AF142E26(self);
	if (mode == 1) {
		if (src) {
			self->narrow = ScePafMisc_E690F751(src, self->narrow);
			self->kind = 1;
		}
	} else {
		PafWString *wide = self->wide;

		if (wide) {
			size_t length = src ? sce_paf_wcslen(src) : 0;
			PafWStringAssign(wide, src, length);
		} else {
			PafWString *value = PafNew(sizeof(*value));

			if (value) {
				size_t length;

				PafWStringInitialize(value);
				length = src ? sce_paf_wcslen(src) : 0;
				PafWStringAssign(value, src, length);
			}
			self->wide = value;
		}
		self->kind = 2;
	}
	return self;
}

/* Two ABI aliases for each public constructor preserve the incoming string
 * and mode registers while clearing the pre-existing variant state. */
static void InitializeStringVariant(PafStringVariant *self)
{
	self->kind = 0;
	self->narrow = 0;
	self->wide = 0;
}

PafStringVariant *ScePafMisc_C11CB5E4(PafStringVariant *self,
				      const char *src, int mode)
{
	InitializeStringVariant(self);
	ScePafMisc_86181366(self, src, mode);
	return self;
}

PafStringVariant *ScePafMisc_E2506D74(PafStringVariant *self,
				      const char *src, int mode)
{
	InitializeStringVariant(self);
	ScePafMisc_86181366(self, src, mode);
	return self;
}

PafStringVariant *ScePafMisc_E13019F7(PafStringVariant *self,
				      const char *src, int mode)
{
	InitializeStringVariant(self);
	ScePafMisc_86181366(self, src, mode);
	return self;
}

PafStringVariant *ScePafMisc_2F0BF4FF(PafStringVariant *self,
				      const char *src, int mode)
{
	InitializeStringVariant(self);
	ScePafMisc_86181366(self, src, mode);
	return self;
}

PafStringVariant *ScePafMisc_DCBA2BD4(PafStringVariant *self,
				      const uint16_t *src, int mode)
{
	InitializeStringVariant(self);
	ScePafMisc_6DBCED34(self, src, mode);
	return self;
}

PafStringVariant *ScePafMisc_E07B9A18(PafStringVariant *self,
				      const uint16_t *src, int mode)
{
	InitializeStringVariant(self);
	ScePafMisc_6DBCED34(self, src, mode);
	return self;
}

/* The cached opposite encoding is generated only on first access. */
uint32_t ScePafMisc_183DD5C3(PafStringVariant *self)
{
	PafWString *wide = self->wide;

	if (!wide) {
		PafString *narrow = self->narrow;

		if (narrow) {
			wide = ScePafMisc_A6ABC0B2((const uint8_t *)narrow->ptr, 0);
			self->wide = wide;
		}
	}
	return wide ? wide->count : 0;
}

uint32_t ScePafMisc_40753A45(PafStringVariant *self)
{
	PafString *narrow = self->narrow;

	if (!narrow) {
		PafWString *wide = self->wide;

		if (wide) {
			narrow = ScePafMisc_E690F751(wide->ptr, 0);
			self->narrow = narrow;
		}
	}
	return narrow ? narrow->count : 0;
}

uint16_t *ScePafMisc_53948F03(PafStringVariant *self)
{
	PafWString *wide = self->wide;

	if (!wide) {
		PafString *narrow = self->narrow;

		if (narrow) {
			wide = ScePafMisc_A6ABC0B2((const uint8_t *)narrow->ptr, 0);
			self->wide = wide;
		}
	}
	return wide ? wide->ptr : 0;
}

char *ScePafMisc_A7C6AF5C(PafStringVariant *self)
{
	PafString *narrow = self->narrow;

	if (!narrow) {
		PafWString *wide = self->wide;

		if (wide) {
			narrow = ScePafMisc_E690F751(wide->ptr, 0);
			self->narrow = narrow;
		}
	}
	return narrow ? narrow->ptr : 0;
}
