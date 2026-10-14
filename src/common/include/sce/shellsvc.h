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

extern struct ShellSvcInterface *SceShellSvc_B31E7F1C(void);

#endif
