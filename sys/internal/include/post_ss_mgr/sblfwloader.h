#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

int sceSblFwLoaderClose(void);
int sceSblFwLoaderLoad(int type, void *dst, SceSize size, SceSize *loaded);
int sceSblFwLoaderSetPath(const char *path, int flags);
