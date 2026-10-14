/*
 * The icon shown while charging: a 90x70 PPM (binary RGB) image, Sony's original.
 * It is not kept in src/: ICON_PPM names a copy extracted from this module in the
 * original firmware, set in fw/<FW>.local.cmake.
 */
	.section .rodata.ewIcon, "a", %progbits
	.global ewIconPpm
ewIconPpm:
	.incbin ICON_PPM
	.size ewIconPpm, .-ewIconPpm
	.section .note.GNU-stack, "", %progbits
