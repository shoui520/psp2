/*
 * SceSysmem: SceUIDDLinkClass, the class of the double links between UID
 * entries. Sony's uid_dlink.c (NSKBL's records name it, and 0.931's os0); in
 * os0 4.00 its functions are the range before uid.c's.
 */

#include <sysmem/sysroot.h>
#include "sysmem_internal.h"

/* SceUIDDLinkClass */
static SceClass *s_dlinkClass;

/* name unknown: SceUIDDLinkClass's constructor and destructor */
static int UIDDLinkCreate(void *object)
{
	void **link = object;

	link[0] = object;
	link[2] = object;
	return 0;
}

static int UIDDLinkDestroy(void *object)
{
	void **link = object;

	link[2] = NULL;
	return 0;
}

int SceSysmemForKernel_7CDB7655(SceClass *cls, const char *name, void *uidclass, SceSize itemsize,
				SceClassCallback create, SceClassCallback destroy)
{
	return sceUIDClassInitClass(cls, name, uidclass, itemsize, create, destroy);
}

#if !SYSMEM_NSKBL
/* name unknown */
void UIDClassSetCallbacks(SceClass *cls, const char *name, SceClassCallback create,
			  SceClassCallback destroy)
{
	cls->name = name;
	cls->create_cb = create;
	cls->destroy_cb = destroy;
}
#endif

SceClass *SceSysmemForKernel_E4193EB6(void)
{
	return s_dlinkClass;
}

#if SYSMEM_NSKBL
/* SceUIDDLinkClass, made by core 0 */
static int ClassInit(UIDManager *manager)
{
	SceClass *cls;
	int ret;

	cls = sceKernelSysrootAlloc(sizeof(SceClass));
	manager->dlinkClass = cls;
	s_dlinkClass = cls;
	ret = sceUIDClassInitClass(cls, "SceUIDDLinkClass", SceSysmemForDriver_85336A1C(), 12,
				   UIDDLinkCreate, UIDDLinkDestroy);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(85);
		return ret;
	}
	return 0;
}

int UIDDLinkStart(UIDManager *manager)
{
	int ret = 0;

	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0) {
		ret = ClassInit(manager);
		if (ret < 0)
			SCE_DBG_ASSERT_FAIL(165);
	}
	sceKernelSysrootCorelockUnlock();
	return ret;
}
#else
/* name unknown */
int UIDDLinkClassInit(UIDManager *manager)
{
	SceClass *cls = manager->dlinkClass;

	s_dlinkClass = cls;
	cls->name = "SceUIDDLinkClass";
	cls->create_cb = UIDDLinkCreate;
	cls->destroy_cb = UIDDLinkDestroy;
	return 0;
}
#endif
