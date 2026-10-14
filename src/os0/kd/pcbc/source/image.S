/*
 * SceSblPcbcBin: carries the encrypted PCBC secure-module image for the secure processor.
 *
 * The module has no code and exports nothing but its module info: the
 * loader of the image finds it through the module's data segment, which
 * holds the image verbatim (data/, one file per distinct image).
 */

	.data
	.balign	16

	.global	image
	.type	image, %object
image:
	.incbin	IMAGE_BIN
	.size	image, . - image
