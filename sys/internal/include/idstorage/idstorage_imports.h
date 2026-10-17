/* IdStorage dependencies recovered from the storage driver's call sites. */
#pragma once
#include <psp2kern/types.h>
#include <sysmem/suspend.h>

struct SceIdStorageAccessAttr {
	SceUInt32 type;
	void *value;
	SceUInt32 size;
};

int SceSblACMgrForDriver_BE5667C5(SceUID pid,
				struct SceIdStorageAccessAttr *attr, SceUInt32 access);
int SceThreadmgrForDriver_56C18B11(void);
int SceThreadmgrForDriver_84EB1EA4(int flags);
