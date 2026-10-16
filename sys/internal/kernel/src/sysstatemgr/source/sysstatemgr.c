/* SceSysStateMgr: complete the kernel bootstrap and execute its configuration. */
#include "sysstatemgr.h"

const char *s_bootimage;
int s_noRemoteRpath;
int s_noRemoteModule;
SceUID s_appProcess;

int SysStateMgrModuleStart(SceSize arglen, const void *argp)
{
	SceUID reserved;
	int ret, time;

	ksceKernelInvokeInitCallback(5);
	ksceKernelFinalizeKbl();
	ksceKernelAddressSpaceUnmap(SCE_KERNEL_ADDRESS_SPACE_ID, 0xf7, 0xc, (void *)0x51000000, 0x1000000);
	ksceKernelAddressSpaceUnmap(SCE_KERNEL_ADDRESS_SPACE_ID, 0xf0, 0xc, (void *)0xe0000000, 0x8000000);
	ksceKernelAddressSpaceUnmap(SCE_KERNEL_ADDRESS_SPACE_ID, 0xf0, 0xc, (void *)0x1a000000, 0x100000);
	SceSysmemForKernel_16A54BFE();
	time = (int)sceKernelGetSystemTimeWide();
	SCE_PRINTF_WITH_INFO(0, 129, "\nPSP2 Kernel Time : %d [msec]\n\n", time / 1000);
	ksceKernelRegisterModulesAfterBoot();
	if (!SceSysrootForKernel_ksceSysrootIsExternalBootMode()) {
		s_bootimage = "os0:kd/bootimage.skprx";
		ConfReader_mountDefaultBootimage();
	}
	if (ksceSblQafMgrIsAllowHost0Access()) {
		ret = ConfReaderExecCommand("host0:psp2config.skprx", 0);
		if (ret != CONF_NOT_FOUND)
			goto configured;
	}
	if (ksceSysrootIsManufacturingMode()) {
		ret = ConfReaderExecCommand("sd0:psp2config.skprx", 0);
		if (ret != CONF_NOT_FOUND)
			goto configured;
		ret = ConfReaderExecCommand("ux0:psp2config.skprx", 0);
		if (ret != CONF_NOT_FOUND)
			goto configured;
	}
	ret = ConfReaderExecCommand(ksceSblAimgrIsDolce() ?
		"os0:psp2config_dolce.skprx" : "os0:psp2config_vita.skprx", 0);
configured:
	ksceKernelUmountBootfs();
	reserved = ksceKernelAllocPartitionMemBlock(SCE_KERNEL_PARTITION_ROOT_ID, "SceSysstatemgrReserved",
		CONF_MEMTYPE, SYSSTATEMGR_RESERVED_SIZE, NULL);
	SCE_DBG_ASSERT_LEVEL(0, reserved > 0, 202);
	ksceKernelUnmapMemBlock(reserved);
	if (ret < 0)
		SceSysrootForKernel_47724459(2);
	else if (s_appProcess <= 0)
		SceSysrootForKernel_47724459(0);
	time = (int)sceKernelGetSystemTimeWide();
	SCE_PRINTF_WITH_INFO(0, 218, "\nPSP2 Kernel Booting Time : %d [msec]\n\n", time / 1000);
	afterBoot();
}
