/* Sony entry.c: module initialization and callback registration. */
#include <psp2kern/types.h>
#include <psp2kern/kernel/proc_event.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/kernel/threadmgr/workqueues.h>

extern const SceProcEventHandler codecEngineProcessEvents;
extern int codecEngineStartPhase2(void);
extern unsigned int codecEngineSystemEventHandler(int phase, unsigned int event);
extern int _sceVeneziaProcessCreateEntry(void);
extern int initCodecEngine(void);

int module_start(SceSize arglen, const void *argp)
{
	if (_sceVeneziaProcessCreateEntry() < 0)
		return 2;
	if (initCodecEngine() < 0)
		return 2;
	if (ksceKernelEnqueueWorkQueue(0x10023, "SceCodecEngineBoot",
		(SceKernelWorkQueueWorkFunction)codecEngineStartPhase2, NULL) < 0)
		return 2;
	if (ksceKernelRegisterSysEventHandler("SceVenezia",
		(SceSysEventHandler)codecEngineSystemEventHandler, NULL) < 0)
		return 2;
	if (ksceKernelRegisterProcEventHandler("SceVenezia",
		&codecEngineProcessEvents, 0) < 0)
		return 2;
	return 0;
}
