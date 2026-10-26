/*
 * xmtx.h: the C library's locks (IAR DLib): the system locks malloc and the streams take,
 * each a mutex the application may implement through the hooks (none here: no-ops).
 */
#ifndef _XMTX_H
#define _XMTX_H

typedef void *_Rmtx;

/* the system locks */
enum {
	_LOCK_LOCALE,
	_LOCK_MALLOC,
	_LOCK_STREAM,
	_LOCK_DEBUG,
	_MAX_LOCK,
};

void _Locksyslock(unsigned int lockno);
void _Unlocksyslock(unsigned int lockno);

void __iar_system_Mtxinit(_Rmtx *m);
void __iar_system_Mtxdst(_Rmtx *m);
void __iar_system_Mtxlock(_Rmtx *m);
void __iar_system_Mtxunlock(_Rmtx *m);

#endif
