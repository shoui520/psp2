/* vitasdk's psp2common/net.h: kernel units (and vitasdk's libc headers they include) take Sony's net.h. */
#pragma once

#ifdef PSP2_KERNEL
#include <net.h>
#else
#include_next <psp2common/net.h>
#endif
