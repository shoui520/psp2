#ifndef PAF_VERSATILEPARAM_IDPARAM_H
#define PAF_VERSATILEPARAM_IDPARAM_H

#include "../std/string.h"

typedef struct IDParam {
	PafString m_id;
	uint32_t m_idhash;
} IDParam;

typedef char IDParamSizeCheck[sizeof(IDParam) == 16 ? 1 : -1];

#define IDParamID2IDHash ScePafResource_5F05DE68

uint32_t IDParamID2IDHash(IDParam *self, const PafString *id);

extern size_t sce_paf_strlen(const char *str);

static inline __attribute__((always_inline))
void IDParamConstructFromString(IDParam *self, const char *id)
{
	PafStringInitialize(&self->m_id);
	if (id) {
		PafString value;

		PafStringInitialize(&value);
		PafStringAssign(&value, id, sce_paf_strlen(id));
		PafStringCopy(&self->m_id, &value);
		self->m_idhash = IDParamID2IDHash(self, &value);
		PafStringDestroy(&value);
	} else {
		self->m_idhash = 0;
	}
}

#endif
