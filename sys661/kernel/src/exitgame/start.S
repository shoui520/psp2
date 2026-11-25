    .set noreorder
    .set noat

    .section .text, "ax", @progbits
    .align 2

    .globl excVecAddrLoad
    .globl excVecAddrStore
    .globl excVecBusFetch
    .globl excVecBusData
    .globl excVecBreak
    .globl excVecReservedInsn
    .globl excVecCopUnusable
    .globl excVecOverflow
    .globl excVecFpu
    .globl excVecNmi
    .globl excVecAddrStoreEntry
    .globl excVecBusFetchEntry
    .globl excVecBusDataEntry
    .globl excVecBreakEntry
    .globl excVecReservedInsnEntry
    .globl excVecCopUnusableEntry
    .globl excVecOverflowEntry
    .globl excVecFpuEntry
    .globl excVecNmiEntry

    .type excVecAddrLoad, @function
    .ent excVecAddrLoad
excVecAddrLoad:
    nop
    nop
    j       .L_exc_common
    nop
    .end excVecAddrLoad
    .size excVecAddrLoad, .-excVecAddrLoad

excVecAddrStore:
    nop
    nop
    .type excVecAddrStoreEntry, @function
    .ent excVecAddrStoreEntry
excVecAddrStoreEntry:
    j       .L_exc_common
    nop
    .end excVecAddrStoreEntry
    .size excVecAddrStoreEntry, .-excVecAddrStoreEntry

excVecBusFetch:
    nop
    nop
    .type excVecBusFetchEntry, @function
    .ent excVecBusFetchEntry
excVecBusFetchEntry:
    j       .L_exc_common
    nop
    .end excVecBusFetchEntry
    .size excVecBusFetchEntry, .-excVecBusFetchEntry

excVecBusData:
    nop
    nop
    .type excVecBusDataEntry, @function
    .ent excVecBusDataEntry
excVecBusDataEntry:
    j       .L_exc_common
    nop
    .end excVecBusDataEntry
    .size excVecBusDataEntry, .-excVecBusDataEntry

excVecBreak:
    nop
    nop
    .type excVecBreakEntry, @function
    .ent excVecBreakEntry
excVecBreakEntry:
    j       .L_exc_common
    nop
    .end excVecBreakEntry
    .size excVecBreakEntry, .-excVecBreakEntry

excVecReservedInsn:
    nop
    nop
    .type excVecReservedInsnEntry, @function
    .ent excVecReservedInsnEntry
excVecReservedInsnEntry:
    j       .L_exc_common
    nop
    .end excVecReservedInsnEntry
    .size excVecReservedInsnEntry, .-excVecReservedInsnEntry

excVecCopUnusable:
    nop
    nop
    .type excVecCopUnusableEntry, @function
    .ent excVecCopUnusableEntry
excVecCopUnusableEntry:
    j       .L_exc_common
    nop
    .end excVecCopUnusableEntry
    .size excVecCopUnusableEntry, .-excVecCopUnusableEntry

excVecOverflow:
    nop
    nop
    .type excVecOverflowEntry, @function
    .ent excVecOverflowEntry
excVecOverflowEntry:
    j       .L_exc_common
    nop
    .end excVecOverflowEntry
    .size excVecOverflowEntry, .-excVecOverflowEntry

excVecFpu:
    nop
    nop
    .type excVecFpuEntry, @function
    .ent excVecFpuEntry
excVecFpuEntry:
    j       .L_exc_common
    nop
    .end excVecFpuEntry
    .size excVecFpuEntry, .-excVecFpuEntry

excVecNmi:
    nop
    nop
    .type excVecNmiEntry, @function
    .ent excVecNmiEntry
excVecNmiEntry:
    cfc0    $v0, $6
    cfc0    $v1, $7
    ctc0    $v0, $4
    ctc0    $v1, $5
    cfc0    $v0, $20
    cfc0    $v1, $19
    ori     $v0, $v0, 0x7c
    ctc0    $v0, $3
    ctc0    $v1, $2
    j       .L_exc_common
    nop

.L_exc_common:
    lui     $v0, %hi(g_ctxPtr)
    lw      $v0, %lo(g_ctxPtr)($v0)
    sw      $k0, 0x68($v0)
    sw      $k1, 0x6c($v0)
    cfc0    $k0, $4
    cfc0    $k1, $5
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
    cfc0    $a0, $0
    cfc0    $a1, $1
    cfc0    $a2, $2
    cfc0    $a3, $3
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
    addu    $k0, $zero, $zero
    addu    $k1, $zero, $zero
    jal     bsod_crash_handler
    nop
    lui     $at, %hi(g_ctxPtr)
    jr      $ra
    sw      $a0, %lo(g_ctxPtr)($at)
    .end excVecNmiEntry
    .size excVecNmiEntry, .-excVecNmiEntry
    nop
    nop
    nop
