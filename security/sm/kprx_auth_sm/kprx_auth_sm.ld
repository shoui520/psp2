/*
 * Linker script for PlayStation Vita kprx_auth_sm.self (4.00-DEBUGGER).
 *
 * Loaded into MeP SRAM at 0x0080b000..0x00815480:
 *   0x0080b000..0x00810438: .text
 *   0x00810438..0x00810910: .rodata
 *   0x00810910..0x00813060: .data
 *   0x00813060..0x00813064: .ctors (file ends at 0x00813064, filesz = 0x8064)
 *   0x00813068..0x00813068: .dtors / .sdata (__gp = 0x0081b068)
 *   0x00813080..0x00815480: .bss (memsz = 0xa480)
 */

OUTPUT_FORMAT("elf32-mep-little")
OUTPUT_ARCH(mep)
ENTRY(_start)

__stack_top = 0x0081ff80;

SECTIONS
{
	. = 0x0080b000;

	.text : {
		KEEP(*(.text.entry))
		KEEP(*(.text.ctors))
		*(.text .text.*)
	}

	.rodata : ALIGN(8) {
		*(.rodata .rodata.*)
	}

	.data : ALIGN(8) {
		KEEP(*(.data.guard))
		*(.data .data.*)
	}

	.ctors : ALIGN(4) {
		__ctors_start = .;
		KEEP(*(.ctors))
		__ctors_end = .;
	}

	. = ALIGN(8);
	.dtors : {
		__dtors_start = .;
		KEEP(*(.dtors))
		__dtors_end = .;
	}

	.sdata : {
		__sdata_start = .;
		*(.sdata .sdata.*)
	}

	__gp = __sdata_start + 0x8000;
	__sdabase = __gp;

	.sbss (NOLOAD) : {
		*(.sbss .sbss.*)
	}

	.bss (NOLOAD) : ALIGN(128) {
		*(.bss .bss.* COMMON)
	}

	/DISCARD/ : {
		*(.comment)
		*(.note*)
		*(.eh_frame)
	}
}
