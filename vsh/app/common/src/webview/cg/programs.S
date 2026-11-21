/*
 * The tile backend's GPU programs (tilebackend.cpp): each Cg program in this folder, compiled by
 * psp2cgc (tools/cgc.py, run by vsh/paf/cg/cg.cmake into the build folder), as the binary it
 * registers with its shader patcher. Sony kept them as initialised data, in this order; the
 * symbols are ours.
 */

	.macro PROGRAM symbol, file
	.section .data.\symbol, "aw"
	.balign 4
	.global \symbol
	.type \symbol, %object
\symbol:
	.incbin "\file"
	.size \symbol, . - \symbol
	.endm

	PROGRAM g_tilebackend_color_f_gxp, "tilebackend_color.fp.gxp"
	PROGRAM g_tilebackend_texture_f_gxp, "tilebackend_texture.fp.gxp"
	PROGRAM g_tilebackend_v_gxp, "tilebackend.vp.gxp"
