#ifndef PAF_COMMON_PROPERTY_LIST_H
#define PAF_COMMON_PROPERTY_LIST_H

#include "../std/string.h"

#define StringElementEmptyString _ZN3paf6common13property_list13StringElement14s_empty_stringE
#define WStringElementEmptyString _ZN3paf6common13property_list14WStringElement14s_empty_stringE

extern PafString StringElementEmptyString;
extern PafWString WStringElementEmptyString;

void PropertyListInitialize(void);

#endif
