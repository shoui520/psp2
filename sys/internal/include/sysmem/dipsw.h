/*
 * SceDipswForDriver (os0:kd/sysmem.skprx): the DIP switches, each export declared once, with
 * sysmem's definition as the prototype.
 */
#pragma once

#include <stdint.h>
#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the DIP switch words the KBL parameters carry (sysmem copies them at boot) */
typedef struct SceDipsw {
	uint32_t cp_timestamp_1;
	uint16_t cp_version;
	uint16_t cp_build_id;
	uint32_t cp_timestamp_2;
	uint32_t aslr_seed;
	uint32_t sce_sdk_flags;		/* 0x10 */
	uint32_t shell_flags;
	uint32_t debug_control_flags;
	uint32_t system_control_flags;
} SceDipsw;

#ifndef __cplusplus
_Static_assert(sizeof(SceDipsw) == 0x20, "SceDipsw size");
#endif

/*
 * The DIP switch numbers. SCE_DIPSW_ENABLE_TOOL_PHYMEMPART is Sony's (its messages print it);
 * no Sony name was found for the others, which are named from what their readers do. Switches
 * 0-63 are the user's (the SDK's SCE_DIPSW_USER_MIN and SCE_DIPSW_USER_MAX).
 */
/* the SDK's */
#define DIPSW_MEMORY_SIZE			0x80	/* the console's, not the tool's */
#define DIPSW_RELEASE_CHECK_MODE		0x81
#define DIPSW_PLATFORM_DOLCE			0x98	/* a PS TV emulated */
#define DIPSW_DEVELOPMENT_MODE			0x9f	/* clear: release mode */
/* the shell's */
#define DIPSW_EXTRA_TTY				0xb8	/* processes open tty7: too */
#define DIPSW_SHELL_CRON			0xba
#define DIPSW_NO_SHELL_DUMMY_PHYMEMPART		0xbe
#define DIPSW_GROW_SHELL_PHYMEMPART		0xbf	/* by 48 MiB at 0x78000000 */
/* debugging */
#define DIPSW_DMAC6				0xc0
#define DIPSW_SYSTEM_DECI			0xc1	/* the system's debug protocols */
#define DIPSW_DECI				0xc2	/* the debug protocols (DECI4p) */
#define DIPSW_NO_DBGUSB				0xc3	/* DECI over SDIO, not USB */
#define DIPSW_UART0_CONSOLE			0xc4
#define DIPSW_UART1_CONSOLE			0xc5
#define DIPSW_SYSTEM_TTY			0xc6
#define DIPSW_TTY_STDIO				0xc7
#define DIPSW_STOP_ON_ASSERT			0xc8
#define DIPSW_ASSERT_LEVEL_1			0xc9
#define DIPSW_ASSERT_LEVEL_2			0xca
#define DIPSW_DISABLE_RECONFIGURATION		0xcb	/* the switches left as set */
#define DIPSW_DEBUG_LEVEL_1			0xcc
#define DIPSW_DEBUG_LEVEL_2			0xcd
#define DIPSW_CRITICAL_ERROR			0xce	/* syscall usage errors reported */
#define SCE_DIPSW_ENABLE_TOOL_PHYMEMPART	0xd2
#define DIPSW_USER_UART				0xd3	/* tty2uart */
#define DIPSW_USER_PA_MAPPING			0xd4
#define DIPSW_USER_PA_MAPPING_ADDRESS		0xd5	/* at 0x78000000, not 0x80000000 */
#define DIPSW_STACK_FILL			0xd8	/* stacks filled, to measure their use */
#define DIPSW_BIG_KERNEL_STACK			0xd9
#define DIPSW_IGNORE_KEYSTONE			0xda
/* the system's */
#define DIPSW_NO_CLEAR				0xe1	/* memory blocks not cleared */
#define DIPSW_HARDWARE_BREAKPOINT		0xe4
#define DIPSW_NO_HDCP				0xe5
#define DIPSW_IO_NO_CHUNKS			0xe7	/* reads not split into chunks */
#define DIPSW_GPU_OVERCLOCK			0xec	/* 166 MHz, not 111 */
#define DIPSW_HOST0_MODULES			0xfb	/* modules loaded from host0: */
#define DIPSW_HOST0_ACCESS			0xfc
#define DIPSW_CONSOLE_LOGGING			0xfd

void sceKernelSetDipsw(unsigned int bit);
int sceKernelCheckDipsw(unsigned int bit);
/* vitasdk: ksceKernelGetDipswInfo */
SceUInt32 SceDipswForDriver_B2AD48BE(unsigned int index);
void sceKernelClearDipsw(unsigned int bit);
int sceKernelCheckDipswForUser(unsigned int bit);
int sceKernelClearDipswForUser(unsigned int bit);
int sceKernelSetDipswForUser(unsigned int bit);

#ifdef __cplusplus
}
#endif
