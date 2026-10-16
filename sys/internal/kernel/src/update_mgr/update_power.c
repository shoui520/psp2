#include "update_internal.h"

int sceSblUsPowerControlForKernel(int operation, int flags)
{
	return SpkgUtilPowerControl(operation, flags);
}
