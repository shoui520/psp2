/*
 * The Solaris types and constants DTrace's CDDL sources use, on the PS Vita
 * kernel (Sony's Solaris compatibility layer, dtrace/sce/). Everything the
 * CDDL code needs before its own headers: the base types, the C library,
 * and the kernel's types.
 */
#pragma once

#include <scetypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <sysmem/sysclib.h>
#include <sysmem/debug.h>
#include <sceerror.h>
#include <kernel/error.h>

/*
 * Sony built the CDDL code with the checked string copies (_FORTIFY_SOURCE): a
 * copy into an object of known size goes through __strncpy_chk or
 * __strlcpy_chk, the rest stays a plain strncpy.
 */
#define strncpy(dst, src, n)	__builtin___strncpy_chk(dst, src, n, __builtin_object_size(dst, 0))
#define strlcpy(dst, src, size)	__strlcpy_chk(dst, src, size, __builtin_object_size(dst, 0))

/* the kernel's errno values are the SCE error codes (sceerror.h) */
#undef EFAULT
#undef EINVAL
#undef ENOENT
#undef EBUSY
#undef ESRCH
#undef ENOMEM
#undef EACCES
#undef E2BIG
#undef ENOTTY
#undef ENOSPC
#undef EAGAIN
#undef EFBIG
#undef EALREADY
#undef ENXIO

#define EFAULT		SCE_ERROR_ERRNO_EFAULT
#define EINVAL		SCE_ERROR_ERRNO_EINVAL
#define ENOENT		SCE_ERROR_ERRNO_ENOENT
#define EBUSY		SCE_ERROR_ERRNO_EBUSY
#define ESRCH		SCE_ERROR_ERRNO_ESRCH
#define ENOMEM		SCE_ERROR_ERRNO_ENOMEM
#define EACCES		SCE_ERROR_ERRNO_EACCES
#define E2BIG		SCE_ERROR_ERRNO_E2BIG
#define ENOTTY		SCE_ERROR_ERRNO_ENOTTY
#define ENOSPC		SCE_ERROR_ERRNO_ENOSPC
#define EAGAIN		SCE_ERROR_ERRNO_EAGAIN
#define EFBIG		SCE_ERROR_ERRNO_EFBIG
#define EALREADY	SCE_ERROR_ERRNO_EALREADY
#define ENXIO		SCE_ERROR_ERRNO_ENXIO

#define DTRACE_INVALID_UID	(-1)

typedef unsigned char		uchar_t;
typedef unsigned short		ushort_t;
typedef unsigned int		uint_t;
typedef unsigned long		ulong_t;
typedef long long		longlong_t;
typedef unsigned long long	u_longlong_t;
typedef char			*caddr_t;
typedef long			off_t;
typedef int			ssize_t;

typedef enum {
	B_FALSE = 0,
	B_TRUE = 1
} boolean_t;

typedef int64_t			hrtime_t;
typedef int			processorid_t;
typedef int			zoneid_t;
typedef int			id_t;
typedef int			pid_t;
typedef int			uid_t;
typedef int			gid_t;
typedef int			pri_t;
typedef SceUID			puid_t;
typedef uint32_t		dev_t;
typedef uint32_t		major_t;
typedef uint32_t		minor_t;
typedef uintptr_t		pc_t;
typedef uintptr_t		user_addr_t;
typedef size_t			user_size_t;

#define NBBY		8	/* bits per byte */

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) < (b) ? (b) : (a))

#define roundup(x, y)	((((x) + ((y) - 1)) / (y)) * (y))

#define P2ROUNDUP(x, align)	(-(-(x) & -(align)))
#define IS_P2ALIGNED(v, a)	((((uintptr_t)(v)) & ((uintptr_t)(a) - 1)) == 0)

#define SEC		1
#define MILLISEC	1000
#define MICROSEC	1000000
#define NANOSEC		1000000000

#define bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define bzero(dst, len)		memset((dst), 0, (len))

#define bcmp(s1, s2, len)	memcmp((s1), (s2), (len))

#define CTF_MODEL_ILP32		1
#define CTF_MODEL_NATIVE	CTF_MODEL_ILP32

#define SCE_ARM_MMU_SMALL_PAGE_SIZE	0x1000

/* ARM's va_list is a struct __va_list; the DTrace headers name it as a type */
typedef va_list __va_list;

#include <dtrace/sce/solaris_debug.h>
#include <modulemgr/modulecb_kernel.h>
