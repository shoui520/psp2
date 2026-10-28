#ifndef PAF_STD_NEW_H
#define PAF_STD_NEW_H

#include <stddef.h>

#define PafPlacementNew _ZnwjPv

void *PafPlacementNew(size_t size, void *where);

#endif
