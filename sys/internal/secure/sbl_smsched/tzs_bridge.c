/* tzs_bridge.c: the secure world's side of the bridge areas */
#include "smsched.h"

#define TZS_BRIDGE_SECURE			1
#define SceSysrootForKernel_C8C8C321	sceKernelSysrootGetSharedMemoryForTZS
#define SceCpuForDriver_103872A5		sceKernelDcacheCleanRange
#define SceCpuForDriver_02796361		sceKernelDcacheInvalidateRange

#include "../../driver/src/smsc_proxy/tzs_bridge.c"
