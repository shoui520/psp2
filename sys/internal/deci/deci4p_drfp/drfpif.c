/*
 * SceDeci4pDrfp: one function per DRFP request. Each builds the request in a
 * packet buffer, exchanges it with the host and converts the host's result
 * (Sony's drfpif.c).
 */

#include <stddef.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <kernel/error.h>
#include "drfp_internal.h"

/* the host's result as an SCE error */
static inline __attribute__((always_inline)) int drfpConvertError(int result)
{
	switch (result) {
	case 0:
		return 0;
	case DRFP_HOST_ENOENT:
		return SCE_ERROR_ERRNO_ENOENT;
	case DRFP_HOST_EIO:
		return SCE_ERROR_ERRNO_EIO;
	case DRFP_HOST_EBADF:
		return SCE_ERROR_ERRNO_ENODEV;
	case DRFP_HOST_ENOMEM:
		return SCE_ERROR_ERRNO_ENOMEM;
	case DRFP_HOST_EACCES:
		return SCE_ERROR_ERRNO_EACCES;
	case DRFP_HOST_EBUSY:
		return SCE_ERROR_ERRNO_EBUSY;
	case DRFP_HOST_EEXIST:
		return SCE_ERROR_ERRNO_EEXIST;
	case DRFP_HOST_ENOTDIR:
		return SCE_ERROR_ERRNO_ENOTDIR;
	case DRFP_HOST_EISDIR:
		return SCE_ERROR_ERRNO_EISDIR;
	case DRFP_HOST_EINVAL:
		return SCE_ERROR_ERRNO_EINVAL;
	case DRFP_HOST_EMFILE:
		return SCE_ERROR_ERRNO_EMFILE;
	case DRFP_HOST_ENOSPC:
		return SCE_ERROR_ERRNO_ENOSPC;
	case DRFP_HOST_EROFS:
		return SCE_ERROR_ERRNO_EROFS;
	case DRFP_HOST_ENAMETOOLONG:
		return SCE_ERROR_ERRNO_ENAMETOOLONG;
	case DRFP_HOST_ENOTEMPTY:
		return SCE_ERROR_ERRNO_ENOTEMPTY;
	case DRFP_HOST_ENOTSUP:
		return SCE_ERROR_ERRNO_ENOTSUP;
	default:
		SceDebugForDriver_391B74B7("DRFP: unknown error code %d\n", result);
		return SCE_ERROR_ERRNO_ENOTSUP;
	}
}

int sceDeci4pDrfpCreate(const char *path, int mode, Deci4pFsStat *stat)
{
	DrfpOpenPacket *pkt;
	SceSize len, size;
	int ret;

	len = strnlen(path, DRFP_PATH_MAX);
	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	size = offsetof(DrfpOpenPacket, req.name.str) + DRFP_STRING_PAD(len);
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->h.code = DRFP_CODE_CREATE;
	pkt->req.mode = DRFP_MODE_FILE_DEFAULT;
	pkt->req.name.len = len + 1;
	memcpy(pkt->req.name.str, path, len + 1);
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 248, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	if (pkt->h.code == DRFP_CODE_UNSUPPORTED) {
		DRFP_PRINTF(2, 262, "CREATE_RES: not supported\n");
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return SCE_ERROR_ERRNO_ENOTSUP;
	}
	if (pkt->res.result == 0) {
		ret = 0;
	} else {
		DRFP_PRINTF(2, 271, "CREATE_RES: ERROR fname = %s, result = 0x%08x\n", path,
			    pkt->res.result);
		ret = drfpConvertError(pkt->res.result);
	}
	if (ret == 0)
		*stat = pkt->res.stat;
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpOpen(const char *path, int flags, int mode, Deci4pFsStat *stat)
{
	DrfpOpenPacket *pkt;
	SceSize len, size;
	int ret;

	len = strnlen(path, DRFP_PATH_MAX);
	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	size = offsetof(DrfpOpenPacket, req.name.str) + DRFP_STRING_PAD(len);
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->h.code = DRFP_CODE_OPEN;
	pkt->req.mode = DRFP_MODE_FILE_DEFAULT;
	pkt->req.flags = flags;
	pkt->req.name.len = len + 1;
	memcpy(pkt->req.name.str, path, len + 1);
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 357, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	if (pkt->res.result == 0) {
		ret = 0;
	} else {
		DRFP_PRINTF(2, 371, "OPEN_RES: ERROR fname = %s, result = 0x%08x\n", path,
			    pkt->res.result);
		ret = drfpConvertError(pkt->res.result);
	}
	if (ret == 0) {
		*stat = pkt->res.stat;
		ret = pkt->res.fd;
	}
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpRead(int fd, void *buf, SceOff offset, SceSize size)
{
	DrfpReadPacket *pkt;
	DrfpReadInfo info;
	SceSize total, chunk;
	int ret;

	total = 0;
	if (size == 0)
		return 0;
	pkt = drfpGetBuffer(DRFP_BUFFER_READ)->base;
	total = 0;
	for (;;) {
		chunk = size - total;
		memset(pkt, 0, sizeof(*pkt));
		if (chunk > DRFP_READ_CHUNK_MAX)
			chunk = DRFP_READ_CHUNK_MAX;
		info.nfrag = 0;
		info.hostResult = 0;
		info.received = 0;
		pkt->h.code = DRFP_CODE_READ;
		pkt->fd = fd;
		pkt->padding = 0;
		pkt->offset = offset;
		pkt->size = chunk;
		info.buf = (char *)buf + total;
		info.size = chunk;
		ret = sceDeci4pDrfpSend(pkt, sizeof(*pkt), &info);
		if (ret != 0) {
			DRFP_PRINTF(0xf, 449, "READ: send error - 0x%08x\n", ret);
			drfpReleaseBuffer(DRFP_BUFFER_READ);
			return SCE_ERROR_ERRNO_EIO;
		}
		if (info.hostResult != 0)
			break;
		offset += info.received;
		total += info.received;
		if (info.received < chunk || total >= size) {
			drfpReleaseBuffer(DRFP_BUFFER_READ);
			return total;
		}
	}
	DRFP_PRINTF(0xf, 455, "READ: error - 0x%08x\n", info.hostResult);
	ret = drfpConvertError(info.hostResult);
	drfpReleaseBuffer(DRFP_BUFFER_READ);
	return ret;
}

int sceDeci4pDrfpWrite(int fd, const void *buf, SceOff offset, SceSize size)
{
	DrfpWritePacket *pkt;
	DrfpWriteInfo info;
	SceSize total, remaining, chunk;
	int ret, last;

	pkt = drfpGetBuffer(DRFP_BUFFER_WRITE)->base;
	if (pkt == NULL)
		return -1;
	total = 0;
	while (total < size) {
		remaining = size - total;
		memset(pkt, 0, sizeof(*pkt));
		chunk = remaining;
		if (chunk > DRFP_WRITE_CHUNK_MAX)
			chunk = DRFP_WRITE_CHUNK_MAX;
		info.state = 0;
		pkt->fd = fd;
		pkt->h.code = DRFP_CODE_WRITE;
		pkt->flags = DRFP_WRITE_MORE;
		pkt->padding0 = 0;
		pkt->padding1 = 0;
		pkt->offset = offset;
		pkt->size = chunk;
		if ((total & (DRFP_READ_CHUNK_MAX - 1)) == 0)
			pkt->flags = DRFP_WRITE_MORE | DRFP_WRITE_SYNC;
		info.size = chunk;
		last = remaining <= DRFP_WRITE_CHUNK_MAX;
		if (last)
			pkt->flags = 0;
		info.buf = (const char *)buf + total;
		ret = sceDeci4pDrfpSend(pkt, chunk + sizeof(*pkt), &info);
		if (ret != 0) {
			DRFP_PRINTF(0xf, 556, "WRITE: send error - 0x%08x\n", ret);
			ret = SCE_ERROR_ERRNO_EIO;
			goto out;
		}
		if (last && pkt->result != 0) {
			DRFP_PRINTF(0xf, 575, "WRITE: error - 0x%08x\n", pkt->result);
			ret = drfpConvertError(pkt->result);
			goto out;
		}
		offset += chunk;
		total += chunk;
	}
	ret = total;
out:
	drfpReleaseBuffer(DRFP_BUFFER_WRITE);
	return ret;
}

int sceDeci4pDrfpFstat(int fd, Deci4pFsStat *stat)
{
	DrfpFdPacket *pkt;
	SceSize size = offsetof(DrfpFdPacket, fd) + sizeof(pkt->fd);
	int ret;

	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->fd = fd;
	pkt->h.code = DRFP_CODE_FSTAT;
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 620, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	ret = drfpConvertError(pkt->res.result);
	if (ret == 0)
		*stat = pkt->res.stat;
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpStat(const char *path, Deci4pFsStat *stat)
{
	DrfpPathPacket *pkt;
	SceSize len, size;
	int ret;

	len = strnlen(path, DRFP_PATH_MAX);
	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	size = offsetof(DrfpPathPacket, name.str) + DRFP_STRING_PAD(len);
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->h.code = DRFP_CODE_STAT;
	pkt->name.len = len + 1;
	memcpy(pkt->name.str, path, len + 1);
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		/* a lookup without the host is common: stay quiet */
		if (ret != SCE_ERROR_ERRNO_ENODEV)
			DRFP_PRINTF(0xf, 693, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	ret = drfpConvertError(pkt->res.result);
	if (ret == 0)
		*stat = pkt->res.stat;
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpFchstat(int fd, SceUInt32 bits, const Deci4pFsStat *stat)
{
	DrfpFchstatPacket *pkt;
	int ret;

	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, sizeof(*pkt));
	pkt->fd = fd;
	pkt->bits = bits;
	pkt->h.code = DRFP_CODE_FCHSTAT;
	pkt->stat = *stat;
	ret = sceDeci4pDrfpSend(pkt, sizeof(*pkt), NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 760, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	if (pkt->h.code == DRFP_CODE_UNSUPPORTED) {
		DRFP_PRINTF(2, 774, "DRFP FCHSTAT_RES: not supported\n");
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return SCE_ERROR_ERRNO_ENOTSUP;
	}
	ret = drfpConvertError(pkt->result);
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpChstat(const char *path, SceUInt32 bits, const Deci4pFsStat *stat)
{
	DrfpChstatPacket *pkt;
	SceSize len, size;
	int ret;

	len = strnlen(path, DRFP_PATH_MAX);
	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	size = offsetof(DrfpChstatPacket, name.str) + DRFP_STRING_PAD(len);
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->h.code = DRFP_CODE_CHSTAT;
	pkt->name.len = len + 1;
	memcpy(pkt->name.str, path, len + 1);
	pkt->bits = bits;
	pkt->stat = *stat;
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 833, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	if (pkt->h.code == DRFP_CODE_UNSUPPORTED) {
		DRFP_PRINTF(2, 847, "DRFP CHSTAT_RES: not supported\n");
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return SCE_ERROR_ERRNO_ENOTSUP;
	}
	ret = drfpConvertError(pkt->result);
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpTranslatePath(const char *base, const char *path, SceSize size, char *out,
			       SceSize *outLen)
{
	DrfpTwoPathPacket *pkt;
	DrfpString *second, *copied;
	SceSize len, len2, pad, pktSize, n;
	int ret;

	len = strnlen(base, DRFP_PATH_MAX);
	len2 = strnlen(path, DRFP_PATH_MAX);
	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	pad = DRFP_STRING_PAD(len);
	pktSize = DRFP_STRING_PAD(len2) + pad + offsetof(DrfpTwoPathPacket, name.str) +
		  sizeof(DrfpString);
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, pktSize);
	pkt->name.len = len + 1;
	pkt->h.code = DRFP_CODE_TRANSLATE_PATH;
	second = (DrfpString *)(pkt->name.str + pad);
	copied = (DrfpString *)((char *)memcpy(pkt->name.str, base, len + 1) + pad);
	copied->len = len2 + 1;
	memcpy(second->str, path, len2 + 1);
	ret = sceDeci4pDrfpSend(pkt, pktSize, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 926, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	ret = drfpConvertError(pkt->res.result);
	if (ret == 0) {
		n = pkt->res.path.len;
		if (n - 1 >= DRFP_PATH_MAX || pkt->res.path.str[n - 1] != '\0') {
			DRFP_PRINTF(0xf, 957, "new path is wrong\n");
			drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
			return -1;
		}
		if (n > size)
			n = size;
		memcpy(out, pkt->res.path.str, n);
		*outLen = n;
	}
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpClose(int fd)
{
	DrfpFdPacket *pkt;
	SceSize size = offsetof(DrfpFdPacket, fd) + sizeof(pkt->fd);
	int ret;

	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->fd = fd;
	pkt->h.code = DRFP_CODE_CLOSE;
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 1009, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	ret = drfpConvertError(pkt->res.result);
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpMkdir(const char *path, int mode, Deci4pFsStat *stat)
{
	DrfpMkdirPacket *pkt;
	SceSize len, size, resSize;
	int ret;

	len = strnlen(path, DRFP_PATH_MAX);
	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	size = offsetof(DrfpMkdirPacket, req.name.str) + DRFP_STRING_PAD(len);
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->h.code = DRFP_CODE_MKDIR;
	pkt->req.mode = DRFP_MODE_DIR_DEFAULT;
	pkt->req.name.len = len + 1;
	memcpy(pkt->req.name.str, path, len + 1);
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 1126, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	resSize = SceDeci4pDfMgrForDebugger_CACAB5F9(&pkt->h.hdr);
	ret = drfpConvertError(pkt->res.result);
	if (ret == 0 && stat != NULL) {
		/* old hosts answer without the new directory's information */
		if (resSize <= offsetof(DrfpMkdirPacket, res.padding)) {
			drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
			return sceDeci4pDrfpStat(path, stat);
		}
		*stat = pkt->res.stat;
	}
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpRmdir(const char *path)
{
	DrfpPathPacket *pkt;
	SceSize len, size;
	int ret;

	len = strnlen(path, DRFP_PATH_MAX);
	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	size = offsetof(DrfpPathPacket, name.str) + DRFP_STRING_PAD(len);
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->h.code = DRFP_CODE_RMDIR;
	pkt->name.len = len + 1;
	memcpy(pkt->name.str, path, len + 1);
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 1210, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	ret = drfpConvertError(pkt->res.result);
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpRemove(const char *path)
{
	DrfpPathPacket *pkt;
	SceSize len, size;
	int ret;

	len = strnlen(path, DRFP_PATH_MAX);
	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	size = offsetof(DrfpPathPacket, name.str) + DRFP_STRING_PAD(len);
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->h.code = DRFP_CODE_REMOVE;
	pkt->name.len = len + 1;
	memcpy(pkt->name.str, path, len + 1);
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 1272, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	ret = drfpConvertError(pkt->res.result);
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpRename(const char *from, const char *to, Deci4pFsStat *stat)
{
	DrfpTwoPathPacket *pkt;
	DrfpString *second, *copied;
	SceSize len, len2, pad, size, resSize;
	int ret;

	len = strnlen(from, DRFP_PATH_MAX);
	len2 = strnlen(to, DRFP_PATH_MAX);
	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	pad = DRFP_STRING_PAD(len);
	size = DRFP_STRING_PAD(len2) + pad + offsetof(DrfpTwoPathPacket, name.str) +
	       sizeof(DrfpString);
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->name.len = len + 1;
	pkt->h.code = DRFP_CODE_RENAME;
	second = (DrfpString *)(pkt->name.str + pad);
	copied = (DrfpString *)((char *)memcpy(pkt->name.str, from, len + 1) + pad);
	copied->len = len2 + 1;
	memcpy(second->str, to, len2 + 1);
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 1362, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	resSize = SceDeci4pDfMgrForDebugger_CACAB5F9(&pkt->h.hdr);
	ret = drfpConvertError(pkt->res.result);
	if (ret == 0 && stat != NULL) {
		/* old hosts answer without the renamed file's information */
		if (resSize <= offsetof(DrfpTwoPathPacket, res.padding)) {
			drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
			return sceDeci4pDrfpStat(to, stat);
		}
		*stat = pkt->res.stat;
	}
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpDopen(const char *path)
{
	DrfpPathPacket *pkt;
	SceSize len, size;
	int ret;

	len = strnlen(path, DRFP_PATH_MAX);
	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	size = offsetof(DrfpPathPacket, name.str) + DRFP_STRING_PAD(len);
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->h.code = DRFP_CODE_DOPEN;
	pkt->name.len = len + 1;
	memcpy(pkt->name.str, path, len + 1);
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 1446, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	ret = drfpConvertError(pkt->res.result);
	if (ret == 0)
		ret = pkt->res.fd;
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpDclose(int fd)
{
	DrfpFdPacket *pkt;
	SceSize size = offsetof(DrfpFdPacket, fd) + sizeof(pkt->fd);
	int ret;

	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->fd = fd;
	pkt->h.code = DRFP_CODE_DCLOSE;
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 1504, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	ret = drfpConvertError(pkt->res.result);
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

/* the entries go to a heap block the caller frees; returns their count */
int sceDeci4pDrfpDread(int fd, DrfpDirent **ent)
{
	DrfpDreadPacket *pkt;
	SceSize size = offsetof(DrfpDreadPacket, req.size) + sizeof(pkt->req.size);
	void *p;
	int ret;

	pkt = drfpGetBuffer(DRFP_BUFFER_DREAD)->base;
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->req.fd = fd;
	pkt->h.code = DRFP_CODE_DREAD;
	pkt->req.count = 1;
	pkt->req.size = DRFP_DREAD_SIZE;
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 1560, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_DREAD);
		return ret;
	}
	ret = drfpConvertError(pkt->res.result);
	if (ret == 0 && pkt->res.count != 0) {
		p = SceSysmemForDriver_7B4CB60A(drfpGetHeap(), pkt->res.size);
		*ent = p;
		if (p != NULL) {
			memcpy(p, pkt->res.ent, pkt->res.size);
			ret = pkt->res.count;
		} else {
			ret = SCE_ERROR_ERRNO_ENOMEM;
		}
	}
	drfpReleaseBuffer(DRFP_BUFFER_DREAD);
	return ret;
}

int sceDeci4pDrfpFtruncate(int fd, SceOff length)
{
	DrfpFtruncatePacket *pkt;
	int ret;

	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, sizeof(*pkt));
	pkt->length = length;
	pkt->fd = fd;
	pkt->h.code = DRFP_CODE_FTRUNCATE;
	ret = sceDeci4pDrfpSend(pkt, sizeof(*pkt), NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 1628, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	ret = drfpConvertError(pkt->result);
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}

int sceDeci4pDrfpTruncate(const char *path, SceOff length)
{
	DrfpTruncatePacket *pkt;
	SceSize len, size;
	int ret;

	len = strnlen(path, DRFP_PATH_MAX);
	pkt = drfpGetBuffer(DRFP_BUFFER_GENERAL)->base;
	size = offsetof(DrfpTruncatePacket, name.str) + DRFP_STRING_PAD(len);
	if (pkt == NULL)
		return -1;
	memset(pkt, 0, size);
	pkt->h.code = DRFP_CODE_TRUNCATE;
	pkt->name.len = len + 1;
	memcpy(pkt->name.str, path, len + 1);
	pkt->length = length;
	ret = sceDeci4pDrfpSend(pkt, size, NULL);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 1689, "sceDeci4pDrfpSend failed 0x%08X\n", ret);
		drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
		return ret;
	}
	ret = drfpConvertError(pkt->result);
	drfpReleaseBuffer(DRFP_BUFFER_GENERAL);
	return ret;
}
