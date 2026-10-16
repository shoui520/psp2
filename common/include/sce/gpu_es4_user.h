/* SceGpuEs4ForUser syscall interface used by the SGX services client. */

#pragma once

#include <stdint.h>

struct SceGpuBridgePackage {
	uint32_t id;
	uint32_t size;
	void *in;
	uint32_t in_size;
	void *out;
	uint32_t out_size;
	uint32_t h_services;
};

int PVRSRVOpen(void);
int PVRSRVRelease(void);
int PVRSRV_BridgeDispatchKM(uint32_t id, struct SceGpuBridgePackage *pkg);
