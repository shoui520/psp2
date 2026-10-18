/* vitasdk's psp2/net/net.h: kernel units (and vitasdk's libc headers they include) take Sony's net.h. */
#pragma once

#ifdef PSP2_KERNEL
#include <net.h>
#else
#include_next <psp2/net/net.h>
#endif
