/*
 * The macroblock loops and the motion saving: code overlays of the picture
 * decoder, loaded per picture (startPicture) by their __*_load and __*_size.
 * Each overlay is one function, at runMacroblocks or saveMacroblockMotion.
 * Included by fw1/pu1/pu1.ld and fw2/codec/codec.ld, which set LOOPS_START
 * (the local RAM after the resident overlay) and DMA_BASE (what the DMA
 * addresses of the image are relative to: fw1 gives addresses, FW2 offsets),
 * and define the memory region "image" the overlays' images go in.
 */
SECTIONS
{
	runMacroblocks = ALIGN(LOOPS_START, 8);
	.loop_sd runMacroblocks : { *(.loop_sd.text) . = ALIGN(8); } AT> image
	.loop_hd runMacroblocks : { *(.loop_hd.text) . = ALIGN(8); } AT> image
	.loop_mbaff runMacroblocks : { *(.loop_mbaff.text) . = ALIGN(8); } AT> image
	saveMacroblockMotion = ALIGN(runMacroblocks + MAX(SIZEOF(.loop_sd),
		MAX(SIZEOF(.loop_hd), SIZEOF(.loop_mbaff))), 8);
	.motion_inferred saveMacroblockMotion : { *(.motion_inferred.text) . = ALIGN(8); } AT> image
	.motion_all saveMacroblockMotion : { *(.motion_all.text) . = ALIGN(8); } AT> image
	__loops_end = saveMacroblockMotion +
		MAX(SIZEOF(.motion_inferred), SIZEOF(.motion_all));
}

__loop_sd_load = LOADADDR(.loop_sd) - DMA_BASE;
__loop_sd_size = SIZEOF(.loop_sd);
__loop_hd_load = LOADADDR(.loop_hd) - DMA_BASE;
__loop_hd_size = SIZEOF(.loop_hd);
__loop_mbaff_load = LOADADDR(.loop_mbaff) - DMA_BASE;
__loop_mbaff_size = SIZEOF(.loop_mbaff);
__motion_inferred_load = LOADADDR(.motion_inferred) - DMA_BASE;
__motion_inferred_size = SIZEOF(.motion_inferred);
__motion_all_load = LOADADDR(.motion_all) - DMA_BASE;
__motion_all_size = SIZEOF(.motion_all);

ASSERT(runSdLoop == runMacroblocks && runHdLoop == runMacroblocks &&
	runMbaffLoop == runMacroblocks, "a loop overlay does not start with its loop")
ASSERT(saveInferredMotion == saveMacroblockMotion && saveAllMotion == saveMacroblockMotion,
	"a motion overlay does not start with its function")
