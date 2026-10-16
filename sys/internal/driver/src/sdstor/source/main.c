/* SceSdstor module initialization. */
#include <psp2kern/types.h>

int sdstorRegisterUsbMassInterrupt(void);
int sddev_init(void);
int sdpart_init(void);
int sdstor_initialize(void);
int sdstor_vfs_initialize(void);
int sdctrl_initialize(void);

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
