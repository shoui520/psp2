    .set noreorder
    .set noat

    .section .text, "ax", @progbits
    .align 2

    .globl exc_vec_0
    .globl exc_vec_1
    .globl exc_vec_2
    .globl exc_vec_3
    .globl exc_vec_4
    .globl exc_vec_5
    .globl exc_vec_6
    .globl exc_vec_7
    .globl exc_vec_8
    .globl exc_vec_9

    .ent exc_vec_0
exc_vec_0:
    nop
    nop
    j       .L_exc_common
    nop
exc_vec_1:
    nop
    nop
    j       .L_exc_common
    nop
exc_vec_2:
    nop
    nop
    j       .L_exc_common
    nop
exc_vec_3:
    nop
    nop
    j       .L_exc_common
    nop
exc_vec_4:
    nop
    nop
    j       .L_exc_common
    nop
exc_vec_5:
    nop
    nop
    j       .L_exc_common
    nop
exc_vec_6:
    nop
    nop
    j       .L_exc_common
    nop
exc_vec_7:
    nop
    nop
    j       .L_exc_common
    nop
exc_vec_8:
    nop
    nop
    j       .L_exc_common
    nop
exc_vec_9:
    nop
    nop
    .word   0x40423000
    .word   0x40433800
    .word   0x40c22000
    .word   0x40c32800
    .word   0x4042a000
    .word   0x40439800
    ori     $v0, $v0, 0x7c
    .word   0x40c21800
    .word   0x40c31000
    j       .L_exc_common
    nop

.L_exc_common:
    lui     $v0, %hi(g_ctxPtr)
    lw      $v0, %lo(g_ctxPtr)($v0)
    sw      $k0, 0x68($v0)
    sw      $k1, 0x6c($v0)
    .word   0x405a2000
    .word   0x405b2800
    sw      $k0, 8($v0)
    sw      $k1, 0xc($v0)
    mfc0    $k0, $12
    lui     $k1, 0x6000
    or      $k0, $k0, $k1
    mtc0    $k0, $12
    nop
    nop
    mfc1    $zero, $f0
    nop
    nop
    nop
    cfc1    $k1, $31
    ctc1    $zero, $31
    ori     $k0, $k0, 0xff19
    xori    $k0, $k0, 0xff19
    mtc0    $k0, $12
    mfc0    $k0, $13
    ori     $k0, $k0, 0x300
    xori    $k0, $k0, 0x300
    mtc0    $k0, $13
    mfc0    $k0, $12
    ori     $k0, $k0, 6
    xori    $k0, $k0, 6
    mtc0    $k0, $12
    nop
    nop
    sw      $zero, 0($v0)
    sw      $at, 4($v0)
    sw      $a0, 0x10($v0)
    sw      $a1, 0x14($v0)
    sw      $a2, 0x18($v0)
    sw      $a3, 0x1c($v0)
    sw      $t0, 0x20($v0)
    sw      $t1, 0x24($v0)
    sw      $t2, 0x28($v0)
    sw      $t3, 0x2c($v0)
    sw      $t4, 0x30($v0)
    sw      $t5, 0x34($v0)
    sw      $t6, 0x38($v0)
    sw      $t7, 0x3c($v0)
    sw      $s0, 0x40($v0)
    sw      $s1, 0x44($v0)
    sw      $s2, 0x48($v0)
    sw      $s3, 0x4c($v0)
    sw      $s4, 0x50($v0)
    sw      $s5, 0x54($v0)
    sw      $s6, 0x58($v0)
    sw      $s7, 0x5c($v0)
    sw      $t8, 0x60($v0)
    sw      $t9, 0x64($v0)
    sw      $gp, 0x70($v0)
    sw      $sp, 0x74($v0)
    sw      $fp, 0x78($v0)
    sw      $ra, 0x7c($v0)
    swc1    $f0, 0x80($v0)
    swc1    $f1, 0x84($v0)
    swc1    $f2, 0x88($v0)
    swc1    $f3, 0x8c($v0)
    swc1    $f4, 0x90($v0)
    swc1    $f5, 0x94($v0)
    swc1    $f6, 0x98($v0)
    swc1    $f7, 0x9c($v0)
    swc1    $f8, 0xa0($v0)
    swc1    $f9, 0xa4($v0)
    swc1    $f10, 0xa8($v0)
    swc1    $f11, 0xac($v0)
    swc1    $f12, 0xb0($v0)
    swc1    $f13, 0xb4($v0)
    swc1    $f14, 0xb8($v0)
    swc1    $f15, 0xbc($v0)
    swc1    $f16, 0xc0($v0)
    swc1    $f17, 0xc4($v0)
    swc1    $f18, 0xc8($v0)
    swc1    $f19, 0xcc($v0)
    swc1    $f20, 0xd0($v0)
    swc1    $f21, 0xd4($v0)
    swc1    $f22, 0xd8($v0)
    swc1    $f23, 0xdc($v0)
    swc1    $f24, 0xe0($v0)
    swc1    $f25, 0xe4($v0)
    swc1    $f26, 0xe8($v0)
    swc1    $f27, 0xec($v0)
    swc1    $f28, 0xf0($v0)
    swc1    $f29, 0xf4($v0)
    swc1    $f30, 0xf8($v0)
    swc1    $f31, 0xfc($v0)
    .word   0x40440000
    .word   0x40450800
    .word   0x40461000
    .word   0x40471800
    mfc0    $t0, $8
    mfc0    $t1, $9
    mfc0    $t2, $11
    mfc0    $t3, $15
    mfc0    $t4, $16
    mfc0    $fp, $24
    mfc0    $t5, $21
    mfc0    $t6, $22
    mfc0    $t7, $25
    mfc0    $t8, $28
    mfc0    $t9, $29
    sw      $zero, 0x100($v0)
    sw      $zero, 0x104($v0)
    sw      $zero, 0x108($v0)
    sw      $zero, 0x10c($v0)
    sw      $zero, 0x110($v0)
    sw      $zero, 0x114($v0)
    sw      $zero, 0x118($v0)
    sw      $zero, 0x11c($v0)
    sw      $t0, 0x120($v0)
    sw      $t1, 0x124($v0)
    sw      $zero, 0x128($v0)
    sw      $t2, 0x12c($v0)
    sw      $a2, 0x130($v0)
    sw      $a3, 0x134($v0)
    sw      $a0, 0x138($v0)
    sw      $t3, 0x13c($v0)
    sw      $t4, 0x140($v0)
    sw      $zero, 0x144($v0)
    sw      $zero, 0x148($v0)
    sw      $zero, 0x14c($v0)
    sw      $zero, 0x150($v0)
    sw      $t5, 0x154($v0)
    sw      $t6, 0x158($v0)
    sw      $zero, 0x15c($v0)
    sw      $fp, 0x160($v0)
    sw      $t7, 0x164($v0)
    sw      $zero, 0x168($v0)
    sw      $zero, 0x16c($v0)
    sw      $t8, 0x170($v0)
    sw      $t9, 0x174($v0)
    sw      $a1, 0x178($v0)
    sw      $zero, 0x17c($v0)
    mflo    $t0
    mfhi    $t1
    cfc1    $t2, $0
    sw      $t0, 0x180($v0)
    sw      $t1, 0x184($v0)
    sw      $t2, 0x188($v0)
    sw      $k1, 0x18c($v0)
    lui     $sp, %hi(g_stack_end)
    addiu   $sp, $sp, %lo(g_stack_end)
    move    $k0, $zero
    move    $k1, $zero
    jal     bsod_crash_handler
    nop
    lui     $at, %hi(g_ctxPtr)
    jr      $ra
    sw      $a0, %lo(g_ctxPtr)($at)
    nop
    nop
    nop
    .end exc_vec_0
