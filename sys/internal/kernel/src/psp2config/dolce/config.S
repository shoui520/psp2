/*
 * SceKernelPsp2Config (PS TV (Dolce)): the kernel boot configuration text.
 *
 * The boot loader reads the text between the two exported addresses: lines of
 * "load"/"tload" <module path>, "if <FLAG>" / "else" / "endif", '#' comments.
 * Development firmwares ship config/tool.txt, retail ones config/cex.txt.
 */

	.section .rodata.psp2config, "a"

	.global	SceKernelPsp2Config_A6483A7C	/* name unknown: start of the text */
	.type	SceKernelPsp2Config_A6483A7C, %object
SceKernelPsp2Config_A6483A7C:
	.incbin	PSP2CONFIG_TXT
	.size	SceKernelPsp2Config_A6483A7C, . - SceKernelPsp2Config_A6483A7C

	.global	SceKernelPsp2Config_C1BA20DA	/* name unknown: end of the text */
	.type	SceKernelPsp2Config_C1BA20DA, %object
SceKernelPsp2Config_C1BA20DA:
	.size	SceKernelPsp2Config_C1BA20DA, 0	/* readers stop at the end address */
