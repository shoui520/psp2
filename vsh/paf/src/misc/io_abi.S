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

	.global ScePafMisc_2E8E9CBB
	.type ScePafMisc_2E8E9CBB, %function
	.thumb_func
ScePafMisc_2E8E9CBB:
	.fnstart
	push {r4, lr}
	adds r4, r0, #0
	ldr r0, [r4]
	cmp.w r0, #-1
	beq .Ldone2E
	blx sceIoClose
	movs.w r0, #-1
	str r0, [r4]
.Ldone2E:
	pop {r4, pc}
	.fnend
	.size ScePafMisc_2E8E9CBB, . - ScePafMisc_2E8E9CBB

	.global ScePafMisc_95F2C107
	.type ScePafMisc_95F2C107, %function
	.thumb_func
ScePafMisc_95F2C107:
	.fnstart
	push {r4, lr}
	adds r4, r0, #0
	adds r0, r1, #0
	blx sceIoDopen
	cmp r0, #0
	bge .Lsuccess95
	b .Ldone95
.Lsuccess95:
	adds r1, r0, #0
	str r1, [r4]
	movs r0, #0
.Ldone95:
	pop {r4, pc}
	.fnend
	.size ScePafMisc_95F2C107, . - ScePafMisc_95F2C107
