#include "guard.h"
#include "paf_crt_mgr.h"

int ScePafStdc_CB26B866(uint32_t *guard)
{
	if (PafCrtManagerGet())
		PafCrtManagerLock(PafCrtManagerGet(), 0);
	else
		__breakpoint(0);
	if (*guard & 1) {
		if (PafCrtManagerGet())
			PafCrtManagerUnlock(PafCrtManagerGet(), 0);
		else
			__breakpoint(0);
		return 0;
	}
	*guard = 2;
	return 1;
}

int ScePafStdc_6AB297BE(uint32_t *guard)
{
	*guard = 1;
	if (PafCrtManagerGet())
		return PafCrtManagerUnlock(PafCrtManagerGet(), 0);
	__breakpoint(0);
	return 0;
}

int ScePafStdc_DDD8FFEA(uint32_t *guard)
{
	*guard = 0;
	if (PafCrtManagerGet())
		return PafCrtManagerUnlock(PafCrtManagerGet(), 0);
	__breakpoint(0);
	return 0;
}
