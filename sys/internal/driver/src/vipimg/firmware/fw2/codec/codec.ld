/*
 * Core 1's program, the codec, from 0x816000: fw2/common/program.ld, then its
 * local RAM parts.
 *
 * Local RAM: the code overlay from 0x40 (VIP_PROGRAM_CODE: the interrupt
 * vectors, the read stream), the macroblock loop and the motion saving after it
 * (fw1/pu1/loops.ld), up to the runtime's code at 0xf10; the saved data from
 * 0x2400 (VIP_PROGRAM_DATA), the unsaved after it with the slice buffers and
 * g_parsed over them (VIP_PARSED_OVERLAP in), up to the runtime's data at 0x2f08.
 */
MEMORY
{
	image (rwx) : ORIGIN = 0x816000, LENGTH = 0xd000
}

ENTRY(codecEntry)
INCLUDE fw2/common/program.ld

SECTIONS
{
	.overlay 0x200040 :
	{
		*(.overlay.vectors)
		*(.overlay.program)
		. = ALIGN(8);
	} AT> image
}

LOOPS_START = ADDR(.overlay) + SIZEOF(.overlay);
DMA_BASE = 0x800000;
INCLUDE fw1/pu1/loops.ld
ASSERT(__loops_end <= 0x200f10, "the codec's overlays overrun the code RAM")

SECTIONS
{
	.saved 0x202400 : { *(.saved.program) . = ALIGN(8); } AT> image
	.local ADDR(.saved) + SIZEOF(.saved) (NOLOAD) : { *(.local.program) }
	.slice ALIGN(ADDR(.local) + SIZEOF(.local), 8) (NOLOAD) :
	{
		*(.local.program.colocated)
		*(.local.program.motion)
	}
	.parsed ADDR(.slice) + 0x200 /* VIP_PARSED_OVERLAP */ (NOLOAD) :
	{
		*(.local.program.parsed)
	}
	ASSERT(ADDR(.parsed) + SIZEOF(.parsed) <= ADDR(.slice) + SIZEOF(.slice),
		"g_parsed past the slice buffers")
	ASSERT(ADDR(.slice) + SIZEOF(.slice) <= 0x202f08,
		"the codec's local data overruns the runtime's")
}

__codec_code_load = LOADADDR(.overlay) - 0x800000;
__codec_code_size = SIZEOF(.overlay);
__codec_local_load = LOADADDR(.saved) - 0x800000;
__codec_local_size = SIZEOF(.saved);
__codec_globals_load = __program_globals_load;
__codec_globals_size = __program_globals_size;
__codec_zero_size = __program_zero_size;
