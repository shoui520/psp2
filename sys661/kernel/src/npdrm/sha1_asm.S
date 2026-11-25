/* Sony handwritten Allegrex SHA-1 routines from npdrm.prx (0x1af0..0x1f6c). */
.set noreorder
.set noat
.text

.global npdrmSha1Transform
.ent npdrmSha1Transform
npdrmSha1Transform:
    addiu $sp, $sp, -64
    sw $ra, 0($sp)
    sw $s0, 4($sp)
    sw $s1, 8($sp)
    sw $s2, 12($sp)
    sw $s3, 16($sp)
    sw $s4, 20($sp)
    sw $s5, 24($sp)
    lw $s0, 0($a1)
    lw $s1, 4($a1)
    lw $s2, 8($a1)
    lw $s3, 12($a1)
    lw $s4, 16($a1)
    jal npdrmSha1TransformCore
    addu $s5, $a1, $zero
    sw $s0, 0($s5)
    sw $s1, 4($s5)
    sw $s2, 8($s5)
    sw $s3, 12($s5)
    sw $s4, 16($s5)
    lw $ra, 0($sp)
    lw $s0, 4($sp)
    lw $s1, 8($sp)
    lw $s2, 12($sp)
    lw $s3, 16($sp)
    lw $s4, 20($sp)
    lw $s5, 24($sp)
    jr $ra
    addiu $sp, $sp, 64
.end npdrmSha1Transform

.global npdrmSha1TransformCore
.ent npdrmSha1TransformCore
npdrmSha1TransformCore:
    addu $t0, $s0, $zero
    addu $t1, $s1, $zero
    addu $t2, $s2, $zero
    addu $t3, $s3, $zero
    addu $t4, $s4, $zero
    addu $v0, $zero, $zero
.L_sha1_1:
    andi $t5, $v0, 0x3c
    sltiu $t6, $v0, 64
    addu $t9, $a0, $t5
    bne $t6, $zero, .L_sha1_2
    lw $t5, 0($t9)
    addiu $t6, $v0, 8
    addiu $t7, $v0, 32
    addiu $t8, $v0, 52
    andi $t6, $t6, 0x3c
    andi $t7, $t7, 0x3c
    andi $t8, $t8, 0x3c
    addu $t6, $a0, $t6
    addu $t7, $a0, $t7
    lw $t6, 0($t6)
    addu $t8, $a0, $t8
    lw $t7, 0($t7)
    xor $t5, $t5, $t6
    lw $t8, 0($t8)
    xor $t7, $t7, $t8
    xor $t5, $t5, $t7
    rotr $t5, $t5, 31
    sw $t5, 0($t9)
.L_sha1_2:
    rotr $t6, $t0, 27
    addu $t5, $t5, $t4
    addu $t5, $t5, $t6
    sltiu $t6, $v0, 80
    bne $t6, $zero, .L_sha1_3
    sltiu $t6, $v0, 160
    bne $t6, $zero, .L_sha1_4
    sltiu $t6, $v0, 240
    bne $t6, $zero, .L_sha1_5
    xor $t6, $t1, $t2
    lui $t8, 0xca62
    xor $t6, $t6, $t3
    ori $t8, $t8, 0xc1d6
    j .L_sha1_6
    addiu $v0, $v0, 4
    nop
    nop
    nop
.L_sha1_3:
    and $t6, $t1, $t2
    nor $t7, $zero, $t1
    lui $t8, 0x5a82
    and $t7, $t7, $t3
    ori $t8, $t8, 0x7999
    or $t6, $t6, $t7
    j .L_sha1_6
    addiu $v0, $v0, 4
.L_sha1_4:
    xor $t6, $t1, $t2
    lui $t8, 0x6ed9
    xor $t6, $t6, $t3
    ori $t8, $t8, 0xeba1
    j .L_sha1_6
    addiu $v0, $v0, 4
    nop
    nop
.L_sha1_5:
    and $t6, $t1, $t2
    and $t7, $t1, $t3
    lui $t8, 0x8f1b
    or $t6, $t6, $t7
    and $t7, $t2, $t3
    ori $t8, $t8, 0xbcdc
    or $t6, $t6, $t7
    addiu $v0, $v0, 4
.L_sha1_6:
    addu $t6, $t6, $t8
    addu $t4, $t3, $zero
    addu $t3, $t2, $zero
    sll $t7, $t1, 30
    srl $t1, $t1, 2
    sltiu $t8, $v0, 320
    or $t2, $t7, $t1
    addu $t1, $t0, $zero
    bne $t8, $zero, .L_sha1_1
    addu $t0, $t5, $t6
    addu $s0, $t0, $s0
    addu $s1, $t1, $s1
    addu $s2, $t2, $s2
    addu $s3, $t3, $s3
    jr $ra
    addu $s4, $t4, $s4
.end npdrmSha1TransformCore

.global npdrmSha1Digest
.ent npdrmSha1Digest
npdrmSha1Digest:
    addiu $sp, $sp, -256
    sw $s0, 0($sp)
    sw $s1, 4($sp)
    sw $s2, 8($sp)
    sw $s3, 12($sp)
    sw $s4, 16($sp)
    sw $s5, 20($sp)
    sw $s6, 24($sp)
    sw $ra, 28($sp)
    addu $s5, $a0, $zero
    sll $s6, $a1, 3
    addiu $a0, $sp, 128
    lui $t0, %hi(g_npdrmSha1Iv)
    addiu $t0, $t0, %lo(g_npdrmSha1Iv)
    lw $s0, 0($t0)
    andi $t2, $s5, 0xf
    lw $s1, 4($t0)
    andi $t3, $a1, 0x3f
    lw $s2, 8($t0)
    or $t2, $t2, $t3
    lw $s3, 12($t0)
    lw $s4, 16($t0)
    bne $t2, $zero, .L_sha1_11
    nop
    beq $a1, $zero, .L_sha1_9
    nop
    nop
    nop
.L_sha1_7:
    addiu $t0, $zero, 16
    addu $t1, $a0, $zero
.L_sha1_8:
    lw $t2, 0($s5)
    addiu $t0, $t0, -1
    addiu $s5, $s5, 4
    wsbw $t2, $t2
    sw $t2, 0($t1)
    bne $t0, $zero, .L_sha1_8
    addiu $t1, $t1, 4
    addiu $a1, $a1, -64
    jal npdrmSha1TransformCore
    nop
    bne $a1, $zero, .L_sha1_7
    nop
.L_sha1_9:
    addiu $v0, $zero, 16
    addu $t0, $a0, $zero
.L_sha1_10:
    addiu $v0, $v0, -4
    sw $zero, 0($t0)
    sw $zero, 4($t0)
    sw $zero, 8($t0)
    sw $zero, 12($t0)
    bne $v0, $zero, .L_sha1_10
    addiu $t0, $t0, 16
    sw $s6, 60($a0)
    lui $t0, 0x8000
    jal npdrmSha1TransformCore
    sw $t0, 0($a0)
    j .L_sha1_23
    nop
    nop
.L_sha1_11:
    sltiu $v0, $a1, 64
    bne $v0, $zero, .L_sha1_13
    nop
    addiu $t0, $zero, 16
    addu $t1, $a0, $zero
.L_sha1_12:
    lwr $t2, 0($s5)
    lwl $t2, 3($s5)
    addiu $t0, $t0, -1
    addiu $s5, $s5, 4
    wsbw $t2, $t2
    sw $t2, 0($t1)
    bne $t0, $zero, .L_sha1_12
    addiu $t1, $t1, 4
    jal npdrmSha1TransformCore
    nop
    j .L_sha1_11
    addiu $a1, $a1, -64
    nop
.L_sha1_13:
    addiu $v0, $zero, 16
    addu $t0, $a0, $zero
.L_sha1_14:
    addiu $v0, $v0, -4
    sw $zero, 0($t0)
    sw $zero, 4($t0)
    sw $zero, 8($t0)
    sw $zero, 12($t0)
    bne $v0, $zero, .L_sha1_14
    addiu $t0, $t0, 16
    sltiu $v0, $a1, 56
    addu $t0, $s5, $zero
    addu $t2, $a1, $zero
    bne $v0, $zero, .L_sha1_18
    nop
    addu $t1, $a0, $zero
    nop
.L_sha1_15:
    lbu $t3, 0($t0)
    addiu $t2, $t2, -1
    addiu $t0, $t0, 1
    sb $t3, 0($t1)
    bne $t2, $zero, .L_sha1_15
    addiu $t1, $t1, 1
    addiu $t3, $zero, 128
    sb $t3, 0($t1)
    addiu $v0, $zero, 16
    addu $t0, $a0, $zero
.L_sha1_16:
    lw $t1, 0($t0)
    addiu $v0, $v0, -1
    wsbw $t1, $t1
    sw $t1, 0($t0)
    bne $v0, $zero, .L_sha1_16
    addiu $t0, $t0, 4
    jal npdrmSha1TransformCore
    nop
    addiu $a1, $a1, -64
    addiu $v0, $zero, 16
    addu $t0, $a0, $zero
.L_sha1_17:
    addiu $v0, $v0, -4
    sw $zero, 0($t0)
    sw $zero, 4($t0)
    sw $zero, 8($t0)
    sw $zero, 12($t0)
    bne $v0, $zero, .L_sha1_17
    addiu $t0, $t0, 16
.L_sha1_18:
    bgtz $a1, .L_sha1_19
    sw $s6, 60($a0)
    sltiu $t0, $a1, 1
    sll $t0, $t0, 31
    j .L_sha1_22
    sw $t0, 0($a0)
.L_sha1_19:
    addu $t0, $s5, $zero
    addu $t1, $a0, $zero
.L_sha1_20:
    lbu $t3, 0($t0)
    addiu $a1, $a1, -1
    addiu $t0, $t0, 1
    sb $t3, 0($t1)
    bne $a1, $zero, .L_sha1_20
    addiu $t1, $t1, 1
    addiu $t0, $zero, 128
    sb $t0, 0($t1)
    addiu $v0, $zero, 14
    addu $t0, $a0, $zero
.L_sha1_21:
    lw $t1, 0($t0)
    addiu $v0, $v0, -1
    wsbw $t1, $t1
    sw $t1, 0($t0)
    bne $v0, $zero, .L_sha1_21
    addiu $t0, $t0, 4
.L_sha1_22:
    jal npdrmSha1TransformCore
    nop
.L_sha1_23:
    wsbw $s0, $s0
    wsbw $s1, $s1
    wsbw $s2, $s2
    wsbw $s3, $s3
    wsbw $s4, $s4
    swr $s0, 0($a2)
    swl $s0, 3($a2)
    swr $s1, 4($a2)
    swl $s1, 7($a2)
    swr $s2, 8($a2)
    swl $s2, 11($a2)
    swr $s3, 12($a2)
    swl $s3, 15($a2)
    swr $s4, 16($a2)
    swl $s4, 19($a2)
    lw $s0, 0($sp)
    lw $s1, 4($sp)
    lw $s2, 8($sp)
    lw $s3, 12($sp)
    lw $s4, 16($sp)
    lw $s5, 20($sp)
    lw $s6, 24($sp)
    lw $ra, 28($sp)
    addu $v0, $zero, $zero
    jr $ra
    addiu $sp, $sp, 256
.end npdrmSha1Digest

.set at
.set reorder
