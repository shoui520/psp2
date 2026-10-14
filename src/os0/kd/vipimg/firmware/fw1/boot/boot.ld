/*
 * fw1's boot block, the image's first 0x1000 bytes, linked at 0: the vectors, the reset and
 * exception code, the entry table at 0x200 and the crash records. It is linked
 * after the programs, whose start and regions it takes from their links (-R).
 */
SECTIONS
{
	/* it runs where the cores see the image at 0 */
	.boot.vectors 0 : AT(0x800000) { *(.boot.vectors) }
	.boot.text : AT(0x800000 + ADDR(.boot.text)) { *(.boot.text) }
	ASSERT(. <= 0x200, "fw1's boot code overruns the entry table")
	.boot.entries 0x200 : AT(0x800200) { *(.boot.entries) }
	.boot.crash ALIGN(8) : AT(0x800000 + ADDR(.boot.crash)) { *(.boot.crash) }
	ASSERT(. <= 0x1000, "fw1's boot block overruns PU0")

	/DISCARD/ : { *(.comment) }
}
