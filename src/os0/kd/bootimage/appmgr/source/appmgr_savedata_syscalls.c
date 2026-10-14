/* Save-data slot syscall dispatch; original source filename unavailable. */

#include <psp2/appmgr.h>

extern int ksceAppMgrSaveDataSlotInit(void *slot);
extern int ksceAppMgrSaveDataSlotCreate(SceAppMgrSaveDataSlot *slot);
extern int ksceAppMgrSaveDataSlotDelete(SceAppMgrSaveDataSlotDelete *slot);
extern int ksceAppMgrSaveDataSlotSetParam(SceAppMgrSaveDataSlot *slot);
extern int ksceAppMgrSaveDataSlotGetParam(SceAppMgrSaveDataSlot *slot);
extern int ksceAppMgrSaveDataSlotSetStatus(void *status);
extern int ksceAppMgrSaveDataSlotGetStatus(void *status);
extern int appmgrSaveDataSlotFileGetParam(unsigned int slotId, void *param);

int appmgrSaveDataSlotInitSyscall(void *slot)
{
	return ksceAppMgrSaveDataSlotInit(slot);
}

int appmgrSaveDataSlotCreateSyscall(SceAppMgrSaveDataSlot *slot)
{
	return ksceAppMgrSaveDataSlotCreate(slot);
}

int appmgrSaveDataSlotDeleteSyscall(SceAppMgrSaveDataSlotDelete *slot)
{
	return ksceAppMgrSaveDataSlotDelete(slot);
}

int appmgrSaveDataSlotSetParamSyscall(SceAppMgrSaveDataSlot *slot)
{
	return ksceAppMgrSaveDataSlotSetParam(slot);
}

int appmgrSaveDataSlotGetParamSyscall(SceAppMgrSaveDataSlot *slot)
{
	return ksceAppMgrSaveDataSlotGetParam(slot);
}

int appmgrSaveDataSlotSetStatusSyscall(void *status)
{
	return ksceAppMgrSaveDataSlotSetStatus(status);
}

int appmgrSaveDataSlotGetStatusSyscall(void *status)
{
	return ksceAppMgrSaveDataSlotGetStatus(status);
}

int appmgrSaveDataSlotFileGetParamSyscall(unsigned int slotId, void *param)
{
	return appmgrSaveDataSlotFileGetParam(slotId, param);
}
