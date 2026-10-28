#ifndef PAF_VERSATILEPARAM_IDPARAM_H
#define PAF_VERSATILEPARAM_IDPARAM_H

#include "../std/string.h"

typedef struct IDParam {
	PafString m_id;
	uint32_t m_idhash;
} IDParam;

typedef char IDParamSizeCheck[sizeof(IDParam) == 16 ? 1 : -1];

#endif
