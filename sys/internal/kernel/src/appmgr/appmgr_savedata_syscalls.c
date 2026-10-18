/* Save-data slot syscall dispatch; original source filename unavailable. */

#include <psp2/appmgr.h>
#include <appmgr/appmgr_rdrfp.h>

extern int SceAppMgrForDriver_B544F2A0(SceAppMgrSaveDataSlotDelete *slot);
extern int SceAppMgrForDriver_D137D486(SceAppMgrSaveDataSlot *slot);
extern int SceAppMgrForDriver_EC9D5A35(void *status);
extern int appmgrSaveDataSlotFileGetParam(unsigned int slotId, void *param);

int appmgrSaveDataSlotInitSyscall(void *slot)
{
	return SceAppMgrForDriver_2277F736(slot);
}

int appmgrSaveDataSlotCreateSyscall(SceAppMgrSaveDataSlot *slot)
{
	return SceAppMgrForDriver_E6F389B8(slot);
}

int appmgrSaveDataSlotDeleteSyscall(SceAppMgrSaveDataSlotDelete *slot)
{
	return SceAppMgrForDriver_B544F2A0(slot);
}

int appmgrSaveDataSlotSetParamSyscall(SceAppMgrSaveDataSlot *slot)
{
	return SceAppMgrForDriver_7E247E47(slot);
}

int appmgrSaveDataSlotGetParamSyscall(SceAppMgrSaveDataSlot *slot)
{
	return SceAppMgrForDriver_D137D486(slot);
}

int appmgrSaveDataSlotSetStatusSyscall(void *status)
{
	return SceAppMgrForDriver_9ADB00FA(status);
}

int appmgrSaveDataSlotGetStatusSyscall(void *status)
{
	return SceAppMgrForDriver_EC9D5A35(status);
}

int appmgrSaveDataSlotFileGetParamSyscall(unsigned int slotId, void *param)
{
	return appmgrSaveDataSlotFileGetParam(slotId, param);
}
