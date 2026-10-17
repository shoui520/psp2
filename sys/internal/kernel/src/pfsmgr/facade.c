/*
 * ScePfsFacadeForKernel: what SceIofilemgr calls for PFS files. A large read
 * maps the file's blocks (ioctl 0x4420), lets the crypt engine decrypt them
 * into the caller's buffer and then reads through the mapping, which PFS
 * finds in a thread-local slot.
 */

#include <psp2kern/kernel/threadmgr.h>
#include <iofilemgr/iofilemgr.h>
#include "pfsmgr_internal.h"

#define SCE_PFS_ERROR_FACADE_UNSUPPORTED	0x8014231c	/* name unknown */

#define PFS_MAP			0x4420	/* name unknown */
#define PFS_UNMAP		0x4422	/* name unknown */

#define PFS_FACADE_CHUNK	0x8000	/* name unknown: what reads are cut at */
#define PFS_FACADE_MAP_MAX	0x40000	/* name unknown: larger reads are mapped */
#define PFS_FACADE_MAP_BLOCKS	4	/* name unknown: blocks one mapping holds */
#define PFS_FACADE_MAPS		2	/* name unknown: mappings in flight */

/* the calling thread's I/O weight (name unknown) */
static inline SceInt32 pfsIoWeight(void)
{
	IoRequest *req = SceIofilemgrForDriver_12F8D58E();

	if (req == NULL)
		return 0x100;
	if (req->mntOpt < 0)
		return req->mntOpt;
	return (SceUInt16)req->mntOpt;
}

/* the calling thread's I/O priority (name unknown) */
static inline SceInt32 pfsIoPriority(void)
{
	IoRequest *req = SceIofilemgrForDriver_12F8D58E();

	if (req == NULL)
		return 0xf;
	return req->priority;
}

static inline int pfsFacadeUnmap(SceUID fd, ScePfsBlockRequest *req)	/* name unknown */
{
	IofilemgrIoctlParam param;

	param.fd = fd;
	param.cmd = PFS_UNMAP;
	param.indata = NULL;
	param.inlen = 0;
	param.outdata = req;
	param.outlen = sizeof(*req);
	return vfsIoctl(&param);
}

/* read through up to two mappings, decrypted by the crypt engine meanwhile */
int pfsFacadeReadMapped(SceUID fd, void *buf, SceSize nbyte, SceOff offset, int mayYield)
{
	IofilemgrIoctlParam ioctl;
	ScePfsMapArg arg;
	ScePfsCryptJob jobs[PFS_FACADE_MAPS];
	ScePfsFileKey key;
	ScePfsBlockRequest reqs[PFS_FACADE_MAPS];
	IofilemgrPreadParam pread;
	SceSize done = 0;
	int maps = 0, total = 0;
	int i, ret, err;

	key.klicensee = NULL;
	if (nbyte == 0)
		goto mapped;
	for (i = 0;; i++) {
		arg.fd = fd;
		arg.buf = (char *)buf + done;
		arg.length = nbyte - done;
		arg.count = PFS_FACADE_MAP_BLOCKS;
		arg.offset = offset + done;
		arg.icvs = NULL;
		arg.plain = NULL;
		arg.disk = NULL;
		arg.key = &key;
		ret = pfsCryptBufferAlloc(pfsIoWeight(), pfsIoPriority(), arg.count, &arg.plain,
					  &arg.disk, &arg.icvs, 1);
		if (ret != 0)
			goto failed;
		ioctl.fd = fd;
		ioctl.cmd = PFS_MAP;
		ioctl.indata = &arg;
		ioctl.inlen = sizeof(arg);
		ioctl.outdata = &reqs[i];
		ioctl.outlen = sizeof(reqs[i]);
		ret = vfsIoctl(&ioctl);
		if (ret != 0) {
			pfsCryptBufferFree(pfsIoWeight(), pfsIoPriority(), arg.count, arg.plain,
					   arg.disk, arg.icvs);
			goto failed;
		}
		if (reqs[i].length == 0) {
			ret = pfsCryptBufferFree(pfsIoWeight(), pfsIoPriority(), arg.count, arg.plain,
						 arg.disk, arg.icvs);
			if (ret != 0)
				goto failed;
			maps = i;
			goto mapped;
		}
		jobs[i].thread = ksceKernelGetThreadId();
		jobs[i].waiting = 0;
		jobs[i].req = &reqs[i];
		jobs[i].result = 0;
		ret = pfsCryptEngineSubmit(&jobs[i]);
		if (ret != 0)
			goto failed;
		done += reqs[i].length;
		if (i == 1) {
			maps = 2;
			goto mapped;
		}
		if (done >= nbyte) {
			maps = 1;
			goto mapped;
		}
	}

failed:
	if (i == 1) {
		pfsCryptEngineCancel(&jobs[0]);
		pfsFacadeUnmap(fd, &reqs[0]);
		pfsBlockRequestFree(&reqs[0]);
	}
	pfsFileKeyClear(&key);
	return ret;

mapped:
	if (mayYield) {
		ret = SceIofilemgrForDriver_15C17487(pfsIoWeight());
		if (ret != 0) {
			for (i = 0; i < maps; i++)
				pfsBlockRequestFree(&reqs[i]);
			pfsFileKeyClear(&key);
			return ret;
		}
	}

	ret = 0;
	for (i = 0; i < maps; i++) {
		err = pfsCryptEngineWait(&jobs[i]);
		if (err == 0)
			err = jobs[i].result;
		if (err != 0) {
			pfsFacadeUnmap(fd, &reqs[i]);
			pfsBlockRequestFree(&reqs[i]);
			ret = err;
			goto cancel;
		}
		pread.fd = fd;
		pread.data = reqs[i].dst;
		pread.size = reqs[i].length;
		pread.padding = 0;
		pread.offset = reqs[i].offset;
		*pfsThreadMapping() = &reqs[i];
		ret = vfsPread(&pread);
		*pfsThreadMapping() = NULL;
		if (ret < 0) {
			pfsFacadeUnmap(fd, &reqs[i]);
			pfsBlockRequestFree(&reqs[i]);
			goto cancel;
		}
		total += ret;
		ret = pfsFacadeUnmap(fd, &reqs[i]);
		if (ret < 0) {
			pfsBlockRequestFree(&reqs[i]);
			goto cancel;
		}
		pfsBlockRequestFree(&reqs[i]);
		ret = total;
	}
	pfsFileKeyClear(&key);
	return ret;

cancel:
	i++;
	if (i < maps) {
		pfsCryptEngineCancel(&jobs[i]);
		pfsFacadeUnmap(fd, &reqs[i]);
		pfsBlockRequestFree(&reqs[i]);
	}
	pfsFileKeyClear(&key);
	return ret;
}

/* pread: the head up to a chunk boundary is read as usual, a large rest mapped */
int ScePfsFacadeForKernel_4238D2D2(IofilemgrPreadParam *param)
{
	IofilemgrPreadParam head;
	ScePfsBlockRequest none;
	SceOff next, pos;
	SceSize rest, mid;
	char *buf;
	SceOff done;
	int ret;

	head.fd = param->fd;
	head.data = param->data;
	none.op = PFS_BLOCK_NONE;
	next = param->offset + (PFS_FACADE_CHUNK - param->offset % PFS_FACADE_CHUNK) %
	       PFS_FACADE_CHUNK;
	if (next == param->offset)
		head.size = PFS_FACADE_CHUNK;
	else
		head.size = next - param->offset;
	if (head.size >= param->size)
		head.size = param->size;
	head.padding = 0;
	head.offset = param->offset;
	*pfsThreadMapping() = &none;
	ret = vfsPread(&head);
	*pfsThreadMapping() = NULL;
	if (ret == 0)
		return 0;
	if (ret > 0)
		done = ret;
	else if (ret == SCE_PFS_ERROR_PASS_THROUGH)
		done = 0;
	else
		return ret;

	pos = param->offset + done;
	buf = (char *)param->data + done;
	rest = param->size - done;
	next = pos + rest;
	mid = 0;
	if (next + (PFS_FACADE_CHUNK - next % PFS_FACADE_CHUNK) % PFS_FACADE_CHUNK - pos >
	    PFS_FACADE_MAP_MAX) {
		ret = pfsFacadeReadMapped(param->fd, buf,
					  (PFS_FACADE_CHUNK - pos % PFS_FACADE_CHUNK) %
					  PFS_FACADE_CHUNK, pos, 0);
		if (ret < 0)
			return ret;
		pos += ret;
		buf += ret;
		rest -= ret;
		mid = ret;
	}
	ret = pfsFacadeReadMapped(param->fd, buf, rest, pos, 1);
	if (ret >= 0)
		ret += done + mid;
	return ret;
}

int ScePfsFacadeForKernel_BD5B21F6(void)
{
	return SCE_PFS_ERROR_FACADE_UNSUPPORTED;
}

int ScePfsFacadeForKernel_58E643C5(IofilemgrPwriteParam *param)
{
	return vfsPwrite(param);
}

int ScePfsFacadeForKernel_CB622FFE(IofilemgrWriteParam *param)
{
	return vfsWrite(param);
}

int ScePfsFacadeForKernel_7F9F4E49(IofilemgrChstatParam *param)
{
	return vfsChstat(param);
}

int ScePfsFacadeForKernel_04352D52(IofilemgrChstatByFdParam *param)
{
	return vfsChstatByFd(param);
}
