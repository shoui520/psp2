/*
 * SceIpmiNonGameApp (vs0:sys/external/libipmi_nongame.suprx):
 * module entry points and the unused exported variable.
 */
#include <scetypes.h>
#include <kernel/modulemgr.h>

/*
 * A 4-byte SceIpmiNonGameApp variable nothing in the library reads (0x8100c01c in .data,
 * initialized to -1). The other SceIpmiNonGameApp variables are the IPMI classes' vtables.
 */
int SceIpmiNonGameApp_3BAA521C = -1;

int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return SCE_KERNEL_STOP_SUCCESS;
}
