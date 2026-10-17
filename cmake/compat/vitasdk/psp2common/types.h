/*
 * vitasdk's psp2common/types.h, for the files and vitasdk headers that still include it: the SCE
 * types come from our SDK headers (Sony's definitions), and only vitasdk's own names remain here,
 * until the files stop using them. Sony's SceIoMode (int, kernel/types.h) would clash with
 * vitasdk's enum of that name, so kernel/types.h is not included and SceMode and
 * SceKernelSysClock keep vitasdk's definitions here.
 */
#ifndef _PSP2COMMON_TYPES_H_
#define _PSP2COMMON_TYPES_H_

#include <stddef.h>
#include <stdint.h>
#include <vitasdk/build_utils.h>
#include <scetypes.h>
#include <kernel/constant.h>

#ifndef SCE_ALIGN
#define SCE_ALIGN(x)				__attribute__((aligned(x)))
#endif
#ifndef SCE_DEPRECATED
#define SCE_DEPRECATED(name)			name
#endif
#ifndef SCE_DEPRECATED_EX
#define SCE_DEPRECATED_EX(name, comment)	name
#endif

typedef int		SceMode;
typedef unsigned int	SceNID;
typedef SceUInt64	SceKernelSysClock;

#define SCE_KERNEL_THREAD_ID_SELF	0
#define SCE_OK				(0)

#define SCE_KERNEL_4GiB		(0x100000000LL)
#define SCE_KERNEL_8GiB		(0x200000000LL)
#define SCE_KERNEL_16GiB	(0x400000000LL)
#define SCE_KERNEL_32GiB	(0x800000000LL)

#endif /* _PSP2COMMON_TYPES_H_ */
