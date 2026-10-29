/* Original object-chain and indexed-tree search exports. */
	.syntax unified
	.thumb
	.text

	.global ScePafMisc_07A5D381
	.type ScePafMisc_07A5D381, %function
	.thumb_func
ScePafMisc_07A5D381:
	.fnstart
	ldr r2, [r0, #4]
	movs r1, #0
	adds r0, r1, #0
	cbz r2, .Lcompare07
	ldr r1, [r2, #4]
.Lcompare07:
	cmp r1, r2
	beq .Lyes07
.Literate07:
	ldr r3, [r1, #8]
	cbz r3, .Ladvance07
	ldr r3, [r3, #4]
	cbz r3, .Ladvance07
	ldr.w ip, [r3, #0x10]
	ands r3, ip, #0xf000
	cmp.w r3, #0x3000
	beq .Ladvance07
	cmp.w r3, #0x2000
	bne .Lbranch07
	ands r3, ip, #0xf00
	beq .Ladvance07
.Lbranch07:
	b .Ldone07
.Ladvance07:
	cbz r1, .Lloopcompare07
	ldr r1, [r1, #4]
.Lloopcompare07:
	cmp r1, r2
	bne .Literate07
.Lyes07:
	movs r0, #1
.Ldone07:
	bx lr
	.fnend
	.size ScePafMisc_07A5D381, . - ScePafMisc_07A5D381

	.global ScePafMisc_14432321
	.type ScePafMisc_14432321, %function
	.thumb_func
ScePafMisc_14432321:
	.fnstart
	ldr r2, [r0]
	ldr r2, [r2, #0x10]
	cbnz r2, .Lhas144
	movs r0, #0
	b .Lendsearch144
.Lhas144:
	ldr r0, [r2]
	ldrsb.w r3, [r0, #0x15]
	cbnz r3, .Lendsearch144
.Ltest144:
	ldr r3, [r0, #0xc]
	cmp r1, r3
	bge .Lgreater144
	ldr r0, [r0, #4]
	ldrsb.w r3, [r0, #0x15]
	b .Lnext144
.Lgreater144:
	cmp r3, r1
	bge .Lendbranch144
	ldr r0, [r0, #8]
	ldrsb.w r3, [r0, #0x15]
	b .Lnext144
.Lendbranch144:
	b .Lendsearch144
.Lnext144:
	cmp r3, #0
	beq .Ltest144
.Lendsearch144:
	movs r1, #0
	cmp r0, r2
	beq .Lzero144
	ldr r0, [r0, #0x10]
	b .Lreturn144
.Lzero144:
	adds r0, r1, #0
.Lreturn144:
	bx lr
	.fnend
	.size ScePafMisc_14432321, . - ScePafMisc_14432321
