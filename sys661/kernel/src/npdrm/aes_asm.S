/* Sony handwritten Allegrex Rijndael/AES routines from npdrm.prx (0x1f70..0x2d18). */
.set noreorder
.set noat
.text

.global npdrmAesSubWord
.ent npdrmAesSubWord
npdrmAesSubWord:
    lui $v0, %hi(g_npdrmAesTables + 2)
    ext $t3, $a0, 24, 8
    ins $v0, $t3, 2, 8
    lbu $t3, %lo(g_npdrmAesTables + 2)($v0)
    ext $t2, $a0, 16, 8
    ins $v0, $t2, 2, 8
    lbu $t2, %lo(g_npdrmAesTables + 2)($v0)
    ext $t1, $a0, 8, 8
    ins $v0, $t1, 2, 8
    lbu $t1, %lo(g_npdrmAesTables + 2)($v0)
    ins $v0, $a0, 2, 8
    lbu $v0, %lo(g_npdrmAesTables + 2)($v0)
    ins $t2, $t3, 8, 8
    ins $t1, $t2, 8, 16
    jr $ra
    ins $v0, $t1, 8, 24
.end npdrmAesSubWord

.global npdrmAesGfMul
.ent npdrmAesGfMul
npdrmAesGfMul:
    lui $v0, %hi(g_npdrmAesGfTables)
    addu $t0, $a0, $v0
    movn $a0, $a1, $a0
    beq $a0, $zero, .L_aes_1
    addu $a1, $v0, $a1
    lbu $t0, %lo(g_npdrmAesGfTables)($t0)
    lbu $t1, %lo(g_npdrmAesGfTables)($a1)
    addu $t0, $t0, $t1
    ext $a0, $t0, 8, 1
    addu $t0, $t0, $a0
    addiu $v0, $v0, 256
    ins $v0, $t0, 0, 8
.L_aes_1:
    jr $ra
    lbu $v0, %lo(g_npdrmAesGfTables)($v0)
.end npdrmAesGfMul

.global npdrmAesInvMixColByte
.ent npdrmAesInvMixColByte
npdrmAesInvMixColByte:
    addu $at, $ra, $zero
    addu $a2, $a0, $zero
    addu $a3, $a1, $zero
    ext $a0, $a2, 0, 8
    jal npdrmAesGfMul
    ext $a1, $a3, 0, 8
    addu $v1, $v0, $zero
    ext $a0, $a2, 8, 8
    jal npdrmAesGfMul
    ext $a1, $a3, 8, 8
    xor $v1, $v1, $v0
    ext $a0, $a2, 16, 8
    jal npdrmAesGfMul
    ext $a1, $a3, 16, 8
    xor $v1, $v1, $v0
    ext $a0, $a2, 24, 8
    jal npdrmAesGfMul
    ext $a1, $a3, 24, 8
    jr $at
    xor $v0, $v1, $v0
.end npdrmAesInvMixColByte

.global npdrmAesInvMixColWord
.ent npdrmAesInvMixColWord
npdrmAesInvMixColWord:
    addu $t2, $ra, $zero
    addu $t3, $a0, $zero
    lui $t5, 0xe09
    ori $t5, $t5, 0xd0b
    rotr $a1, $t5, 0
    jal npdrmAesInvMixColByte
    addu $a0, $t3, $zero
    sll $t4, $v0, 24
    rotr $a1, $t5, 8
    jal npdrmAesInvMixColByte
    addu $a0, $t3, $zero
    ins $t4, $v0, 16, 8
    rotr $a1, $t5, 16
    jal npdrmAesInvMixColByte
    addu $a0, $t3, $zero
    ins $t4, $v0, 8, 8
    rotr $a1, $t5, 24
    jal npdrmAesInvMixColByte
    addu $a0, $t3, $zero
    jr $t2
    or $v0, $v0, $t4
.end npdrmAesInvMixColWord

.global npdrmAesEncryptBlock
.ent npdrmAesEncryptBlock
npdrmAesEncryptBlock:
    addiu $sp, $sp, -48
    sw $a0, 0($sp)
    sw $a1, 4($sp)
    sw $a2, 8($sp)
    sw $a3, 12($sp)
    sw $s0, 16($sp)
    sw $s1, 20($sp)
    sw $s2, 24($sp)
    sw $s3, 28($sp)
    sw $s4, 32($sp)
    sw $s5, 36($sp)
    sw $s6, 40($sp)
    mtlo $s7
    mthi $ra
    lui $t8, %hi(g_npdrmAesTables)
    addiu $t9, $a0, 0
    lw $at, 968($a0)
    lw $t1, 964($a0)
    addu $v1, $a2, $zero
    addiu $at, $at, -2
    xori $t0, $t1, 0x4
    beq $t0, $zero, .L_aes_3
    xori $t0, $t1, 0x6
    beq $t0, $zero, .L_aes_5
    xori $t0, $t1, 0x8
    bnel $t0, $zero, .L_aes_10
    addiu $v0, $zero, -1
    lw $t0, 0($a1)
    lw $t1, 4($a1)
    lw $t2, 8($a1)
    lw $t3, 12($a1)
    lw $t4, 16($a1)
    lw $t5, 20($a1)
    lw $t6, 24($a1)
    lw $t7, 28($a1)
    lw $s0, 0($t9)
    lw $s1, 4($t9)
    lw $s2, 8($t9)
    lw $s3, 12($t9)
    lw $s4, 16($t9)
    lw $s5, 20($t9)
    lw $s6, 24($t9)
    lw $s7, 28($t9)
    xor $s0, $s0, $t0
    xor $s1, $s1, $t1
    xor $s2, $s2, $t2
    xor $s3, $s3, $t3
    xor $s4, $s4, $t4
    xor $s5, $s5, $t5
    xor $s6, $s6, $t6
    xor $s7, $s7, $t7
.L_aes_2:
    addiu $t9, $t9, 32
    ext $a1, $s1, 8, 8
    ext $a2, $s3, 16, 8
    ext $a3, $s4, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s0, 2, 8
    xor $t0, $a0, $a3
    ext $a1, $s2, 8, 8
    ext $a2, $s4, 16, 8
    ext $a3, $s5, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s1, 2, 8
    xor $t1, $a0, $a3
    ext $a1, $s3, 8, 8
    ext $a2, $s5, 16, 8
    ext $a3, $s6, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s2, 2, 8
    xor $t2, $a0, $a3
    ext $a1, $s4, 8, 8
    ext $a2, $s6, 16, 8
    ext $a3, $s7, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s3, 2, 8
    xor $t3, $a0, $a3
    ext $a1, $s5, 8, 8
    ext $a2, $s7, 16, 8
    ext $a3, $s0, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s4, 2, 8
    xor $t4, $a0, $a3
    ext $a1, $s6, 8, 8
    ext $a2, $s0, 16, 8
    ext $a3, $s1, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s5, 2, 8
    xor $t5, $a0, $a3
    ext $a1, $s7, 8, 8
    ext $a2, $s1, 16, 8
    ext $a3, $s2, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s6, 2, 8
    xor $t6, $a0, $a3
    ext $a1, $s0, 8, 8
    ext $a2, $s2, 16, 8
    ext $a3, $s3, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s7, 2, 8
    xor $t7, $a0, $a3
    lw $s0, 0($t9)
    lw $s1, 4($t9)
    lw $s2, 8($t9)
    lw $s3, 12($t9)
    lw $s4, 16($t9)
    lw $s5, 20($t9)
    lw $s6, 24($t9)
    lw $s7, 28($t9)
    xor $s0, $s0, $t0
    xor $s1, $s1, $t1
    xor $s2, $s2, $t2
    xor $s3, $s3, $t3
    xor $s4, $s4, $t4
    xor $s5, $s5, $t5
    xor $s6, $s6, $t6
    xor $s7, $s7, $t7
    bne $at, $zero, .L_aes_2
    addiu $at, $at, -1
    ins $t8, $s1, 2, 8
    ext $a1, $s2, 8, 8
    ext $a2, $s4, 16, 8
    ext $a3, $s5, 24, 8
    jal npdrmAesEncLastCol
    lw $t1, 36($t9)
    xor $t1, $t1, $a0
    ins $t8, $s2, 2, 8
    ext $a1, $s3, 8, 8
    ext $a2, $s5, 16, 8
    ext $a3, $s6, 24, 8
    jal npdrmAesEncLastCol
    lw $t2, 40($t9)
    xor $t2, $t2, $a0
    ins $t8, $s3, 2, 8
    ext $a1, $s4, 8, 8
    ext $a2, $s6, 16, 8
    ext $a3, $s7, 24, 8
    jal npdrmAesEncLastCol
    lw $t3, 44($t9)
    xor $t3, $t3, $a0
    ins $t8, $s4, 2, 8
    ext $a1, $s5, 8, 8
    ext $a2, $s7, 16, 8
    ext $a3, $s0, 24, 8
    jal npdrmAesEncLastCol
    lw $t4, 48($t9)
    xor $t4, $t4, $a0
    sw $t4, 16($v1)
    ins $t8, $s5, 2, 8
    ext $a1, $s6, 8, 8
    ext $a2, $s0, 16, 8
    ext $a3, $s1, 24, 8
    jal npdrmAesEncLastCol
    lw $t5, 52($t9)
    xor $t5, $t5, $a0
    sw $t5, 20($v1)
    ins $t8, $s6, 2, 8
    ext $a1, $s7, 8, 8
    ext $a2, $s1, 16, 8
    ext $a3, $s2, 24, 8
    jal npdrmAesEncLastCol
    lw $t6, 56($t9)
    xor $t6, $t6, $a0
    sw $t6, 24($v1)
    ins $t8, $s7, 2, 8
    ext $a1, $s0, 8, 8
    ext $a2, $s2, 16, 8
    ext $a3, $s3, 24, 8
    jal npdrmAesEncLastCol
    lw $t7, 60($t9)
    xor $t7, $t7, $a0
    sw $t7, 28($v1)
    ext $a1, $s1, 8, 8
    ext $a2, $s3, 16, 8
    ext $a3, $s4, 24, 8
    j .L_aes_8
    lw $t0, 32($t9)
.L_aes_3:
    lw $t0, 0($a1)
    lw $t1, 4($a1)
    lw $t2, 8($a1)
    lw $t3, 12($a1)
    lw $s0, 0($t9)
    lw $s1, 4($t9)
    lw $s2, 8($t9)
    lw $s3, 12($t9)
    xor $s0, $s0, $t0
    xor $s1, $s1, $t1
    xor $s2, $s2, $t2
    xor $s3, $s3, $t3
.L_aes_4:
    addiu $t9, $t9, 16
    ext $a1, $s1, 8, 8
    ext $a2, $s2, 16, 8
    ext $a3, $s3, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s0, 2, 8
    xor $t0, $a0, $a3
    ext $a1, $s2, 8, 8
    ext $a2, $s3, 16, 8
    ext $a3, $s0, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s1, 2, 8
    xor $t1, $a0, $a3
    ext $a1, $s3, 8, 8
    ext $a2, $s0, 16, 8
    ext $a3, $s1, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s2, 2, 8
    xor $t2, $a0, $a3
    ext $a1, $s0, 8, 8
    ext $a2, $s1, 16, 8
    ext $a3, $s2, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s3, 2, 8
    xor $t3, $a0, $a3
    lw $s0, 0($t9)
    lw $s1, 4($t9)
    lw $s2, 8($t9)
    lw $s3, 12($t9)
    xor $s0, $s0, $t0
    xor $s1, $s1, $t1
    xor $s2, $s2, $t2
    xor $s3, $s3, $t3
    bne $at, $zero, .L_aes_4
    addiu $at, $at, -1
    ins $t8, $s1, 2, 8
    ext $a1, $s2, 8, 8
    ext $a2, $s3, 16, 8
    ext $a3, $s0, 24, 8
    jal npdrmAesEncLastCol
    lw $t1, 20($t9)
    xor $t1, $t1, $a0
    ins $t8, $s2, 2, 8
    ext $a1, $s3, 8, 8
    ext $a2, $s0, 16, 8
    ext $a3, $s1, 24, 8
    jal npdrmAesEncLastCol
    lw $t2, 24($t9)
    xor $t2, $t2, $a0
    ins $t8, $s3, 2, 8
    ext $a1, $s0, 8, 8
    ext $a2, $s1, 16, 8
    ext $a3, $s2, 24, 8
    jal npdrmAesEncLastCol
    lw $t3, 28($t9)
    xor $t3, $t3, $a0
    ext $a1, $s1, 8, 8
    ext $a2, $s2, 16, 8
    ext $a3, $s3, 24, 8
    j .L_aes_8
    lw $t0, 16($t9)
.L_aes_5:
    lw $t0, 0($a1)
    lw $t1, 4($a1)
    lw $t2, 8($a1)
    lw $t3, 12($a1)
    lw $t4, 16($a1)
    lw $t5, 20($a1)
    lw $s0, 0($t9)
    lw $s1, 4($t9)
    lw $s2, 8($t9)
    lw $s3, 12($t9)
    lw $s4, 16($t9)
    lw $s5, 20($t9)
    xor $s0, $s0, $t0
    xor $s1, $s1, $t1
    xor $s2, $s2, $t2
    xor $s3, $s3, $t3
    xor $s4, $s4, $t4
    xor $s5, $s5, $t5
.L_aes_6:
    addiu $t9, $t9, 24
    ext $a1, $s1, 8, 8
    ext $a2, $s2, 16, 8
    ext $a3, $s3, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s0, 2, 8
    xor $t0, $a0, $a3
    ext $a1, $s2, 8, 8
    ext $a2, $s3, 16, 8
    ext $a3, $s4, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s1, 2, 8
    xor $t1, $a0, $a3
    ext $a1, $s3, 8, 8
    ext $a2, $s4, 16, 8
    ext $a3, $s5, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s2, 2, 8
    xor $t2, $a0, $a3
    ext $a1, $s4, 8, 8
    ext $a2, $s5, 16, 8
    ext $a3, $s0, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s3, 2, 8
    xor $t3, $a0, $a3
    ext $a1, $s5, 8, 8
    ext $a2, $s0, 16, 8
    ext $a3, $s1, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s4, 2, 8
    xor $t4, $a0, $a3
    ext $a1, $s0, 8, 8
    ext $a2, $s1, 16, 8
    ext $a3, $s2, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s5, 2, 8
    xor $t5, $a0, $a3
    lw $s0, 0($t9)
    lw $s1, 4($t9)
    lw $s2, 8($t9)
    lw $s3, 12($t9)
    lw $s4, 16($t9)
    lw $s5, 20($t9)
    xor $s0, $s0, $t0
    xor $s1, $s1, $t1
    xor $s2, $s2, $t2
    xor $s3, $s3, $t3
    xor $s4, $s4, $t4
    xor $s5, $s5, $t5
    bne $at, $zero, .L_aes_6
    addiu $at, $at, -1
    ins $t8, $s1, 2, 8
    ext $a1, $s2, 8, 8
    ext $a2, $s3, 16, 8
    ext $a3, $s4, 24, 8
    jal npdrmAesEncLastCol
    lw $t1, 28($t9)
    xor $t1, $t1, $a0
    ins $t8, $s2, 2, 8
    ext $a1, $s3, 8, 8
    ext $a2, $s4, 16, 8
    ext $a3, $s5, 24, 8
    jal npdrmAesEncLastCol
    lw $t2, 32($t9)
    xor $t2, $t2, $a0
    ins $t8, $s3, 2, 8
    ext $a1, $s4, 8, 8
    ext $a2, $s5, 16, 8
    ext $a3, $s0, 24, 8
    jal npdrmAesEncLastCol
    lw $t3, 36($t9)
    xor $t3, $t3, $a0
    ins $t8, $s4, 2, 8
    ext $a1, $s5, 8, 8
    ext $a2, $s0, 16, 8
    ext $a3, $s1, 24, 8
    jal npdrmAesEncLastCol
    lw $t4, 40($t9)
    xor $t4, $t4, $a0
    sw $t4, 16($v1)
    ins $t8, $s5, 2, 8
    ext $a1, $s0, 8, 8
    ext $a2, $s1, 16, 8
    ext $a3, $s2, 24, 8
    jal npdrmAesEncLastCol
    lw $t5, 44($t9)
    xor $t5, $t5, $a0
    sw $t5, 20($v1)
    ext $a1, $s1, 8, 8
    ext $a2, $s2, 16, 8
    ext $a3, $s3, 24, 8
    j .L_aes_8
    lw $t0, 24($t9)
.end npdrmAesEncryptBlock

.global npdrmAesDecryptBlock
.ent npdrmAesDecryptBlock
npdrmAesDecryptBlock:
    addiu $sp, $sp, -48
    sw $a0, 0($sp)
    sw $a1, 4($sp)
    sw $a2, 8($sp)
    sw $a3, 12($sp)
    sw $s0, 16($sp)
    sw $s1, 20($sp)
    sw $s2, 24($sp)
    sw $s3, 28($sp)
    sw $s4, 32($sp)
    sw $s5, 36($sp)
    sw $s6, 40($sp)
    mtlo $s7
    mthi $ra
    lui $t8, %hi(g_npdrmAesTables)
    addiu $t8, $t8, 1024
    addiu $v0, $t8, 1024
    addiu $t9, $a0, 480
    lw $at, 968($a0)
    lw $t1, 964($a0)
    addu $v1, $a2, $zero
    addiu $at, $at, -2
    xori $t0, $t1, 0x4
    beq $t0, $zero, .L_aes_11
    xori $t0, $t1, 0x6
    beq $t0, $zero, .L_aes_13
    xori $t0, $t1, 0x8
    bnel $t0, $zero, .L_aes_10
    addiu $v0, $zero, -1
    lw $t0, 0($a1)
    lw $t1, 4($a1)
    lw $t2, 8($a1)
    lw $t3, 12($a1)
    lw $t4, 16($a1)
    lw $t5, 20($a1)
    lw $t6, 24($a1)
    lw $t7, 28($a1)
    lw $s0, 0($t9)
    lw $s1, 4($t9)
    lw $s2, 8($t9)
    lw $s3, 12($t9)
    lw $s4, 16($t9)
    lw $s5, 20($t9)
    lw $s6, 24($t9)
    lw $s7, 28($t9)
    xor $s0, $s0, $t0
    xor $s1, $s1, $t1
    xor $s2, $s2, $t2
    xor $s3, $s3, $t3
    xor $s4, $s4, $t4
    xor $s5, $s5, $t5
    xor $s6, $s6, $t6
    xor $s7, $s7, $t7
.L_aes_7:
    addiu $t9, $t9, 32
    ext $a1, $s7, 8, 8
    ext $a2, $s5, 16, 8
    ext $a3, $s4, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s0, 2, 8
    xor $t0, $a0, $a3
    ext $a1, $s0, 8, 8
    ext $a2, $s6, 16, 8
    ext $a3, $s5, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s1, 2, 8
    xor $t1, $a0, $a3
    ext $a1, $s1, 8, 8
    ext $a2, $s7, 16, 8
    ext $a3, $s6, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s2, 2, 8
    xor $t2, $a0, $a3
    ext $a1, $s2, 8, 8
    ext $a2, $s0, 16, 8
    ext $a3, $s7, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s3, 2, 8
    xor $t3, $a0, $a3
    ext $a1, $s3, 8, 8
    ext $a2, $s1, 16, 8
    ext $a3, $s0, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s4, 2, 8
    xor $t4, $a0, $a3
    ext $a1, $s4, 8, 8
    ext $a2, $s2, 16, 8
    ext $a3, $s1, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s5, 2, 8
    xor $t5, $a0, $a3
    ext $a1, $s5, 8, 8
    ext $a2, $s3, 16, 8
    ext $a3, $s2, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s6, 2, 8
    xor $t6, $a0, $a3
    ext $a1, $s6, 8, 8
    ext $a2, $s4, 16, 8
    ext $a3, $s3, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s7, 2, 8
    xor $t7, $a0, $a3
    lw $s0, 0($t9)
    lw $s1, 4($t9)
    lw $s2, 8($t9)
    lw $s3, 12($t9)
    lw $s4, 16($t9)
    lw $s5, 20($t9)
    lw $s6, 24($t9)
    lw $s7, 28($t9)
    xor $s0, $s0, $t0
    xor $s1, $s1, $t1
    xor $s2, $s2, $t2
    xor $s3, $s3, $t3
    xor $s4, $s4, $t4
    xor $s5, $s5, $t5
    xor $s6, $s6, $t6
    xor $s7, $s7, $t7
    bne $at, $zero, .L_aes_7
    addiu $at, $at, -1
    ins $v0, $s1, 0, 8
    ext $a1, $s0, 8, 8
    ext $a2, $s6, 16, 8
    ext $a3, $s5, 24, 8
    jal npdrmAesDecLastCol
    lw $t1, 36($t9)
    xor $t1, $t1, $a0
    ins $v0, $s2, 0, 8
    ext $a1, $s1, 8, 8
    ext $a2, $s7, 16, 8
    ext $a3, $s6, 24, 8
    jal npdrmAesDecLastCol
    lw $t2, 40($t9)
    xor $t2, $t2, $a0
    ins $v0, $s3, 0, 8
    ext $a1, $s2, 8, 8
    ext $a2, $s0, 16, 8
    ext $a3, $s7, 24, 8
    jal npdrmAesDecLastCol
    lw $t3, 44($t9)
    xor $t3, $t3, $a0
    ins $v0, $s4, 0, 8
    ext $a1, $s3, 8, 8
    ext $a2, $s1, 16, 8
    ext $a3, $s0, 24, 8
    jal npdrmAesDecLastCol
    lw $t4, 48($t9)
    xor $t4, $t4, $a0
    sw $t4, 16($v1)
    ins $v0, $s5, 0, 8
    ext $a1, $s4, 8, 8
    ext $a2, $s2, 16, 8
    ext $a3, $s1, 24, 8
    jal npdrmAesDecLastCol
    lw $t5, 52($t9)
    xor $t5, $t5, $a0
    sw $t5, 20($v1)
    ins $v0, $s6, 0, 8
    ext $a1, $s5, 8, 8
    ext $a2, $s3, 16, 8
    ext $a3, $s2, 24, 8
    jal npdrmAesDecLastCol
    lw $t6, 56($t9)
    xor $t6, $t6, $a0
    sw $t6, 24($v1)
    ins $v0, $s7, 0, 8
    ext $a1, $s6, 8, 8
    ext $a2, $s4, 16, 8
    ext $a3, $s3, 24, 8
    jal npdrmAesDecLastCol
    lw $t7, 60($t9)
    xor $t7, $t7, $a0
    sw $t7, 28($v1)
    ext $a1, $s7, 8, 8
    ext $a2, $s5, 16, 8
    ext $a3, $s4, 24, 8
    j .L_aes_9
    lw $t0, 32($t9)
.L_aes_8:
    sw $t1, 4($v1)
    jal npdrmAesEncLastCol
    ins $t8, $s0, 2, 8
    sw $t2, 8($v1)
    xor $t0, $t0, $a0
    addu $v0, $zero, $zero
    sw $t3, 12($v1)
    mflo $s7
    mfhi $ra
    lw $a0, 0($sp)
    lw $a1, 4($sp)
    lw $a2, 8($sp)
    lw $a3, 12($sp)
    lw $s0, 16($sp)
    lw $s1, 20($sp)
    lw $s2, 24($sp)
    lw $s3, 28($sp)
    lw $s4, 32($sp)
    lw $s5, 36($sp)
    lw $s6, 40($sp)
    sw $t0, 0($v1)
    jr $ra
    addiu $sp, $sp, 48
.L_aes_9:
    sw $t1, 4($v1)
    jal npdrmAesDecLastCol
    ins $v0, $s0, 0, 8
    sw $t2, 8($v1)
    xor $t0, $t0, $a0
    addu $v0, $zero, $zero
    sw $t3, 12($v1)
    mflo $s7
    mfhi $ra
    lw $a0, 0($sp)
    lw $a1, 4($sp)
    lw $a2, 8($sp)
    lw $a3, 12($sp)
    lw $s0, 16($sp)
    lw $s1, 20($sp)
    lw $s2, 24($sp)
    lw $s3, 28($sp)
    lw $s4, 32($sp)
    lw $s5, 36($sp)
    lw $s6, 40($sp)
    sw $t0, 0($v1)
.L_aes_10:
    jr $ra
    addiu $sp, $sp, 48
.L_aes_11:
    lw $t0, 0($a1)
    lw $t1, 4($a1)
    lw $t2, 8($a1)
    lw $t3, 12($a1)
    lw $s0, 0($t9)
    lw $s1, 4($t9)
    lw $s2, 8($t9)
    lw $s3, 12($t9)
    xor $s0, $s0, $t0
    xor $s1, $s1, $t1
    xor $s2, $s2, $t2
    xor $s3, $s3, $t3
.L_aes_12:
    addiu $t9, $t9, 16
    ext $a1, $s3, 8, 8
    ext $a2, $s2, 16, 8
    ext $a3, $s1, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s0, 2, 8
    xor $t0, $a0, $a3
    ext $a1, $s0, 8, 8
    ext $a2, $s3, 16, 8
    ext $a3, $s2, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s1, 2, 8
    xor $t1, $a0, $a3
    ext $a1, $s1, 8, 8
    ext $a2, $s0, 16, 8
    ext $a3, $s3, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s2, 2, 8
    xor $t2, $a0, $a3
    ext $a1, $s2, 8, 8
    ext $a2, $s1, 16, 8
    ext $a3, $s0, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s3, 2, 8
    xor $t3, $a0, $a3
    lw $s0, 0($t9)
    lw $s1, 4($t9)
    lw $s2, 8($t9)
    lw $s3, 12($t9)
    xor $s0, $s0, $t0
    xor $s1, $s1, $t1
    xor $s2, $s2, $t2
    xor $s3, $s3, $t3
    bne $at, $zero, .L_aes_12
    addiu $at, $at, -1
    ins $v0, $s1, 0, 8
    ext $a1, $s0, 8, 8
    ext $a2, $s3, 16, 8
    ext $a3, $s2, 24, 8
    jal npdrmAesDecLastCol
    lw $t1, 20($t9)
    xor $t1, $t1, $a0
    ins $v0, $s2, 0, 8
    ext $a1, $s1, 8, 8
    ext $a2, $s0, 16, 8
    ext $a3, $s3, 24, 8
    jal npdrmAesDecLastCol
    lw $t2, 24($t9)
    xor $t2, $t2, $a0
    ins $v0, $s3, 0, 8
    ext $a1, $s2, 8, 8
    ext $a2, $s1, 16, 8
    ext $a3, $s0, 24, 8
    jal npdrmAesDecLastCol
    lw $t3, 28($t9)
    xor $t3, $t3, $a0
    ext $a1, $s3, 8, 8
    ext $a2, $s2, 16, 8
    ext $a3, $s1, 24, 8
    j .L_aes_9
    lw $t0, 16($t9)
.L_aes_13:
    lw $t0, 0($a1)
    lw $t1, 4($a1)
    lw $t2, 8($a1)
    lw $t3, 12($a1)
    lw $t4, 16($a1)
    lw $t5, 20($a1)
    lw $s0, 0($t9)
    lw $s1, 4($t9)
    lw $s2, 8($t9)
    lw $s3, 12($t9)
    lw $s4, 16($t9)
    lw $s5, 20($t9)
    xor $s0, $s0, $t0
    xor $s1, $s1, $t1
    xor $s2, $s2, $t2
    xor $s3, $s3, $t3
    xor $s4, $s4, $t4
    xor $s5, $s5, $t5
.L_aes_14:
    addiu $t9, $t9, 24
    ext $a1, $s5, 8, 8
    ext $a2, $s4, 16, 8
    ext $a3, $s3, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s0, 2, 8
    xor $t0, $a0, $a3
    ext $a1, $s0, 8, 8
    ext $a2, $s5, 16, 8
    ext $a3, $s4, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s1, 2, 8
    xor $t1, $a0, $a3
    ext $a1, $s1, 8, 8
    ext $a2, $s0, 16, 8
    ext $a3, $s5, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s2, 2, 8
    xor $t2, $a0, $a3
    ext $a1, $s2, 8, 8
    ext $a2, $s1, 16, 8
    ext $a3, $s0, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s3, 2, 8
    xor $t3, $a0, $a3
    ext $a1, $s3, 8, 8
    ext $a2, $s2, 16, 8
    ext $a3, $s1, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s4, 2, 8
    xor $t4, $a0, $a3
    ext $a1, $s4, 8, 8
    ext $a2, $s3, 16, 8
    ext $a3, $s2, 24, 8
    jal npdrmAesMixCol
    ins $t8, $s5, 2, 8
    xor $t5, $a0, $a3
    lw $s0, 0($t9)
    lw $s1, 4($t9)
    lw $s2, 8($t9)
    lw $s3, 12($t9)
    lw $s4, 16($t9)
    lw $s5, 20($t9)
    xor $s0, $s0, $t0
    xor $s1, $s1, $t1
    xor $s2, $s2, $t2
    xor $s3, $s3, $t3
    xor $s4, $s4, $t4
    xor $s5, $s5, $t5
    xor $s5, $s5, $t5
    bne $at, $zero, .L_aes_14
    addiu $at, $at, -1
    ins $v0, $s1, 0, 8
    ext $a1, $s0, 8, 8
    ext $a2, $s5, 16, 8
    ext $a3, $s4, 24, 8
    jal npdrmAesDecLastCol
    lw $t1, 28($t9)
    xor $t1, $t1, $a0
    ins $v0, $s2, 0, 8
    ext $a1, $s1, 8, 8
    ext $a2, $s0, 16, 8
    ext $a3, $s5, 24, 8
    jal npdrmAesDecLastCol
    lw $t2, 32($t9)
    xor $t2, $t2, $a0
    ins $v0, $s3, 0, 8
    ext $a1, $s2, 8, 8
    ext $a2, $s1, 16, 8
    ext $a3, $s0, 24, 8
    jal npdrmAesDecLastCol
    lw $t3, 36($t9)
    xor $t3, $t3, $a0
    ins $v0, $s4, 0, 8
    ext $a1, $s3, 8, 8
    ext $a2, $s2, 16, 8
    ext $a3, $s1, 24, 8
    jal npdrmAesDecLastCol
    lw $t4, 40($t9)
    xor $t4, $t4, $a0
    sw $t4, 16($v1)
    ins $v0, $s5, 0, 8
    ext $a1, $s4, 8, 8
    ext $a2, $s3, 16, 8
    ext $a3, $s2, 24, 8
    jal npdrmAesDecLastCol
    lw $t5, 44($t9)
    xor $t5, $t5, $a0
    sw $t5, 20($v1)
    ext $a1, $s5, 8, 8
    ext $a2, $s4, 16, 8
    ext $a3, $s3, 24, 8
    j .L_aes_9
    lw $t0, 24($t9)
.end npdrmAesDecryptBlock

.global npdrmAesMixCol
.ent npdrmAesMixCol
npdrmAesMixCol:
    lw $a0, %lo(g_npdrmAesTables)($t8)
    ins $t8, $a1, 2, 8
    lw $a1, %lo(g_npdrmAesTables)($t8)
    ins $t8, $a2, 2, 8
    lw $a2, %lo(g_npdrmAesTables)($t8)
    ins $t8, $a3, 2, 8
    lw $a3, %lo(g_npdrmAesTables)($t8)
    rotr $a1, $a1, 24
    xor $a0, $a0, $a1
    rotr $a2, $a2, 16
    xor $a0, $a0, $a2
    jr $ra
    rotr $a3, $a3, 8
.end npdrmAesMixCol

.global npdrmAesEncLastCol
.ent npdrmAesEncLastCol
npdrmAesEncLastCol:
    lbu $a0, %lo(g_npdrmAesTables + 2)($t8)
    ins $t8, $a1, 2, 8
    lbu $a1, %lo(g_npdrmAesTables + 2)($t8)
    ins $t8, $a2, 2, 8
    lbu $a2, %lo(g_npdrmAesTables + 2)($t8)
    ins $t8, $a3, 2, 8
    lbu $a3, %lo(g_npdrmAesTables + 2)($t8)
    ins $a0, $a1, 8, 8
    ins $a0, $a2, 16, 8
    jr $ra
    ins $a0, $a3, 24, 8
.end npdrmAesEncLastCol

.global npdrmAesDecLastCol
.ent npdrmAesDecLastCol
npdrmAesDecLastCol:
    lbu $a0, %lo(g_npdrmAesTables)($v0)
    ins $v0, $a1, 0, 8
    lbu $a1, %lo(g_npdrmAesTables)($v0)
    ins $v0, $a2, 0, 8
    lbu $a2, %lo(g_npdrmAesTables)($v0)
    ins $v0, $a3, 0, 8
    lbu $a3, %lo(g_npdrmAesTables)($v0)
    ins $a0, $a1, 8, 8
    ins $a0, $a2, 16, 8
    jr $ra
    ins $a0, $a3, 24, 8
.end npdrmAesDecLastCol

.set at
.set reorder
