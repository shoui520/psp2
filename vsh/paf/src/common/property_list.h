#ifndef PAF_COMMON_PROPERTY_LIST_H
#define PAF_COMMON_PROPERTY_LIST_H

#include "../std/string.h"

#define StringElementEmptyString ScePafCommon_F3FEA96C
#define WStringElementEmptyString ScePafCommon_00AD39F1

extern PafString StringElementEmptyString;
extern PafWString WStringElementEmptyString;

void PropertyListInitialize(void);

#endif
