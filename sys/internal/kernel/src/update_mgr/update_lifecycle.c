#include <sysmem/sysroot.h>
#include <iofilemgr/iofilemgr.h>
#include <threadmgr/threadmgr.h>
#include <syscon/syscon.h>
#include <sysstatemgr/sysstatemgr.h>
#include "update_internal.h"
#include <sysmem/qafmgr.h>

int module_start(SceSize arglen, const void *argp)
{
	SceIoStat stat;
	int hostUpdate = 0;
	int mode;
	int result;
	SceUID thread;

	if (UpdateUtilInit() != 0 || StorageUtilInit() != 0)
		goto finish;
	updateSysconHardwareInfo = SceSysconForDriver_CBD6D8BC();
	updateSysconFirmwareVersion = SceSysconForDriver_FF86F4C5();
	SceSysconForDriver_D2F456DC((unsigned char *)&updateSysconDownloaderVersion);
	if (updateSysconFirmwareVersion > 0x400ff) {
		updateUseSysconNvsService = 1;
		updateUseMotionService = 1;
		if (updateSysconFirmwareVersion > 0x90902) {
			updateUseSysconUpdateService = 1;
			updateUseSysconConfigStorage = 1;
		} else if (updateSysconFirmwareVersion <= 0x704ff) {
			goto checkDownloader;
		}
		if (updateSysconHardwareInfo != 0x101003)
			updateUseSysconPowerControl = 1;
	}
checkDownloader:
	mode = SceSysconForDriver_EBE3262C();
	if (mode < 0 || (mode & 0x40)) {
		updateSysconDownloader = 1;
		if ((updateSysconDownloaderVersion & 0xffffff) >= 0x3600)
			updateUseSysconSnvs = 1;
	} else {
		updateSysconDownloader = 0;
		if (updateSysconFirmwareVersion > 0x90902)
			updateUseSysconSnvs = 1;
	}
	if (SceQafMgrForDriver_3CB55F98() &&
	    sceIoGetstat("host0:PSP2UPDAT.PUP", &stat) == 0) {
		hostUpdate = 1;
		updateInitializeAsync = 0;
	}
	if (SceSysrootForDriver_55392965() || SceSysrootForDriver_B0E1FC67())
		updateInitializeAsync = 0;
	if (updateInitializeAsync) {
		thread = sceKernelCreateThread("SceSblUsInitialize",
			sceSblUpdateMgrInitPeripheralsPhase2, 253, 8192, 0x50000, 2, 0);
		if (thread < 0)
			goto finish;
		if (sceKernelStartThread(thread, 4, &updateThreadArgument) != 0) {
			sceKernelDeleteThread(thread);
			goto finish;
		}
	} else if (sceSblUpdateMgrInitPeripheralsPhase2(4, &updateThreadArgument) != 0) {
		goto finish;
	}
	if (SceQafMgrForDriver_3CB55F98() && hostUpdate && updateInitialized) {
		result = UpdateMgrdoUpdate(UpdateUtilGetHeapId(), "host0:PSP2UPDAT.PUP");
		if (result != (int)0x800f0202) {
			if (result == 0) {
				SceSysrootForDriver_93CD44CD(8);
				sceSblUsPowerControlForKernel(1, 1);
				for (;;)
					;
			}
			goto finish;
		}
	}
	if (updateInitializeAsync) {
		thread = sceKernelCreateThread("SceSblUsInvalidateVersion",
			invalidate_prev_system_info, 253, 8192, 0x50000, 2, 0);
		if (thread >= 0 && sceKernelStartThread(thread, 4, &updateThreadArgument) != 0)
			sceKernelDeleteThread(thread);
	} else {
		invalidate_prev_system_info(4, &updateThreadArgument);
	}
finish:
	UpdateMgrRegisterEventHandlers();
	return 0;
}
