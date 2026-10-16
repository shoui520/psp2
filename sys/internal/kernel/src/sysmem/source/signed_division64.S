/* Signed EABI 64-bit division; preserve native high-word zero flags. */
.syntax unified
.thumb
.text
.balign 4
.global __aeabi_ldivmod
.type __aeabi_ldivmod, %function
.thumb_func
__aeabi_ldivmod:
	cbnz r3, .Lsigned64_nonzero
	cbnz r2, .Lsigned64_nonzero
	cmp r1, #0
	it eq
	cmpeq r0, #0
	itt lt
	movlt r0, #0
	movlt.w r1, #0x80000000
	itt gt
	mvngt.w r1, #0x80000000
	movgt.w r0, #-1
	push {r0, r1, r2}
	ldr r0, .Lsigned64_zero_target
	adr r1, .Lsigned64_zero_target
	add r0, r1
	str r0, [sp, #8]
	pop {r0, r1, pc}
.Lsigned64_zero_target:
	.word HandleDivisionByZero - .Lsigned64_zero_target
.Lsigned64_nonzero:
	sub sp, #8
	mov ip, sp
	stmdb sp!, {ip, lr}
	bl SignedDivision64WithRemainder
	ldr.w lr, [sp, #4]
	add sp, #8
	pop {r2, r3}
	bx lr
	nop
.size __aeabi_ldivmod, . - __aeabi_ldivmod

/* Names describe proved arithmetic roles; Sony spellings are unknown. */
.type SignedDivision64WithRemainder, %function
.thumb_func
SignedDivision64WithRemainder:
	stmdb sp!, {r4, r5, r6, r7, r8, r9, sl, lr}
	mov r7, r2
	mov r8, r0
	mov r9, r1
	mov sl, r3
	ldr r6, [sp, #32]
	bl SignedDivision64Quotient
	umull r4, r5, r7, r0
	mul.w r7, r1, r7
	subs.w r4, r8, r4
	mla r2, r0, sl, r7
	add r5, r2
	sbc.w r5, r9, r5
	strd r4, r5, [r6]
	ldmia.w sp!, {r4, r5, r6, r7, r8, r9, sl, pc}
.size SignedDivision64WithRemainder, . - SignedDivision64WithRemainder

/* Signed magnitudes, base-2^16 estimates, correction and sign restoration. */
.type SignedDivision64Quotient, %function
.thumb_func
SignedDivision64Quotient:
.Lsigned64_quotient_000:
	stmdb sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
.Lsigned64_quotient_004:
	negs r4, r0
.Lsigned64_quotient_006:
	sbc.w r5, r1, r1, lsl #1
.Lsigned64_quotient_00a:
	cmp r1, #0
.Lsigned64_quotient_00c:
	mov.w r6, #4294967295
.Lsigned64_quotient_010:
	sub sp, #20
.Lsigned64_quotient_012:
	ittt ge
.Lsigned64_quotient_014:
	movge r4, r0
.Lsigned64_quotient_016:
	movge r5, r1
.Lsigned64_quotient_018:
	movge r6, #0
.Lsigned64_quotient_01a:
	cmp r3, #0
.Lsigned64_quotient_01c:
	blt.w .Lsigned64_quotient_1ac
.Lsigned64_quotient_020:
	mov ip, r4
.Lsigned64_quotient_022:
	mov r0, r5
.Lsigned64_quotient_024:
	mov lr, r2
.Lsigned64_quotient_026:
	mov r1, r3
.Lsigned64_quotient_028:
	mov r8, r2
.Lsigned64_quotient_02a:
	mov r7, r4
.Lsigned64_quotient_02c:
	mov r9, r5
.Lsigned64_quotient_02e:
	cmp r3, #0
.Lsigned64_quotient_030:
	bne.n .Lsigned64_quotient_0e2
.Lsigned64_quotient_032:
	cmp r2, r5
.Lsigned64_quotient_034:
	bls.n .Lsigned64_quotient_10e
.Lsigned64_quotient_036:
	clz r3, r2
.Lsigned64_quotient_03a:
	cbz r3, .Lsigned64_quotient_052
.Lsigned64_quotient_03c:
	rsb r2, r3, #32
.Lsigned64_quotient_040:
	lsl.w r9, r5, r3
.Lsigned64_quotient_044:
	lsl.w r8, lr, r3
.Lsigned64_quotient_048:
	lsls r7, r3
.Lsigned64_quotient_04a:
	lsr.w r2, r4, r2
.Lsigned64_quotient_04e:
	orr.w r9, r2, r9
.Lsigned64_quotient_052:
	mov.w r4, r8, lsr #16
.Lsigned64_quotient_056:
	mov r0, r9
.Lsigned64_quotient_058:
	uxth.w sl, r8
.Lsigned64_quotient_05c:
	mov r1, r4
.Lsigned64_quotient_05e:
	bl __aeabi_uidiv
.Lsigned64_quotient_062:
	mov r1, r4
.Lsigned64_quotient_064:
	mov fp, r0
.Lsigned64_quotient_066:
	mov r0, r9
.Lsigned64_quotient_068:
	bl __aeabi_uidivmod
.Lsigned64_quotient_06c:
	mul.w r0, sl, fp
.Lsigned64_quotient_070:
	lsrs r2, r7, #16
.Lsigned64_quotient_072:
	orr.w r1, r2, r1, lsl #16
.Lsigned64_quotient_076:
	cmp r0, r1
.Lsigned64_quotient_078:
	bls.n .Lsigned64_quotient_092
.Lsigned64_quotient_07a:
	adds.w r1, r1, r8
.Lsigned64_quotient_07e:
	add.w r3, fp, #4294967295
.Lsigned64_quotient_082:
	bcs.n .Lsigned64_quotient_090
.Lsigned64_quotient_084:
	cmp r0, r1
.Lsigned64_quotient_086:
	itt hi
.Lsigned64_quotient_088:
	subhi.w fp, fp, #2
.Lsigned64_quotient_08c:
	addhi r1, r8
.Lsigned64_quotient_08e:
	bhi.n .Lsigned64_quotient_092
.Lsigned64_quotient_090:
	mov fp, r3
.Lsigned64_quotient_092:
	sub.w r9, r1, r0
.Lsigned64_quotient_096:
	mov r1, r4
.Lsigned64_quotient_098:
	uxth r7, r7
.Lsigned64_quotient_09a:
	mov r0, r9
.Lsigned64_quotient_09c:
	bl __aeabi_uidiv
.Lsigned64_quotient_0a0:
	mov r1, r4
.Lsigned64_quotient_0a2:
	mov r5, r0
.Lsigned64_quotient_0a4:
	mov r0, r9
.Lsigned64_quotient_0a6:
	bl __aeabi_uidivmod
.Lsigned64_quotient_0aa:
	mul.w sl, sl, r5
.Lsigned64_quotient_0ae:
	orr.w r1, r7, r1, lsl #16
.Lsigned64_quotient_0b2:
	cmp sl, r1
.Lsigned64_quotient_0b4:
	bls.n .Lsigned64_quotient_0ca
.Lsigned64_quotient_0b6:
	adds.w r8, r1, r8
.Lsigned64_quotient_0ba:
	add.w r3, r5, #4294967295
.Lsigned64_quotient_0be:
	bcs.n .Lsigned64_quotient_0c8
.Lsigned64_quotient_0c0:
	cmp sl, r8
.Lsigned64_quotient_0c2:
	it hi
.Lsigned64_quotient_0c4:
	subhi r5, #2
.Lsigned64_quotient_0c6:
	bhi.n .Lsigned64_quotient_0ca
.Lsigned64_quotient_0c8:
	mov r5, r3
.Lsigned64_quotient_0ca:
	orr.w r3, r5, fp, lsl #16
.Lsigned64_quotient_0ce:
	movs r4, #0
.Lsigned64_quotient_0d0:
	mov r0, r3
.Lsigned64_quotient_0d2:
	mov r1, r4
.Lsigned64_quotient_0d4:
	cbz r6, .Lsigned64_quotient_0dc
.Lsigned64_quotient_0d6:
	negs r0, r0
.Lsigned64_quotient_0d8:
	sbc.w r1, r1, r1, lsl #1
.Lsigned64_quotient_0dc:
	add sp, #20
.Lsigned64_quotient_0de:
	ldmia.w sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
.Lsigned64_quotient_0e2:
	cmp r3, r5
.Lsigned64_quotient_0e4:
	itt hi
.Lsigned64_quotient_0e6:
	movhi r4, #0
.Lsigned64_quotient_0e8:
	movhi r3, r4
.Lsigned64_quotient_0ea:
	bhi.n .Lsigned64_quotient_0d0
.Lsigned64_quotient_0ec:
	clz r5, r1
.Lsigned64_quotient_0f0:
	cmp r5, #0
.Lsigned64_quotient_0f2:
	bne.w .Lsigned64_quotient_260
.Lsigned64_quotient_0f6:
	cmp r1, r0
.Lsigned64_quotient_0f8:
	it cs
.Lsigned64_quotient_0fa:
	cmpcs r2, ip
.Lsigned64_quotient_0fc:
	ite hi
.Lsigned64_quotient_0fe:
	movhi r4, #0
.Lsigned64_quotient_100:
	movls r4, #1
.Lsigned64_quotient_102:
	itt ls
.Lsigned64_quotient_104:
	movls r3, #1
.Lsigned64_quotient_106:
	movls r4, r5
.Lsigned64_quotient_108:
	bls.n .Lsigned64_quotient_0d0
.Lsigned64_quotient_10a:
	mov r3, r4
.Lsigned64_quotient_10c:
	b.n .Lsigned64_quotient_0d0
.Lsigned64_quotient_10e:
	cbnz r2, .Lsigned64_quotient_11a
.Lsigned64_quotient_110:
	mov r1, r2
.Lsigned64_quotient_112:
	movs r0, #1
.Lsigned64_quotient_114:
	bl __aeabi_uidiv
.Lsigned64_quotient_118:
	mov r8, r0
.Lsigned64_quotient_11a:
	clz r3, r8
.Lsigned64_quotient_11e:
	cmp r3, #0
.Lsigned64_quotient_120:
	bne.n .Lsigned64_quotient_1b6
.Lsigned64_quotient_122:
	sub.w r9, r9, r8
.Lsigned64_quotient_126:
	mov.w r5, r8, lsr #16
.Lsigned64_quotient_12a:
	uxth.w sl, r8
.Lsigned64_quotient_12e:
	movs r4, #1
.Lsigned64_quotient_130:
	mov r1, r5
.Lsigned64_quotient_132:
	mov r0, r9
.Lsigned64_quotient_134:
	bl __aeabi_uidiv
.Lsigned64_quotient_138:
	mov r1, r5
.Lsigned64_quotient_13a:
	mov fp, r0
.Lsigned64_quotient_13c:
	mov r0, r9
.Lsigned64_quotient_13e:
	bl __aeabi_uidivmod
.Lsigned64_quotient_142:
	mul.w r0, sl, fp
.Lsigned64_quotient_146:
	lsrs r2, r7, #16
.Lsigned64_quotient_148:
	orr.w r1, r2, r1, lsl #16
.Lsigned64_quotient_14c:
	cmp r0, r1
.Lsigned64_quotient_14e:
	bls.n .Lsigned64_quotient_16a
.Lsigned64_quotient_150:
	adds.w r1, r1, r8
.Lsigned64_quotient_154:
	add.w r3, fp, #4294967295
.Lsigned64_quotient_158:
	bcs.w .Lsigned64_quotient_33a
.Lsigned64_quotient_15c:
	cmp r0, r1
.Lsigned64_quotient_15e:
	itt hi
.Lsigned64_quotient_160:
	subhi.w fp, fp, #2
.Lsigned64_quotient_164:
	addhi r1, r8
.Lsigned64_quotient_166:
	bls.w .Lsigned64_quotient_33a
.Lsigned64_quotient_16a:
	subs r2, r1, r0
.Lsigned64_quotient_16c:
	mov r1, r5
.Lsigned64_quotient_16e:
	uxth r7, r7
.Lsigned64_quotient_170:
	mov r0, r2
.Lsigned64_quotient_172:
	str r2, [sp, #4]
.Lsigned64_quotient_174:
	bl __aeabi_uidiv
.Lsigned64_quotient_178:
	ldr r2, [sp, #4]
.Lsigned64_quotient_17a:
	mov r1, r5
.Lsigned64_quotient_17c:
	mov r9, r0
.Lsigned64_quotient_17e:
	mov r0, r2
.Lsigned64_quotient_180:
	bl __aeabi_uidivmod
.Lsigned64_quotient_184:
	mul.w sl, sl, r9
.Lsigned64_quotient_188:
	orr.w r1, r7, r1, lsl #16
.Lsigned64_quotient_18c:
	cmp sl, r1
.Lsigned64_quotient_18e:
	bls.n .Lsigned64_quotient_1a6
.Lsigned64_quotient_190:
	adds.w r8, r1, r8
.Lsigned64_quotient_194:
	add.w r3, r9, #4294967295
.Lsigned64_quotient_198:
	bcs.n .Lsigned64_quotient_1a4
.Lsigned64_quotient_19a:
	cmp sl, r8
.Lsigned64_quotient_19c:
	it hi
.Lsigned64_quotient_19e:
	subhi.w r9, r9, #2
.Lsigned64_quotient_1a2:
	bhi.n .Lsigned64_quotient_1a6
.Lsigned64_quotient_1a4:
	mov r9, r3
.Lsigned64_quotient_1a6:
	orr.w r3, r9, fp, lsl #16
.Lsigned64_quotient_1aa:
	b.n .Lsigned64_quotient_0d0
.Lsigned64_quotient_1ac:
	mvns r6, r6
.Lsigned64_quotient_1ae:
	negs r2, r2
.Lsigned64_quotient_1b0:
	sbc.w r3, r3, r3, lsl #1
.Lsigned64_quotient_1b4:
	b.n .Lsigned64_quotient_020
.Lsigned64_quotient_1b6:
	rsb r2, r3, #32
.Lsigned64_quotient_1ba:
	lsl.w r8, r8, r3
.Lsigned64_quotient_1be:
	lsl.w r4, r9, r3
.Lsigned64_quotient_1c2:
	mov.w r5, r8, lsr #16
.Lsigned64_quotient_1c6:
	lsr.w r9, r9, r2
.Lsigned64_quotient_1ca:
	lsr.w fp, r7, r2
.Lsigned64_quotient_1ce:
	lsls r7, r3
.Lsigned64_quotient_1d0:
	mov r1, r5
.Lsigned64_quotient_1d2:
	mov r0, r9
.Lsigned64_quotient_1d4:
	bl __aeabi_uidiv
.Lsigned64_quotient_1d8:
	mov r1, r5
.Lsigned64_quotient_1da:
	uxth.w sl, r8
.Lsigned64_quotient_1de:
	orr.w fp, fp, r4
.Lsigned64_quotient_1e2:
	mov r2, r0
.Lsigned64_quotient_1e4:
	mov r0, r9
.Lsigned64_quotient_1e6:
	str r2, [sp, #4]
.Lsigned64_quotient_1e8:
	bl __aeabi_uidivmod
.Lsigned64_quotient_1ec:
	ldr r2, [sp, #4]
.Lsigned64_quotient_1ee:
	mov.w r3, fp, lsr #16
.Lsigned64_quotient_1f2:
	mul.w r0, sl, r2
.Lsigned64_quotient_1f6:
	orr.w r1, r3, r1, lsl #16
.Lsigned64_quotient_1fa:
	cmp r0, r1
.Lsigned64_quotient_1fc:
	bls.n .Lsigned64_quotient_216
.Lsigned64_quotient_1fe:
	adds.w r1, r1, r8
.Lsigned64_quotient_202:
	add.w r3, r2, #4294967295
.Lsigned64_quotient_206:
	bcs.w .Lsigned64_quotient_346
.Lsigned64_quotient_20a:
	cmp r0, r1
.Lsigned64_quotient_20c:
	itt hi
.Lsigned64_quotient_20e:
	subhi r2, #2
.Lsigned64_quotient_210:
	addhi r1, r8
.Lsigned64_quotient_212:
	bls.w .Lsigned64_quotient_346
.Lsigned64_quotient_216:
	sub.w r9, r1, r0
.Lsigned64_quotient_21a:
	mov r1, r5
.Lsigned64_quotient_21c:
	str r2, [sp, #4]
.Lsigned64_quotient_21e:
	uxth.w fp, fp
.Lsigned64_quotient_222:
	mov r0, r9
.Lsigned64_quotient_224:
	bl __aeabi_uidiv
.Lsigned64_quotient_228:
	mov r1, r5
.Lsigned64_quotient_22a:
	mov r4, r0
.Lsigned64_quotient_22c:
	mov r0, r9
.Lsigned64_quotient_22e:
	bl __aeabi_uidivmod
.Lsigned64_quotient_232:
	mul.w r9, sl, r4
.Lsigned64_quotient_236:
	ldr r2, [sp, #4]
.Lsigned64_quotient_238:
	orr.w r1, fp, r1, lsl #16
.Lsigned64_quotient_23c:
	cmp r9, r1
.Lsigned64_quotient_23e:
	bls.n .Lsigned64_quotient_256
.Lsigned64_quotient_240:
	adds.w r1, r1, r8
.Lsigned64_quotient_244:
	add.w r3, r4, #4294967295
.Lsigned64_quotient_248:
	bcs.n .Lsigned64_quotient_254
.Lsigned64_quotient_24a:
	cmp r9, r1
.Lsigned64_quotient_24c:
	itt hi
.Lsigned64_quotient_24e:
	subhi r4, #2
.Lsigned64_quotient_250:
	addhi r1, r8
.Lsigned64_quotient_252:
	bhi.n .Lsigned64_quotient_256
.Lsigned64_quotient_254:
	mov r4, r3
.Lsigned64_quotient_256:
	sub.w r9, r1, r9
.Lsigned64_quotient_25a:
	orr.w r4, r4, r2, lsl #16
.Lsigned64_quotient_25e:
	b.n .Lsigned64_quotient_130
.Lsigned64_quotient_260:
	rsb r3, r5, #32
.Lsigned64_quotient_264:
	lsls r2, r5
.Lsigned64_quotient_266:
	lsls r1, r5
.Lsigned64_quotient_268:
	lsl.w ip, r0, r5
.Lsigned64_quotient_26c:
	lsr.w lr, lr, r3
.Lsigned64_quotient_270:
	str r2, [sp, #12]
.Lsigned64_quotient_272:
	orr.w r2, lr, r1
.Lsigned64_quotient_276:
	lsr.w r4, r0, r3
.Lsigned64_quotient_27a:
	lsr.w sl, r7, r3
.Lsigned64_quotient_27e:
	mov.w r9, r2, lsr #16
.Lsigned64_quotient_282:
	mov r0, r4
.Lsigned64_quotient_284:
	uxth.w fp, r2
.Lsigned64_quotient_288:
	orr.w sl, sl, ip
.Lsigned64_quotient_28c:
	mov r1, r9
.Lsigned64_quotient_28e:
	str r2, [sp, #4]
.Lsigned64_quotient_290:
	bl __aeabi_uidiv
.Lsigned64_quotient_294:
	mov r1, r9
.Lsigned64_quotient_296:
	mov r8, r0
.Lsigned64_quotient_298:
	mov r0, r4
.Lsigned64_quotient_29a:
	bl __aeabi_uidivmod
.Lsigned64_quotient_29e:
	mul.w r0, fp, r8
.Lsigned64_quotient_2a2:
	mov.w r4, sl, lsr #16
.Lsigned64_quotient_2a6:
	ldr r2, [sp, #4]
.Lsigned64_quotient_2a8:
	orr.w r4, r4, r1, lsl #16
.Lsigned64_quotient_2ac:
	cmp r0, r4
.Lsigned64_quotient_2ae:
	bls.n .Lsigned64_quotient_2c4
.Lsigned64_quotient_2b0:
	adds r4, r4, r2
.Lsigned64_quotient_2b2:
	add.w r1, r8, #4294967295
.Lsigned64_quotient_2b6:
	bcs.n .Lsigned64_quotient_342
.Lsigned64_quotient_2b8:
	cmp r0, r4
.Lsigned64_quotient_2ba:
	itt hi
.Lsigned64_quotient_2bc:
	subhi.w r8, r8, #2
.Lsigned64_quotient_2c0:
	addhi r4, r4, r2
.Lsigned64_quotient_2c2:
	bls.n .Lsigned64_quotient_342
.Lsigned64_quotient_2c4:
	sub.w ip, r4, r0
.Lsigned64_quotient_2c8:
	mov r1, r9
.Lsigned64_quotient_2ca:
	uxth.w sl, sl
.Lsigned64_quotient_2ce:
	mov r0, ip
.Lsigned64_quotient_2d0:
	strd r2, ip, [sp, #4]
.Lsigned64_quotient_2d4:
	bl __aeabi_uidiv
.Lsigned64_quotient_2d8:
	ldr.w ip, [sp, #8]
.Lsigned64_quotient_2dc:
	mov r1, r9
.Lsigned64_quotient_2de:
	mov r4, r0
.Lsigned64_quotient_2e0:
	mov r0, ip
.Lsigned64_quotient_2e2:
	bl __aeabi_uidivmod
.Lsigned64_quotient_2e6:
	mul.w fp, fp, r4
.Lsigned64_quotient_2ea:
	ldr r2, [sp, #4]
.Lsigned64_quotient_2ec:
	orr.w r1, sl, r1, lsl #16
.Lsigned64_quotient_2f0:
	cmp fp, r1
.Lsigned64_quotient_2f2:
	bls.n .Lsigned64_quotient_306
.Lsigned64_quotient_2f4:
	adds r1, r1, r2
.Lsigned64_quotient_2f6:
	add.w r0, r4, #4294967295
.Lsigned64_quotient_2fa:
	bcs.n .Lsigned64_quotient_33e
.Lsigned64_quotient_2fc:
	cmp fp, r1
.Lsigned64_quotient_2fe:
	itt hi
.Lsigned64_quotient_300:
	subhi r4, #2
.Lsigned64_quotient_302:
	addhi r1, r1, r2
.Lsigned64_quotient_304:
	bls.n .Lsigned64_quotient_33e
.Lsigned64_quotient_306:
	sub.w fp, r1, fp
.Lsigned64_quotient_30a:
	ldr r1, [sp, #12]
.Lsigned64_quotient_30c:
	orr.w r0, r4, r8, lsl #16
.Lsigned64_quotient_310:
	umull r2, r3, r0, r1
.Lsigned64_quotient_314:
	cmp fp, r3
.Lsigned64_quotient_316:
	bcc.n .Lsigned64_quotient_334
.Lsigned64_quotient_318:
	ite ne
.Lsigned64_quotient_31a:
	movne r3, #0
.Lsigned64_quotient_31c:
	moveq r3, #1
.Lsigned64_quotient_31e:
	lsl.w r4, r7, r5
.Lsigned64_quotient_322:
	cmp r4, r2
.Lsigned64_quotient_324:
	ite cs
.Lsigned64_quotient_326:
	movcs r4, #0
.Lsigned64_quotient_328:
	andcc.w r4, r3, #1
.Lsigned64_quotient_32c:
	mov r3, r0
.Lsigned64_quotient_32e:
	cmp r4, #0
.Lsigned64_quotient_330:
	beq.w .Lsigned64_quotient_0d0
.Lsigned64_quotient_334:
	subs r3, r0, #1
.Lsigned64_quotient_336:
	movs r4, #0
.Lsigned64_quotient_338:
	b.n .Lsigned64_quotient_0d0
.Lsigned64_quotient_33a:
	mov fp, r3
.Lsigned64_quotient_33c:
	b.n .Lsigned64_quotient_16a
.Lsigned64_quotient_33e:
	mov r4, r0
.Lsigned64_quotient_340:
	b.n .Lsigned64_quotient_306
.Lsigned64_quotient_342:
	mov r8, r1
.Lsigned64_quotient_344:
	b.n .Lsigned64_quotient_2c4
.Lsigned64_quotient_346:
	mov r2, r3
.Lsigned64_quotient_348:
	b.n .Lsigned64_quotient_216
.Lsigned64_quotient_34a:
	nop 
.size SignedDivision64Quotient, . - SignedDivision64Quotient

.section .note.GNU-stack,"",%progbits
