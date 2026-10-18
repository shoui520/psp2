/*
 * SceIofilemgr: the daemon (Sony's daemon.c).
 *
 * The "SceIofilemgrDaemon" thread flushes a device ten seconds after it was
 * last written to. Each kind of device (one per I/O scheduler type) has a
 * flush timer; the one that expires first runs as the "SceIofilemgrAlarm"
 * alarm, which wakes the daemon. The daemon then sends the flush devctl to
 * every mounted device of that kind.
 */

#include "iofilemgr_internal.h"

/* the time a written device waits before it is flushed, in microseconds (name unknown) */
#define IOF_DAEMON_FLUSH_DELAY		10000000

/* the devctl that flushes a mounted file system (name unknown) */
#define IOF_DAEMON_DEVCTL_FLUSH		0x3803

/* the event flag bit the alarm sets (name unknown) */
#define IOF_DAEMON_EVF_ALARM		1

/* the facility of a Sony error code, (error >> 16) & 0xfff (names unknown) */
#define IOF_DAEMON_ERROR_FACILITY(e)	(((unsigned int)(e) << 4) >> 20)
#define IOF_DAEMON_FACILITY_0x32	0x32
#define IOF_DAEMON_FACILITY_0x3D	0x3d

/* the end of a device list (name unknown) */
#define IOF_DAEMON_DEV_END		(-1)

/* a device the daemon flushes: a mount and its mount option (names unknown) */
typedef struct IofDaemonDev {
	int mntId;		/* 0: a free slot */
	int mntOpt;		/* -1: not mounted yet */
} IofDaemonDev;

/* the flush timer of one kind of device (names unknown) */
typedef struct IofDaemonTimer {
	int mntOpt;		/* the scheduler type, 0: unused */
	int isExpired;		/* set by the alarm, cleared when the timer restarts */
	SceUInt64 lastWriteTime;
	SceUInt64 expireTime;	/* when the alarm went off */
} IofDaemonTimer;

#define IOF_DAEMON_TIMER_NUM		4

/* the daemon's state (names unknown) */
SceKernelFastMutex g_iofDaemonLock;	/* "SceIofileDaemonLock" */
IofDaemonDev *g_iofDaemonInitialDevList;
int g_iofDaemonExpiredTimer;		/* the timer whose alarm went off last */
IofDaemonDev *g_iofDaemonDevList;
SceUID g_iofDaemonEvf;
IofDaemonTimer g_iofDaemonTimer[IOF_DAEMON_TIMER_NUM];
int g_iofDaemonTimerNum;
SceUID g_iofDaemonAlarm;
SceUID g_iofDaemonThread;

static SceUInt32 iofDaemonAlarmHandler(void *arg);

/* name unknown: the timer of the kind of device mntOpt restarts at now; its index, -1 if none */
static inline int iofDaemonRestartTimer(int mntOpt, SceUInt64 now)
{
	int i;

	for (i = 0; i < g_iofDaemonTimerNum; i++) {
		if ((mntOpt & g_iofDaemonTimer[i].mntOpt) == mntOpt) {
			g_iofDaemonTimer[i].lastWriteTime = now;
			g_iofDaemonTimer[i].isExpired = 0;
			return i;
		}
	}
	return -1;
}

/* name unknown: the alarm of timer index goes off after usec */
static void iofDaemonSetAlarm(int index, SceUInt32 usec)
{
	if (g_iofDaemonAlarm > 0)
		sceKernelCancelAlarm(g_iofDaemonAlarm);
	g_iofDaemonAlarm = ksceKernelRegisterTimer("SceIofilemgrAlarm", usec,
						   iofDaemonAlarmHandler, (void *)index);
}

/* name unknown */
static SceUInt32 iofDaemonAlarmHandler(void *arg)
{
	int index = (int)arg;
	IofDaemonTimer *t = &g_iofDaemonTimer[index];

	t->isExpired = 1;
	g_iofDaemonExpiredTimer = index;
	t->expireTime = sceKernelGetSystemTimeWide();
	ksceKernelSetEventFlag(g_iofDaemonEvf, IOF_DAEMON_EVF_ALARM);
	return 0;
}

int iof_daemon_main(SceSize args, void *argp)
{
	IofilemgrDevctlParam param;
	SceVfsMountData *data;
	IofDaemonDev *dev;
	SceUInt64 now, oldest, diff, time;
	int index = 0, mntOpt = 0;
	int ret, next, i;

	while (1) {
		if (mntOpt == 0 || g_iofDaemonTimer[index].isExpired == 0) {
			now = sceKernelGetSystemTimeWide();
			ksceKernelLockFastMutex(&g_iofDaemonLock);
			if (g_iofDaemonTimerNum > 0) {
				oldest = now;
				next = -1;
				for (i = 0; i < g_iofDaemonTimerNum; i++) {
					if (g_iofDaemonTimer[i].mntOpt != 0 &&
					    g_iofDaemonTimer[i].isExpired == 0 &&
					    g_iofDaemonTimer[i].lastWriteTime < oldest) {
						oldest = g_iofDaemonTimer[i].lastWriteTime;
						next = i;
					}
				}
				if (next != -1) {
					if (oldest == now) {
						iofDaemonSetAlarm(next, IOF_DAEMON_FLUSH_DELAY);
					} else {
						diff = now - oldest;
						iofDaemonSetAlarm(next, diff <= IOF_DAEMON_FLUSH_DELAY ?
								  IOF_DAEMON_FLUSH_DELAY - (SceUInt32)diff : 0);
					}
				}
			}
			ksceKernelUnlockFastMutex(&g_iofDaemonLock);

			ret = ksceKernelWaitEventFlag(g_iofDaemonEvf, IOF_DAEMON_EVF_ALARM,
						      SCE_KERNEL_EVF_WAITMODE_AND |
						      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL,
						      NULL);
			index = g_iofDaemonExpiredTimer;
			mntOpt = g_iofDaemonTimer[index].mntOpt;
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 109, "ret = %x\n", ret);
				SCE_DBG_ASSERT(0, 110);
			}
		}

		for (dev = g_iofDaemonDevList; dev->mntId != IOF_DAEMON_DEV_END; dev++) {
			if (dev->mntId != 0 && mntOpt == dev->mntOpt)
				break;
		}
		if (dev->mntId == IOF_DAEMON_DEV_END)
			goto no_device;

		data = mnt_get_mount_data(dev->mntId);
		if (data == NULL) {
			ret = SCE_ERROR_ERRNO_EINVAL;
		} else {
			param.dev = data->assign_name;
			param.cmd = IOF_DAEMON_DEVCTL_FLUSH;
			param.indata = NULL;
			param.inlen = 0;
			param.outdata = NULL;
			param.outlen = 0;
			vfs_increment_io_count();
			vfs_set_sync_pending();
			ret = vfsDevctl(&param);
			vfs_decrement_io_count();
			if (ret < 0) {
				if (ret == SCE_ERROR_ERRNO_ENOENT)
					dev->mntId = 0;
				else if (ret == SCE_ERROR_ERRNO_ENODEV || ret == SCE_ERROR_ERRNO_EIO)
					goto no_device;
			} else if (ret == 0) {
				goto keep;
			}
		}
		if (ret != SCE_ERROR_ERRNO_ENOENT &&
		    IOF_DAEMON_ERROR_FACILITY(ret) != IOF_DAEMON_FACILITY_0x32 &&
		    IOF_DAEMON_ERROR_FACILITY(ret) != IOF_DAEMON_FACILITY_0x3D) {
			sceKernelDelayThread(100);
			goto keep;
		}
		mntOpt = 0;
		g_iofDaemonTimer[index].isExpired = 0;
		g_iofDaemonTimer[index].lastWriteTime = sceKernelGetSystemTimeWide();
		time = g_iofDaemonTimer[index].lastWriteTime;
		goto check;

no_device:
		ksceKernelLockFastMutex(&g_iofDaemonLock);
		for (i = 0; i < g_iofDaemonTimerNum; i++) {
			if ((g_iofDaemonTimer[i].mntOpt & mntOpt) == mntOpt) {
				mntOpt = 0;
				g_iofDaemonTimer[i].isExpired = 0;
				g_iofDaemonTimer[i].mntOpt = 0;
				break;
			}
		}
		ksceKernelUnlockFastMutex(&g_iofDaemonLock);
keep:
		time = g_iofDaemonTimer[index].lastWriteTime;
check:
		if (g_iofDaemonTimer[index].expireTime < time)
			g_iofDaemonTimer[index].isExpired = 0;
	}
}

/* name unknown */
#define DEVLIST_ADD(dev, id, line) do {					\
	mntOpt = mnt_get_mount_opt(id);						\
	if (mntOpt == 0) {							\
		SCE_DBG_PRINTF_LEVEL(1, 0xf, line, "devlist_init failed id=%x ret=%x\n", \
				     id, SCE_ERROR_ERRNO_EINVAL);		\
	} else {								\
		(dev)->mntId = (id);						\
		(dev)->mntOpt = mntOpt;						\
		(dev)++;							\
	}									\
} while (0)

void devlist_initialize(IofDaemonDev *dev)
{
	int mntOpt;

	memset(dev, 0, 0x100);
	DEVLIST_ADD(dev, 0x100, 208);
	DEVLIST_ADD(dev, 0x400, 217);
	DEVLIST_ADD(dev, 0x600, 226);
	DEVLIST_ADD(dev, 0x700, 235);
	DEVLIST_ADD(dev, 0x800, 244);
	DEVLIST_ADD(dev, 0xa00, 253);
	dev[0].mntId = 0x10000;
	dev[0].mntOpt = -1;
	dev[1].mntOpt = -1;
	dev[2].mntId = IOF_DAEMON_DEV_END;
	dev[1].mntId = 0x20000;
}

void iofile_daemon_start(void)
{
	SceUInt64 now;
	int ret;

	g_iofDaemonDevList = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, 0x100);
	if (g_iofDaemonDevList == NULL)
		SCE_DBG_ASSERT(0, 324);
	devlist_initialize(g_iofDaemonDevList);
	g_iofDaemonInitialDevList = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, 0x100);
	if (g_iofDaemonInitialDevList == NULL)
		SCE_DBG_ASSERT(0, 334);
	ret = ksceKernelInitializeFastMutex(&g_iofDaemonLock, "SceIofileDaemonLock", 2, NULL);
	if (ret < 0)
		SCE_DBG_ASSERT(0, 342);
	devlist_initialize(g_iofDaemonInitialDevList);

	now = sceKernelGetSystemTimeWide();
	g_iofDaemonTimer[0].mntOpt = IO_SCHED_TYPE_INTERNAL;
	g_iofDaemonTimer[0].lastWriteTime = now;
	g_iofDaemonTimer[1].lastWriteTime = now;
	g_iofDaemonTimer[2].lastWriteTime = now;
	g_iofDaemonTimer[1].mntOpt = IO_SCHED_TYPE_REMOVABLE;
	g_iofDaemonTimer[2].mntOpt = IO_SCHED_TYPE_GAME_CARD;
	g_iofDaemonTimer[0].isExpired = 0;
	g_iofDaemonTimer[1].isExpired = 0;
	g_iofDaemonTimer[2].isExpired = 0;
	g_iofDaemonExpiredTimer = 0x100;	/* sic: not a timer index */
	g_iofDaemonTimerNum = 3;
	iofDaemonSetAlarm(0, IOF_DAEMON_FLUSH_DELAY);

	g_iofDaemonThread = ksceKernelCreateThread("SceIofilemgrDaemon", iof_daemon_main, 0x65,
						   0x2000, 0x10000000, 0x80000, NULL);
	if (g_iofDaemonThread < 0)
		SCE_DBG_ASSERT(0, 359);
	g_iofDaemonEvf = ksceKernelCreateEventFlag("SceIofilemgrDaemon", 0, 0, NULL);
	if (g_iofDaemonEvf < 0)
		SCE_DBG_ASSERT(0, 367);
	ret = ksceKernelStartThread(g_iofDaemonThread, 0, NULL);
	if (ret < 0)
		SCE_DBG_ASSERT(0, 372);
}

/* name unknown: a device of kind mntOpt was written to, its flush timer restarts */
int iofile_daemon_notify_write(int mntOpt)
{
	SceUInt64 now, oldest, diff;
	int ret, index, i;

	now = sceKernelGetSystemTimeWide();
	ret = ksceKernelLockFastMutex(&g_iofDaemonLock);
	if (ret < 0)
		return ret;
	index = iofDaemonRestartTimer(mntOpt, now);
	if (index < 0) {
		ksceKernelUnlockFastMutex(&g_iofDaemonLock);
		return SCE_ERROR_ERRNO_EINVAL;
	}

	oldest = now;
	for (i = 0; i < g_iofDaemonTimerNum; i++) {
		if (g_iofDaemonTimer[i].mntOpt != 0 && g_iofDaemonTimer[i].isExpired == 0 &&
		    g_iofDaemonTimer[i].lastWriteTime < oldest) {
			oldest = g_iofDaemonTimer[i].lastWriteTime;
			index = i;
		}
	}
	if (oldest == now) {
		iofDaemonSetAlarm(index, IOF_DAEMON_FLUSH_DELAY);
	} else if (g_iofDaemonTimer[index].mntOpt != mntOpt) {
		diff = now - oldest;
		iofDaemonSetAlarm(index, diff <= IOF_DAEMON_FLUSH_DELAY ?
				  IOF_DAEMON_FLUSH_DELAY - (SceUInt32)diff : 0);
	}
	ksceKernelUnlockFastMutex(&g_iofDaemonLock);
	return 0;
}

void iofile_daemon_notify_mount(int mntId)
{
	IofDaemonDev *dev;
	int mntOpt, i;

	switch (mntId) {
	case 0x800:
	case 0xa00:
	case 0x10000:
	case 0x20000:
		break;
	default:
		return;
	}

	mntOpt = mnt_get_mount_opt(mntId);
	for (dev = g_iofDaemonDevList; dev->mntId != IOF_DAEMON_DEV_END; dev++) {
		if (dev->mntId == mntId) {
			dev->mntOpt = mntOpt;
			goto found;
		}
	}
	for (dev = g_iofDaemonDevList; dev->mntId != IOF_DAEMON_DEV_END; dev++) {
		if (dev->mntId == 0) {
			dev->mntId = mntId;
			dev->mntOpt = mntOpt;
			break;
		}
	}
found:
	ksceKernelLockFastMutex(&g_iofDaemonLock);
	for (i = 0; i < g_iofDaemonTimerNum; i++) {
		if ((mntOpt & g_iofDaemonTimer[i].mntOpt) == mntOpt) {
			iofile_daemon_notify_write(mntOpt);
			ksceKernelUnlockFastMutex(&g_iofDaemonLock);
			return;
		}
	}
	for (i = 0; i < g_iofDaemonTimerNum; i++) {
		if (g_iofDaemonTimer[i].mntOpt == 0) {
			g_iofDaemonTimer[i].mntOpt = mntOpt;
			g_iofDaemonTimer[i].lastWriteTime = sceKernelGetSystemTimeWide();
			g_iofDaemonTimer[i].isExpired = 0;
			iofile_daemon_notify_write(mntOpt);
			ksceKernelUnlockFastMutex(&g_iofDaemonLock);
			return;
		}
	}
	SCE_DBG_PRINTF_LEVEL(1, 0xf, 677, "unexpected error !! mntId=%x\n", mntId);
	ksceKernelUnlockFastMutex(&g_iofDaemonLock);
}
