/*
 * Hand-written Allegrex MIPS Rijndael/AES block cipher routines
 * matching Sony's SceNetInet assembly implementation.
 */

	.set	noreorder
	.set	noat

	.text

/* u32 rijndaelSboxWord(u32 word) */
	.globl	rijndaelSboxWord
	.ent	rijndaelSboxWord
	.type	rijndaelSboxWord, @function
rijndaelSboxWord:
	lui	$v0, %hi(g_rijndael_tables + 2)
	ext	$t3, $a0, 24, 8
	ins	$v0, $t3, 2, 8
	lbu	$t3, %lo(g_rijndael_tables + 2)($v0)
	ext	$t2, $a0, 16, 8
	ins	$v0, $t2, 2, 8
	lbu	$t2, %lo(g_rijndael_tables + 2)($v0)
	ext	$t1, $a0, 8, 8
	ins	$v0, $t1, 2, 8
	lbu	$t1, %lo(g_rijndael_tables + 2)($v0)
	ins	$v0, $a0, 2, 8
	lbu	$v0, %lo(g_rijndael_tables + 2)($v0)
	ins	$t2, $t3, 8, 8
	ins	$t1, $t2, 8, 16
	jr	$ra
	ins	$v0, $t1, 8, 24
	.end	rijndaelSboxWord

/* u32 rijndaelGfMul(u32 lhs, u32 rhs) */
	.globl	rijndaelGfMul
	.ent	rijndaelGfMul
	.type	rijndaelGfMul, @function
rijndaelGfMul:
	lui	$v0, %hi(g_rijndael_tables + 2304)
	addu	$t0, $a0, $v0
	movn	$a0, $a1, $a0
	beqz	$a0, .Lgf_zero
	addu	$a1, $v0, $a1
	lbu	$t0, %lo(g_rijndael_tables + 2304)($t0)
	lbu	$t1, %lo(g_rijndael_tables + 2304)($a1)
	addu	$t0, $t0, $t1
	ext	$a0, $t0, 8, 1
	addu	$t0, $t0, $a0
	addiu	$v0, $v0, 256
	ins	$v0, $t0, 0, 8
.Lgf_zero:
	jr	$ra
	lbu	$v0, %lo(g_rijndael_tables + 2304)($v0)
	.end	rijndaelGfMul

/* u32 rijndaelInvMixColByte(u32 colWord, u32 coeffWord) */
	.globl	rijndaelInvMixColByte
	.ent	rijndaelInvMixColByte
	.type	rijndaelInvMixColByte, @function
rijndaelInvMixColByte:
	move	$at, $ra
	move	$a2, $a0
	move	$a3, $a1
	ext	$a0, $a2, 0, 8
	jal	rijndaelGfMul
	ext	$a1, $a3, 0, 8
	move	$v1, $v0
	ext	$a0, $a2, 8, 8
	jal	rijndaelGfMul
	ext	$a1, $a3, 8, 8
	xor	$v1, $v1, $v0
	ext	$a0, $a2, 16, 8
	jal	rijndaelGfMul
	ext	$a1, $a3, 16, 8
	xor	$v1, $v1, $v0
	ext	$a0, $a2, 24, 8
	jal	rijndaelGfMul
	ext	$a1, $a3, 24, 8
	jr	$at
	xor	$v0, $v1, $v0
	.end	rijndaelInvMixColByte

/* u32 rijndaelInvMixColWord(u32 colWord) */
	.globl	rijndaelInvMixColWord
	.ent	rijndaelInvMixColWord
	.type	rijndaelInvMixColWord, @function
rijndaelInvMixColWord:
	move	$t2, $ra
	move	$t3, $a0
	lui	$t5, 0x0e09
	ori	$t5, $t5, 0x0d0b
	ror	$a1, $t5, 0
	jal	rijndaelInvMixColByte
	move	$a0, $t3
	sll	$t4, $v0, 24
	ror	$a1, $t5, 8
	jal	rijndaelInvMixColByte
	move	$a0, $t3
	ins	$t4, $v0, 16, 8
	ror	$a1, $t5, 16
	jal	rijndaelInvMixColByte
	move	$a0, $t3
	ins	$t4, $v0, 8, 8
	ror	$a1, $t5, 24
	jal	rijndaelInvMixColByte
	move	$a0, $t3
	jr	$t2
	or	$v0, $v0, $t4
	.end	rijndaelInvMixColWord

/* int rijndaelEncryptBlock(const void *ctx, const void *src, void *dst) */
	.globl	rijndaelEncryptBlock
	.ent	rijndaelEncryptBlock
	.type	rijndaelEncryptBlock, @function
rijndaelEncryptBlock:
	addiu	$sp, $sp, -48
	sw	$a0, 0($sp)
	sw	$a1, 4($sp)
	sw	$a2, 8($sp)
	sw	$a3, 12($sp)
	sw	$s0, 16($sp)
	sw	$s1, 20($sp)
	sw	$s2, 24($sp)
	sw	$s3, 28($sp)
	sw	$s4, 32($sp)
	sw	$s5, 36($sp)
	sw	$s6, 40($sp)
	mtlo	$s7
	mthi	$ra
	lui	$t8, %hi(g_rijndael_tables)
	addiu	$t8, $t8, %lo(g_rijndael_tables)
	addiu	$t9, $a0, 0
	lw	$at, 968($a0)
	lw	$t1, 964($a0)
	move	$v1, $a2
	addiu	$at, $at, -2
	xori	$t0, $t1, 4
	beqz	$t0, .Lenc_nb4
	xori	$t0, $t1, 6
	beqz	$t0, .Lenc_nb6
	xori	$t0, $t1, 8
	bnezl	$t0, .Lret_err
	li	$v0, -1

.Lenc_nb8:
	lw	$t0, 0($a1)
	lw	$t1, 4($a1)
	lw	$t2, 8($a1)
	lw	$t3, 12($a1)
	lw	$t4, 16($a1)
	lw	$t5, 20($a1)
	lw	$t6, 24($a1)
	lw	$t7, 28($a1)
	lw	$s0, 0($t9)
	lw	$s1, 4($t9)
	lw	$s2, 8($t9)
	lw	$s3, 12($t9)
	lw	$s4, 16($t9)
	lw	$s5, 20($t9)
	lw	$s6, 24($t9)
	lw	$s7, 28($t9)
	xor	$s0, $s0, $t0
	xor	$s1, $s1, $t1
	xor	$s2, $s2, $t2
	xor	$s3, $s3, $t3
	xor	$s4, $s4, $t4
	xor	$s5, $s5, $t5
	xor	$s6, $s6, $t6
	xor	$s7, $s7, $t7
.Lenc_nb8_loop:
	addiu	$t9, $t9, 32
	ext	$a1, $s1, 8, 8
	ext	$a2, $s3, 16, 8
	ext	$a3, $s4, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s0, 2, 8
	xor	$t0, $a0, $a3
	ext	$a1, $s2, 8, 8
	ext	$a2, $s4, 16, 8
	ext	$a3, $s5, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s1, 2, 8
	xor	$t1, $a0, $a3
	ext	$a1, $s3, 8, 8
	ext	$a2, $s5, 16, 8
	ext	$a3, $s6, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s2, 2, 8
	xor	$t2, $a0, $a3
	ext	$a1, $s4, 8, 8
	ext	$a2, $s6, 16, 8
	ext	$a3, $s7, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s3, 2, 8
	xor	$t3, $a0, $a3
	ext	$a1, $s5, 8, 8
	ext	$a2, $s7, 16, 8
	ext	$a3, $s0, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s4, 2, 8
	xor	$t4, $a0, $a3
	ext	$a1, $s6, 8, 8
	ext	$a2, $s0, 16, 8
	ext	$a3, $s1, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s5, 2, 8
	xor	$t5, $a0, $a3
	ext	$a1, $s7, 8, 8
	ext	$a2, $s1, 16, 8
	ext	$a3, $s2, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s6, 2, 8
	xor	$t6, $a0, $a3
	ext	$a1, $s0, 8, 8
	ext	$a2, $s2, 16, 8
	ext	$a3, $s3, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s7, 2, 8
	xor	$t7, $a0, $a3
	lw	$s0, 0($t9)
	lw	$s1, 4($t9)
	lw	$s2, 8($t9)
	lw	$s3, 12($t9)
	lw	$s4, 16($t9)
	lw	$s5, 20($t9)
	lw	$s6, 24($t9)
	lw	$s7, 28($t9)
	xor	$s0, $s0, $t0
	xor	$s1, $s1, $t1
	xor	$s2, $s2, $t2
	xor	$s3, $s3, $t3
	xor	$s4, $s4, $t4
	xor	$s5, $s5, $t5
	xor	$s6, $s6, $t6
	xor	$s7, $s7, $t7
	bnez	$at, .Lenc_nb8_loop
	addiu	$at, $at, -1
	ins	$t8, $s1, 2, 8
	ext	$a1, $s2, 8, 8
	ext	$a2, $s4, 16, 8
	ext	$a3, $s5, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t1, 36($t9)
	xor	$t1, $t1, $a0
	ins	$t8, $s2, 2, 8
	ext	$a1, $s3, 8, 8
	ext	$a2, $s5, 16, 8
	ext	$a3, $s6, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t2, 40($t9)
	xor	$t2, $t2, $a0
	ins	$t8, $s3, 2, 8
	ext	$a1, $s4, 8, 8
	ext	$a2, $s6, 16, 8
	ext	$a3, $s7, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t3, 44($t9)
	xor	$t3, $t3, $a0
	ins	$t8, $s4, 2, 8
	ext	$a1, $s5, 8, 8
	ext	$a2, $s7, 16, 8
	ext	$a3, $s0, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t4, 48($t9)
	xor	$t4, $t4, $a0
	sw	$t4, 16($v1)
	ins	$t8, $s5, 2, 8
	ext	$a1, $s6, 8, 8
	ext	$a2, $s0, 16, 8
	ext	$a3, $s1, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t5, 52($t9)
	xor	$t5, $t5, $a0
	sw	$t5, 20($v1)
	ins	$t8, $s6, 2, 8
	ext	$a1, $s7, 8, 8
	ext	$a2, $s1, 16, 8
	ext	$a3, $s2, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t6, 56($t9)
	xor	$t6, $t6, $a0
	sw	$t6, 24($v1)
	ins	$t8, $s7, 2, 8
	ext	$a1, $s0, 8, 8
	ext	$a2, $s2, 16, 8
	ext	$a3, $s3, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t7, 60($t9)
	xor	$t7, $t7, $a0
	sw	$t7, 28($v1)
	ext	$a1, $s1, 8, 8
	ext	$a2, $s3, 16, 8
	ext	$a3, $s4, 24, 8
	j	.Lenc_done
	lw	$t0, 32($t9)

.Lenc_nb4:
	lw	$t0, 0($a1)
	lw	$t1, 4($a1)
	lw	$t2, 8($a1)
	lw	$t3, 12($a1)
	lw	$s0, 0($t9)
	lw	$s1, 4($t9)
	lw	$s2, 8($t9)
	lw	$s3, 12($t9)
	xor	$s0, $s0, $t0
	xor	$s1, $s1, $t1
	xor	$s2, $s2, $t2
	xor	$s3, $s3, $t3
.Lenc_nb4_loop:
	addiu	$t9, $t9, 16
	ext	$a1, $s1, 8, 8
	ext	$a2, $s2, 16, 8
	ext	$a3, $s3, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s0, 2, 8
	xor	$t0, $a0, $a3
	ext	$a1, $s2, 8, 8
	ext	$a2, $s3, 16, 8
	ext	$a3, $s0, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s1, 2, 8
	xor	$t1, $a0, $a3
	ext	$a1, $s3, 8, 8
	ext	$a2, $s0, 16, 8
	ext	$a3, $s1, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s2, 2, 8
	xor	$t2, $a0, $a3
	ext	$a1, $s0, 8, 8
	ext	$a2, $s1, 16, 8
	ext	$a3, $s2, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s3, 2, 8
	xor	$t3, $a0, $a3
	lw	$s0, 0($t9)
	lw	$s1, 4($t9)
	lw	$s2, 8($t9)
	lw	$s3, 12($t9)
	xor	$s0, $s0, $t0
	xor	$s1, $s1, $t1
	xor	$s2, $s2, $t2
	xor	$s3, $s3, $t3
	bnez	$at, .Lenc_nb4_loop
	addiu	$at, $at, -1
	ins	$t8, $s1, 2, 8
	ext	$a1, $s2, 8, 8
	ext	$a2, $s3, 16, 8
	ext	$a3, $s0, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t1, 20($t9)
	xor	$t1, $t1, $a0
	ins	$t8, $s2, 2, 8
	ext	$a1, $s3, 8, 8
	ext	$a2, $s0, 16, 8
	ext	$a3, $s1, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t2, 24($t9)
	xor	$t2, $t2, $a0
	ins	$t8, $s3, 2, 8
	ext	$a1, $s0, 8, 8
	ext	$a2, $s1, 16, 8
	ext	$a3, $s2, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t3, 28($t9)
	xor	$t3, $t3, $a0
	ext	$a1, $s1, 8, 8
	ext	$a2, $s2, 16, 8
	ext	$a3, $s3, 24, 8
	j	.Lenc_done
	lw	$t0, 16($t9)

.Lenc_nb6:
	lw	$t0, 0($a1)
	lw	$t1, 4($a1)
	lw	$t2, 8($a1)
	lw	$t3, 12($a1)
	lw	$t4, 16($a1)
	lw	$t5, 20($a1)
	lw	$s0, 0($t9)
	lw	$s1, 4($t9)
	lw	$s2, 8($t9)
	lw	$s3, 12($t9)
	lw	$s4, 16($t9)
	lw	$s5, 20($t9)
	xor	$s0, $s0, $t0
	xor	$s1, $s1, $t1
	xor	$s2, $s2, $t2
	xor	$s3, $s3, $t3
	xor	$s4, $s4, $t4
	xor	$s5, $s5, $t5
.Lenc_nb6_loop:
	addiu	$t9, $t9, 24
	ext	$a1, $s1, 8, 8
	ext	$a2, $s2, 16, 8
	ext	$a3, $s3, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s0, 2, 8
	xor	$t0, $a0, $a3
	ext	$a1, $s2, 8, 8
	ext	$a2, $s3, 16, 8
	ext	$a3, $s4, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s1, 2, 8
	xor	$t1, $a0, $a3
	ext	$a1, $s3, 8, 8
	ext	$a2, $s4, 16, 8
	ext	$a3, $s5, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s2, 2, 8
	xor	$t2, $a0, $a3
	ext	$a1, $s4, 8, 8
	ext	$a2, $s5, 16, 8
	ext	$a3, $s0, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s3, 2, 8
	xor	$t3, $a0, $a3
	ext	$a1, $s5, 8, 8
	ext	$a2, $s0, 16, 8
	ext	$a3, $s1, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s4, 2, 8
	xor	$t4, $a0, $a3
	ext	$a1, $s0, 8, 8
	ext	$a2, $s1, 16, 8
	ext	$a3, $s2, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s5, 2, 8
	xor	$t5, $a0, $a3
	lw	$s0, 0($t9)
	lw	$s1, 4($t9)
	lw	$s2, 8($t9)
	lw	$s3, 12($t9)
	lw	$s4, 16($t9)
	lw	$s5, 20($t9)
	xor	$s0, $s0, $t0
	xor	$s1, $s1, $t1
	xor	$s2, $s2, $t2
	xor	$s3, $s3, $t3
	xor	$s4, $s4, $t4
	xor	$s5, $s5, $t5
	bnez	$at, .Lenc_nb6_loop
	addiu	$at, $at, -1
	ins	$t8, $s1, 2, 8
	ext	$a1, $s2, 8, 8
	ext	$a2, $s3, 16, 8
	ext	$a3, $s4, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t1, 28($t9)
	xor	$t1, $t1, $a0
	ins	$t8, $s2, 2, 8
	ext	$a1, $s3, 8, 8
	ext	$a2, $s4, 16, 8
	ext	$a3, $s5, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t2, 32($t9)
	xor	$t2, $t2, $a0
	ins	$t8, $s3, 2, 8
	ext	$a1, $s4, 8, 8
	ext	$a2, $s5, 16, 8
	ext	$a3, $s0, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t3, 36($t9)
	xor	$t3, $t3, $a0
	ins	$t8, $s4, 2, 8
	ext	$a1, $s5, 8, 8
	ext	$a2, $s0, 16, 8
	ext	$a3, $s1, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t4, 40($t9)
	xor	$t4, $t4, $a0
	sw	$t4, 16($v1)
	ins	$t8, $s5, 2, 8
	ext	$a1, $s0, 8, 8
	ext	$a2, $s1, 16, 8
	ext	$a3, $s2, 24, 8
	jal	rijndaelFinalEncSub
	lw	$t5, 44($t9)
	xor	$t5, $t5, $a0
	sw	$t5, 20($v1)
	ext	$a1, $s1, 8, 8
	ext	$a2, $s2, 16, 8
	ext	$a3, $s3, 24, 8
	j	.Lenc_done
	lw	$t0, 24($t9)
	.end	rijndaelEncryptBlock

/* int rijndaelDecryptBlock(const void *ctx, const void *src, void *dst) */
	.globl	rijndaelDecryptBlock
	.ent	rijndaelDecryptBlock
	.type	rijndaelDecryptBlock, @function
rijndaelDecryptBlock:
	addiu	$sp, $sp, -48
	sw	$a0, 0($sp)
	sw	$a1, 4($sp)
	sw	$a2, 8($sp)
	sw	$a3, 12($sp)
	sw	$s0, 16($sp)
	sw	$s1, 20($sp)
	sw	$s2, 24($sp)
	sw	$s3, 28($sp)
	sw	$s4, 32($sp)
	sw	$s5, 36($sp)
	sw	$s6, 40($sp)
	mtlo	$s7
	mthi	$ra
	lui	$t8, %hi(g_rijndael_tables + 1024)
	addiu	$t8, $t8, %lo(g_rijndael_tables + 1024)
	addiu	$v0, $t8, 1024
	addiu	$t9, $a0, 480
	lw	$at, 968($a0)
	lw	$t1, 964($a0)
	move	$v1, $a2
	addiu	$at, $at, -2
	xori	$t0, $t1, 4
	beqz	$t0, .Ldec_nb4
	xori	$t0, $t1, 6
	beqz	$t0, .Ldec_nb6
	xori	$t0, $t1, 8
	bnezl	$t0, .Lret_err
	li	$v0, -1

.Ldec_nb8:
	lw	$t0, 0($a1)
	lw	$t1, 4($a1)
	lw	$t2, 8($a1)
	lw	$t3, 12($a1)
	lw	$t4, 16($a1)
	lw	$t5, 20($a1)
	lw	$t6, 24($a1)
	lw	$t7, 28($a1)
	lw	$s0, 0($t9)
	lw	$s1, 4($t9)
	lw	$s2, 8($t9)
	lw	$s3, 12($t9)
	lw	$s4, 16($t9)
	lw	$s5, 20($t9)
	lw	$s6, 24($t9)
	lw	$s7, 28($t9)
	xor	$s0, $s0, $t0
	xor	$s1, $s1, $t1
	xor	$s2, $s2, $t2
	xor	$s3, $s3, $t3
	xor	$s4, $s4, $t4
	xor	$s5, $s5, $t5
	xor	$s6, $s6, $t6
	xor	$s7, $s7, $t7
.Ldec_nb8_loop:
	addiu	$t9, $t9, 32
	ext	$a1, $s7, 8, 8
	ext	$a2, $s5, 16, 8
	ext	$a3, $s4, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s0, 2, 8
	xor	$t0, $a0, $a3
	ext	$a1, $s0, 8, 8
	ext	$a2, $s6, 16, 8
	ext	$a3, $s5, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s1, 2, 8
	xor	$t1, $a0, $a3
	ext	$a1, $s1, 8, 8
	ext	$a2, $s7, 16, 8
	ext	$a3, $s6, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s2, 2, 8
	xor	$t2, $a0, $a3
	ext	$a1, $s2, 8, 8
	ext	$a2, $s0, 16, 8
	ext	$a3, $s7, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s3, 2, 8
	xor	$t3, $a0, $a3
	ext	$a1, $s3, 8, 8
	ext	$a2, $s1, 16, 8
	ext	$a3, $s0, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s4, 2, 8
	xor	$t4, $a0, $a3
	ext	$a1, $s4, 8, 8
	ext	$a2, $s2, 16, 8
	ext	$a3, $s1, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s5, 2, 8
	xor	$t5, $a0, $a3
	ext	$a1, $s5, 8, 8
	ext	$a2, $s3, 16, 8
	ext	$a3, $s2, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s6, 2, 8
	xor	$t6, $a0, $a3
	ext	$a1, $s6, 8, 8
	ext	$a2, $s4, 16, 8
	ext	$a3, $s3, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s7, 2, 8
	xor	$t7, $a0, $a3
	lw	$s0, 0($t9)
	lw	$s1, 4($t9)
	lw	$s2, 8($t9)
	lw	$s3, 12($t9)
	lw	$s4, 16($t9)
	lw	$s5, 20($t9)
	lw	$s6, 24($t9)
	lw	$s7, 28($t9)
	xor	$s0, $s0, $t0
	xor	$s1, $s1, $t1
	xor	$s2, $s2, $t2
	xor	$s3, $s3, $t3
	xor	$s4, $s4, $t4
	xor	$s5, $s5, $t5
	xor	$s6, $s6, $t6
	xor	$s7, $s7, $t7
	bnez	$at, .Ldec_nb8_loop
	addiu	$at, $at, -1
	ins	$v0, $s1, 0, 8
	ext	$a1, $s0, 8, 8
	ext	$a2, $s6, 16, 8
	ext	$a3, $s5, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t1, 36($t9)
	xor	$t1, $t1, $a0
	ins	$v0, $s2, 0, 8
	ext	$a1, $s1, 8, 8
	ext	$a2, $s7, 16, 8
	ext	$a3, $s6, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t2, 40($t9)
	xor	$t2, $t2, $a0
	ins	$v0, $s3, 0, 8
	ext	$a1, $s2, 8, 8
	ext	$a2, $s0, 16, 8
	ext	$a3, $s7, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t3, 44($t9)
	xor	$t3, $t3, $a0
	ins	$v0, $s4, 0, 8
	ext	$a1, $s3, 8, 8
	ext	$a2, $s1, 16, 8
	ext	$a3, $s0, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t4, 48($t9)
	xor	$t4, $t4, $a0
	sw	$t4, 16($v1)
	ins	$v0, $s5, 0, 8
	ext	$a1, $s4, 8, 8
	ext	$a2, $s2, 16, 8
	ext	$a3, $s1, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t5, 52($t9)
	xor	$t5, $t5, $a0
	sw	$t5, 20($v1)
	ins	$v0, $s6, 0, 8
	ext	$a1, $s5, 8, 8
	ext	$a2, $s3, 16, 8
	ext	$a3, $s2, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t6, 56($t9)
	xor	$t6, $t6, $a0
	sw	$t6, 24($v1)
	ins	$v0, $s7, 0, 8
	ext	$a1, $s6, 8, 8
	ext	$a2, $s4, 16, 8
	ext	$a3, $s3, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t7, 60($t9)
	xor	$t7, $t7, $a0
	sw	$t7, 28($v1)
	ext	$a1, $s7, 8, 8
	ext	$a2, $s5, 16, 8
	ext	$a3, $s4, 24, 8
	j	.Ldec_done
	lw	$t0, 32($t9)

.Lenc_done:
	sw	$t1, 4($v1)
	jal	rijndaelFinalEncSub
	ins	$t8, $s0, 2, 8
	sw	$t2, 8($v1)
	xor	$t0, $t0, $a0
	move	$v0, $zero
	sw	$t3, 12($v1)
	mflo	$s7
	mfhi	$ra
	lw	$a0, 0($sp)
	lw	$a1, 4($sp)
	lw	$a2, 8($sp)
	lw	$a3, 12($sp)
	lw	$s0, 16($sp)
	lw	$s1, 20($sp)
	lw	$s2, 24($sp)
	lw	$s3, 28($sp)
	lw	$s4, 32($sp)
	lw	$s5, 36($sp)
	lw	$s6, 40($sp)
	sw	$t0, 0($v1)
	jr	$ra
	addiu	$sp, $sp, 48

.Ldec_done:
	sw	$t1, 4($v1)
	jal	rijndaelFinalDecSub
	ins	$v0, $s0, 0, 8
	sw	$t2, 8($v1)
	xor	$t0, $t0, $a0
	move	$v0, $zero
	sw	$t3, 12($v1)
	mflo	$s7
	mfhi	$ra
	lw	$a0, 0($sp)
	lw	$a1, 4($sp)
	lw	$a2, 8($sp)
	lw	$a3, 12($sp)
	lw	$s0, 16($sp)
	lw	$s1, 20($sp)
	lw	$s2, 24($sp)
	lw	$s3, 28($sp)
	lw	$s4, 32($sp)
	lw	$s5, 36($sp)
	lw	$s6, 40($sp)
	sw	$t0, 0($v1)
.Lret_err:
	jr	$ra
	addiu	$sp, $sp, 48

.Ldec_nb4:
	lw	$t0, 0($a1)
	lw	$t1, 4($a1)
	lw	$t2, 8($a1)
	lw	$t3, 12($a1)
	lw	$s0, 0($t9)
	lw	$s1, 4($t9)
	lw	$s2, 8($t9)
	lw	$s3, 12($t9)
	xor	$s0, $s0, $t0
	xor	$s1, $s1, $t1
	xor	$s2, $s2, $t2
	xor	$s3, $s3, $t3
.Ldec_nb4_loop:
	addiu	$t9, $t9, 16
	ext	$a1, $s3, 8, 8
	ext	$a2, $s2, 16, 8
	ext	$a3, $s1, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s0, 2, 8
	xor	$t0, $a0, $a3
	ext	$a1, $s0, 8, 8
	ext	$a2, $s3, 16, 8
	ext	$a3, $s2, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s1, 2, 8
	xor	$t1, $a0, $a3
	ext	$a1, $s1, 8, 8
	ext	$a2, $s0, 16, 8
	ext	$a3, $s3, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s2, 2, 8
	xor	$t2, $a0, $a3
	ext	$a1, $s2, 8, 8
	ext	$a2, $s1, 16, 8
	ext	$a3, $s0, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s3, 2, 8
	xor	$t3, $a0, $a3
	lw	$s0, 0($t9)
	lw	$s1, 4($t9)
	lw	$s2, 8($t9)
	lw	$s3, 12($t9)
	xor	$s0, $s0, $t0
	xor	$s1, $s1, $t1
	xor	$s2, $s2, $t2
	xor	$s3, $s3, $t3
	bnez	$at, .Ldec_nb4_loop
	addiu	$at, $at, -1
	ins	$v0, $s1, 0, 8
	ext	$a1, $s0, 8, 8
	ext	$a2, $s3, 16, 8
	ext	$a3, $s2, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t1, 20($t9)
	xor	$t1, $t1, $a0
	ins	$v0, $s2, 0, 8
	ext	$a1, $s1, 8, 8
	ext	$a2, $s0, 16, 8
	ext	$a3, $s3, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t2, 24($t9)
	xor	$t2, $t2, $a0
	ins	$v0, $s3, 0, 8
	ext	$a1, $s2, 8, 8
	ext	$a2, $s1, 16, 8
	ext	$a3, $s0, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t3, 28($t9)
	xor	$t3, $t3, $a0
	ext	$a1, $s3, 8, 8
	ext	$a2, $s2, 16, 8
	ext	$a3, $s1, 24, 8
	j	.Ldec_done
	lw	$t0, 16($t9)

.Ldec_nb6:
	lw	$t0, 0($a1)
	lw	$t1, 4($a1)
	lw	$t2, 8($a1)
	lw	$t3, 12($a1)
	lw	$t4, 16($a1)
	lw	$t5, 20($a1)
	lw	$s0, 0($t9)
	lw	$s1, 4($t9)
	lw	$s2, 8($t9)
	lw	$s3, 12($t9)
	lw	$s4, 16($t9)
	lw	$s5, 20($t9)
	xor	$s0, $s0, $t0
	xor	$s1, $s1, $t1
	xor	$s2, $s2, $t2
	xor	$s3, $s3, $t3
	xor	$s4, $s4, $t4
	xor	$s5, $s5, $t5
.Ldec_nb6_loop:
	addiu	$t9, $t9, 24
	ext	$a1, $s5, 8, 8
	ext	$a2, $s4, 16, 8
	ext	$a3, $s3, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s0, 2, 8
	xor	$t0, $a0, $a3
	ext	$a1, $s0, 8, 8
	ext	$a2, $s5, 16, 8
	ext	$a3, $s4, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s1, 2, 8
	xor	$t1, $a0, $a3
	ext	$a1, $s1, 8, 8
	ext	$a2, $s0, 16, 8
	ext	$a3, $s5, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s2, 2, 8
	xor	$t2, $a0, $a3
	ext	$a1, $s2, 8, 8
	ext	$a2, $s1, 16, 8
	ext	$a3, $s0, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s3, 2, 8
	xor	$t3, $a0, $a3
	ext	$a1, $s3, 8, 8
	ext	$a2, $s2, 16, 8
	ext	$a3, $s1, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s4, 2, 8
	xor	$t4, $a0, $a3
	ext	$a1, $s4, 8, 8
	ext	$a2, $s3, 16, 8
	ext	$a3, $s2, 24, 8
	jal	rijndaelRoundSub
	ins	$t8, $s5, 2, 8
	xor	$t5, $a0, $a3
	lw	$s0, 0($t9)
	lw	$s1, 4($t9)
	lw	$s2, 8($t9)
	lw	$s3, 12($t9)
	lw	$s4, 16($t9)
	lw	$s5, 20($t9)
	xor	$s0, $s0, $t0
	xor	$s1, $s1, $t1
	xor	$s2, $s2, $t2
	xor	$s3, $s3, $t3
	xor	$s4, $s4, $t4
	xor	$s5, $s5, $t5
	xor	$s5, $s5, $t5
	bnez	$at, .Ldec_nb6_loop
	addiu	$at, $at, -1
	ins	$v0, $s1, 0, 8
	ext	$a1, $s0, 8, 8
	ext	$a2, $s5, 16, 8
	ext	$a3, $s4, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t1, 28($t9)
	xor	$t1, $t1, $a0
	ins	$v0, $s2, 0, 8
	ext	$a1, $s1, 8, 8
	ext	$a2, $s0, 16, 8
	ext	$a3, $s5, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t2, 32($t9)
	xor	$t2, $t2, $a0
	ins	$v0, $s3, 0, 8
	ext	$a1, $s2, 8, 8
	ext	$a2, $s1, 16, 8
	ext	$a3, $s0, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t3, 36($t9)
	xor	$t3, $t3, $a0
	ins	$v0, $s4, 0, 8
	ext	$a1, $s3, 8, 8
	ext	$a2, $s2, 16, 8
	ext	$a3, $s1, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t4, 40($t9)
	xor	$t4, $t4, $a0
	sw	$t4, 16($v1)
	ins	$v0, $s5, 0, 8
	ext	$a1, $s4, 8, 8
	ext	$a2, $s3, 16, 8
	ext	$a3, $s2, 24, 8
	jal	rijndaelFinalDecSub
	lw	$t5, 44($t9)
	xor	$t5, $t5, $a0
	sw	$t5, 20($v1)
	ext	$a1, $s5, 8, 8
	ext	$a2, $s4, 16, 8
	ext	$a3, $s3, 24, 8
	j	.Ldec_done
	lw	$t0, 24($t9)
	.end	rijndaelDecryptBlock

/* Internal T-table round lookup helper */
	.globl	rijndaelRoundSub
	.ent	rijndaelRoundSub
	.type	rijndaelRoundSub, @function
rijndaelRoundSub:
	lw	$a0, 0($t8)
	ins	$t8, $a1, 2, 8
	lw	$a1, 0($t8)
	ins	$t8, $a2, 2, 8
	lw	$a2, 0($t8)
	ins	$t8, $a3, 2, 8
	lw	$a3, 0($t8)
	ror	$a1, $a1, 24
	xor	$a0, $a0, $a1
	ror	$a2, $a2, 16
	xor	$a0, $a0, $a2
	jr	$ra
	ror	$a3, $a3, 8
	.end	rijndaelRoundSub

/* Internal final-round forward S-box lookup helper */
	.globl	rijndaelFinalEncSub
	.ent	rijndaelFinalEncSub
	.type	rijndaelFinalEncSub, @function
rijndaelFinalEncSub:
	lbu	$a0, 2($t8)
	ins	$t8, $a1, 2, 8
	lbu	$a1, 2($t8)
	ins	$t8, $a2, 2, 8
	lbu	$a2, 2($t8)
	ins	$t8, $a3, 2, 8
	lbu	$a3, 2($t8)
	ins	$a0, $a1, 8, 8
	ins	$a0, $a2, 16, 8
	jr	$ra
	ins	$a0, $a3, 24, 8
	.end	rijndaelFinalEncSub

/* Internal final-round inverse S-box lookup helper */
	.globl	rijndaelFinalDecSub
	.ent	rijndaelFinalDecSub
	.type	rijndaelFinalDecSub, @function
rijndaelFinalDecSub:
	lbu	$a0, 0($v0)
	ins	$v0, $a1, 0, 8
	lbu	$a1, 0($v0)
	ins	$v0, $a2, 0, 8
	lbu	$a2, 0($v0)
	ins	$v0, $a3, 0, 8
	lbu	$a3, 0($v0)
	ins	$a0, $a1, 8, 8
	ins	$a0, $a2, 16, 8
	jr	$ra
	ins	$a0, $a3, 24, 8
	.end	rijndaelFinalDecSub
