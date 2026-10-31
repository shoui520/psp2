#ifndef SCE_SHELLSVC_H
#define SCE_SHELLSVC_H

#include <stdint.h>

/* ARM32 ShellSvc virtual request ABI used by dialog and utility clients. */
struct ShellSvcInterface {
	void **methods;
};

struct ShellSvcBuffer {
	void *address;
	uint32_t size;
};

typedef int (*ShellSvcRequest)(struct ShellSvcInterface *, uint32_t,
			       const struct ShellSvcBuffer *, uint32_t,
			       int *, const struct ShellSvcBuffer *, uint32_t);

#ifdef __cplusplus
namespace IPMI {
class Client;
struct EventFlagInfo;
}

/* ShellSvc's exports as ShellUtil calls them (shell_svc_client.cpp, not converted yet). */
extern "C" {
IPMI::Client *SceShellSvc_B31E7F1C(void);
int SceShellSvc_D943CE15(int event, int (*callback)(int event), void *pArg);
int SceShellSvc_7CBF442B(int event);
int SceShellSvc_FB03FBC9(int event);
int SceShellSvc_A0B067AC(IPMI::EventFlagInfo *pEventFlag, int index);
int SceShellSvc_8D4F9F2E(int index, SceUInt32 *pTimeout);
}
#else
extern struct ShellSvcInterface *SceShellSvc_B31E7F1C(void);
#endif

#endif
