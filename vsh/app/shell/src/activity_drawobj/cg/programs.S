/*
 * ActivityDrawObj GPU programs: each Cg program in this folder, compiled by psp2cgc
 * (tools/cgc.py, run by vsh/paf/cg/cg.cmake into the build folder).
 */

	.macro PROGRAM symbol, file
	.section .rodata.\symbol, "a"
	.balign 4
	.global \symbol
	.type \symbol, %object
\symbol:
	.incbin "\file"
	.size \symbol, . - \symbol
	.global \symbol\()_size
	.balign 4
\symbol\()_size:
	.word . - \symbol - 4
	.endm

	PROGRAM g_activityInternalVpGxp, "activity_internal.vp.gxp"
	PROGRAM g_activityInternalFpGxp, "activity_internal.fp.gxp"
	PROGRAM g_activityColorOnlyVpGxp, "activity_color_only.vp.gxp"
	PROGRAM g_activityColorOnlyFpGxp, "activity_color_only.fp.gxp"
	PROGRAM g_activityInternalPartialVpGxp, "activity_internal_partial.vp.gxp"
	PROGRAM g_activityColorOnlyPartialVpGxp, "activity_color_only_partial.vp.gxp"
	PROGRAM g_activityPartialMaskVpGxp, "activity_partial_mask.vp.gxp"
	PROGRAM g_activityPartialMaskFpGxp, "activity_partial_mask.fp.gxp"
	PROGRAM g_activityInternalFullVpGxp, "activity_internal_full.vp.gxp"
	PROGRAM g_activityColorOnlyFullVpGxp, "activity_color_only_full.vp.gxp"
	PROGRAM g_activityFullMaskVpGxp, "activity_full_mask.vp.gxp"
	PROGRAM g_activityFullMaskFpGxp, "activity_full_mask.fp.gxp"
	PROGRAM g_activityLinkIconVpGxp, "activity_linkicon.vp.gxp"
	PROGRAM g_activityLinkIconFpGxp, "activity_linkicon.fp.gxp"
	PROGRAM g_activityAvatorVpGxp, "activity_avator.vp.gxp"
	PROGRAM g_activityAvatorFpGxp, "activity_avator.fp.gxp"
