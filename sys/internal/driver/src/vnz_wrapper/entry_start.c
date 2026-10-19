/* Sony entry.c: module initialization and callback registration. */
#include <stddef.h>
#include <scetypes.h>
#include <sysmem/procevent.h>
#include <sysmem/suspend.h>
#include <threadmgr/threadmgr.h>
#include "venezia_internal.h"

int module_start(SceSize arglen, const void *argp)
{
	if (_sceVeneziaProcessCreateEntry() < 0)
		return 2;
	if (initCodecEngine() < 0)
		return 2;
	if (sceKernelScheduleWorkTask(0x10023, "SceCodecEngineBoot",
		codecEngineStartPhase2, NULL) < 0)
		return 2;
	if (sceKernelRegisterSysEventHandler("SceVenezia",
		(SceKernelSysEventHandler)codecEngineSystemEventHandler, NULL) < 0)
		return 2;
	if (sceKernelRegisterProcEventHandler("SceVenezia",
		&codecEngineProcessEvents, NULL) < 0)
		return 2;
	return 0;
}
