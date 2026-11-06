/* SceLibLocationInternal's start: SceTinyLibc's runtime up. */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/modulemgr.h>
#include <modulemgr/module_info.h>
#include "../../../../../../lowlayer/src/tinylibc/tinylibc.h"

/* the threads module_start and module_stop run on */
extern const SceModuleThreadParam sce_module_start_thread_parameter = {
	4, 0, 0x1000, 0, 0
};
extern const SceModuleThreadParam sce_module_stop_thread_parameter = {
	4, 0, 0x1000, 0, 0
};

extern "C" int module_start(SceSize args, const void *argp)
{
	(void)args;
	(void)argp;
	tinyLibcInit();
	return SCE_KERNEL_START_SUCCESS;
}
