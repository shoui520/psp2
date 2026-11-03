/*
 * SceGpuEs4User_E98914E4's textured pixel event program: Imagination's
 * codegen/pds/pixelevent.asm.c for the SGX543 with MSAA_NO_DOWNSCALE and
 * FIX_HW_BRN_31175, where the tile coordinates the pixel event carries must
 * be scaled for multisampling (BRN31175). The macro 0/Y is
 * the scale the sample mode needs.
 *
 * The end of tile block is built in temp_EOT2: the tile coordinates in ir0
 * are masked out, shifted and merged back into the rest of the word.
 */
temp dword param0, temp_EOT2;
data dword EOT0, EOT1, EOT2;
data dword EOR0, EOR1, EOR2;
data dword PTOFF0, PTOFF1, PTOFF2;

	!if0 bra	not_eot

	/* keep everything but the tile coordinates */
	and		temp_EOT2, 0xffff0000, ir0	/* TILEX_CLRMSK & TILEY_CLRMSK */
	/* scale the tile X */
	and		param0, ir0, 0x000000ff		/* ~TILEX_CLRMSK */
	shl		param0, param0, 0
	or		temp_EOT2, param0, temp_EOT2
	/* scale the tile Y */
	and		param0, ir0, 0x0000ff00		/* ~TILEY_CLRMSK */
	shl		param0, param0, 1
	or		temp_EOT2, param0, temp_EOT2

	movs	doutu, EOT0, EOT1, temp_EOT2
	halt

not_eot:
	!if1 bra	not_eor

	and		param0, 0x00000100, ir1		/* PIX_EVENT_ZEROCNT */
	tstz	p0, param0
	and		param0, ir0, 0xfffeffff		/* ~PDM_EOR */
	or		param0, param0, 0x00020000	/* PDM_EOR_EOT */
	p0	mov32	param0, EOR2

	movs	doutu, EOR0, EOR1, param0
	halt

not_eor:
	movs	doutu, PTOFF0, PTOFF1, PTOFF2
	halt
