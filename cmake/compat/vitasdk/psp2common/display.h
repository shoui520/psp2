/*
 * vitasdk's psp2common/display.h. Kernel code takes the display's types and error codes from display.h (Sony's SDK names), and so do the vitasdk
 * kernel headers that include this one. User code keeps vitasdk's header until the user-mode SDK
 * headers.
 */
#ifdef PSP2_KERNEL
#ifndef _PSP2COMMON_DISPLAY_H_
#define _PSP2COMMON_DISPLAY_H_

#include <display.h>

#endif /* _PSP2COMMON_DISPLAY_H_ */
#else
#include_next <psp2common/display.h>
#endif
