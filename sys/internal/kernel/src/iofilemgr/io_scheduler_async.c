/*
 * SceIofilemgr: the asynchronous I/O calls (Sony's io_scheduler_async.c).
 *
 * Each call makes an event object that holds its request, hands the request
 * to the dispatcher and returns the event's UID. The scheduler runs the
 * request later and sets the event; sceIoComplete then takes the result and
 * destroys the event. Paths are copied to the scheduler heap and user buffers
 * mapped into the kernel, since the caller's memory may be gone by then.
 */

#include <excpmgr/excpmgr_cpu.h>
#include <intrmgr/syscall.h>
#include <sysmem/sysmem.h>
#include "iofilemgr_internal.h"

SceClass g_ioAsyncEventClass;

/* 4.00 builds this file with _FORTIFY_SOURCE; Sony's heap declaration gives the size it checks */
#if FW_VERSION >= 0x04000000
void *SceSysmemForDriver_7B4CB60A(SceUID uid, SceSize size) __attribute__((alloc_size(2)));
#define strncpy(dst, src, len)	__builtin___strncpy_chk(dst, src, len, __builtin_object_size(dst, 1))
#endif

/* the processes whose events come from the kernel heap (names unknown) */

/* an event object's UID type (name unknown) */
#define IO_ASYNC_EVENT_UID_TYPE		4

/* the bit TPIDRURO has while the kernel works for a user thread's syscall */
#define IO_ASYNC_FROM_USER(tpidruro)	(((tpidruro) >> 16) & 1)

/* the scheduler type that cannot read asynchronously (name unknown) */
#define IO_SCHED_TYPE_SYNC_READ_ONLY	0x400

/* the class's object callbacks (names unknown) */
static int sceIoAsyncEventClassCreate(void *item)
{
	return 0;
}

static int sceIoAsyncEventClassDestroy(void *item)
{
	return 0;
}

/* name unknown */
int sceIoAsyncInitialize(void)
{
	return sceKernelUIDInheritedEventClassInit(&g_ioAsyncEventClass,
						   "SceUIDIoAsyncEventClass", sizeof(IoAsyncEvent),
						   sceIoAsyncEventClassCreate,
						   sceIoAsyncEventClassDestroy);
}

SceUID sceIoAsyncEventCreate(IoAsyncEvent **event)
{
	IoAsyncEvent *obj = NULL;
	UIDCreateOpt opt;
	SceUID pid, uid;
	int ret, closed;

	pid = ksceKernelGetProcessIdFromTLS();
	switch (pid) {
	case SCE_KERNEL_PROCESS_ID:
	case SCE_KERNEL_DUMMY_PROCESS_GAME_ID:
	case SCE_KERNEL_DUMMY_PROCESS_SYSTEM_ID:
		uid = SceSysmemForDriver_56A13E90(&g_ioAsyncEventClass, "SceIoAsyncEvent",
					   (SceObjectBase **)&obj);
		break;
	default:
		opt.flags = UID_CREATE_OPT_PROCESS_HEAP | UID_CREATE_OPT_TYPE;
		opt.object = NULL;
		opt.heapGuid = 0;
		opt.pid = pid;
		opt.heapKind = 0;
		opt.type = 0;
		opt.attr = 0;
		opt.ownerPid = 0;
		opt.cls = NULL;
		if (SceThreadmgrForDriver_1096042E(0) != 0)
			opt.heapKind = 1;
		else
			opt.heapKind = 0;
		opt.type = IO_ASYNC_EVENT_UID_TYPE;
		uid = sceGUIDKernelCreateWithOpt(&g_ioAsyncEventClass, "SceIoAsyncEvent",
						  (SceGUIDKernelCreateOpt *)&opt,
						  (SceObjectBase **)&obj);
		break;
	}
	if (uid < 0)
		return uid;

	ret = sceKernelEventObjectInit(uid, 0x100, 0, 0, 0);
	if (ret < 0) {
		closed = sceGUIDClose(uid);
		SCE_DBG_ASSERT(closed == 0, 117);
		return ret;
	}
	obj->request.asyncEvent = uid;
	*event = obj;
	return uid;
}

int sceIoAsyncEventDestroy(SceUID uid)
{
	SceUID handle;
	void *object;
	int ret;

	handle = SceSysmemForKernel_89D33DF0(uid);
	if (handle < 0)
		return handle;
	if (!SceSysmemForKernel_7F7D5C4A(SceSysmemForKernel_368F1C36(handle),
					 &g_ioAsyncEventClass))
		return SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	object = SceSysmemForKernel_FA6C098C(uid);
	SCE_DBG_ASSERT(object != NULL, 143);
	ret = sceGUIDClose(handle);
	if (ret < 0)
		return ret;
	return sceGUIDClose(uid);
}

/* name unknown: a path's copy on the scheduler heap */
char *sceIoAsyncAllocPath(const char *path)
{
	char *copy;

	copy = SceSysmemForDriver_7B4CB60A(g_ioSchedHeap, VFS_PATH_BUFF_SIZE);
	if (copy != NULL) {
		strncpy(copy, path, VFS_PATH_BUFF_SIZE - 1);
		copy[VFS_PATH_BUFF_SIZE - 1] = '\0';
	}
	return copy;
}

/* name unknown */
int sceIoAsyncFreePath(const char *path)
{
	return sceKernelFreeHeapMemory(g_ioSchedHeap, (void *)path);
}

SceUID ksceIoOpenAsync(const char *file, int flags, SceMode mode, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	SceUInt32 tls;
	char *path;
	SceUID uid;
	int ret;

	tls = sceSyscallEnter();
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0) {
		ret = uid;
		goto out;
	}
	path = sceIoAsyncAllocPath(file);
	if (path == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	ret = getPathMntOpt(path, 1);
	if (ret >= 0) {
		ret = sceIoRequestCreateIoRequestForOpen(&event->request, ret,
							 getThreadIoPriority(), 1, uid,
							 asyncParam, path, flags, mode);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
								    &event->request);
			if (ret >= 0) {
				sceSyscallLeave(tls);
				return uid;
			}
		}
	}
	sceKernelFreeHeapMemory(g_ioSchedHeap, path);
destroy:
	sceIoAsyncEventDestroy(uid);
out:
	sceSyscallLeave(tls);
	return ret;
}

int ksceIoCloseAsync(SceUID fd, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	SceUID guid;
	int ret, err;

	tls = sceSyscallEnter();
	guid = fd;
	if (IO_ASYNC_FROM_USER(readTpidruro()))
		guid = SceSysmemForDriver_45D22597(0, fd);
	ret = getFileSchedParam(guid, &info, 1);
	if (ret == 0) {
		ret = sceIoAsyncEventCreate(&event);
		if (ret >= 0) {
			err = sceIoRequestCreateIoRequestForClose(&event->request, info.mntOpt,
								  info.priority, 1, ret,
								  asyncParam, fd);
			if (err >= 0)
				err = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
									    &event->request);
			if (err < 0) {
				sceIoAsyncEventDestroy(ret);
				ret = err;
			}
		}
	}
	sceSyscallLeave(tls);
	return ret;
}

SceUID ksceIoReadAsync(SceUID fd, void *data, SceSize size, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	void *kernelPage;
	SceSize kernelSize;
	unsigned int kernelOffset;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	SceUID uid, mapUid;
	IoChunk *chunks;
	int err;

	tls = sceSyscallEnter();
	uid = getFileSchedParam(fd, &info, 0);
	if (uid != 0)
		goto out;
	if (info.mntOpt >= 0 && (SceUInt16)info.mntOpt == IO_SCHED_TYPE_SYNC_READ_ONLY) {
		uid = SCE_ERROR_ERRNO_EUNSUP;
		goto out;
	}
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0)
		goto out;

	chunks = SceSysmemForDriver_7B4CB60A(g_ioSchedHeap, IO_REQUEST_CHUNK_NUM * sizeof(IoChunk));
	if (chunks == NULL) {
		err = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	err = sceIoRequestCreateIoRequestForRead(&event->request, info.mntOpt, info.priority, 1,
						 uid, asyncParam, chunks, IO_REQUEST_CHUNK_NUM,
						 fd, data, size);
	if (err < 0)
		goto free;
	if (size == 0) {
		mapUid = 0;
	} else if (event->request.fromUser == 1) {
		mapUid = SceSysmemForDriver_7D4F8B5F("SceIoUserMapReadAsync", IO_USER_MAP_WRITE,
						    SceSblACMgrForKernel_sceSblACMgrIsGameProgram(0) == 1 ?
						    IO_USER_MAP_FLAGS_GAME : IO_USER_MAP_FLAGS,
						    data, size, &kernelPage, &kernelSize,
						    &kernelOffset);
		if (mapUid < 0) {
			err = ioUserMapErrorToErrno(mapUid);
			goto free;
		}
		event->request.args.rw.mappedData = (char *)kernelPage + kernelOffset;
		event->request.ioData = (char *)kernelPage + kernelOffset;
		event->request.args.rw.mapUid = mapUid;
		event->request.args.rw.offset = -1;
		event->request.ioDone = 0;
	} else {
		mapUid = 0;
		event->request.ioData = data;
		event->request.args.rw.mapUid = 0;
		event->request.args.rw.mappedData = NULL;
		event->request.ioDone = 0;
		event->request.args.rw.offset = -1;
	}
	err = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, &event->request);
	if (err >= 0)
		goto out;
	ioUserUnmap(mapUid, data, size);
free:
	sceKernelFreeHeapMemory(g_ioSchedHeap, chunks);
destroy:
	sceIoAsyncEventDestroy(uid);
	uid = err;
out:
	sceSyscallLeave(tls);
	return uid;
}

SceUID ksceIoWriteAsync(SceUID fd, const void *data, SceSize size, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	void *kernelPage;
	SceSize kernelSize;
	unsigned int kernelOffset;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	SceUID uid, mapUid;
	IoChunk *chunks;
	int err;

	tls = sceSyscallEnter();
	uid = getFileSchedParam(fd, &info, 0);
	if (uid != 0)
		goto out;
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0)
		goto out;

	chunks = SceSysmemForDriver_7B4CB60A(g_ioSchedHeap, IO_REQUEST_CHUNK_NUM * sizeof(IoChunk));
	if (chunks == NULL) {
		err = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	err = sceIoRequestCreateIoRequestForWrite(&event->request, info.mntOpt, info.priority, 1,
						  uid, asyncParam, chunks, IO_REQUEST_CHUNK_NUM,
						  fd, data, size);
	if (err < 0)
		goto free;
	if (size == 0) {
		mapUid = 0;
	} else if (event->request.fromUser == 1) {
		mapUid = SceSysmemForDriver_7D4F8B5F("SceIoUserMapWriteAsync", IO_USER_MAP_READ,
						    SceSblACMgrForKernel_sceSblACMgrIsGameProgram(0) == 1 ?
						    IO_USER_MAP_FLAGS_GAME : IO_USER_MAP_FLAGS,
						    data, size, &kernelPage, &kernelSize,
						    &kernelOffset);
		if (mapUid < 0) {
			err = ioUserMapErrorToErrno(mapUid);
			goto free;
		}
		event->request.args.write.mappedData = (char *)kernelPage + kernelOffset;
		event->request.ioData = (char *)kernelPage + kernelOffset;
		event->request.args.write.mapUid = mapUid;
		event->request.ioDone = 0;
	} else {
		mapUid = 0;
		event->request.ioData = (void *)data;
		event->request.args.write.mapUid = 0;
		event->request.args.write.mappedData = NULL;
		event->request.ioDone = 0;
	}
	err = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, &event->request);
	if (err >= 0)
		goto out;
	ioUserUnmap(mapUid, data, size);
free:
	sceKernelFreeHeapMemory(g_ioSchedHeap, chunks);
destroy:
	sceIoAsyncEventDestroy(uid);
	uid = err;
out:
	sceSyscallLeave(tls);
	return uid;
}

SceUID ksceIoLseekAsync(SceUID fd, SceOff offset, int whence, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	int ret, err;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (ret == 0) {
		ret = sceIoAsyncEventCreate(&event);
		if (ret >= 0) {
			err = sceIoRequestCreateIoRequestForLseek(&event->request, info.mntOpt,
								  info.priority, 1, ret,
								  asyncParam, fd, offset, whence);
			if (err >= 0)
				err = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
									    &event->request);
			if (err < 0) {
				sceIoAsyncEventDestroy(ret);
				ret = err;
			}
		}
	}
	sceSyscallLeave(tls);
	return ret;
}

SceUID ksceIoRemoveAsync(const char *file, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	SceUInt32 tls;
	char *path;
	SceUID uid;
	int ret;

	tls = sceSyscallEnter();
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0) {
		ret = uid;
		goto out;
	}
	path = sceIoAsyncAllocPath(file);
	if (path == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	ret = getPathMntOpt(path, 1);
	if (ret >= 0) {
		ret = sceIoRequestCreateIoRequestForRemove(&event->request, ret,
							   getThreadIoPriority(), 1, uid,
							   asyncParam, path);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
								    &event->request);
			if (ret >= 0) {
				sceSyscallLeave(tls);
				return uid;
			}
		}
	}
	sceKernelFreeHeapMemory(g_ioSchedHeap, path);
destroy:
	sceIoAsyncEventDestroy(uid);
out:
	sceSyscallLeave(tls);
	return ret;
}

SceUID ksceIoDopenAsync(const char *dirname, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	SceUInt32 tls;
	char *path;
	SceUID uid;
	int ret;

	tls = sceSyscallEnter();
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0) {
		ret = uid;
		goto out;
	}
	path = sceIoAsyncAllocPath(dirname);
	if (path == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	ret = getPathMntOpt(path, 1);
	if (ret >= 0) {
		ret = sceIoRequestCreateIoRequestForDopen(&event->request, ret,
							  getThreadIoPriority(), 1, uid,
							  asyncParam, path);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
								    &event->request);
			if (ret >= 0) {
				sceSyscallLeave(tls);
				return uid;
			}
		}
	}
	sceKernelFreeHeapMemory(g_ioSchedHeap, path);
destroy:
	sceIoAsyncEventDestroy(uid);
out:
	sceSyscallLeave(tls);
	return ret;
}

int ksceIoDcloseAsync(SceUID fd, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	SceUID guid;
	int ret, err;

	tls = sceSyscallEnter();
	guid = fd;
	if (IO_ASYNC_FROM_USER(readTpidruro()))
		guid = SceSysmemForDriver_45D22597(0, fd);
	ret = getFileSchedParam(guid, &info, 1);
	if (ret == 0) {
		ret = sceIoAsyncEventCreate(&event);
		if (ret >= 0) {
			err = sceIoRequestCreateIoRequestForDclose(&event->request, info.mntOpt,
								   info.priority, 1, ret,
								   asyncParam, fd);
			if (err >= 0)
				err = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
									    &event->request);
			if (err < 0) {
				sceIoAsyncEventDestroy(ret);
				ret = err;
			}
		}
	}
	sceSyscallLeave(tls);
	return ret;
}

int ksceIoDreadAsync(SceUID fd, SceIoDirent *dir, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	int ret, err;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (ret == 0) {
		ret = sceIoAsyncEventCreate(&event);
		if (ret >= 0) {
			err = sceIoRequestCreateIoRequestForDread(&event->request, info.mntOpt,
								  info.priority, 1, ret,
								  asyncParam, fd, dir);
			if (err >= 0)
				err = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
									    &event->request);
			if (err < 0) {
				sceIoAsyncEventDestroy(ret);
				ret = err;
			}
		}
	}
	sceSyscallLeave(tls);
	return ret;
}

SceUID ksceIoMkdirAsync(const char *dir, SceMode mode, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	SceUInt32 tls;
	char *path;
	SceUID uid;
	int ret;

	tls = sceSyscallEnter();
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0) {
		ret = uid;
		goto out;
	}
	path = sceIoAsyncAllocPath(dir);
	if (path == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	ret = getPathMntOpt(path, 1);
	if (ret >= 0) {
		ret = sceIoRequestCreateIoRequestForMkdir(&event->request, ret,
							  getThreadIoPriority(), 1, uid,
							  asyncParam, path, mode);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
								    &event->request);
			if (ret >= 0) {
				sceSyscallLeave(tls);
				return uid;
			}
		}
	}
	sceKernelFreeHeapMemory(g_ioSchedHeap, path);
destroy:
	sceIoAsyncEventDestroy(uid);
out:
	sceSyscallLeave(tls);
	return ret;
}

SceUID ksceIoRmdirAsync(const char *dirname, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	SceUInt32 tls;
	char *path;
	SceUID uid;
	int ret;

	tls = sceSyscallEnter();
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0) {
		ret = uid;
		goto out;
	}
	path = sceIoAsyncAllocPath(dirname);
	if (path == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	ret = getPathMntOpt(path, 1);
	if (ret >= 0) {
		ret = sceIoRequestCreateIoRequestForRmdir(&event->request, ret,
							  getThreadIoPriority(), 1, uid,
							  asyncParam, path);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
								    &event->request);
			if (ret >= 0) {
				sceSyscallLeave(tls);
				return uid;
			}
		}
	}
	sceKernelFreeHeapMemory(g_ioSchedHeap, path);
destroy:
	sceIoAsyncEventDestroy(uid);
out:
	sceSyscallLeave(tls);
	return ret;
}

SceUID ksceIoRenameAsync(const char *oldname, const char *newname, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	char *oldPath, *newPath;
	SceUInt32 tls;
	SceUID uid;
	int ret;

	tls = sceSyscallEnter();
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0) {
		ret = uid;
		goto out;
	}
	oldPath = sceIoAsyncAllocPath(oldname);
	if (oldPath == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	newPath = sceIoAsyncAllocPath(newname);
	if (newPath == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto free_old;
	}
	ret = getPathMntOpt(oldPath, 1);
	if (ret >= 0) {
		ret = sceIoRequestCreateIoRequestForRename(&event->request, ret,
							   getThreadIoPriority(), 1, uid,
							   asyncParam, oldPath, newPath);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
								    &event->request);
			if (ret >= 0) {
				sceSyscallLeave(tls);
				return uid;
			}
		}
	}
	sceKernelFreeHeapMemory(g_ioSchedHeap, newPath);
free_old:
	sceKernelFreeHeapMemory(g_ioSchedHeap, oldPath);
destroy:
	sceIoAsyncEventDestroy(uid);
out:
	sceSyscallLeave(tls);
	return ret;
}

SceUID ksceIoChstatAsync(const char *file, SceIoStat *stat, unsigned int bits,
			 IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	SceUInt32 tls;
	char *path;
	SceUID uid;
	int ret;

	tls = sceSyscallEnter();
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0) {
		ret = uid;
		goto out;
	}
	path = sceIoAsyncAllocPath(file);
	if (path == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	ret = getPathMntOpt(path, !(bits & SCE_CST_SIZE));
	if (ret >= 0) {
		ret = sceIoRequestCreateIoRequestForChstat(&event->request, ret,
							   getThreadIoPriority(), 1, uid,
							   asyncParam, path, stat, bits);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
								    &event->request);
			if (ret >= 0) {
				sceSyscallLeave(tls);
				return uid;
			}
		}
	}
	sceKernelFreeHeapMemory(g_ioSchedHeap, path);
destroy:
	sceIoAsyncEventDestroy(uid);
out:
	sceSyscallLeave(tls);
	return ret;
}

SceUID ksceIoGetstatAsync(const char *file, SceIoStat *stat, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	SceUInt32 tls;
	char *path;
	SceUID uid;
	int ret;

	tls = sceSyscallEnter();
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0) {
		ret = uid;
		goto out;
	}
	path = sceIoAsyncAllocPath(file);
	if (path == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	ret = getPathMntOpt(path, 1);
	if (ret >= 0) {
		ret = sceIoRequestCreateIoRequestForGetstat(&event->request, ret,
							    getThreadIoPriority(), 1, uid,
							    asyncParam, path, stat);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
								    &event->request);
			if (ret >= 0) {
				sceSyscallLeave(tls);
				return uid;
			}
		}
	}
	sceKernelFreeHeapMemory(g_ioSchedHeap, path);
destroy:
	sceIoAsyncEventDestroy(uid);
out:
	sceSyscallLeave(tls);
	return ret;
}

SceUID ksceIoSyncAsync(const char *device, unsigned int flag, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	SceUInt32 tls;
	char *path;
	SceUID uid;
	int ret;

	tls = sceSyscallEnter();
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0) {
		ret = uid;
		goto out;
	}
	path = sceIoAsyncAllocPath(device);
	if (path == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	ret = getPathMntOpt(path, 1);
	if (ret >= 0) {
		ret = sceIoRequestCreateIoRequestForSync(&event->request, ret,
							 getThreadIoPriority(), 1, uid,
							 asyncParam, path, flag);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
								    &event->request);
			if (ret >= 0) {
				sceSyscallLeave(tls);
				return uid;
			}
		}
	}
	sceKernelFreeHeapMemory(g_ioSchedHeap, path);
destroy:
	sceIoAsyncEventDestroy(uid);
out:
	sceSyscallLeave(tls);
	return ret;
}

SceUID ksceIoPreadAsync(SceUID fd, void *data, SceSize size, SceOff offset,
			IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	void *kernelPage;
	SceSize kernelSize;
	unsigned int kernelOffset;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	SceUID uid, mapUid;
	IoChunk *chunks;
	int err;

	tls = sceSyscallEnter();
	uid = getFileSchedParam(fd, &info, 0);
	if (uid != 0)
		goto out;
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0)
		goto out;

	chunks = SceSysmemForDriver_7B4CB60A(g_ioSchedHeap, IO_REQUEST_CHUNK_NUM * sizeof(IoChunk));
	if (chunks == NULL) {
		err = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	err = sceIoRequestCreateIoRequestForPread(&event->request, info.mntOpt, info.priority, 1,
						  uid, asyncParam, chunks, IO_REQUEST_CHUNK_NUM,
						  fd, data, size, offset);
	if (err < 0)
		goto free;
	if (size == 0) {
		mapUid = 0;
	} else if (event->request.fromUser == 1) {
		mapUid = SceSysmemForDriver_7D4F8B5F("SceIoUserMapPreadAsync", IO_USER_MAP_WRITE,
						    SceSblACMgrForKernel_sceSblACMgrIsGameProgram(0) == 1 ?
						    IO_USER_MAP_FLAGS_GAME : IO_USER_MAP_FLAGS,
						    data, size, &kernelPage, &kernelSize,
						    &kernelOffset);
		if (mapUid < 0) {
			err = ioUserMapErrorToErrno(mapUid);
			goto free;
		}
		event->request.args.rw.mappedData = (char *)kernelPage + kernelOffset;
		event->request.ioData = (char *)kernelPage + kernelOffset;
		event->request.args.rw.mapUid = mapUid;
		event->request.ioDone = 0;
	} else {
		mapUid = 0;
		event->request.ioData = data;
		event->request.args.rw.mapUid = 0;
		event->request.args.rw.mappedData = NULL;
		event->request.ioDone = 0;
	}
	err = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, &event->request);
	if (err >= 0)
		goto out;
	ioUserUnmap(mapUid, data, size);
free:
	sceKernelFreeHeapMemory(g_ioSchedHeap, chunks);
destroy:
	sceIoAsyncEventDestroy(uid);
	uid = err;
out:
	sceSyscallLeave(tls);
	return uid;
}

SceUID ksceIoPwriteAsync(SceUID fd, const void *data, SceSize size, SceOff offset,
			 IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	void *kernelPage;
	SceSize kernelSize;
	unsigned int kernelOffset;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	SceUID uid, mapUid;
	IoChunk *chunks;
	int err;

	tls = sceSyscallEnter();
	uid = getFileSchedParam(fd, &info, 0);
	if (uid != 0)
		goto out;
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0)
		goto out;

	chunks = SceSysmemForDriver_7B4CB60A(g_ioSchedHeap, IO_REQUEST_CHUNK_NUM * sizeof(IoChunk));
	if (chunks == NULL) {
		err = SCE_ERROR_ERRNO_ENOMEM;
		goto destroy;
	}
	err = sceIoRequestCreateIoRequestForPwrite(&event->request, info.mntOpt, info.priority,
						   1, uid, asyncParam, chunks,
						   IO_REQUEST_CHUNK_NUM, fd, data, size, offset);
	if (err < 0)
		goto free;
	if (size == 0) {
		mapUid = 0;
	} else if (event->request.fromUser == 1) {
		mapUid = SceSysmemForDriver_7D4F8B5F("SceIoUserMapPwriteAsync", IO_USER_MAP_READ,
						    SceSblACMgrForKernel_sceSblACMgrIsGameProgram(0) == 1 ?
						    IO_USER_MAP_FLAGS_GAME : IO_USER_MAP_FLAGS,
						    data, size, &kernelPage, &kernelSize,
						    &kernelOffset);
		if (mapUid < 0) {
			err = ioUserMapErrorToErrno(mapUid);
			goto free;
		}
		event->request.args.rw.mappedData = (char *)kernelPage + kernelOffset;
		event->request.ioData = (char *)kernelPage + kernelOffset;
		event->request.args.rw.mapUid = mapUid;
		event->request.ioDone = 0;
	} else {
		mapUid = 0;
		event->request.ioData = (void *)data;
		event->request.args.rw.mapUid = 0;
		event->request.args.rw.mappedData = NULL;
		event->request.ioDone = 0;
	}
	err = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, &event->request);
	if (err >= 0)
		goto out;
	ioUserUnmap(mapUid, data, size);
free:
	sceKernelFreeHeapMemory(g_ioSchedHeap, chunks);
destroy:
	sceIoAsyncEventDestroy(uid);
	uid = err;
out:
	sceSyscallLeave(tls);
	return uid;
}

int ksceIoChstatByFdAsync(SceUID fd, SceIoStat *stat, int bits, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	int ret, err;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (ret == 0) {
		ret = sceIoAsyncEventCreate(&event);
		if (ret >= 0) {
			err = sceIoRequestCreateIoRequestForChstatByFd(&event->request,
								       info.mntOpt,
								       info.priority, 1, ret,
								       asyncParam, fd, stat,
								       bits);
			if (err >= 0)
				err = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
									    &event->request);
			if (err < 0) {
				sceIoAsyncEventDestroy(ret);
				ret = err;
			}
		}
	}
	sceSyscallLeave(tls);
	return ret;
}

int ksceIoGetstatByFdAsync(SceUID fd, SceIoStat *stat, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	int ret, err;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (ret == 0) {
		ret = sceIoAsyncEventCreate(&event);
		if (ret >= 0) {
			err = sceIoRequestCreateIoRequestForGetstatByFd(&event->request,
									info.mntOpt,
									info.priority, 1, ret,
									asyncParam, fd, stat);
			if (err >= 0)
				err = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
									    &event->request);
			if (err < 0) {
				sceIoAsyncEventDestroy(ret);
				ret = err;
			}
		}
	}
	sceSyscallLeave(tls);
	return ret;
}

int ksceIoSyncByFdAsync(SceUID fd, int flag, IoAsyncParam *asyncParam)
{
	IoAsyncEvent *event = NULL;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	int ret, err;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (ret == 0) {
		ret = sceIoAsyncEventCreate(&event);
		if (ret >= 0) {
			err = sceIoRequestCreateIoRequestForSyncByFd(&event->request, info.mntOpt,
								     info.priority, 1, ret,
								     asyncParam, fd, flag);
			if (err >= 0)
				err = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher,
									    &event->request);
			if (err < 0) {
				sceIoAsyncEventDestroy(ret);
				ret = err;
			}
		}
	}
	sceSyscallLeave(tls);
	return ret;
}

int ksceIoCancel(SceUID uid)
{
	IoAsyncEvent *event = NULL;
	IoScheduler *sched;
	SceUInt32 tls;
	int ret, released;

	tls = sceSyscallEnter();
	ret = SceSysmemForDriver_00ED6C14(uid, &g_ioAsyncEventClass, (SceObjectBase **)&event);
	if (ret < 0) {
		sceSyscallLeave(tls);
		return ret;
	}
	sched = sceIoSchedDispatcherGetIoScheduler(&g_schedDispatcher, event->request.mntOpt);
	SCE_DBG_ASSERT_FUNC(sched != NULL, 1550, "sceIoCancel");
	ret = sceIoSchedulerCancelAsyncIoRequest(sched, &event->request);
	if (ret >= 0) {
		released = sceGUIDReleaseObject(uid);
		SCE_DBG_ASSERT_FUNC(released == 0, 1560, "sceIoCancel");
	}
	sceSyscallLeave(tls);
	return ret;
}

int sceIoComplete(SceUID uid)
{
	SceUInt32 pattern = 0;
	IoAsyncEvent *event = NULL;
	SceUInt32 tls;
	int ret, released;

	tls = sceSyscallEnter();
	ret = sceKernelGetEventPattern(uid, &pattern);
	if (ret >= 0) {
		if (pattern != 0) {
			sceSyscallLeave(tls);
			return SCE_ERROR_ERRNO_EBUSY;
		}
		ret = SceSysmemForDriver_00ED6C14(uid, &g_ioAsyncEventClass,
						   (SceObjectBase **)&event);
		if (ret >= 0) {
			released = sceGUIDReleaseObject(uid);
			SCE_DBG_ASSERT(released >= 0, 1597);
			ret = sceIoAsyncEventDestroy(uid);
		}
	}
	sceSyscallLeave(tls);
	return ret;
}

/* sceIoCompleteMultiple (name unknown): completes the events in order, up to the first that is not done */
int SceIofilemgrForDriver_B499287E(IoAsyncParam *params, int num, int *numCompleted)
{
	SceUInt32 pattern = 0;
	SceUInt32 tls;
	int ret, i;

	tls = sceSyscallEnter();
	ret = 0;
	for (i = 0; i < num; i++, params++) {
		ret = sceKernelGetEventPattern(params->event, &pattern);
		if (ret < 0)
			break;
		if (pattern != 0) {
			ret = SCE_ERROR_ERRNO_EBUSY;
			break;
		}
		if (SceSysmemForKernel_FA6C098C(params->event) == NULL) {
			ret = SCE_ERROR_ERRNO_EINVAL;
			break;
		}
		ret = sceIoAsyncEventDestroy(params->event);
		if (ret < 0)
			break;
	}
	*numCompleted = i;
	sceSyscallLeave(tls);
	return ret;
}

/* name unknown: a process's events go when it ends */
void sceIoAsyncProcessFinalize(SceUID pid)
{
	sceIoSchedDispatcherFreeAllAsyncEvents(&g_schedDispatcher, pid);
}

int ksceIoIoctlAsync(SceUID fd, unsigned int cmd, const void *indata, SceSize inlen,
		     void *outdata, SceSize outlen, IoAsyncParam *asyncParam)
{
	return SCE_KERNEL_ERROR_UNSUP;
}

int ksceIoDevctlAsync(const char *dev, unsigned int cmd, const void *indata, SceSize inlen,
		      void *outdata, SceSize outlen, IoAsyncParam *asyncParam)
{
	return SCE_KERNEL_ERROR_UNSUP;
}
