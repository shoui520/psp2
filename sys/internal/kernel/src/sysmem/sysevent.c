/*
 * SceSysmem: system event handlers (sysevent.c). Each handler is a UID object of
 * SceUIDSysEventClass on one list; a dispatch calls them in registration
 * order until one fails.
 */

#include "sysmem_internal.h"
#include <sysmem/sysevent.h>
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

/* the handler object a list link belongs to */
#define SYS_EVENT_OBJECT(l) \
	((SysEventObject *)((char *)(l) - offsetof(SysEventObject, link)))

_Static_assert(sizeof(SysEventObject) == 0x20, "SysEventObject size");

void (*g_sysEventPreHook)(int id, int event, void *args, void *handler);
void (*g_sysEventPostHook)(int id, int event, int result, SceUID failed);

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
	ret = SceSysmemForDriver_61317102(&s_sysEventClass, "SceUIDSysEventClass", SceSysmemForDriver_85336A1C(),
				    sizeof(SysEventObject), SysEventCreate, SysEventDestroy);
	if (ret < 0)
		SCE_DBG_PRINTF(0xb, 102, "sceUIDClassInitClass() failed: 0x%08X\n", ret);
	return ret;
}

SceUID sceKernelRegisterSysEventHandler(const char *name, SceKernelSysEventHandler handler,
					  void *args)
{
	SysEventObject *object;
	SysEventLink *tail;
	SceUID uid;

	uid = SceSysmemForDriver_56A13E90(&s_sysEventClass, name, (SceObjectBase **)&object);
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

int sceKernelUnregisterSysEventHandler(SceUID uid)
{
	SceObjectBase *object;
	int ret;

	ret = SceSysmemForKernel_0E43E113(uid, &s_sysEventClass, &object);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 140, "sceKernelUnregisterSysEventHandler",
				    "sceGUIDGetObjectWithClass() failed: 0x%08X\n", ret);
		return ret;
	}
	return sceGUIDClose(uid);
}

/* every handler in order; the first failure stops it, *failed is its UID */
int SceKernelSuspendForDriver_D4622EA8(int id, int event, void *eventArgs, SceUID *failed)
{
	SysEventObject *object;
	int state, ret = 0;

	state = SceCpuForDriver_821FC0EE();
	for (object = SYS_EVENT_OBJECT(s_sysEventList[0]);
	     &object->link != (SysEventLink *)s_sysEventList;
	     object = SYS_EVENT_OBJECT(object->link.next)) {
		sceKernelCpuResumeIntr(state);
		if (g_sysEventPreHook != NULL)
			g_sysEventPreHook(id, event, object->link.args, object->link.handler);
		ret = object->link.handler(id, event, eventArgs, object->link.args);
		state = SceCpuForDriver_821FC0EE();
		if (g_sysEventPostHook != NULL)
			g_sysEventPostHook(id, event, ret,
					   ret >= 0 ? 0 : failed != NULL ? object->link.uid : 0);
		if (ret < 0 && failed != NULL) {
			*failed = object->link.uid;
			sceKernelCpuResumeIntr(state);
			return ret;
		}
	}
	sceKernelCpuResumeIntr(state);
	return ret;
}
