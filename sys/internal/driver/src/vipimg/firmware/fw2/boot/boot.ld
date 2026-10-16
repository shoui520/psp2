/*
 * fw2's boot block and the runtime both cores run, linked first; the programs
 * take the runtime's symbols from this link (-R), and the program records
 * (programs.S, at 0x2000-0x2300) are linked last.
 *
 * The image (FW2 at image+0x80000) is linked at 0x800000; the cores see it at 0
 * too. Fixed places: the command blocks at 0x1400, the program records at
 * 0x2000, the reported values at 0x2ff8. The runtime's local RAM code goes at
 * the top of the code RAM (0x200f10-0x201000), its data at the top of the data
 * RAM (0x202f08-0x203000); the programs have what is below.
 */
MEMORY
{
	image (rwx) : ORIGIN = 0x800000, LENGTH = 0x5000
}

ENTRY(bootReset)

SECTIONS
{
	/* the boot block runs where the cores see the image at 0 */
	.boot.vectors 0 : AT(0x800000) { *(.boot.vectors) }
	.boot.text ALIGN(4) : AT(0x800000 + ADDR(.boot.text)) { *(.boot.text) }
	ASSERT(. <= 0x1400, "fw2's boot code overruns the command blocks")
	.boot.commands 0x1400 (NOLOAD) : { *(.boot.commands) }
	.boot.version 0x2ff8 : AT(0x802ff8) { *(.boot.version) }

	. = 0x803008;

	.text ALIGN(8) : { *(.text .text.*) . = ALIGN(8); } > image
	.rodata : { *(.rodata .rodata.*) . = ALIGN(8); } > image
	.data : { *(.data .data.*) . = ALIGN(8); } > image
	.bss (NOLOAD) : ALIGN(8) { *(.bss .bss.* COMMON) } > image

	.lram.text 0x200f10 : { *(.lram.text) . = ALIGN(8); } AT> image
	ASSERT(. <= 0x201000, "the runtime's local RAM code overruns the code RAM")
	.lram.data 0x202f08 : { *(.lram.data) . = ALIGN(8); } AT> image
	ASSERT(. <= 0x203000, "the runtime's local RAM data overruns the data RAM")
	__lram_text_load = LOADADDR(.lram.text);
	__lram_text_start = ADDR(.lram.text);
	__lram_text_size = SIZEOF(.lram.text);
	__lram_data_load = LOADADDR(.lram.data);
	__lram_data_start = ADDR(.lram.data);
	__lram_data_size = SIZEOF(.lram.data);

	/DISCARD/ : { *(.comment) }
}
