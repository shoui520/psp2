/*
 * SceIofilemgr: error events (Sony's ioerror.c).
 *
 * A process creates an error event for one mount; when a file system on that
 * mount reports an error (sceIoSetErrorEvent), the error is written to the
 * creator's IoErrorEventInfo and the event is set, once, until the process
 * clears it.
 */

#include <sysmem/sysmem.h>
#include "iofilemgr_internal.h"

struct ThreadmgrWaitableClass g_ioErrorEventClass;

/* the name of an event: this prefix and the mount's device, "SceIoErrorEventUx0" (names unknown) */
#define IOERROR_EVENT_NAME_SIZE		0x20
#define IOERROR_EVENT_PREFIX_LEN	15	/* strlen("SceIoErrorEvent") */
#define IOERROR_EVENT_DEVICE_LEN	16

/* the only event bit an error event waits for (name unknown) */
#define IOERROR_EVENT_PATTERN		1

/* sceGUIDGetUIDVectorByClass: every visible UID (name unknown) */
#define IOERROR_UID_VISIBILITY		7

/* the UIDs ioerror_process_finalize takes at a time */
#define IOERROR_FINALIZE_UID_NUM	64

/* the class's object callbacks (names unknown) */
static int ioErrorEventClassCreate(void *item)
{
	return 0;
}

static int ioErrorEventClassDestroy(void *item)
{
	return 0;
}

/* name unknown */
int ioerror_initialize(void)
{
	return sceKernelUIDInheritedEventClassInit(&g_ioErrorEventClass,
						   "SceUIDIoErrorEventClass", sizeof(IoErrorEvent),
						   ioErrorEventClassCreate, ioErrorEventClassDestroy);
}

SceUID sceIoCreateErrorEvent(int mntId, SceUInt32 pattern, IoErrorEventInfo *userInfo)
{
	IoErrorEvent *event = NULL;
	IoErrorEventInfo info;
	SceVfsMountData *data;
	char name[IOERROR_EVENT_NAME_SIZE];
	SceUID uid;
	int ret, closed, i;
	char c;

	if (mntId != 0 && pattern == IOERROR_EVENT_PATTERN) {
		ret = sceKernelCopyFromUser(&info, userInfo, sizeof(info));
		if (ret < 0)
			return ret;
		if (info.reserved0[0] == 0 && info.reserved0[1] == 0 && info.reserved1[0] == 0 &&
		    info.reserved1[1] == 0 && mnt_find_mount_point(mntId) != NULL &&
		    (data = mnt_get_mount_data(mntId)) != NULL) {
			strncpy(name, "SceIoErrorEvent", sizeof(name) - 1);
			i = 0;
			while (1) {
				c = data->assignName[i];
				if (i == 0)
					c = toupper(c);
				if (c == ':') {
					name[IOERROR_EVENT_PREFIX_LEN + i] = '\0';
					break;
				}
				name[IOERROR_EVENT_PREFIX_LEN + i] = c;
				if (c == '\0' || ++i == IOERROR_EVENT_DEVICE_LEN)
					break;
			}
			name[sizeof(name) - 1] = '\0';

			uid = sceGUIDKernelCreate(&g_ioErrorEventClass.base, name,
						   (SceObjectBase **)&event);
			if (uid < 0)
				return uid;
			ret = sceKernelEventObjectInit(uid, 0x100, 0, 0);
			if (ret < 0) {
				closed = sceGUIDClose(uid);
				SCE_DBG_ASSERT_FUNC(closed == 0, 148, "sceIoCreateErrorEvent");
				return ret;
			}
			event->pattern = IOERROR_EVENT_PATTERN;
			event->mntId = mntId;
			event->userInfo = userInfo;
			event->pid = sceKernelGetProcessId();
			event->isSet = 0;
			return uid;
		}
	}
	return SCE_ERROR_ERRNO_EINVAL;
}

int SceIofilemgrForDriver_40B933C7(SceUID uid)
{
	IoErrorEvent *event;
	int ret;

	ret = sceGUIDReferObjectWithClass(uid, &g_ioErrorEventClass.base, (SceObjectBase **)&event);
	if (ret >= 0) {
		event->isSet = 0;
		sceGUIDReleaseObject(uid);
	}
	return ret;
}

int SceIofilemgrForDriver_C6158F8D(SceUID uid)
{
	SceUID handle;

	handle = sceGUIDOpenByGUID(uid);
	if (handle < 0)
		return handle;
	if (!SceSysmemForKernel_7F7D5C4A(SceSysmemForKernel_368F1C36(handle),
					 &g_ioErrorEventClass.base))
		return SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	sceGUIDClose(handle);
	return sceGUIDClose(uid);
}

/* a file system reports an error on a mount: each event of the mount that waits for it is set */
int sceIoSetErrorEvent(int mntId, SceUInt32 pattern, const IoErrorEventInfo *error)
{
	IoErrorEventInfo info;
	SceSize num;
	IoErrorEvent *event;
	SceUID *uids;
	int ret, copied, i;

	ret = sceGUIDGetUIDVectorByClass(&g_ioErrorEventClass.base, IOERROR_UID_VISIBILITY, NULL, 0,
					  &num);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 220, "sceGUIDGetUIDVectorByClass failed ... 0x%x %d\n",
				     ret, ret);
		return ret;
	}
	if (ret == 0)
		return 0;

	uids = sceKernelAllocHeapMemory(g_vfsNcacheHeap, ret * sizeof(SceUID));
	if (uids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = sceGUIDGetUIDVectorByClass(&g_ioErrorEventClass.base, IOERROR_UID_VISIBILITY, uids, ret,
					  &num);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 244, "sceGUIDGetUIDVectorByClass failed ... 0x%x %d\n",
				     ret, ret);
		sceKernelFreeHeapMemory(g_vfsNcacheHeap, uids);
		return ret;
	}
	for (i = 0; i < (int)num; i++) {
		ret = sceGUIDReferObjectWithClass(uids[i], &g_ioErrorEventClass.base,
						   (SceObjectBase **)&event);
		if (ret == 0) {
			if (event->mntId == mntId && (pattern & event->pattern) != 0 &&
			    event->isSet == 0) {
				info.mntId = mntId;
				info.error = error->error;
				info.reserved0[0] = ret;
				info.reserved0[1] = ret;
				info.detail = error->detail;
				info.reserved1[0] = ret;
				info.reserved1[1] = ret;
				copied = sceKernelCopyToUserProc(event->pid, event->userInfo, &info,
								  sizeof(info));
				ret = sceKernelSetEventWithNotifyCallback(uids[i], pattern, copied, 0);
				if (ret < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0xf, 279,
							     "sceKernelSetEventWithNotifyCallback failed %d 0x%x uid=0x%x\n",
							     ret, ret, uids[i]);
				else
					event->isSet = 1;
				sceGUIDReleaseObject(uids[i]);
			}
		} else if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 288,
					     "sceGUIDReferObjectWithClass failed %d 0x%x uid=0x%x\n",
					     ret, ret, uids[i]);
		}
	}
	sceKernelFreeHeapMemory(g_vfsNcacheHeap, uids);
	return ret;
}

/* name unknown: a process's error events go when it ends */
int ioerror_process_finalize(SceUID pid)
{
	SceUID puids[IOERROR_FINALIZE_UID_NUM];
	SceSize num;
	SceUID guid;
	int ret, i;

	do {
		ret = scePUIDGetUIDVectorByClass(pid, &g_ioErrorEventClass.base, IOERROR_UID_VISIBILITY,
						  puids, IOERROR_FINALIZE_UID_NUM, &num);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 312,
					     "scePUIDGetUIDVectorByClass failed 0x%x %d\n", ret, ret);
			break;
		}
		if ((int)num <= 0)
			break;
		for (i = 0; i < (int)num; i++) {
			guid = SceSysmemForDriver_45D22597(pid, puids[i]);
			if (guid < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 322,
						     "PUIDtoGUID failed err= %d 0x%x puid=0x%x\n",
						     guid, guid, puids[i]);
				continue;
			}
			ret = SceIofilemgrForDriver_C6158F8D(guid);
			if (ret == 0) {
				SceSysmemForDriver_84A4AF5E(pid, puids[i]);
			} else {
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 331,
						     "sceIoDeleteErrorEvent failed %d 0x%x uid=0x%x %d\n",
						     ret, ret, guid, guid);
			}
		}
	} while (num == IOERROR_FINALIZE_UID_NUM);
	return ret;
}
