/*
 * The KBL parameters: what the boot loader hands the kernel (sysroot keeps a
 * pointer to them). This replaces vitasdk's psp2kern/kernel/kbl/kbl.h, whose
 * include guard it takes, so it must come before any vitasdk header that
 * includes that file (psp2kern/kernel/sysroot.h does). The layout and names
 * are vitasdk's; the words vitasdk leaves as offsets are named from their
 * readers, or reserved where no module reads them.
 */

#pragma once

#ifdef _PSP2KERN_KERNEL_KBL_KBL_H_
#error "include sce/kbl_param.h before vitasdk's psp2kern/kernel/kbl/kbl.h"
#endif
#define _PSP2KERN_KERNEL_KBL_KBL_H_

#include <psp2kern/types.h>
#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/ssmgr.h>
#include <stdint.h>

/* SceKblParam.poweroff_factor */
#define SCE_KBL_POWEROFF_PANIC_REBOOT	0x00000001	/* a fatal error rebooted the system */
#define SCE_KBL_POWEROFF_PANIC		0x00000010	/* a fatal error powered it off */
#define SCE_KBL_POWEROFF_USB_ENUM	0x00090000	/* sceSysrootIsUsbEnumWakeup */

typedef struct SceKblParam {		/* 0x200 bytes since 3.60 */
	uint16_t version;
	uint16_t size;
	uint32_t current_fw_version;
	uint32_t factory_fw_version;
	uint32_t reserved0[2];
	uint8_t reserved1[0xc];
	uint8_t qa_flags[0x10];
	uint8_t boot_flags[0x10];
	union {
		SceDipsw dipsw;
		uint32_t dipswWords[8];	/* the switches by word, as the boot loaders read them */
	};
	SceKernelPARange dram;
	uint32_t reserved2;
	uint32_t boot_type_indicator_1;
	SceOpenPsId openpsid;
	SceKernelPARange secure_kernel;
	SceKernelPARange context_auth_sm;
	SceKernelPARange kprx_auth_sm;
	SceKernelPARange prog_rvk;
	ScePsCode pscode;
	uint32_t __stack_chk_guard;
	uint32_t reserved3;
	uint8_t session_id[0x10];
	uint32_t poweroff_factor;	/* why the system last went down */
	uint32_t wakeup_factor;
	uint32_t usb_info;
	uint32_t hold_ctrl;
	uint32_t suspendinfo_adr;
	uint32_t hardware_info;
	uint32_t boot_type_indicator_2;
	uint32_t reserved4;
	uint32_t kbl_load_time;		/* microseconds second_loader took; 0 on a resume */
	uint32_t reserved5;
	uint8_t hardware_flags[0x10];
	uint32_t bootldr_revision;
	uint32_t magic;
	uint8_t coredump_session_key[0x20];
	uint8_t unused[0xe0];
} __attribute__((packed)) SceKblParam;

_Static_assert(sizeof(SceKblParam) == 0x200, "SceKblParam");
