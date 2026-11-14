/*
 * xmtx.h: the SDK's internal lock declarations, with Sony's psp2 definitions (SDK Release
 * 03.570.011, target/include/xmtx.h): the C library's locks are lightweight mutexes.
 */
#ifndef _XMTX
#define _XMTX

#include <scetypes.h>
#include <yvals.h>

typedef _SceLibcMutexType _Rmtx;

SceInt32 _Mtxinit(_SceLibcMutexType *mtx, const char *name);
SceInt32 _Mtxdst(_SceLibcMutexType *mtx);
SceInt32 _Mtxlock(_SceLibcMutexType *mtx);
SceInt32 _Mtxunlock(_SceLibcMutexType *mtx);

#endif /* _XMTX */
