/*
 * SceSysmem: system event handlers (sysevent.c). Each handler is a UID object of
 * SceUIDSysEventClass on one list; a dispatch calls them in registration
 * order until one fails.
 */

#include "sysmem_internal.h"
#include "sce/sysevent.h"
#include "sysevent_internal.h"

/* a handler's list link */
typedef struct SysEventLink {
	struct SysEventLink *next;
	struct SysEventLink *prev;
	SceUID uid;
	SceKernelSysEventHandler handler;
	void *args;
} SysEventLink;

typedef struct SysEventObject {
	SceObjectBase base;
	SysEventLink link;			/* 0x08 */
	SceUInt32 reserved;
} SysEventObject;

_Static_assert(sizeof(SysEventObject) == 0x20, "SysEventObject size");

void (*g_sysEventPreHook)(int id, int event, void *args, void *handler);
void (*g_sysEventPostHook)(int id, int event, int result);

/* the list head (next, prev) and SceUIDSysEventClass */
static SysEventLink *s_sysEventList[2];
static SceClass s_sysEventClass;

static int SysEventCreate(void *object)
{
	return 0;
}

static int SysEventDestroy(void *object)
{
	SysEventLink *link = &((SysEventObject *)object)->link;

	link->next->prev = link->prev;
	link->prev->next = link->next;
	link->next = link;
	link->prev = link;
	return 0;
}

void SceKernelSuspendForDriver_D6124071(void *hook)
{
	g_sysEventPreHook = hook;
}

void SceKernelSuspendForDriver_B5C58EE8(void *hook)
{
	g_sysEventPostHook = hook;
}

int sceKernelSysEventInit(void)
{
	int ret;

	s_sysEventList[0] = (SysEventLink *)s_sysEventList;
	s_sysEventList[1] = (SysEventLink *)s_sysEventList;
	ret = ksceUIDClassInitClass(&s_sysEventClass, "SceUIDSysEventClass", ksceKernelGetUIDClass(),
				    sizeof(SysEventObject), SysEventCreate, SysEventDestroy);
	if (ret < 0)
		SCE_DBG_PRINTF(0xb, 102, "sceUIDClassInitClass() failed: 0x%08X\n", ret);
	return ret;
}

SceUID ksceKernelRegisterSysEventHandler(const char *name, SceKernelSysEventHandler handler,
					  void *args)
{
	SysEventObject *object;
	SysEventLink *tail;
	SceUID uid;

	uid = ksceGUIDKernelCreate(&s_sysEventClass, name, (SceObjectBase **)&object);
	if (uid < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 118, "sceKernelRegisterSysEventHandler",
				    "sceGUIDKernelCreate() failed: 0x%08X\n", uid);
		return uid;
	}
	tail = s_sysEventList[1];
	object->link.next = (SysEventLink *)s_sysEventList;
	object->link.prev = tail;
	object->link.uid = uid;
	object->link.handler = handler;
	object->link.args = args;
	s_sysEventList[1] = &object->link;
	tail->next = &object->link;
	return uid;
}

int ksceKernelUnregisterSysEventHandler(SceUID uid)
{
	SceObjectBase *object;
	int ret;

	ret = ksceGUIDGetObjectWithClass(uid, &s_sysEventClass, &object);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 140, "sceKernelUnregisterSysEventHandler",
				    "sceGUIDGetObjectWithClass() failed: 0x%08X\n", ret);
		return ret;
	}
	return ksceGUIDClose(uid);
}

/* every handler in order; the first failure stops it, *failed is its UID */
int ksceKernelSysEventDispatch(int id, int event, void *eventArgs, SceUID *failed)
{
	SysEventLink *link;
	int state, ret = 0;

	state = ksceKernelCpuSuspendIntr();
	for (link = s_sysEventList[0]; link != (SysEventLink *)s_sysEventList; link = link->next) {
		ksceKernelCpuResumeIntr(state);
		if (g_sysEventPreHook != NULL)
			g_sysEventPreHook(id, event, link->args, link->handler);
		ret = link->handler(id, event, eventArgs, link->args);
		state = ksceKernelCpuSuspendIntr();
		if (g_sysEventPostHook != NULL)
			g_sysEventPostHook(id, event, ret);
		if (ret < 0 && failed != NULL) {
			*failed = link->uid;
			ksceKernelCpuResumeIntr(state);
			return ret;
		}
	}
	ksceKernelCpuResumeIntr(state);
	return ret;
}
