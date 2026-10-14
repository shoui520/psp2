/* Process capability queries used by Sysroot and DIP-switch services. */
#include <stdint.h>
#include <psp2kern/types.h>
#include <sce/acmgr.h>

/* Names unknown; capabilities are numbered most-significant bit first. */
__attribute__((noinline))
int CheckProcessCapability(SceUID processId, unsigned int capability)
{
	SceSelfAuthInfo info;

	if (ksceSysrootGetSelfAuthInfo(processId, &info))
		return 0;
	return (info.capability[(int32_t)capability >> 3] >> (~capability & 7)) & 1;
}

/* only these capabilities can be checked this way */
#define CAPABILITY_DEBUG		1
#define CAPABILITY_85			0x85	/* PLACEHOLDER: name */

#define ACMGR_ERROR_UNKNOWN_CAPABILITY	0x800f0916
#define ACMGR_ERROR_NO_CAPABILITY	0x800f0902

/* 0 when the process has the capability */
int CheckProcessCapabilityGranted(SceUID processId, unsigned int capability)
{
	if (capability != CAPABILITY_DEBUG && capability != CAPABILITY_85)
		return ACMGR_ERROR_UNKNOWN_CAPABILITY;
	return CheckProcessCapability(processId, capability) == 1 ? 0 : ACMGR_ERROR_NO_CAPABILITY;
}

__attribute__((noinline))
int CheckProcessDebugCapability(SceUID processId)
{
	return CheckProcessCapability(processId, 1);
}
