/* Allegrex write-buffer synchronization used by the IPL KIRK driver. */

#include "ipl.h"

#define SYSREG_BUS_SYNC HW(0xBD000004)

int sceSysregSync(u32 mask)
{
	sync();
	mask &= 5;
	SYSREG_BUS_SYNC = mask;
	while ((SYSREG_BUS_SYNC & mask) != 0)
		;
	return 0;
}
