/* SceSdstor module initialization. */
#include <scetypes.h>
#include "sdstor_internal.h"

int module_start(SceSize arglen, const void *argp)
{
	int result;

	if (sdstorRegisterUsbMassInterrupt() != 0)
		return 2;
	if (sddev_init() != 0)
		return 2;
	if (sdpart_init() != 0)
		return 2;
	if (sdstor_initialize() != 0)
		return 2;
	if (sdstor_vfs_initialize() != 0)
		return 2;
	result = sdctrl_initialize();
	if (result != 0)
		return 2;
	return result;
}
