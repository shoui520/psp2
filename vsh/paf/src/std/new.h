#ifndef PAF_STD_NEW_H
#define PAF_STD_NEW_H

#include <stddef.h>

#define PafPlacementNew _ZnwjPv
#define PafNew _Znwj
#define PafNewArray _Znaj
#define PafAlignedNew _Znwjj
#define PafAlignedNewArray _Znajj
#define PafDelete _ZdlPv
#define PafDeleteArray _ZdaPv
#define PafAlignedDelete _ZdlPvjj
#define PafAlignedDeleteArray _ZdaPvjj

void *PafPlacementNew(size_t size, void *where);
void __cxa_pure_virtual(void);
int ScePafStdc_25A6D9BB(void);
void *PafNew(size_t size);
void *PafNewArray(size_t size);
void *PafAlignedNew(size_t size, size_t align);
void *PafAlignedNewArray(size_t size, size_t align);
void PafDelete(void *ptr);
void PafDeleteArray(void *ptr);
void PafAlignedDelete(void *ptr, size_t size, size_t align);
void PafAlignedDeleteArray(void *ptr, size_t size, size_t align);

#endif
