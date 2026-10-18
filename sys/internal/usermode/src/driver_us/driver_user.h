/*
 * SceDriverUser: user-side front ends of kernel services (AppMgr, Display,
 * Error, Fios2 overlays, Rtc, DrmBridge) and the SceMotion library.
 */

#ifndef _DRIVER_USER_H
#define _DRIVER_USER_H

#include <stddef.h>
#include <scetypes.h>

/*
 * A syscall takes at most four register arguments. Further arguments, and the
 * size of a caller buffer the kernel writes back to, travel in a block on the
 * caller's stack that the kernel copies in. Words the wrapper leaves unset are
 * not read for that call.
 */
typedef struct SceSyscallArgs2 {
	SceUInt32 arg[2];
} SceSyscallArgs2;

typedef struct SceSyscallArgs4 {
	SceUInt32 arg[4];
} SceSyscallArgs4;

typedef struct SceSyscallArgs6 {
	SceUInt32 arg[6];
} SceSyscallArgs6;

typedef struct SceSyscallArgs8 {
	SceUInt32 arg[8];
} SceSyscallArgs8;

/* user parameter blocks whose first word is their own size */
typedef struct SceSizedParam {
	SceSize size;
} SceSizedParam;

#endif /* _DRIVER_USER_H */
