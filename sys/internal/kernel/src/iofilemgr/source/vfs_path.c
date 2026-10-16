/*
 * SceIofilemgr: path length checks (file name unknown: no debug records).
 */

#include "iofilemgr_internal.h"

/*
 * Walk a UTF-8 path of at most max bytes: *len gets its length in
 * characters, *nameLen the characters after the device's ':', both with the
 * terminator.
 */
int vfs_check_path_length(const char *path, SceSize *len, SceSize *nameLen, SceSize max)
{
	const SceUInt8 *p = (const SceUInt8 *)path;
	int bytes = 0, chars = 0, nameChars = 0, colon = 0;
	int n;
	SceUInt8 c;
	const SceUInt8 *end;

	while (bytes < (int)max) {
		c = *p;
		if ((signed char)c < 0) {
			/* a multibyte sequence: its lead byte gives the length */
			if (c < 0xc2)
				return SCE_ERROR_ERRNO_EINVAL;
			n = __builtin_clz(~((SceUInt32)c << 24));
			if (n > 6)
				return SCE_ERROR_ERRNO_EINVAL;
			if (n < 2) {
				p++;
			} else {
				c = p[1];
				p += 2;
				if ((c & 0xc0) != 0x80)
					return SCE_ERROR_ERRNO_EINVAL;
				for (end = p + (n - 2); p != end;)
					if ((*p++ & 0xc0) != 0x80)
						return SCE_ERROR_ERRNO_EINVAL;
			}
			bytes += n;
			nameChars++;
		} else {
			p++;
			if (c == '\0') {
				*len = chars + 1;
				*nameLen = nameChars + 1;
				return 0;
			}
			nameChars++;
			if (c == ':' && !colon) {
				nameChars = 0;
				colon = 1;
			}
			bytes++;
		}
		chars++;
	}
	return SCE_ERROR_ERRNO_ENAMETOOLONG;
}
