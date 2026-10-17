/* tzs_bridge.c: the secure world's side of the bridge areas */
#include "smsched.h"

#define TZS_BRIDGE_SECURE			1
#define ksceKernelSysrootGetSharedMemory	sceKernelSysrootGetSharedMemoryForTZS
#define ksceKernelDcacheCleanRange		sceKernelDcacheCleanRange
#define ksceKernelDcacheInvalidateRange		sceKernelDcacheInvalidateRange

#include "../../driver/src/smsc_proxy/tzs_bridge.c"
