/*
 * SceVeneziaImage: carries the firmware image for Venezia (the codec engine).
 *
 * The module has no code and exports nothing but its module info: the
 * loader of the image finds it through the module's data segment, which
 * holds the image built from firmware/ (IMAGE_BIN, CMakeLists.txt).
 */

	.data
	.balign	16

	.global	image
	.type	image, %object
image:
	.incbin	IMAGE_BIN
	.size	image, . - image
