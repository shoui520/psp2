/* Two native filesystem directory-creation adapters. */
	.syntax unified
	.thumb
	.text

	.global ScePafMisc_53EAF163
	.type ScePafMisc_53EAF163, %function
	.thumb_func
ScePafMisc_53EAF163:
	.fnstart
	push {r4, lr}
	blx sceIoMkdir
	pop {r4, pc}
	.fnend
	.size ScePafMisc_53EAF163, . - ScePafMisc_53EAF163

	.global ScePafMisc_198A4C81
	.type ScePafMisc_198A4C81, %function
	.thumb_func
ScePafMisc_198A4C81:
	.fnstart
	push {r4, lr}
	movs r1, #6
	blx sceIoMkdir
	pop {r4, pc}
	.fnend
	.size ScePafMisc_198A4C81, . - ScePafMisc_198A4C81
