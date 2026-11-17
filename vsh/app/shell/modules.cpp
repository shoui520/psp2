/*
 * The modules the shell loads at start: its two external libraries (from the host's vs0 when
 * the QA flags allow it) and the system modules it uses. The file's and every name here are
 * ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <libkernel/libkernel.h>
#include <ss_mgr/ss_mgr.h>
#include <sysmodule/sysmodule.h>
#include "shell.h"

#define SHELL_EXTERNAL_MODULE_COUNT	2
#define SHELL_SYSMODULE_COUNT		12

static const char *const s_externalModules[SHELL_EXTERNAL_MODULE_COUNT] = {
	"sys/external/libshellsvc.suprx",
	"sys/external/libcdlg.suprx",
};

static const SceUInt32 s_sysmodules[SHELL_SYSMODULE_COUNT] = {
	SCE_SYSMODULE_INTERNAL_SQLITE_VSH,
	SCE_SYSMODULE_INTERNAL_BXCE,
	SCE_SYSMODULE_INTERNAL_INI_FILE_PROCESSOR,
	SCE_SYSMODULE_INTERNAL_COMMON_GUI_DIALOG,
	SCE_SYSMODULE_INTERNAL_DBUTIL,
	SCE_SYSMODULE_NET,
	SCE_SYSMODULE_HTTPS,
	SCE_SYSMODULE_NP,
	SCE_SYSMODULE_NP_UTILITY,
	SCE_SYSMODULE_INTERNAL_NP_ACTIVITY_NET,
	SCE_SYSMODULE_INTERNAL_ACTIVITY_DB,
	SCE_SYSMODULE_NP_WEBAPI,
};

static SceUID s_externalModuleIds[SHELL_EXTERNAL_MODULE_COUNT];

int shellModulesLoad(void)
{
	int ret = 0;
	char argument = 1;
	int status;

	for (unsigned int i = 0; i < SHELL_EXTERNAL_MODULE_COUNT; i++) {
		const char *name = s_externalModules[i];
		char hostPath[128];
		char path[128];
		SceUID id = -1;

		if (sceSblQafMgrIsAllowRemoteSysmoduleLoad()) {
			sce_paf_snprintf(hostPath, sizeof(hostPath), "host0:vs0/%s", name);
			id = sceKernelOpenModule(hostPath, sizeof(argument), &argument, 0, NULL,
						 &status);
			if (id <= 0)
				id = -1;
		}
		if (id == -1) {
			sce_paf_snprintf(path, sizeof(path), "vs0:%s", name);
			id = sceKernelOpenModule(path, sizeof(argument), &argument, 0, NULL, &status);
		}
		s_externalModuleIds[i] = id;
		if (id < 0)
			ret = id;
	}
	for (unsigned int i = 0; i < SHELL_SYSMODULE_COUNT; i++) {
		status = sceSysmoduleLoadModuleInternal(s_sysmodules[i]);
		if (status != 0)
			ret = status;
	}
	return ret;
}
