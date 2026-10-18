/*
 * vitasdk's psp2common/kernel/modulemgr.h. Kernel code takes the module manager's types and
 * constants from modulemgr/modulemgr.h (with kernel/modulemgr.h, Sony's SDK names), and so do the
 * vitasdk kernel headers that include this one. User code keeps vitasdk's header until the
 * user-mode SDK headers.
 */
#ifdef PSP2_KERNEL
#ifndef _PSP2COMMON_KERNEL_MODULEMGR_H_
#define _PSP2COMMON_KERNEL_MODULEMGR_H_

#include <modulemgr/modulemgr.h>

#endif /* _PSP2COMMON_KERNEL_MODULEMGR_H_ */
#else
#include_next <psp2common/kernel/modulemgr.h>
#endif
