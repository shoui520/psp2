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
	PROGRAM _ZN3paf5graph17UniformBlurShader8s_binaryE, "misc/blur/uniform_blur.fp.gxp"
	PROGRAM _ZN3paf5graph13DobInitShader8s_binaryE, "misc/dof/dob_init.fp.gxp"
	PROGRAM _ZN3paf5graph15DobSpreadShader8s_binaryE, "misc/dof/dob_spread.fp.gxp"
	PROGRAM _ZN3paf5graph16DobCombineShader8s_binaryE, "misc/dof/dob_combine.fp.gxp"
	PROGRAM _ZN3paf5graph17SkydomeFragShader8s_binaryE, "drawobj/wave/skydome.fp.gxp"
	PROGRAM _ZN3paf5graph11ClearShader8s_binaryE, "misc/texture_ops/clear.fp.gxp"
	PROGRAM _ZN3paf5graph14PureCopyShader8s_binaryE, "misc/texture_ops/pure_copy.fp.gxp"
	PROGRAM _ZN3paf5graph10CopyShader8s_binaryE, "misc/texture_ops/copy.fp.gxp"
	PROGRAM _ZN3paf5graph10HalfShader8s_binaryE, "misc/texture_ops/half.fp.gxp"
	PROGRAM _ZN3paf5graph14QuadVertShader8s_binaryE, "misc/texture_ops/quad.vp.gxp"
	PROGRAM _ZN3paf5graph16GridNormalShader8s_binaryE, "misc/texture_ops/grid_normal.fp.gxp"
	PROGRAM _ZN3paf5graph17GridNormal8Shader8s_binaryE, "misc/texture_ops/grid_normal_8.fp.gxp"
	PROGRAM _ZN3paf5graph14FlipQuadShader8s_binaryE, "misc/texture_ops/flipquad.fp.gxp"
	PROGRAM _ZN3paf5graph13FlipTriShader8s_binaryE, "misc/texture_ops/fliptri.fp.gxp"
	PROGRAM _ZN3paf5graph14BoxBlur1Shader8s_binaryE, "misc/texture_ops/box_blur_1.fp.gxp"
	PROGRAM _ZN3paf5graph14BoxBlurNShader8s_binaryE, "misc/texture_ops/box_blur_n.fp.gxp"
	PROGRAM _ZN3paf5graph14BoxBlur2Shader8s_binaryE, "misc/texture_ops/box_blur_2.fp.gxp"
	PROGRAM _ZN3paf5graph14BoxBlur3Shader8s_binaryE, "misc/texture_ops/box_blur_3.fp.gxp"
	PROGRAM _ZN3paf5graph14BoxBlur4Shader8s_binaryE, "misc/texture_ops/box_blur_4.fp.gxp"
	PROGRAM _ZN3paf5graph14BoxBlur5Shader8s_binaryE, "misc/texture_ops/box_blur_5.fp.gxp"
	PROGRAM _ZN3paf5graph18VariableBlurShader8s_binaryE, "misc/blur/variable_blur.fp.gxp"
	PROGRAM _ZN3paf5graph22BoxBlurNModulateShader8s_binaryE, "misc/texture_ops/box_blur_n_modulate.fp.gxp"
	PROGRAM _ZN3paf5graph22BoxBlur1ModulateShader8s_binaryE, "misc/texture_ops/box_blur_1_modulate.fp.gxp"
	PROGRAM _ZN3paf5graph22BoxBlur2ModulateShader8s_binaryE, "misc/texture_ops/box_blur_2_modulate.fp.gxp"
	PROGRAM _ZN3paf5graph22BoxBlur3ModulateShader8s_binaryE, "misc/texture_ops/box_blur_3_modulate.fp.gxp"
	PROGRAM _ZN3paf5graph22BoxBlur4ModulateShader8s_binaryE, "misc/texture_ops/box_blur_4_modulate.fp.gxp"
	PROGRAM _ZN3paf5graph22BoxBlur5ModulateShader8s_binaryE, "misc/texture_ops/box_blur_5_modulate.fp.gxp"
	PROGRAM _ZN3paf5graph13BicubicShader8s_binaryE, "misc/texture_ops/bicubic.fp.gxp"
