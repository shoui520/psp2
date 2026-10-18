/*
 * vitasdk's psp2common/kernel/iofilemgr.h. Kernel code takes the I/O file manager's types and
 * constants from iofilemgr/iofilemgr.h (with kernel/iofilemgr.h, Sony's SDK names), and so do the
 * vitasdk kernel headers that include this one. User code keeps vitasdk's header until the
 * user-mode SDK headers.
 */
#ifdef PSP2_KERNEL
#ifndef _PSP2COMMON_KERNEL_IOFILEMGR_H_
#define _PSP2COMMON_KERNEL_IOFILEMGR_H_

#include <iofilemgr/iofilemgr.h>

#endif /* _PSP2COMMON_KERNEL_IOFILEMGR_H_ */
#else
#include_next <psp2common/kernel/iofilemgr.h>
#endif
