/*
 * paf::ModuleImpl: one loaded module. It is loaded when made, started by the
 * first Module of its path, and stopped and unloaded when deleted.
 */
#include <module/module.h>
#include <std/string.h>
#include <std/stdlib.h>
#include <kernel/modulemgr.h>

using namespace paf;

enum {
	/* sceKernelLoadModule's flag for the common dialogs' modules */
	LOAD_MODULE_FLAG_CDLG = 0x10,
	ERROR_LOAD_MODULE = -2135991552,	/* 0x80AF5B00 */
	ERROR_START_MODULE = -2135991551	/* 0x80AF5B01 */
};

ModuleImpl::ModuleImpl(const char *name, int32_t option,
		       const Module::ExtraOption *extra_option)
	: name(name), handle(0)
{
	SceKernelLoadModuleOpt opt;

	this->option = option;
	ref_count = 1;
	result = 0;
	sce_paf_memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	if ((this->option & Module::Option_LoadTypeCdlg) != 0)
		handle = sceKernelLoadModule(name, LOAD_MODULE_FLAG_CDLG, &opt);
	else
		handle = sceKernelLoadModule(name, 0, &opt);
	if (handle < 0) {
		sce_paf_printf_dummy();
		result = ERROR_LOAD_MODULE;
		handle = 0;
	}
}

ModuleImpl::~ModuleImpl()
{
	if (handle > 0) {
		SceKernelStopModuleOpt stop_opt;
		int status;
		SceKernelUnloadModuleOpt unload_opt;

		sce_paf_memset(&stop_opt, 0, sizeof(stop_opt));
		stop_opt.size = sizeof(stop_opt);
		if (sceKernelStopModule(handle, 0, NULL, 0, &stop_opt, &status) < 0) {
			sce_paf_printf_dummy();
		} else {
			sce_paf_memset(&unload_opt, 0, sizeof(unload_opt));
			unload_opt.size = sizeof(unload_opt);
			if (sceKernelUnloadModule(handle, 0, &unload_opt) < 0)
				sce_paf_printf_dummy();
		}
	}
}

void ModuleImpl::Start(Module *referer, const char *funcname)
{
	if (handle <= 0)
		return;

	SceKernelStartModuleOpt opt;
	int status;
	int ret;

	sce_paf_memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	if ((option & Module::Option_UseInterface) != 0)
		ret = sceKernelStartModule(handle, sizeof(referer), &referer, 0, &opt, &status);
	else
		ret = sceKernelStartModule(handle, 0, NULL, 0, &opt, &status);
	if (ret < 0) {
		sce_paf_printf_dummy();
		result = ERROR_START_MODULE;
	}
}
