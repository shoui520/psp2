/*
 * Overlay resolution: a path is rewritten by every overlay whose destination
 * contains it, in table order. An opaque overlay always applies; a
 * translucent or writable one only when the rewritten path exists further
 * down; a "newer" one when its file is newer than what is below it.
 */

#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include "fios2_internal.h"

static inline int fiosIsSeparator(int c)		/* name unknown */
{
	return c == ':' || c == '/' || c == '\0' || c == '\\';
}

/*
 * Resolves path from the overlay at index on, with every order up to 255
 * when the process is new enough, and returns the modification time of
 * what it resolves to.
 */
static int fiosOverlayResolveTime(SceFiosKernelResolveParam *param, int index, char *out,
				  const char *path, SceUInt64 *time)	/* name unknown */
{
	SceIoStat stat;
	SceUInt32 sdkVersion;
	uint8_t maxOrder = param->maxOrder;
	char *resolved;
	int ret, i;

	if (SceProcessmgrForDriver_D141C076(param->pid, &sdkVersion) == 0 && sdkVersion >= 0x1800000)
		param->maxOrder = 0xff;
	for (i = 0; i < (int)sizeof(*time); i++)
		((uint8_t *)time)[i] = 0;

	resolved = fiosPathAlloc();
	if (resolved == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(resolved, 0, FIOS_PATH_MAX);
	__strlcpy_chk(resolved, path, FIOS_PATH_MAX, (size_t)-1);
	ret = sceFiosKernelOverlayResolve(param, index, out, resolved);
	param->maxOrder = maxOrder;
	if (ret >= 0) {
		i = sceFiosKernelPlatformGetstat(resolved, &stat);
		if (i == SCE_ERROR_ERRNO_EICV) {
			*time = 0;
		} else {
			if (i != 0)
				ret = i;
			*time = sceFiosKernelPlatformDateTimeToTime(&stat.st_mtime);
		}
	}
	fiosPathFree(resolved);
	return ret;
}

/*
 * Rewrites path (normalized in place) through the overlays from index on;
 * out is scratch of maxPath bytes. Returns 0, the first overlay used plus
 * one, 65 when index is past the table, or an error.
 */
int sceFiosKernelOverlayResolve(SceFiosKernelResolveParam *param, int index, char *out, char *path)
{
	SceFiosKernelOverlayTable table;
	SceFiosOverlay *overlay;
	SceUInt64 newest = 0, time;
	SceUInt32 sdkVersion;
	char *resolved;
	char *p;
	size_t len, newLen, rest;
	unsigned short dstLen, srcLen;
	int first, merged, lastMerged, newerFrom;
	int srcSep, dropSep, found, ret, n, type;
	signed char c, last;
	SceUID caller;

	resolved = fiosPathAlloc();
	if (path == NULL) {
		ret = SCE_FIOS_ERROR_BAD_PTR;
		goto out;
	}
	if ((unsigned int)param->resolveFlag > 1) {
		ret = SCE_FIOS_ERROR_BAD_RESOLVE_TYPE;
		goto out;
	}
	if (param->maxPath == 0) {
		ret = SCE_FIOS_ERROR_PATH_TOO_LONG;
		goto out;
	}
	for (p = path; *p != '\0'; p++) {
		if (p == path + param->maxPath - 1) {
			ret = SCE_FIOS_ERROR_PATH_TOO_LONG;
			goto out;
		}
	}
	caller = ksceKernelGetProcessIdFromTLS();
	if (!fiosCallerIsPrivileged(caller) && param->pid != -1 && caller != param->pid) {
		ret = 0;
		goto out;
	}

	memset(out, 0, FIOS_PATH_MAX);
	len = sceFiosKernelPathNormalizeDefault(out, path, param->maxPath);
	if (len >= param->maxPath) {
		ret = SCE_FIOS_ERROR_BAD_PATH;
		goto out;
	}
	memcpy(path, out, len + 1);

	sceFiosKernelRwlockReadLock(&g_fiosOverlayLock);
	table = sceFiosKernelOverlayGetTable(param->pid);
	n = *table.count;
	if (index >= n) {
		ret = 0x41;
		sceFiosKernelRwlockReadUnlock(&g_fiosOverlayLock);
		goto out;
	}

	first = SCE_FIOS_OVERLAY_MAX_OVERLAYS;
	lastMerged = 0;
	newerFrom = SCE_FIOS_OVERLAY_MAX_OVERLAYS;
	merged = 0;
	ret = 0;
	do {
		overlay = &table.overlays[index];
		srcLen = overlay->src_len;
		dstLen = overlay->dst_len;
		time = 0;
		if (overlay->pid != -1 && param->pid != overlay->pid)
			goto next;
		if (overlay->order < param->minOrder || overlay->order > param->maxOrder)
			goto next;
		if (strncmp(path, overlay->dst, dstLen) != 0)
			goto next;

		/* the destination must end at a component boundary of path */
		if (dstLen == 0) {
			last = 1;
			c = path[0];
		} else {
			last = fiosIsSeparator(path[dstLen - 1]);
			c = path[dstLen];
		}
		if (c != '/' && c != ':' && c != '\0' && c != '\\' && !last)
			goto next;

		if (srcLen == 0) {
			dropSep = 0;
			srcSep = 0;
		} else {
			last = overlay->src[srcLen - 1];
			if (last == ':' || last == '/') {
				dropSep = last == c;
				srcSep = 1;
			} else {
				srcSep = last == '\\';
				dropSep = last == c && srcSep;
			}
		}
		memcpy(out, overlay->src, srcLen);
		if (c != '/' && c != '\\' && c != '\0' && !srcSep)
			out[srcLen++] = '/';
		rest = len - dstLen;
		if (dropSep) {
			dstLen++;
			rest--;
		}
		newLen = rest + srcLen;
		if (newLen >= param->maxPath) {
			ret = SCE_FIOS_ERROR_PATH_TOO_LONG;
			goto unlock;
		}
		memcpy(out + srcLen, path + dstLen, rest + 1);
		if (resolved == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			goto unlock;
		}
		memcpy(resolved, out, newLen + 1);

		type = overlay->type;
		switch (type) {
		case SCE_FIOS_OVERLAY_TYPE_OPAQUE:
			merged = 0;
			break;
		case SCE_FIOS_OVERLAY_TYPE_WRITABLE:
			if (param->resolveFlag == 1) {
				merged = 0;
				break;
			}
			if (fiosOverlayResolveTime(param, index + 1, out, resolved, &time) < 0)
				goto next;
			merged = 1;
			break;
		default:
			if (fiosOverlayResolveTime(param, index + 1, out, resolved, &time) < 0)
				goto next;
			if (type == SCE_FIOS_OVERLAY_TYPE_TRANSLUCENT) {
				merged = 1;
				break;
			}
			if (type != SCE_FIOS_OVERLAY_TYPE_NEWER)
				goto next;
			if (merged != 0) {
				ret = SceProcessmgrForDriver_D141C076(param->pid, &sdkVersion);
				if ((ret == 0 && sdkVersion < 0x3500000) || index < newerFrom) {
					merged = lastMerged;
					if (merged == 0) {
						merged = 1;
						break;
					}
					goto compare;
				}
			}
			found = fiosOverlayResolveTime(param, index + 1, out, path, &newest);
			newerFrom = found > 0 ? found - 1 : SCE_FIOS_OVERLAY_MAX_OVERLAYS;
			merged = found >= 0;
			if (!merged) {
				merged = 1;
				break;
			}
compare:
			/* the newer of the two stays */
			if (newest != 0 && newest >= time) {
				lastMerged = merged;
				goto next;
			}
			break;
		}

		if (index <= first)
			first = index;
		memcpy(path, resolved, newLen + 1);
		len = newLen;
		newest = time;
		lastMerged = merged;
next:
		index++;
		n = *table.count;
	} while (index < n);
	sceFiosKernelRwlockReadUnlock(&g_fiosOverlayLock);
	if (ret != 0)
		goto out;
	ret = first != 0 ? first + 1 : 0;
	goto out;

unlock:
	sceFiosKernelRwlockReadUnlock(&g_fiosOverlayLock);
out:
	if (resolved != NULL)
		fiosPathFree(resolved);
	return ret;
}

/* runs on the extended kernel stack */
int sceFiosKernelOverlayResolveCallback(void *argument)
{
	SceFiosKernelResolveArgs *args = (SceFiosKernelResolveArgs *)argument;
	SceFiosKernelResolveParam *param = args->param;
	const char *inPath = args->inPath;
	char *outPath = args->outPath;
	char *scratch;
	SceSize len;
	int ret;

	if (inPath == NULL || outPath == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	if (param->maxPath > FIOS_PATH_MAX)
		return SCE_FIOS_ERROR_BAD_SIZE;
	if (param->maxPath == 0)
		return SCE_FIOS_ERROR_PATH_TOO_LONG;
	for (len = 0; inPath[len] != '\0'; )
		if (++len == param->maxPath)
			return SCE_FIOS_ERROR_PATH_TOO_LONG;
	if (param->maxOrder < param->minOrder)
		return SCE_FIOS_ERROR_BAD_ORDER;

	memcpy(outPath, inPath, len + 1);
	scratch = fiosPathAlloc();
	if (scratch == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = sceFiosKernelOverlayResolve(param, 0, scratch, outPath);
	fiosPathFree(scratch);
	return ret < 0 ? ret : 0;
}
