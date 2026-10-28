/*
 * start.S: Stage 1 reset vector, secondary CPU (ME) dispatch, and initial
 * I-cache / D-cache tag initialization at 0xBFC00000..0xBFC0017F for the
 * ePSP (Allegrex) BootROM payload in compat_sm.self.
 *
 * At cold reset on CPU 0 (SC), Stage 1 initializes the caches, copies the
 * 3,636-byte Stage 2 image from 0xBFC00180 to 0xA0010000 (uncached alias of
 * 0x80010000), patches 0xBFC00000..0xBFC00010 with a 5-instruction trampoline
 * that sets $sp = 0x80013FF0 and jumps to bootRomMain (0x80010000), triggers
 * an SC reset via HW(0xBC10004C) |= 2, and halts.
 */

	.set	noreorder
	.set	noat

	.section .text.reset, "ax", @progbits

	.globl	_start
	.type	_start, @function
_start:
	ctc0	$v0, $4
	lui	$v0, 0xbc10
	lw	$v0, 0($v0)
	bnez	$v0, .Lsecondary_cpu
	nop
	ctc0	$zero, $9
	jal	bootRomInitDcache
	nop
	jal	bootRomInitIcache
	nop

	/* Copy Stage 2 (3,636 bytes) from 0xBFC00180 to 0xA0010000 */
	lui	$a0, 0xa001
	lui	$a1, 0xbfc0
	addiu	$a1, $a1, 0x180
	li	$a2, 0x0e34
.Lcopy_stage2:
	lw	$t0, 0($a1)
	addiu	$a1, $a1, 4
	sw	$t0, 0($a0)
	addiu	$a2, $a2, -4
	bgtz	$a2, .Lcopy_stage2
	addiu	$a0, $a0, 4

	/* Overwrite 0xBFC00000..0xBFC00010 with trampoline to 0x80010000 ($sp = 0x80013FF0) */
	lui	$a0, 0xbfc0
	lui	$t0, 0x3c1d		/* lui   $sp, 0x8001 */
	ori	$t0, $t0, 0x8001
	sw	$t0, 0($a0)
	lui	$t0, 0x37bd		/* ori   $sp, $sp, 0x3ff0 */
	ori	$t0, $t0, 0x3ff0
	sw	$t0, 4($a0)
	lui	$t0, 0x3c19		/* lui   $t9, 0x8001 */
	ori	$t0, $t0, 0x8001
	sw	$t0, 8($a0)
	lui	$t0, 0x0320		/* jr    $t9 */
	ori	$t0, $t0, 0x0008
	sw	$t0, 12($a0)
	li	$t0, 0x0000000f		/* sync */
	sw	$t0, 16($a0)

	/* Trigger SC reset via HW(0xBC10004C) |= 2 and halt */
	lui	$a0, 0xbc10
	lw	$t0, 0x4c($a0)
	ori	$t0, $t0, 0x0002
	sw	$t0, 0x4c($a0)
	.word	0x70000000		/* halt */

	/* 0xBFC000A0: Secondary CPU (ME) / warm reset entry */
.Lsecondary_cpu:
	cfc0	$v0, $9
	beqz	$v0, .Lclear_bev_jump
	nop
	jr	$v0
	nop

.Lclear_bev_jump:
	cfc0	$v0, $4
	mfc0	$k1, $12
	lui	$k0, 0xffbf
	ori	$k0, $k0, 0xffff
	and	$k1, $k1, $k0
	mtc0	$k1, $12
	mfc0	$k0, $25
	jr	$k0
	nop

	/* 0xBFC000E0: Exception / NMI vector */
	.org	0xe0
	b	_start
	nop

	/* 0xBFC000E8: Stage 1 I-cache tag clear and invalidate */
	.type	bootRomInitIcache, @function
bootRomInitIcache:
	mfc0	$t0, $16
	li	$t1, 4096
	ext	$t0, $t0, 9, 3
	sllv	$t1, $t1, $t0
	mtc0	$zero, $28
	mtc0	$zero, $29
	addu	$t0, $zero, $zero
.Licache_loop:
	cache	0x01, 0($t0)
	cache	0x03, 0($t0)
	addiu	$t0, $t0, 64
	bne	$t0, $t1, .Licache_loop
	nop
	jr	$ra
	nop

	/* 0xBFC00120: Stage 1 D-cache tag clear */
	.type	bootRomInitDcache, @function
bootRomInitDcache:
	mfc0	$t0, $16
	li	$t1, 4096
	ext	$t0, $t0, 6, 3
	sllv	$t1, $t1, $t0
	mtc0	$zero, $28
	mtc0	$zero, $29
	addu	$t0, $zero, $zero
.Ldcache_loop:
	cache	0x11, 0($t0)
	cache	0x13, 0($t0)
	addiu	$t0, $t0, 64
	bne	$t0, $t1, .Ldcache_loop
	nop
	jr	$ra
	nop
