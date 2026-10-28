/*
 * SceSblPcffBin: carries the ePSP Flash0 firmware image (SceCompatBootFile[])
 * loaded at 0x8B000000 by compat.skprx.
 *
 * The module has no code and exports nothing but its module info: the
 * loader of the image finds it through the module's data segment, which
 * holds the image verbatim.
 */

	.data
	.balign	16

	.global	image
	.type	image, %object
image:
	.incbin	IMAGE_BIN
	.size	image, . - image
