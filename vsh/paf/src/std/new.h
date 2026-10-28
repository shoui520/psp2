#ifndef PAF_STD_NEW_H
#define PAF_STD_NEW_H

#include <stddef.h>

#define PafPlacementNew _ZnwjPv
#define __cxa_pure_virtual ScePafStdc_2730262C
#define PafNew ScePafStdc_FC5CD359
#define PafNewArray ScePafStdc_D8CC1AFE
#define PafAlignedNew ScePafStdc_6AF4A8C8
#define PafAlignedNewArray ScePafStdc_FFAB5F8E
#define PafDelete ScePafStdc_1B77082E
#define PafDeleteArray ScePafStdc_5C468294
#define PafAlignedDelete ScePafStdc_47CC57A5
#define PafAlignedDeleteArray ScePafStdc_EDC2DA6C

void *PafPlacementNew(size_t size, void *where);
void __cxa_pure_virtual(void);
void *PafNew(size_t size);
void *PafNewArray(size_t size);
void *PafAlignedNew(size_t size, size_t align);
void *PafAlignedNewArray(size_t size, size_t align);
void PafDelete(void *ptr);
void PafDeleteArray(void *ptr);
void PafAlignedDelete(void *ptr, size_t size, size_t align);
void PafAlignedDeleteArray(void *ptr, size_t size, size_t align);

#endif
