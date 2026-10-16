/* Application-state queries registered for debugger clients. */

#include <sce/sysroot_appmgr.h>

extern int appmgrGetAppInfoByPid(int pid, void *out);
extern int appmgrGetAppInfoByAppId(int appId, void *out);
extern int appmgrGetAppList(void *out);

void appmgrRegisterDebuggerCallbacks(void)
{
	SceSysrootForDriver_2BE874EF(appmgrGetAppInfoByPid);
	SceSysrootForDriver_3B19B06B(appmgrGetAppInfoByAppId);
	SceSysrootForDriver_70AD47A9(appmgrGetAppList);
}
