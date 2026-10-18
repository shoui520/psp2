/*
 * vitasdk's psp2common/power.h. Kernel code takes the power service's error codes and callback arguments from power.h (Sony's SDK names), and so do the vitasdk
 * kernel headers that include this one. User code keeps vitasdk's header until the user-mode SDK
 * headers.
 */
#ifdef PSP2_KERNEL
#ifndef _PSP2COMMON_POWER_H_
#define _PSP2COMMON_POWER_H_

#include <power.h>

#endif /* _PSP2COMMON_POWER_H_ */
#else
#include_next <psp2common/power.h>
#endif
