/*
 * What the packets tell the PS4 about this system: its software version, its PS code, whether it
 * is a PS TV, the application's title and the protocol's version.
 */
#include <libkernel/libkernel.h>
#include <modulemgr/module_info.h>
#include <kernel/threadmgr.h>
#include <appmgr.h>
#include <vshbridge/vshbridge.h>
#include "companion_util_internal.h"

namespace SceCompanionUtil {

int GetSystemSwVersionString(char *buf, SceSize size)
{
	SceKernelSystemSwVersion info;
	int ret;

	sceClibMemset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelGetSystemSwVersion(&info);
	if (ret == 0) {
		sceClibSnprintf(buf, size, "%s", info.versionString);
		ret = 0;
	}
	return ret;
}

int GetPsCodeString(char *buf, SceSize size)
{
	SceUInt16 code[4];

	vshSblAimgrGetPscode(code);
	sceClibSnprintf(buf, size, "%x:%x:%x:%x", code[0], code[1], code[2], code[3]);
	return 0;
}

SceUInt32 IsDolce(void)
{
	return vshSblAimgrIsDolce();
}

int GetAppTitle(char *buf, SceSize size)
{
	return sceAppMgrAppParamGetString(sceKernelGetProcessId(), 12, buf, size);
}

int GetProtocolVersionString(char *buf, SceSize size)
{
	sceClibSnprintf(buf, size, "%02d.%02d", 2, 0);
	return 0;
}

} /* namespace SceCompanionUtil */
