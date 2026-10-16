/*
 * A stream parser's link (parser0.ld and parser1.ld set the region "image" and
 * name the record symbols): fw2/common/program.ld, then its local RAM parts.
 *
 * Local RAM: the code overlay from 0x40 (VIP_PROGRAM_CODE: the interrupt
 * vectors, the program's code, the library's), the loader after it, up to the
 * runtime's code at 0xf10; the saved data from 0x2400 (VIP_PROGRAM_DATA), the
 * unsaved after it, up to the runtime's data at 0x2f08. The image addresses
 * the program reads by DMA are offsets into the image, as the firmware's are.
 */
INCLUDE fw2/common/program.ld

SECTIONS
{
	.overlay 0x200040 :
	{
		*(.overlay.vectors)
		*(.overlay.program)
		*(.overlay.library)
		. = ALIGN(8);
	} AT> image
	.loader ADDR(.overlay) + SIZEOF(.overlay) :
	{
		*(.loader.program)
		. = ALIGN(8);
	} AT> image
	ASSERT(. <= 0x200f10, "the parser's overlay and loader overrun the code RAM")
	g_loaderImage = LOADADDR(.loader) - 0x800000;	/* DMA addresses are offsets */
	g_loaderStart = ADDR(.loader);
	g_loaderSize = SIZEOF(.loader);

	.saved 0x202400 :
	{
		*(.saved.library)
		*(.saved.program)
		. = ALIGN(8);
	} AT> image
	.local ADDR(.saved) + SIZEOF(.saved) (NOLOAD) :
	{
		*(.local.library)
		*(.local.program)
	}
	ASSERT(. <= 0x202f08, "the parser's local data overruns the runtime's")

	__program_code_load = LOADADDR(.overlay) - 0x800000;
	__program_code_size = SIZEOF(.overlay);
	__program_local_load = LOADADDR(.saved) - 0x800000;
	__program_local_size = SIZEOF(.saved);
}
