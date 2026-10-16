/*
 * SKBL skeleton: the functions reset.S calls (boot.c and memcpy_chk.c in Sony's debug
 * records).
 *
 * Skeleton (T2): the bodies are placeholders except SsxInit's.
 */

#include "skeleton.h"
#include <stdint.h>
#include <sce/pervasive_regs.h>
#include <sce/ssx_firewall_regs.h>

#define PERVASIVE_RESET		((ScePervasiveResetRegisters *)PERVASIVE_RESET_BASE)
#define SSX_REGISTER_TARGET(xbar) ((SceSsxRegisterTarget *)SSX_REGISTER_TARGET_BASE(xbar))
#define LPDDR0_FIREWALL		((SceSsxFirewallRegisters *)SSX_FIREWALL_LPDDR0_BASE)
#define LPDDR1_FIREWALL		((SceSsxFirewallRegisters *)SSX_FIREWALL_LPDDR1_BASE)

/* name unknown: the interconnects' timeouts and region 2 of the LPDDR2
 * firewalls; the Debug Bus (and the TOOL units' LPDDR2 on it) only when it is
 * out of reset */
int SsxInit(void)
{
	if (PERVASIVE_RESET->debugBus == 0) {
		SSX_REGISTER_TARGET(SSX_DEBUG_BUS)->networkControl = 0x400;
		LPDDR1_FIREWALL->region[2].addrMatch = 0xe5880038;
		LPDDR1_FIREWALL->region[2].reqInfoPermission = 1;
	}
	SSX_REGISTER_TARGET(SSX_IFTU_XBAR)->networkControl = 0;
	SSX_REGISTER_TARGET(SSX_MAIN_XBAR)->networkControl = 0;
	SSX_REGISTER_TARGET(SSX_CENTER_XBAR)->networkControl = 0;
	SSX_REGISTER_TARGET(SSX_VIDEO_XBAR)->networkControl = 0;
	SSX_REGISTER_TARGET(SSX_VIP_XBAR)->networkControl = 0;
	LPDDR0_FIREWALL->region[2].addrMatch = 0xe6000038;
	LPDDR0_FIREWALL->region[2].reqInfoPermission = 1;
	/* read back: the write has reached the firewall */
	(void)LPDDR0_FIREWALL->region[2].reqInfoPermission;
	__asm__ volatile("dsb sy" : : : "memory");
	return 0;
}


STUB(boot)
STUB(boot_und_handler)
STUB(boot_svc_handler)
STUB(boot_prefetch_abort_handler)
STUB(boot_data_abort_handler)
STUB(boot_irq_handler)
STUB(boot_fiq_handler)
STUB(monitor_call_handler)
STUB(memcpy)
