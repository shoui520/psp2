/* Descriptive filename; registration tables read from the original module. */
#include "hid_internal.h"

const SceKernelProcEventHandler hidProcessHandlers = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = hidProcessCreate,
	.exit = hidProcessExit,
	.kill = hidProcessKill,
	.suspend = hidProcessStop,
	.resume = processResumeStart,
	.notify = NULL,
};

const SysrootHidOps hidSysrootOps = {
	.size = sizeof(SysrootHidOps),
	.dumpDevices = hidDumpDevices,
	.dumpLists = hidDumpLists,
};
