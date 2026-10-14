/* Descriptive filename; registration tables read from the original module. */
#include "hid_internal.h"

const SceProcEventHandler hidProcessHandlers = {
	.size = sizeof(SceProcEventHandler),
	.create = hidProcessCreate,
	.exit = hidProcessExit,
	.kill = hidProcessKill,
	.stop = hidProcessStop,
	.start = processResumeStart,
	.switch_process = NULL,
};

const SysrootHidOps hidSysrootOps = {
	.size = sizeof(SysrootHidOps),
	.dumpDevices = hidDumpDevices,
	.dumpLists = hidDumpLists,
};
