	.set push
	.set noreorder
	.set noat
	.section .text
	.globl me_boot_code
	.globl me_boot_code_end
	.type me_boot_code, @function
me_boot_code:
	lui	$v0, 0xbc10
	lw	$v0, 0($v0)
	beqz	$v0, 1f
	nop
	.word	0x40424800
	jr	$v0
	nop
	nop
1:
	addiu	$t1, $zero, 0
	lui	$a0, 0x0
	ori	$a0, $a0, 0x0
	lui	$a3, 0xbc10
	sw	$t1, 0x40($a3)
	addiu	$t1, $zero, 7
	sw	$t1, 0x50($a3)
	addiu	$t1, $zero, -1
	sw	$t1, 0x4($a3)
	sync
	mfc0	$k0, $16
	mtc0	$zero, $28
	mtc0	$zero, $29
	ext	$t0, $k0, 9, 3
	addiu	$t9, $zero, 0x800
	sllv	$k1, $t9, $t0
2:
	addi	$k1, $k1, -0x40
	bnez	$k1, 2b
	cache	0x1, 0($k1)
	ext	$t0, $k0, 6, 3
	sllv	$k1, $t9, $t0
3:
	addi	$k1, $k1, -0x40
	bnez	$k1, 3b
	cache	0x11, 0($k1)
	mtc0	$zero, $13
	lui	$k0, 0x2000
	mtc0	$k0, $12
	lui	$k0, 0xbcc0
	addiu	$t1, $zero, 1
	sw	$t1, 0x10($k0)
4:
	lw	$t0, 0x10($k0)
	andi	$t0, $t0, 0x1
	beq	$t0, $t1, 4b
	nop
	addiu	$t1, $zero, 1
	sw	$t1, 0x70($k0)
	addiu	$t1, $zero, 8
	sw	$t1, 0x30($k0)
	addiu	$t1, $zero, 2
	sw	$t1, 0x40($k0)
	la	$ra, decompressAndRunMeImage
	lui	$sp, 0x8840
	jr	$ra
	sync
me_boot_code_end:
	.size me_boot_code, me_boot_code_end - me_boot_code
	.set pop
