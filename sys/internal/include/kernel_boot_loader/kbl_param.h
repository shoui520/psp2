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
#error "include kernel_boot_loader/kbl_param.h before vitasdk's psp2kern/kernel/kbl/kbl.h"
#endif
#define _PSP2KERN_KERNEL_KBL_KBL_H_

/* SceKblParam.poweroff_factor */
#define SCE_KBL_POWEROFF_PANIC_REBOOT	0x00000001	/* a fatal error rebooted the system */
#define SCE_KBL_POWEROFF_PANIC		0x00000010	/* a fatal error powered it off */
#define SCE_KBL_POWEROFF_USB_ENUM	0x00090000	/* sceSysrootIsUsbEnumWakeup */

/* SceKblParam.boot_type_indicator_1 (names unknown; the henkaku wiki's meanings) */
#define BOOT_TYPE_EXTERNAL		0x00000001	/* external boot (manufacturing images) */
#define BOOT_TYPE_PRODUCT_MODE		0x00000004
#define BOOT_TYPE_MEMORY_TEST		0x00000008	/* never set by the release second_loader */
#define BOOT_TYPE_SPECIAL_MEDIA		0x00000040	/* modules come from sd0: (NSKBL) */
#define BOOT_TYPE_JIG			0x00010000	/* the jig handshake passed */
#define BOOT_TYPE_RESUME		0x00020000	/* resuming: no boot logo */
#define BOOT_TYPE_SD_CARD		0x00040000	/* an SD card to mount as sd0: (NSKBL) */
#define BOOT_TYPE_SD_MODE		0x00080000

#define SCE_KBL_PARAM_MAGIC		0xcbac03aa	/* SceKblParam.magic */

/* SceKblParam.wakeup_factor (names unknown; the henkaku wiki's meanings) */
#define SCE_KBL_WAKEUP_RESUME		0x00000080	/* resuming from suspend */
#define SCE_KBL_WAKEUP_REASON_MASK	0x0000007f	/* why the system was powered on */
#define SCE_KBL_WAKEUP_PANIC_REBOOT	0x17		/* a fatal error rebooted it */

/* the offsets assembly reads the parameters at (checked below) */
#define SCE_KBL_PARAM_BOOT_TYPE_INDICATOR_1	0x6c
#define SCE_KBL_PARAM_WAKEUP_FACTOR		0xc4

#ifndef __ASSEMBLER__

#include <scetypes.h>
#include <sysmem/memblock.h>
#include <sysmem/dipsw.h>
#include <ss_mgr/ss_mgr.h>
#include <stdint.h>

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
	SceKernelOpenPsId openpsid;
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
_Static_assert(__builtin_offsetof(SceKblParam, boot_type_indicator_1) ==
	       SCE_KBL_PARAM_BOOT_TYPE_INDICATOR_1, "SceKblParam.boot_type_indicator_1");
_Static_assert(__builtin_offsetof(SceKblParam, wakeup_factor) == SCE_KBL_PARAM_WAKEUP_FACTOR,
	       "SceKblParam.wakeup_factor");

#endif /* __ASSEMBLER__ */
