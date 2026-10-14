/*
 * PFS's errors as errno codes, and what each format version uses.
 */

#include "pfsmgr_internal.h"

#define SCE_PFS_ERROR_KEEP		0x80142318	/* name unknown: passed on as it is */
#define SCE_PFS_ERROR_FACILITY		0x014		/* PFS's errors are 0x8014xxxx */

int pfsErrorToErrno(int error)
{
	if (error != SCE_PFS_ERROR_KEEP && error < 0 &&
	    ((SceUInt32)error << 4) >> 20 == SCE_PFS_ERROR_FACILITY) {
		switch (error & 0xff) {
		case 0x00:
			return 0;
		case 0x01:
			return SCE_ERROR_ERRNO_ERANGE;
		case 0x02:
			return SCE_ERROR_ERRNO_EICV;
		case 0x03:
			return SCE_ERROR_ERRNO_ENOSPC;
		case 0x04:
			return SCE_ERROR_ERRNO_ENOENT;
		case 0x05:
			return SCE_ERROR_ERRNO_EALREADY;
		case 0x06:
			return SCE_ERROR_ERRNO_ESRCH;
		case 0x07:
			return SCE_ERROR_ERRNO_ESTALE;
		case 0x08:
			return SCE_ERROR_ERRNO_EIO;
		case 0x09:
			return SCE_ERROR_ERRNO_EINVAL;
		case 0x0a:
		case 0x15:
			return SCE_ERROR_ERRNO_EFORMAT;
		case 0x0b:
			return SCE_ERROR_ERRNO_ENOMEM;
		case 0x0c:
			return SCE_ERROR_ERRNO_EOPNOTSUPP;
		case 0x0e:
			return SCE_ERROR_ERRNO_EFAULT;
		case 0x0f:
			return SCE_ERROR_ERRNO_EBADFD;
		case 0x10:
			return SCE_ERROR_ERRNO_EUNSUP;
		case 0x11:
			return SCE_ERROR_ERRNO_ENOTDIR;
		case 0x12:
			return SCE_ERROR_ERRNO_EISDIR;
		case 0x13:
			return SCE_ERROR_ERRNO_EROFS;
		case 0x14:
			return SCE_ERROR_ERRNO_EPERM;
		case 0x17:
			return SCE_ERROR_ERRNO_EXDEV;
		case 0x18:
			return SCE_ERROR_ERRNO_EBUSY;
		case 0x19:
			return SCE_ERROR_ERRNO_EDQUOT;
		case 0x1c:
			return SCE_ERROR_ERRNO_ENOTSUP;
		case 0x1d:
			return SCE_ERROR_ERRNO_ENAMETOOLONG;
		}
	}
	return error;
}

/* what each format version uses (names unknown) */
static const SceUInt32 g_pfsVersionUnicv[] = { 0, 1, 1, 1, 1, 2 };

static const struct {
	SceUInt32 hash;
	SceUInt32 icv;
} g_pfsVersionParams[] = {
	{ 0, 0 },
	{ 0, 1 },
	{ 1, 1 },
	{ 2, 1 },
};

SceUInt32 pfsVersionIcvParam(SceUInt32 version)
{
	if (version > 3)
		version = 0;
	return g_pfsVersionParams[version].icv;
}

SceUInt32 pfsVersionHashParam(SceUInt32 version)
{
	if (version > 3)
		version = 0;
	return g_pfsVersionParams[version].hash;
}

SceUInt32 pfsVersionUnicvParam(SceUInt32 version)
{
	if (version > 5)
		version = 0;
	return g_pfsVersionUnicv[version];
}
