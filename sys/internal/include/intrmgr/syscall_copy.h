/*
 * Copying the arguments of a system call from user memory, as Sony's syscall
 * exports do: the argument block first, then each buffer or string into a
 * kernel copy allocated on the stack (alloca) only when the caller passed it.
 * Used by SceVshBridge, SceDrmBridge and SceIofilemgr.
 */

#pragma once

#include <scetypes.h>
#include <sysmem/sysmem.h>
#include <kernel/error.h>

/* the argument block of the calls with one variable size (the blocks are 8 byte aligned) */
typedef struct SyscallSizeArgs {
	SceSize size;
	SceUInt32 reserved;
} __attribute__((aligned(8))) SyscallSizeArgs;

/* the argument block of the calls without a sized buffer: read, not used */
typedef struct SyscallUnusedArgs {
	SceUInt32 reserved[2];
} SyscallUnusedArgs;

/* the result of a call whose output was then copied out */
static inline int copyResult(int ret, int copy)
{
	return copy < 0 ? copy : ret;
}

/* in, optional: size bytes */
#define BLOCK_IN(_ptr, _user, _size) do {				\
	if ((_user) != NULL) {						\
		(_ptr) = __builtin_alloca(_size);			\
		ret = SceSysmemForDriver_BC996A7A((_ptr), (_user), (_size));	\
		if (ret < 0)						\
			return ret;					\
	}								\
} while (0)

/* in, optional: up to max bytes, the size from the argument block */
#define BLOCK_IN_SIZED(_ptr, _user, _size, _max) do {			\
	if ((_user) != NULL) {						\
		if ((_size) > (_max))					\
			return SCE_KERNEL_ERROR_NO_MEMORY;		\
		(_ptr) = __builtin_alloca(_max);			\
		ret = SceSysmemForDriver_BC996A7A((_ptr), (_user), (_size));	\
		if (ret < 0)						\
			return ret;					\
	}								\
} while (0)

/* in, optional: a string of up to size bytes, which must be terminated */
#define STRING_IN(_ptr, _user, _size) do {				\
	if ((_user) != NULL) {						\
		(_ptr) = __builtin_alloca(_size);			\
		ret = SceSysmemForDriver_DB3EC244((_ptr), (_user), (_size));	\
		if (ret < 0)						\
			return ret;					\
		if (ret == (_size) && ((signed char *)(_ptr))[(_size) - 1] != 0)	\
			return SCE_KERNEL_ERROR_NO_MEMORY;		\
	}								\
} while (0)

/* the argument block */
#define ARGS_IN(_args, _uargs) do {					\
	ret = SceSysmemForDriver_BC996A7A(&(_args), (_uargs), sizeof(_args));	\
	if (ret < 0)							\
		return ret;						\
} while (0)

/* out, optional, zeroed first: size bytes; the call's result unless the copy fails */
#define BLOCK_OUT(_user, _size, _call_null, _call_k) do {		\
	SceUInt8 *k;							\
									\
	if ((_user) == NULL)						\
		return _call_null;					\
	k = __builtin_alloca(_size);					\
	__builtin_memset(k, 0, (_size));				\
	ret = _call_k;							\
	return copyResult(ret, SceSysmemForDriver_6D88EF8A((_user), k, (_size)));	\
} while (0)

/* out, optional, zeroed first: up to max bytes, the size from the argument block */
#define BLOCK_OUT_SIZED(_user, _size, _max, _call_null, _call_k) do {	\
	SceUInt8 *k;							\
									\
	if ((_user) == NULL)						\
		return _call_null;					\
	if ((_size) > (_max))						\
		return SCE_KERNEL_ERROR_NO_MEMORY;			\
	k = __builtin_alloca(_max);					\
	__builtin_memset(k, 0, (_max));					\
	ret = _call_k;							\
	return copyResult(ret, SceSysmemForDriver_6D88EF8A((_user), k, (_size)));	\
} while (0)

