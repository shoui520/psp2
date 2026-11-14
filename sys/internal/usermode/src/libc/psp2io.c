/*
 * psp2io.c: the C library's system interface on psp2. Files are FIOS2 file handles; the
 * standard streams (file numbers 0, 1 and 2) are the process's own I/O descriptors, written
 * with SceIofilemgr. A failure sets errno from the SCE error code and returns -1.
 */
#include <errno.h>
#include <stdarg.h>
#include <sceerror.h>
#include <kernel/iofilemgr.h>
#include <fios2/fios2_api.h>
#include <fios2/fios2_errors.h>

#include "psp2io.h"

/* errno for an SCE error: its status when it is an errno error, else EIO */
static inline int sceErrorToErrno(int err)
{
	int e = EIO;

	if (SCE_ERROR_FACILITY(err) == SCE_ERROR_FACILITY_ERRNO)
		e = (unsigned short)err;
	return e;
}

/* FIOS2 open flags for SceIofilemgr's */
int _FiosOpenFlags(int flags)
{
	int fiosFlags = 0;

	if (flags & SCE_O_RDONLY)
		fiosFlags = SCE_FIOS_O_READ;
	if (flags & SCE_O_WRONLY)
		fiosFlags |= SCE_FIOS_O_WRITE;
	if (flags & SCE_O_APPEND)
		fiosFlags |= SCE_FIOS_O_APPEND;
	if (flags & SCE_O_CREAT)
		fiosFlags |= SCE_FIOS_O_CREAT;
	if (flags & SCE_O_TRUNC)
		fiosFlags |= SCE_FIOS_O_TRUNC;
	if (flags & SCE_O_RDONLY)
		fiosFlags |= SCE_FIOS_O_READ;
	return fiosFlags;
}

/* errno for a FIOS2 error */
int _FiosErrorToErrno(int err)
{
	switch (err) {
	case SCE_FIOS_ERROR_UNIMPLEMENTED:
	case SCE_FIOS_ERROR_CANT_ALLOCATE_OP:
	case SCE_FIOS_ERROR_CANT_ALLOCATE_FH:
	case SCE_FIOS_ERROR_CANT_ALLOCATE_DH:
	case SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK:
	case SCE_FIOS_ERROR_BAD_PTR:
	case SCE_FIOS_ERROR_BAD_OFFSET:
	case SCE_FIOS_ERROR_BAD_SIZE:
	case SCE_FIOS_ERROR_BAD_IOVCNT:
	case SCE_FIOS_ERROR_BAD_OP:
	case SCE_FIOS_ERROR_BAD_DH:
	case SCE_FIOS_ERROR_BAD_ALIGNMENT:
	case SCE_FIOS_ERROR_DECOMPRESSION:
		err = SCE_ERROR_ERRNO_EIO;
		break;
	case SCE_FIOS_ERROR_BAD_PATH:
		err = SCE_ERROR_ERRNO_ENOENT;
		break;
	case SCE_FIOS_ERROR_BAD_FH:
		err = SCE_ERROR_ERRNO_EBADF;
		break;
	case SCE_FIOS_ERROR_NOT_A_DIRECTORY:
		err = SCE_ERROR_ERRNO_ENOTDIR;
		break;
	case SCE_FIOS_ERROR_EOF:
		err = SCE_ERROR_ERRNO_ERANGE;
		break;
	case SCE_FIOS_ERROR_ACCESS:
		err = SCE_ERROR_ERRNO_EPERM;
		break;
	case SCE_FIOS_ERROR_READ_ONLY:
		err = SCE_ERROR_ERRNO_EROFS;
		break;
	case SCE_FIOS_ERROR_MEDIA_GONE:
		err = SCE_ERROR_ERRNO_ENODEV;
		break;
	}
	return sceErrorToErrno(err);
}

int _Read(int fd, void *buf, size_t nbyte)
{
	int res;

	if (fd == 0) {
		if (_StdinFd < 0)
			return _NOT_OPEN;
		res = sceIoRead(_StdinFd, buf, nbyte);
		if (res < 0) {
			errno = sceErrorToErrno(res);
			return -1;
		}
	} else {
		if (!sceFiosIsValidHandle(fd))
			return _NOT_OPEN;
		res = sceFiosFHReadSync(NULL, fd, buf, nbyte);
		if (res < 0) {
			errno = _FiosErrorToErrno(res);
			return -1;
		}
	}
	return res;
}

int _Write(int fd, const void *buf, size_t nbyte)
{
	int res;

	if (fd == 1 || fd == 2) {
		if (fd == 1)
			fd = _StdoutFd;
		if (fd == 2)
			fd = _StderrFd;
		if (fd < 0)
			return _NOT_OPEN;
		res = sceIoWrite(fd, buf, nbyte);
		if (res < 0) {
			errno = sceErrorToErrno(res);
			return -1;
		}
	} else {
		if (!sceFiosIsValidHandle(fd))
			return _NOT_OPEN;
		res = sceFiosFHWriteSync(NULL, fd, buf, nbyte);
		if (res < 0) {
			errno = _FiosErrorToErrno(res);
			return -1;
		}
	}
	return res;
}

int _Open(const char *path, int flags, ...)
{
	SceFiosOpenParams params = SCE_FIOS_OPENPARAMS_INITIALIZER;
	SceFiosFH fh;
	int mode = 0;
	int res;
	va_list ap;

	va_start(ap, flags);
	if (flags & SCE_O_CREAT)
		mode = va_arg(ap, int);
	params.openFlags = _FiosOpenFlags(flags);
	res = sceFiosFHOpenWithModeSync(NULL, &fh, path, &params, mode);
	va_end(ap);
	if (res != 0) {
		errno = _FiosErrorToErrno(res);
		return -1;
	}
	return fh;
}

int _Close(int fd)
{
	int res;

	if (fd < 3)
		return 0;
	res = sceFiosFHCloseSync(NULL, fd);
	if (res != 0) {
		errno = _FiosErrorToErrno(res);
		return -1;
	}
	return res;
}

long long _Lseek(int fd, long long offset, int whence)
{
	SceUID uid;
	SceFiosOffset pos;
	int res;

	if (fd == 0) {
		uid = _StdinFd;
	} else if (fd == 1 || fd == 2) {
		if (fd == 1)
			uid = _StdoutFd;
		else if (fd == 2)
			uid = _StderrFd;
	} else {
		pos = sceFiosFHSeek(fd, offset, whence);
		if (pos < 0) {
			errno = _FiosErrorToErrno(pos);
			return -1;
		}
		return pos;
	}
	if (uid < 0)
		return _NOT_OPEN;
	res = sceIoLseek(uid, offset, whence);
	if (res < 0) {
		errno = sceErrorToErrno(res);
		res = -1;
	}
	return res;
}

int _Isatty(int fd)
{
	return fd == 0 || fd == 1 || fd == 2;
}

int rename(const char *oldname, const char *newname)
{
	int res = sceFiosRenameSync(NULL, oldname, newname);

	if (res != 0) {
		errno = _FiosErrorToErrno(res);
		return -1;
	}
	return res;
}

int remove(const char *filename)
{
	int res = sceFiosDeleteSync(NULL, filename);

	if (res != 0) {
		errno = _FiosErrorToErrno(res);
		return -1;
	}
	return res;
}

int _Mkdir(const char *path)
{
	int res = sceFiosDirectoryCreateSync(NULL, path);

	if (res != 0) {
		errno = _FiosErrorToErrno(res);
		return -1;
	}
	return res;
}

int _Rmdir(const char *path)
{
	int res = sceFiosDirectoryDeleteSync(NULL, path);

	if (res != 0) {
		errno = _FiosErrorToErrno(res);
		return -1;
	}
	return res;
}

int _Truncate(const char *path, long long length)
{
	int res = sceFiosFileTruncateSync(NULL, path, length);

	if (res != 0) {
		errno = _FiosErrorToErrno(res);
		return -1;
	}
	return res;
}
