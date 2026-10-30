/* Ownership operations for PAF's narrow/wide dynamic-string variants. */
#include <stdint.h>
#include "../std/string.h"
#include "../std/new.h"

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
