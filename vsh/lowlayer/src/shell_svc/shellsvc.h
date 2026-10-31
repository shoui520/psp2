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
#include "shell_svc_client.h"
#else
extern struct ShellSvcInterface *SceShellSvc_B31E7F1C(void);
/* module_start's and module_stop's (shell_svc_ipc_client.cpp) */
int SceShellSvc_E91AEAD4(void);
int shellSvcFinalize(void);
#endif

#endif
