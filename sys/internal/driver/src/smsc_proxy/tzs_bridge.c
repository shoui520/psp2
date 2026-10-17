/* tzs_bridge.c: the Non-secure kernel's side of the bridge areas */
#include <psp2kern/kernel/cpu.h>
#include <sysmem/sysroot.h>

#define TZS_BRIDGE_SECURE			0

int ksceKernelCpuId(void);

#include "common/sbl/tzs_bridge.c"
