/* Adhoc 160-bit device certificate. */
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "adhoc.h"
#include <idstorage/idstorage.h>

extern GcAuthAdhocCertificate gcAuthAdhocCertificate;

int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
		size_t size, int keyId);

int GcAuthAdhocReadCertificate160(void)
{
	int permission;
	int result;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	if (sceIdStorageLookup(0x42, 264,
		&gcAuthAdhocCertificate.certificate160, 184) >= 0 ||
	    sceIdStorageLookup(0x42, 264,
		&gcAuthAdhocCertificate.certificate160, 184) >= 0) {
		if (GcAuthExecuteKirkCommand(NULL,
			&gcAuthAdhocCertificate.certificate160, 18, 184, 0) != 0)
			result = (int)0x808a0107;
		else
			result = 0;
	} else {
		result = (int)0x808a010e;
	}
	sceKernelSetThreadAccessLevel(permission);
	return result;
}
