/*
 * PAF's GPU programs: each Cg program in this folder, compiled by psp2cgc (tools/cgc.py, run by
 * cg.cmake into the build folder), as the binary its class hands to the shader patcher.
 */

	.macro PROGRAM symbol, file
	.section .rodata.\symbol, "a"
	.balign 4
	.global \symbol
	.type \symbol, %object
\symbol:
	.incbin "\file"
	.size \symbol, . - \symbol
	.endm

	PROGRAM _ZN3paf5graph3arl9ClearBase10FragShader8s_binaryE, "arl/clear.fp.gxp"
	PROGRAM _ZN3paf5graph3arl9ClearBase10VertShader8s_binaryE, "arl/clear.vp.gxp"
	PROGRAM _ZN3paf5graph3arl15FrameBufferCopy10FragShader8s_binaryE, "arl/copy.fp.gxp"
	PROGRAM _ZN3paf5graph3arl15FrameBufferCopy10VertShader8s_binaryE, "arl/copy.vp.gxp"
	PROGRAM _ZN3paf2ui20g_canvas_brush_f_gxpE, "canvas/brush.fp.gxp"
	PROGRAM _ZN3paf2ui20g_canvas_brush_v_gxpE, "canvas/brush.vp.gxp"
	PROGRAM _ZN3paf2ui20g_canvas_clear_f_gxpE, "canvas/clear.fp.gxp"
	PROGRAM _ZN3paf2ui20g_canvas_clear_v_gxpE, "canvas/clear.vp.gxp"
	PROGRAM _ZN3paf2ui19g_canvas_copy_f_gxpE, "canvas/copy.fp.gxp"
	PROGRAM _ZN3paf2ui19g_canvas_copy_v_gxpE, "canvas/copy.vp.gxp"
	PROGRAM _ZN3paf2ui20g_canvas_point_f_gxpE, "canvas/point.fp.gxp"
	PROGRAM _ZN3paf2ui20g_canvas_point_v_gxpE, "canvas/point.vp.gxp"
