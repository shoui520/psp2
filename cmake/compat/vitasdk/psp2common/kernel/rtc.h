/*
 * vitasdk's psp2common/kernel/rtc.h. Kernel code takes the real-time clock's types and error codes from rtc.h (Sony's SDK names), and so do the vitasdk
 * kernel headers that include this one. User code keeps vitasdk's header until the user-mode SDK
 * headers.
 */
#ifdef PSP2_KERNEL
#ifndef _PSP2COMMON_KERNEL_RTC_H_
#define _PSP2COMMON_KERNEL_RTC_H_

#include <rtc.h>

#endif /* _PSP2COMMON_KERNEL_RTC_H_ */
#else
#include_next <psp2common/kernel/rtc.h>
#endif
