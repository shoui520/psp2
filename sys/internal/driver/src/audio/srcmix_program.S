/*
 * The SrcMix program, assembled from srcmix.asm by tools/srcmixasm.py (CMakeLists.txt).
 * audioInitializeSrcMix uploads it in two parts: the first 256 words, then the rest.
 */
.section .rodata.audioSrcMixProgram, "a", %progbits
.balign 4

.global audioSrcMixProgram
.type audioSrcMixProgram, %object
audioSrcMixProgram:
	.incbin "srcmix.bin", 0, 256 * 4
.size audioSrcMixProgram, . - audioSrcMixProgram

.global audioSrcMixSecondaryProgram
.type audioSrcMixSecondaryProgram, %object
audioSrcMixSecondaryProgram:
	.incbin "srcmix.bin", 256 * 4
.size audioSrcMixSecondaryProgram, . - audioSrcMixSecondaryProgram
