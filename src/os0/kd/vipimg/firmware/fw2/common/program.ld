/*
 * What every FW2 program's link has (included by the parsers' and the codec's
 * scripts, which define the memory region "image" for the program's DRAM part):
 * its DRAM code, and its context, the $gp globals each context holds a copy of.
 *
 * The context is linked at 0: initialised globals first (the program's own
 * globals block, g_codec or g_parser, at its start), then the zeroed ones and
 * the ones past $gp's reach (.context.far). $gp points 0x8000 into it. The
 * records give the context's image and sizes, the addresses as offsets into the
 * image (the ARM side sizes a context from them).
 */
SECTIONS
{
	.text : { *(.text .text.*) . = ALIGN(8); } > image
	.rodata : { *(.rodata .rodata.*) . = ALIGN(8); } > image
	.data : { *(.data .data.*) . = ALIGN(8); } > image
	.bss (NOLOAD) : { *(.bss .bss.* COMMON) } > image
	ASSERT(SIZEOF(.data) + SIZEOF(.bss) == 0, "a program has data outside its context")

	.context.data 0 : ALIGN(8)
	{
		*command.o(.sdata)
		*(.sdata .sdata.*)
		. = ALIGN(8);
	} AT> image
	.context.zero ADDR(.context.data) + SIZEOF(.context.data) (NOLOAD) :
	{
		*decoder.o(.sbss)
		*(.sbss .sbss.* .scommon)
		*(.context.far)
		. = ALIGN(8);
	}
	__sdabase = ADDR(.context.data) + 0x8000;
	__program_globals_load = LOADADDR(.context.data) - 0x800000;
	__program_globals_size = SIZEOF(.context.data);
	__program_zero_size = SIZEOF(.context.zero);

	/DISCARD/ : { *(.comment) }
}
