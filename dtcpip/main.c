/*
 * swagner (vs0:app/NPXS10080/eboot.bin): the application's main. The process's
 * application parameter picks the mode: "dtcpip::reset" clears the DTCPIP registry (only
 * with the QAF manager's permission) and returns; anything else runs the DTCP-IP
 * middleware (its title id in the parameter is kept), gated on a tool system's
 * MagicGate permission.
 */

#include <stddef.h>
#include <scetypes.h>
#include <string.h>
#include <appmgr.h>
#include <kernel/processmgr.h>
#include "../../../internal/include/appmgr/appmgr.h"
#include <libkernel/libkernel.h>

/* the application's identity, from the parameter after "dtcpip:" */
char g_swagnerIdentity[16];

extern int vshSblAimgrIsTool(void);
extern int vshSblQafMgrIsAllowLoadMagicGate(void);
extern int vshSblQafMgrIsAllowDtcpIpReset(void);
extern int ksceRegMgrSetKeyBin(const char *category, const char *name, const void *value, int size);

/* the middleware's main (SwMwMain) */
int swagnerMain(void);

/* the key manager's reset (SWKeyManager) */
int swKeyManagerOpen(void);
void swKeyManagerReset(int handle);

int main(int argc, char *argv[])
{
	char param[1040];
	int result;
	int handle;
	char devCert[4];

	result = sceAppMgrGetAppParam(param);
	if (memcmp(param, "dtcpip::reset", 13) != 0) {
		g_swagnerIdentity[0] = 0;
		if (memcmp(param, "dtcpip:", 7) == 0) {
			memcpy(g_swagnerIdentity, param + 8, 9);
			g_swagnerIdentity[9] = 0;
		}
		if (vshSblAimgrIsTool() != 1 || vshSblQafMgrIsAllowLoadMagicGate() != 0) {
			result = swagnerMain();
			sceKernelExitProcess(result);
		}
	} else {
		if (vshSblQafMgrIsAllowDtcpIpReset() != 0) {
			handle = swKeyManagerOpen();
			swKeyManagerReset(handle);
			memset(devCert, 0, 3);
			result = ksceRegMgrSetKeyBin("/CONFIG/DTCPIP", "devcert", devCert, 3);
		}
	}
	return result;
}
