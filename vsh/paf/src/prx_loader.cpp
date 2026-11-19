/*
 * The PAF PRX loader: an application's static object whose constructor loads ScePaf before
 * the application's other constructors run, with the global heap the application asks for.
 * Applications compile this file (libpaf does not): it is the header code each of them carries.
 * The file's, the class's and the settings' names are ours (no record names them); the
 * message is Sony's.
 *
 * The application's settings, from its build:
 *   PAF_PRX_LOADER_HEAP_SIZE		the global heap's size
 *   PAF_PRX_LOADER_RESERVED0/1		ScePafInit's reserved words
 */
#include <stddef.h>
#include <scetypes.h>
#include <sysmodule/sysmodule.h>
#include <libkernel/libkernel.h>

#include "module.h"

#ifndef PAF_PRX_LOADER_HEAP_SIZE
#define PAF_PRX_LOADER_HEAP_SIZE	0x500000
#endif
#ifndef PAF_PRX_LOADER_RESERVED0
#define PAF_PRX_LOADER_RESERVED0	60000
#endif
#ifndef PAF_PRX_LOADER_RESERVED1
#define PAF_PRX_LOADER_RESERVED1	0x40000
#endif

/* what the sysmodule leaves in the status before the module's start writes it */
#define PAF_PRX_LOADER_STATUS_UNSET	0xDEADBEEF

extern "C" void *__dso_handle;
extern "C" void sce_paf_private_atexit(void *dso);

/* the settings, global so the application may change them before the loader runs */
SceUInt32 pafPrxLoaderHeapSize = PAF_PRX_LOADER_HEAP_SIZE;
SceInt32 pafPrxLoaderReserved0 = PAF_PRX_LOADER_RESERVED0;
SceInt32 pafPrxLoaderReserved1 = PAF_PRX_LOADER_RESERVED1;
SceInt32 pafPrxLoaderHeapAlignment;
SceInt32 pafPrxLoaderHeapDisableAssertOnAllocFailure;

/* the sysmodule call with the module's status: a header function, out of line */
__attribute__((noinline)) inline int pafPrxLoaderLoadModule(SceUInt32 id, SceSize args, void *argp,
							   SceUInt32 flags, int *status)
{
	SysmoduleUserOpt opt;

	opt.flags = flags;
	opt.status = status;
	return sceSysmoduleLoadModuleInternalWithArg(id, args, argp, &opt);
}

#ifdef PAF_PRX_LOADER_KEEP_UNLOAD
/* the unload call beside it, which a module whose build keeps unused functions carries */
__attribute__((noinline)) int pafPrxLoaderUnloadModule(SceUInt32 id, SceSize args, void *argp,
							     SceUInt32 flags, int *status)
{
	SysmoduleUserOpt opt;

	opt.flags = flags;
	opt.status = status;
	return sceSysmoduleUnloadModuleInternalWithArg(id, args, argp, &opt);
}
#endif

class PafPrxLoader {
public:
	PafPrxLoader()
	{
		ScePafInit init;
		int status;
		int ret;

		init.global_heap_size = pafPrxLoaderHeapSize;
		init.reserved[0] = pafPrxLoaderReserved0;
		init.reserved[1] = pafPrxLoaderReserved1;
		init.cdlg_mode = 0;
		init.global_heap_alignment = pafPrxLoaderHeapAlignment;
		init.global_heap_disable_assert_on_alloc_failure =
			pafPrxLoaderHeapDisableAssertOnAllocFailure;
		status = (int)PAF_PRX_LOADER_STATUS_UNSET;
		ret = pafPrxLoaderLoadModule(SCE_SYSMODULE_INTERNAL_PAF, sizeof(init), &init, 0,
					     &status);
		if ((ret | status) != 0) {
			sceClibPrintf("[PAF PRX Loader] Failed to load the PAF prx. (return value 0x%x, result code 0x%x)\n",
				      ret, status);
			__breakpoint(0);
		}
		sce_paf_private_atexit(__dso_handle);
	}

	~PafPrxLoader()
	{
	}
};

static PafPrxLoader s_pafPrxLoader;
