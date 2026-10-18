/*
 * errno.h: the SDK's error numbers, with Sony's psp2 values (SDK Release 03.570.011,
 * target/include/errno.h): the C library's own codes, then the POSIX names it gives no code of
 * its own (1000 and up). errno is the calling thread's, from the user-mode C library.
 */
#ifndef _ERRNO
#define _ERRNO

#define EPERM		0x01
#define ENOENT		0x02
#define ESRCH		0x03
#define EINTR		0x04
#define EIO		0x05
#define ENXIO		0x06
#define E2BIG		0x07
#define ENOEXEC		0x08
#define EBADF		0x09
#define ECHILD		0x0a
#define EAGAIN		0x0b
#define ENOMEM		0x0c
#define EACCES		0x0d
#define EFAULT		0x0e
#define EBUSY		0x10
#define EEXIST		0x11
#define EXDEV		0x12
#define ENODEV		0x13
#define ENOTDIR		0x14
#define EISDIR		0x15
#define EINVAL		0x16
#define ENFILE		0x17
#define EMFILE		0x18
#define ENOTTY		0x19
#define EFBIG		0x1b
#define ENOSPC		0x1c
#define ESPIPE		0x1d
#define EROFS		0x1e
#define EMLINK		0x1f
#define EPIPE		0x20
#define EDOM		0x21
#define ERANGE		0x22
#define EDEADLK		0x2d
#define ENOLCK		0x2e
#define EBADMSG		0x4d
#define ENOSYS		0x58
#define ENOTEMPTY	0x5a
#define ENAMETOOLONG	0x5b
#define ETIMEDOUT	0x74
#define EINPROGRESS	0x77
#define EMSGSIZE	0x7a
#define ENOTSUP		0x86
#define EILSEQ		0x8a
#define ECANCELED	0x8c
#define EFPOS		0x98

#define EADDRINUSE	1002
#define EADDRNOTAVAIL	1003
#define EAFNOSUPPORT	1004
#define EALREADY	1006
#define ECONNABORTED	1012
#define ECONNREFUSED	1013
#define ECONNRESET	1014
#define EDESTADDRREQ	1016
#define EHOSTUNREACH	1021
#define EIDRM		1022
#define EISCONN		1028
#define ELOOP		1030
#define ENETDOWN	1035
#define ENETRESET	1036
#define ENETUNREACH	1037
#define ENOBUFS		1039
#define ENODATA		1040
#define ENOLINK		1045
#define ENOMSG		1047
#define ENOPROTOOPT	1048
#define ENOSR		1050
#define ENOSTR		1051
#define ENOTCONN	1053
#define ENOTRECOVERABLE	1056
#define ENOTSOCK	1057
#define EOPNOTSUPP	1061
#define EOTHER		1062
#define EOVERFLOW	1063
#define EOWNERDEAD	1064
#define EPROTO		1067
#define EPROTONOSUPPORT	1068
#define EPROTOTYPE	1069
#define ETIME		1074
#define ETXTBSY		1076
#define EWOULDBLOCK	1077

#ifdef __cplusplus
extern "C" {
#endif

int *_sceLibcErrnoLoc(void);
#define errno		(*_sceLibcErrnoLoc())

#ifdef __cplusplus
}
#endif

#endif /* _ERRNO */
