/*
 * The secure kernel's SceDebugForTZS (the TrustZone build of SceSysmem): the os0
 * SceDebugForDriver functions under their ForTZS NIDs. A secure-world build includes this
 * header before any other, so the os0 names in sysmem/debug.h and in the code it shares with
 * os0 link to the ForTZS imports.
 */
#pragma once

#define SceDebugForDriver_02B04343	SceDebugForTZS_0D799DAE
#define SceDebugForDriver_391B5B74	SceDebugForTZS_563B623D
#define SceDebugForDriver_FD753E7A	SceDebugForTZS_618F89E6
#define SceDebugForDriver_D9703808	SceDebugForTZS_D26EB4BB
#define sceKernelGetAssertLevel	sceKernelGetAssertLevel

#include <fw.h>
#include <sysmem/debug.h>

/* Sony built the 3.6x secure kernel without its debug output */
#if !FW_SINCE(0x04000000)
#undef SCE_DBG_PRINTF
#define SCE_DBG_PRINTF(...)		((void)0)
#undef SCE_DBG_PRINTF_LEVEL
#define SCE_DBG_PRINTF_LEVEL(...)	((void)0)
#undef SCE_DBG_ASSERT_FAIL
#define SCE_DBG_ASSERT_FAIL(...)	((void)0)
#endif
