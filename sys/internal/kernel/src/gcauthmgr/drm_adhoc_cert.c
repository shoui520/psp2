/* Adhoc device certificates stored in IdStorage. */
#include <threadmgr/threadmgr.h>
#include "adhoc.h"
#include <idstorage/idstorage.h>

GcAuthAdhocCertificate gcAuthAdhocCertificate;

int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
		size_t size, int keyId);

int GcAuthAdhocReadCertificate224(void)
{
	int permission;
	int result;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	if (sceIdStorageLookup(0x44, 392,
		gcAuthAdhocCertificate.certificate224.serialized, 120) < 0 ||
	    sceIdStorageLookup(0x45, 0,
		gcAuthAdhocCertificate.certificate224.serialized + 120, 112) < 0) {
		sceKernelSetThreadAccessLevel(permission);
		return (int)0x808a010e;
	}
	result = (int)0x808a0107;
	if (GcAuthExecuteKirkCommand(NULL,
		&gcAuthAdhocCertificate.certificate224, 25, 232, 0) == 0)
		result = 0;
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

