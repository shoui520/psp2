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
		/* Keep the initialized state at its retail ROM addresses. */
		. = 0x00002684;
		KEEP(*(.fl.slot8Seed))
		. = 0x000026a4;
		KEEP(*(.fl.slot9Seed))
		. = 0x000026c4;
		KEEP(*(.fl.signatureSeed))
		. = 0x000026e4;
		KEEP(*(.fl.signatureExponent))
		. = 0x000026f4;
		KEEP(*(.fl.signatureDigestPrefix))
		. = 0x00002706;
		KEEP(*(.fl.permissionBits))
		. = 0x00002710;
		KEEP(*(.fl.emmcTimeUnits))
		. = 0x00002730;
		KEEP(*(.fl.emmcTimeMultipliers))
		. = 0x00002740;
		KEEP(*(.fl.emmcClockUnits))
		. = 0x00002760;
		KEEP(*(.fl.emmcClockMultipliers))
		. = 0x00002770;
		KEEP(*(.fl.sdifClockUnits))
		. = 0x00002790;
		KEEP(*(.fl.sdifClockMultipliers))
		. = 0x00002800;
		KEEP(*(.fl.slot8Iv))
		. = 0x00002810;
		KEEP(*(.fl.slot9Iv))
		. = 0x00002820;
		KEEP(*(.fl.keyIvs))
		. = 0x00002880;
		KEEP(*(.fl.derivedKeyIvs))
		. = 0x000028d0;
		KEEP(*(.fl.digestIv))
		. = 0x000028e0;
		KEEP(*(.fl.payloadIv))
		. = 0x000028f0;
		KEEP(*(.fl.fallbackModulus))
		. = 0x000029f0;
		KEEP(*(.fl.externalBootKey))
		. = 0x00002a20;
		*(.rodata .rodata.*)
		*(.sdata .sdata.*)
		*(.data .data.*)
		. = ALIGN(8);
	} > ram
	__program_start = ADDR(.program);
	__program_load = LOADADDR(.program);
	__program_size = SIZEOF(.program);

	.bss (NOLOAD) : ALIGN(8)
	{
		__bss_start = .;
		*(.sbss .sbss.*)
		*(.bss .bss.* COMMON)
		. = ALIGN(8);
		__bss_end = .;
	} > ram
	__sdabase = ADDR(.bss);
	__stack_top = 0x5fff0;

	.signature 0x43f80 : { KEEP(*(.signature)) } > rom

	/DISCARD/ : { *(.comment) *(.note*) }
}
