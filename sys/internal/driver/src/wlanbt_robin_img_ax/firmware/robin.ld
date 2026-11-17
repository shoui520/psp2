/*
 * Linker script for the Marvell 88W8787 ("Robin") WLAN/Bluetooth firmware
 * (os0:kd/wlanbt_robin_img_ax.skprx).
 *
 * Memory map:
 *   ITCM     0x00000000 .. 0x00100000  (RO code/rodata + RW data + BSS + stacks)
 *   Mask ROM 0x03f00000 .. 0x03f48000  (ThreadX, OSA, MDEV, BT controller, AEABI)
 *   DTCM     0x04000000 .. 0x04010000  (RW data + BSS + ROM hook table at 0x0400fc24)
 *   MMIO     0x80000000 .. 0x90020000  (SoC APU/Timers/IRQ/SDIO/WLAN MAC/BBP/RF)
 *   SQRAM0   0xc0000000 .. 0xc00071b4  (BSS0 + RW data + BSS1)
 *   SQRAM1   0xc0007200 .. 0xc0040000  (RO uAP/P2P code/rodata + RW data + BSS/pools)
 */
INCLUDE boot/rom_syms.ld

/*
 * Objects the original keeps inside others and addresses on their own: the
 * members of the block init clears, the WMM bits in the last byte of each
 * copy of the WMM element, and the initial stack's top (__user_initial_stackheap).
 */
wlanPrimaryEvents	= wlanDtcmState + 0x3c;		/* 0x04008828 */
wlanProtection		= wlanDtcmState + 0x58;		/* 0x04008844 */
wlanTxRateFlags		= wlanDtcmState + 0xba;		/* 0x040088a6 */
wlanHwInfo		= wlanDtcmState + 0xc0;		/* 0x040088ac */
wlanAntennaProfile	= wlanDtcmState + 0xde;		/* 0x040088ca */
wlanIbssJoinState	= wlanDtcmState + 0xe2;		/* 0x040088ce */
wlanSavedMgmtPriority	= wlanDtcmState + 0xee;		/* 0x040088da */
wlanDataEnd		= wlanCommandScanWindow + 0x2bc;	/* 0x00057e9a */

/*
 * The original's sections sit at fixed addresses because RVCT made them that
 * size. GCC 4.8 makes the code larger (ITCM code by 0x3df0 bytes, SQRAM1 code by
 * 0x5e0), so the sections that follow it are not at the original's addresses:
 * ITCM holds code, data and BSS in sequence and ends at the heap base
 * (__user_initial_stackheap, boot/reset.S), SQRAM1 holds code, data and BSS.
 * DTCM and SQRAM0 keep the original's addresses.
 */
MEMORY
{
	itcm       (rwx) : ORIGIN = 0x00000000, LENGTH = 0x00063800
	dtcm_rw    (rw)  : ORIGIN = 0x04000100, LENGTH = 0x00000f70
	dtcm_bss   (rw)  : ORIGIN = 0x04001070, LENGTH = 0x0000ef90
	sqram0_b0  (rw)  : ORIGIN = 0xc0000000, LENGTH = 0x00001100
	sqram0_rw  (rw)  : ORIGIN = 0xc0001100, LENGTH = 0x00004e64
	sqram0_b1  (rw)  : ORIGIN = 0xc0005f64, LENGTH = 0x00001250
	sqram1     (rwx) : ORIGIN = 0xc0007200, LENGTH = 0x00038e00
}

ENTRY(bootReset)

SECTIONS
{
	.sqram1.ro 0xc0007200 : {
		*sqram/*.o(.text .text.* .rodata .rodata.* i.* .constdata .conststring .conststrlit)
		*(.sqram1.ro .sqram1.ro.*)
		. = ALIGN(4);
	} > sqram1

	.itcm.ro 0x00000000 : {
		KEEP(*(.boot.vectors))
		*(EXCLUDE_FILE(*sqram/*.o) .text EXCLUDE_FILE(*sqram/*.o) .text.* EXCLUDE_FILE(*sqram/*.o) i.*)
		*(.text.boot)
		*(.glue_7 .glue_7t .vfp11_veneer .v4_bx)
		. = ALIGN(4);
		*(EXCLUDE_FILE(*sqram/*.o) .rodata EXCLUDE_FILE(*sqram/*.o) .rodata.*)
		*(EXCLUDE_FILE(*sqram/*.o) .constdata EXCLUDE_FILE(*sqram/*.o) .conststring EXCLUDE_FILE(*sqram/*.o) .conststrlit)
		. = ALIGN(4);
		__region_table_start = .;
		KEEP(*(.rodata.region_table))
		__region_table_end = .;
		. = ALIGN(4);
	} > itcm

	.itcm.rw : {
		*(.itcm.rw .itcm.rw.*)
		*(.data .data.*)
		. = ALIGN(4);
		*(.itcm.patch)
		. = ALIGN(4);
	} > itcm

	.itcm.bss (NOLOAD) : {
		*(.itcm.bss .itcm.bss.*)
		*(.bss .bss.* COMMON)
		. = ALIGN(4);
		__itcm_end = .;
	} > itcm

	.dtcm.rw 0x04000100 : {
		*(.dtcm.rw .dtcm.rw.*)
		. = ALIGN(4);
	} > dtcm_rw

	.dtcm.bss 0x04001070 (NOLOAD) : {
		*(.dtcm.bss .dtcm.bss.*)
		. = ALIGN(4);
	} > dtcm_bss

	.sqram0.bss0 0xc0000000 (NOLOAD) : {
		*(.sqram0.bss0 .sqram0.bss0.*)
		. = ALIGN(4);
	} > sqram0_b0

	.sqram0.rw 0xc0001100 : {
		*(.sqram0.rw .sqram0.rw.*)
		. = ALIGN(4);
	} > sqram0_rw

	.sqram0.bss1 0xc0005f64 (NOLOAD) : {
		*(.sqram0.bss1 .sqram0.bss1.*)
		. = ALIGN(4);
	} > sqram0_b1

	.sqram1.rw : {
		*(.sqram1.rw .sqram1.rw.*)
		. = ALIGN(4);
	} > sqram1

	.sqram1.bss (NOLOAD) : {
		*(.sqram1.bss .sqram1.bss.*)
		. = ALIGN(4);
	} > sqram1

	/*
 * The initial stack sits between the BSS and the heap base (0x63800): the
 * original leaves 0xa65 bytes there.
 */
	ASSERT(__itcm_end + 0xa65 <= 0x63800, "ITCM image runs into the initial stack and the heap base")

	/DISCARD/ : {
		*(.ARM.exidx* .ARM.extab* .ARM.attributes .comment .note*)
	}
}
