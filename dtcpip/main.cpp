/*
 * swagner (vs0:app/NPXS10080/eboot.bin): the application's main. The process's application
 * parameter picks the mode: "dtcpip::reset" clears the DTCPIP registry (only with the QAF
 * manager's permission) and returns; anything else keeps the title id the parameter carries
 * after "dtcpip:" and runs the DTCP-IP middleware, gated on a tool system's MagicGate
 * permission.
 */

/* the shared scetypes.h's C++03 static-assert fallback declares a symbol, which clashes
 * with a header that expands it inside an extern "C" block; a typedef has no linkage */
#ifdef __cplusplus
#undef _Static_assert
#define _Static_assert(expr, msg) typedef char _StaticAssert[(expr) ? 1 : -1]
#endif

#include <stddef.h>
#include <scetypes.h>
#include <string.h>
#include <appmgr.h>
#include <kernel/processmgr.h>
#include "appmgr/appmgr.h"

/* the application's identity, from the parameter after "dtcpip:" */
char g_swagnerIdentity[16];

extern "C" int vshSblAimgrIsTool(void);
extern "C" int vshSblQafMgrIsAllowLoadMagicGate(void);
extern "C" int vshSblQafMgrIsAllowDtcpIpReset(void);
extern "C" int ksceRegMgrSetKeyBin(const char *category, const char *name, const void *value,
				  int size);

/* the middleware's main (SwMwMain) */
extern "C" int swagnerMain(void);

/* the key manager: its handle and its reset (SWKeyManager) */
extern "C" int swKeyManagerOpen(void);
extern "C" void swKeyManagerReset(int handle);

extern "C" int main(int argc, char *argv[])
{
	bool allowReset;
	bool isTool;
	bool allowMagicGate;
	int res;
	char devCert[3];
	char param[1024];

	res = 0;
	res = sceAppMgrGetAppParam(param);
	if (memcmp(param, "dtcpip::reset", 13) == 0) {
		allowReset = vshSblQafMgrIsAllowDtcpIpReset() != 0;
		if (allowReset) {
			swKeyManagerReset(swKeyManagerOpen());
			memset(devCert, 0, 3);
			res = ksceRegMgrSetKeyBin("/CONFIG/DTCPIP", "devcert", devCert, 3);
		}
		return res;
	}

	g_swagnerIdentity[0] = 0;
	if (memcmp(param, "dtcpip:", 7) == 0) {
		memcpy(g_swagnerIdentity, param + 8, 9);
		g_swagnerIdentity[9] = 0;
	}
	isTool = vshSblAimgrIsTool() == 1;
	if (isTool) {
		allowMagicGate = vshSblQafMgrIsAllowLoadMagicGate() != 0;
		if (!allowMagicGate)
			return res;
	}
	res = swagnerMain();
	sceKernelExitProcess(res);
	return res;
}
