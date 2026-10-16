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

MEMORY
{
	itcm_ro    (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00054764
	itcm_rw    (rw)  : ORIGIN = 0x00054764, LENGTH = 0x000014e8
	itcm_bss   (rw)  : ORIGIN = 0x00055c4c, LENGTH = 0x0009e5f4
	dtcm_rw    (rw)  : ORIGIN = 0x04000100, LENGTH = 0x00000f70
	dtcm_bss   (rw)  : ORIGIN = 0x04001070, LENGTH = 0x0000ef90
	sqram0_b0  (rw)  : ORIGIN = 0xc0000000, LENGTH = 0x00001100
	sqram0_rw  (rw)  : ORIGIN = 0xc0001100, LENGTH = 0x00004e64
	sqram0_b1  (rw)  : ORIGIN = 0xc0005f64, LENGTH = 0x00001250
	sqram1_ro  (rx)  : ORIGIN = 0xc0007200, LENGTH = 0x000088a8
	sqram1_rw  (rw)  : ORIGIN = 0xc000faa8, LENGTH = 0x000024ec
	sqram1_bss (rw)  : ORIGIN = 0xc0011f94, LENGTH = 0x0002e06c
}

ENTRY(bootReset)

SECTIONS
{
	.sqram1.ro 0xc0007200 : {
		*sqram/*.o(.text .text.* .rodata .rodata.*)
		*(.sqram1.ro .sqram1.ro.*)
		. = ALIGN(4);
	} > sqram1_ro

	.itcm.ro 0x00000000 : {
		KEEP(*(.boot.vectors))
		*(EXCLUDE_FILE(*sqram/*.o) .text EXCLUDE_FILE(*sqram/*.o) .text.*)
		*(.text.boot)
		*(.glue_7 .glue_7t .vfp11_veneer .v4_bx)
		. = ALIGN(4);
		*(EXCLUDE_FILE(*sqram/*.o) .rodata EXCLUDE_FILE(*sqram/*.o) .rodata.*)
		. = ALIGN(4);
		__region_table_start = .;
		KEEP(*(.rodata.region_table))
		__region_table_end = .;
		. = ALIGN(4);
	} > itcm_ro

	.itcm.rw 0x00054764 : {
		*(.itcm.rw .itcm.rw.*)
		*(.data .data.*)
		. = ALIGN(4);
	} > itcm_rw

	.itcm.bss 0x00055c4c (NOLOAD) : {
		*(.itcm.bss .itcm.bss.*)
		*(.bss .bss.* COMMON)
		. = ALIGN(4);
	} > itcm_bss

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

	.sqram1.rw 0xc000faa8 : {
		*(.sqram1.rw .sqram1.rw.*)
		. = ALIGN(4);
	} > sqram1_rw

	.sqram1.bss 0xc0011f94 (NOLOAD) : {
		*(.sqram1.bss .sqram1.bss.*)
		. = ALIGN(4);
	} > sqram1_bss

	/DISCARD/ : {
		*(.ARM.exidx* .ARM.extab* .ARM.attributes .comment .note*)
	}
}
