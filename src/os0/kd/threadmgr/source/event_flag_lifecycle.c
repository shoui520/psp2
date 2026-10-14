/*
 * SceKernelThreadMgr (threadmgr_for_user.c): the user side of event flag
 * lifetimes (open, close, delete by PUID) and the user name copy every
 * create and open uses.
 */

#include "private.h"
#include "event_flag.h"
#include "scheduler.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/intrmgr.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr_for_user.c"

#define UID_NAME_SIZE			32
#define UID_ATTR_NAME_CHECKED		0x00000080	/* a name too long is an error */
#define SDK_VERSION_CLOSE_MISMATCH	0x03100000	/* SDKs before 3.10 may close what they created */
#define SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS	0x80028002
#define USER_ERROR_UID_NAME_TOO_LONG	0xc0024002

extern int scePUIDGetAttr(SceUID processId, SceUID uid, uint32_t *attributes);
extern int kscePUIDGetClass(SceUID processId, SceUID uid, SceClass **cls);
extern int kscePUIDClose(SceUID processId, SceUID uid);
extern SceClass *SceSysmemForKernel_368F1C36(SceUID uid);		/* a GUID's class */
extern SceUID SceSysmemForKernel_BDE00106(const char *name);		/* a GUID by name */
extern int ksceKernelStrncpyFromUser(char *dest, const char *source, unsigned int size);

/* copyinUIDNameWithAttr: a user's object name; with UID_ATTR_NAME_CHECKED, at most 31 chars */
int copyinUIDNameWithAttr(char *name, const char *userName, uint32_t attributes)
{
	int result;

	if (userName == NULL) {
		SCE_DBG_PRINTF(0, 118, " k: error: copyinUIDNameWithAttr() pName_us=NULL\n");
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}
	result = ksceKernelStrncpyFromUser(name, userName, UID_NAME_SIZE);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 123, " k: error: copyinUIDNameWithAttr() -> "
			       "sceKernelStrncpyFromUser()=0x%08X\n", result);
		return result;
	}
	if (!(attributes & UID_ATTR_NAME_CHECKED) || result < UID_NAME_SIZE)
		return 0;
	name[UID_NAME_SIZE - 1] = '\0';
	SCE_DBG_PRINTF(0, 129, "***\n***\n***\n*** error: copyinUIDNameWithAttr(\"%s\") = "
		       "SCE_KERNEL_ERROR_UID_NAME_TOO_LONG\n***\n***\n***\n", name);
	return USER_ERROR_UID_NAME_TOO_LONG;
}

/* deletes or closes a user's event flag */
int closeEventFlagForUser(SceUID uid, int delete)
{
	SceClass *cls;
	uint32_t attributes, version;

	if (kscePUIDGetClass(0, uid, &cls) < 0 ||
	    cls != s_threadmgrRuntime.manager->eventFlagClass ||
	    scePUIDGetAttr(0, uid, &attributes) < 0)
		return USER_ERROR_UNKNOWN_EVENT_FLAG_ID;
	if (!delete)
		attributes ^= PUID_ATTR_OPENED;
	if (attributes & PUID_ATTR_OPENED) {
		if (SceSysrootForDriver_CAE820B2(0, &version) < 0)
			version = SDK_VERSION_CLOSE_MISMATCH;
		if (version >= SDK_VERSION_CLOSE_MISMATCH) {
			SCE_DBG_PRINTF(0, 1523, "*** error: closeEventFlagForUser(0x%08X, 0x%08X), "
				       "delete/close mismatch\n", uid, delete);
			return USER_ERROR_UNKNOWN_EVENT_FLAG_ID;
		}
	}
	return kscePUIDClose(0, uid);
}

/* sceKernelDeleteEventFlag (user) */
int sceKernelDeleteEventFlag(SceUID uid)
{
	return closeEventFlagForUser(uid, 1);
}

/* sceKernelCloseEventFlag (user) */
int sceKernelCloseEventFlag(SceUID uid)
{
	return closeEventFlagForUser(uid, 0);
}

/* sceKernelOpenEventFlag (user): a PUID for the event flag named userName */
int sceKernelOpenEventFlag(const char *userName)
{
	UIDCreateOpt opt;
	char name[UID_NAME_SIZE];
	int result;

	result = copyinUIDNameWithAttr(name, userName, UID_ATTR_NAME_CHECKED);
	if (result < 0)
		return result;
	result = SceSysmemForKernel_BDE00106(name);
	if (result < 0)
		return result;
	if (s_threadmgrRuntime.manager->eventFlagClass !=
	    SceSysmemForKernel_368F1C36(result))
		return SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	opt = (UIDCreateOpt){
		.flags = UID_CREATE_OPT_ATTR,
		.attr = PUID_ATTR_OPENED,
	};
	return SceSysmemForKernel_1FF2749A(currentThreadContext()->processId, result, &opt);
}

/* sceKernelSetEventFlag (user) */
int sceKernelSetEventFlag(SceUID uid, uint32_t pattern)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_FLAG_ID;
	return ksceKernelSetEventFlag(guid, pattern);
}
