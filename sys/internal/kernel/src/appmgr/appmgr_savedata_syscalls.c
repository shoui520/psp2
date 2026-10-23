/* Save-data slot syscall dispatch; original source filename unavailable. */

#include <appmgr/appmgr_savedata.h>
#include <appmgr/appmgr_rdrfp.h>
#include <appmgr/appmgr.h>
#include "app_state_internal.h"
#include "mount.h"
#include "appmgr_savedata_slot.h"

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

int appmgrSaveDataDataSaveSyscall(SceAppMgrSaveDataData *operation)
{
	return appmgrSaveDataDataSave(operation, 1);
}

int appmgrSaveDataDataSave2Syscall(SceAppMgrSaveDataData *operation)
{
	return appmgrSaveDataDataSave(operation, 0);
}

int appmgrSaveDataDataRemoveSyscall(SceAppMgrSaveDataDataDelete *operation)
{
	return appmgrSaveDataDataRemove(operation, 1);
}

int appmgrSaveDataDataRemove2Syscall(SceAppMgrSaveDataDataDelete *operation)
{
	return appmgrSaveDataDataRemove(operation, 0);
}

int appmgrSaveDataGetQuotaSyscall(SceAppMgrSaveDataQuota *quota)
{
	return SceAppMgrForDriver_E104C37E(quota);
}

int appmgrSaveDataGetCachedRequiredSizeKiBSyscall(void)
{
	return sceAppMgrSaveDataGetCachedRequiredSizeKiB();
}

int appmgrSaveDataMountSyscall(const SceAppMgrSaveDataMountParam *param)
{
	return SceAppMgrForDriver_E73C9516(param);
}

int appmgrSaveDataUmountSyscall(const SceAppMgrSaveDataUmountParam *param)
{
	return SceAppMgrForDriver_2B080268(param);
}

int appmgrSaveDataSlotFileOpenSyscall(const char *mountPoint,
				     AppmgrSaveDataSlotFileInfo *info)
{
	return appmgrOpenSaveDataSlotFile(mountPoint, info);
}

int appmgrSaveDataSlotFileGetParamSyscall(unsigned int slotId, void *param)
{
	return appmgrSaveDataSlotFileGetParam(slotId, param);
}

int appmgrSaveDataSlotFileCloseSyscall(const char *mountPoint)
{
	return appmgrCloseSaveDataSlotFile(mountPoint);
}

int appmgrSaveDataAddMountSyscall(const AppmgrAddMountParam *param)
{
	return sceAppMgrSaveDataAddMount(param);
}
