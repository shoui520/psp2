/*
 * SceVshBridge: copying the user buffers of the exports (vshbridge_user.c,
 * drmbridge.c). Every kernel copy is allocated on the stack (alloca) only
 * when the caller passed the buffer.
 */

#pragma once

#include <psp2kern/types.h>
#include "sce/vshbridge_imports.h"
#include <alloca.h>

#include <sce/syscall_copy.h>

/* the stack of the requests that run on their own (ksceKernelRunWithStack) */
#define VSH_STACK_SIZE			0x2000

/* Marlin personalization (SceVshBridge and SceDrmBridge) */
typedef struct VshMlnpsnlAuth1Args {
	void *out2;
	SceSize aSize;				/* up to 0x20 */
	SceSize bSize;				/* up to 8 */
	SceSize out1Size;			/* up to 0x80 */
	SceSize out2Size;			/* up to 0x40 */
	SceUInt32 reserved;
} __attribute__((aligned(8))) VshMlnpsnlAuth1Args;

typedef struct VshMlnpsnlAuth2Args {
	SceSize aSize;				/* up to 0x50 */
	SceSize bSize;				/* up to 0x40 */
	SceSize outSize;			/* up to 0x10 */
	SceUInt32 reserved;
} __attribute__((aligned(8))) VshMlnpsnlAuth2Args;

/* the body of an Auth1 export: (a, b, out, uargs) to _impl(a, b, out1, out2) */
#define MLNPSNL_AUTH1(_impl) do {					\
	VshMlnpsnlAuth1Args args;					\
	SceUInt8 *pa = NULL, *pb = NULL, *out1 = NULL, *out2 = NULL;	\
	int ret, ret2;							\
									\
	ret = ksceKernelCopyFromUser(&args, uargs, sizeof(args));	\
	if (ret < 0)							\
		return ret;						\
	if (a != NULL) {						\
		if (args.aSize > 0x20)					\
			return SCE_KERNEL_ERROR_NO_MEMORY;		\
		pa = alloca(0x20);				\
		ret = ksceKernelCopyFromUser(pa, a, args.aSize);	\
		if (ret < 0)						\
			return ret;					\
	}								\
	if (b != NULL) {						\
		if (args.bSize > 8)					\
			return SCE_KERNEL_ERROR_NO_MEMORY;		\
		pb = alloca(8);				\
		ret = ksceKernelCopyFromUser(pb, b, args.bSize);	\
		if (ret < 0)						\
			return ret;					\
	}								\
	if (out != NULL) {						\
		if (args.out1Size > 0x80)				\
			return SCE_KERNEL_ERROR_NO_MEMORY;		\
		out1 = alloca(0x80);				\
		__builtin_memset(out1, 0, 0x80);			\
	}								\
	if (args.out2 != NULL) {					\
		if (args.out2Size > 0x40)				\
			return SCE_KERNEL_ERROR_NO_MEMORY;		\
		out2 = alloca(0x40);				\
		__builtin_memset(out2, 0, 0x40);			\
	}								\
	ret = _impl(pa, pb, out1, out2);				\
	if (out != NULL) {						\
		ret2 = ksceKernelCopyToUser(out, out1, args.out1Size);	\
		if (ret2 < 0)						\
			return ret2;					\
	}								\
	if (args.out2 == NULL)						\
		return ret;						\
	return copyResult(ret, ksceKernelCopyToUser(args.out2, out2, args.out2Size));	\
} while (0)

/* the body of an Auth2 export: (a, b, out, uargs) to _impl(a, b, out) */
#define MLNPSNL_AUTH2(_impl) do {					\
	VshMlnpsnlAuth2Args args;					\
	SceUInt8 *pa = NULL, *pb = NULL, *kout;				\
	int ret;							\
									\
	ret = ksceKernelCopyFromUser(&args, uargs, sizeof(args));	\
	if (ret < 0)							\
		return ret;						\
	if (a != NULL) {						\
		if (args.aSize > 0x50)					\
			return SCE_KERNEL_ERROR_NO_MEMORY;		\
		pa = alloca(0x50);				\
		ret = ksceKernelCopyFromUser(pa, a, args.aSize);	\
		if (ret < 0)						\
			return ret;					\
	}								\
	if (b != NULL) {						\
		if (args.bSize > 0x40)					\
			return SCE_KERNEL_ERROR_NO_MEMORY;		\
		pb = alloca(0x40);				\
		ret = ksceKernelCopyFromUser(pb, b, args.bSize);	\
		if (ret < 0)						\
			return ret;					\
	}								\
	if (out == NULL)						\
		return _impl(pa, pb, NULL);				\
	if (args.outSize > 0x10)					\
		return SCE_KERNEL_ERROR_NO_MEMORY;			\
	kout = alloca(0x10);					\
	__builtin_memset(kout, 0, 0x10);				\
	ret = _impl(pa, pb, kout);					\
	return copyResult(ret, ksceKernelCopyToUser(out, kout, args.outSize));	\
} while (0)
