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

	.global ScePafMisc_B002E56C
	.type ScePafMisc_B002E56C, %function
	.thumb_func
ScePafMisc_B002E56C:
	.fnstart
	push {r4, lr}
	sub sp, #0x60
	movw r4, #:lower16:__stack_chk_guard
	movt r4, #:upper16:__stack_chk_guard
	ldr r1, [r4]
	str r1, [sp, #0x58]
	add r1, sp, #0
	blx sceIoGetstat
	cmp r0, #0
	bne .LnotdirB002
	ldr r0, [sp]
	ands r0, r0, #0x1000
	beq .LnotdirB002
	movs r0, #1
	b .LcheckB002
.LnotdirB002:
	movs r0, #0
.LcheckB002:
	ldr r2, [sp, #0x58]
	ldr r1, [r4]
	cmp r1, r2
	bne .LfailB002
	add sp, #0x60
	pop {r4, pc}
.LfailB002:
	blx __stack_chk_fail
	nop
	.fnend
	.size ScePafMisc_B002E56C, . - ScePafMisc_B002E56C
