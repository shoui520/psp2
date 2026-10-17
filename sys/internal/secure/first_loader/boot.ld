/*
 * first_loader: a 16 KiB ROM at the Cmep's vector base 0x40000. The vectors and reset
 * run in place; the program is stored from 0x40180 and run at 0x5c000 after reset
 * copies it (eight bytes at a time, so its size is rounded up to eight). The last
 * 0x80 bytes hold the copyright notice and the build date.
 */

OUTPUT_FORMAT("elf32-mep-little")
OUTPUT_ARCH(mep)
ENTRY(reset)

MEMORY
{
	rom (rx)  : ORIGIN = 0x40000, LENGTH = 0x4000
	ram (rwx) : ORIGIN = 0x5c000, LENGTH = 0x4000
}

SECTIONS
{
	.vectors 0x40000 : { KEEP(*(.vectors)) } > rom
	.reset : { KEEP(*(.reset)) } > rom

	.program 0x5c000 : AT(0x40180)
	{
		KEEP(*(.text.start))
		*(.text .text.*)
		*(.rodata .rodata.*)
		*(.data .data.*)
		. = ALIGN(8);
	} > ram
	__program_start = ADDR(.program);
	__program_load = LOADADDR(.program);
	__program_size = SIZEOF(.program);

	.bss (NOLOAD) : { *(.bss .bss.* COMMON) } > ram

	.signature 0x43f80 : { KEEP(*(.signature)) } > rom

	/DISCARD/ : { *(.comment) *(.note*) }
}
