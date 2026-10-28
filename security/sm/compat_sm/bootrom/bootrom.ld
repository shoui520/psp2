/*
 * bootrom.ld: Linker script for the 4,096-byte ePSP (Allegrex) BootROM image
 * embedded in os0:sm/compat_sm.self (loaded at 0xBFC00000 / F00D 0xE0100000).
 *
 * Layout in the 4,096-byte binary image (LMA 0x0000..0x1000):
 *   0x0000..0x017f (VMA 0xbfc00000..0xbfc0017f): Stage 1 reset & cache init
 *   0x0180..0x0eff (VMA 0x80010000..0x80010d7f): Stage 2 C code (copied to 0xa0010000)
 *   0x0f00..0x0fff (VMA 0x80010d80..0x80010e7f): Stage 2 pinned data & SHA-1 IV
 */
OUTPUT_ARCH(mips:allegrex)
ENTRY(_start)

SECTIONS
{
	.text.reset 0xbfc00000 : AT(0x00000000) SUBALIGN(4) {
		KEEP(*(.text.reset))
		. = 0x180;
	} = 0

	.text 0x80010000 : AT(0x00000180) SUBALIGN(4) {
		KEEP(*(.text.entry))
		*(.text .text.*)
		*(.rodata .rodata.*)
		ASSERT(. <= 0x80010d80, "bootrom .text overruns pinned .data at 0x80010d80");
		. = 0xd80;
	} = 0

	.data 0x80010d80 : AT(0x00000f00) SUBALIGN(4) {
		KEEP(*(.data.pinned))
		*(.data .data.* .sdata .sdata.*)
		ASSERT(. <= 0x80010e34, "bootrom .data overruns 3636-byte copy window at 0x80010e34");
		. = 0x100;
	} = 0

	/DISCARD/ : {
		*(.bss .bss.* .sbss .sbss.* COMMON)
		*(.MIPS.abiflags)
		*(.reginfo)
		*(.pdr)
		*(.comment)
		*(.gnu.attributes)
		*(.eh_frame)
	}
}
