#include "../include/common_asm.h"

    .type sysmemSaveCpuContext, @function
sysmemSaveCpuContext:
    addu $t9, $ra, $zr
    sw $s0, 0($a0)
    sw $s1, 4($a0)
    sw $s2, 8($a0)
    sw $s3, 12($a0)
    sw $s4, 16($a0)
    sw $s5, 20($a0)
    sw $s6, 24($a0)
    sw $s7, 28($a0)
    sw $k0, 32($a0)
    sw $k1, 36($a0)
    sw $gp, 40($a0)
    sw $sp, 44($a0)
    sw $fp, 48($a0)
    sw $ra, 52($a0)
    mfc0 $v0, COP0_STATE_STATUS
    lui $t1, 0x2000
    or $t1, $v0, $t1
    mtc0 $t1, COP0_STATE_STATUS
    nop
    nop
    cfc1 $t0, $0
    cfc1 $t1, $31
    sw $t0, 56($a0)
    sw $t1, 60($a0)
    swc1 $0, 64($a0)
    swc1 $1, 68($a0)
    swc1 $2, 72($a0)
    swc1 $3, 76($a0)
    swc1 $4, 80($a0)
    swc1 $5, 84($a0)
    swc1 $6, 88($a0)
    swc1 $7, 92($a0)
    swc1 $8, 96($a0)
    swc1 $9, 100($a0)
    swc1 $10, 104($a0)
    swc1 $11, 108($a0)
    swc1 $12, 112($a0)
    swc1 $13, 116($a0)
    swc1 $14, 120($a0)
    swc1 $15, 124($a0)
    swc1 $16, 128($a0)
    swc1 $17, 132($a0)
    swc1 $18, 136($a0)
    swc1 $19, 140($a0)
    swc1 $20, 144($a0)
    swc1 $21, 148($a0)
    swc1 $22, 152($a0)
    swc1 $23, 156($a0)
    swc1 $24, 160($a0)
    swc1 $25, 164($a0)
    swc1 $26, 168($a0)
    swc1 $27, 172($a0)
    swc1 $28, 176($a0)
    swc1 $29, 180($a0)
    swc1 $30, 184($a0)
    swc1 $31, 188($a0)
    mtc0 $v0, COP0_STATE_STATUS
    nop
    nop
    addiu $t8, $a0, 192
    addu $t0, $zr, $zr
    addu $t1, $zr, $zr
    addu $t2, $zr, $zr
    addu $t3, $zr, $zr
    addu $t4, $zr, $zr
    addu $t5, $zr, $zr
    addu $t6, $zr, $zr
    bal sysmemStore8Words
    addu $t7, $zr, $zr
    mfc0 $t0, COP0_STATE_BAD_VADDR
    mfc0 $t1, COP0_STATE_COUNT
    addu $t2, $zr, $zr
    mfc0 $t3, COP0_STATE_COMPARE
    mfc0 $t4, COP0_STATE_STATUS
    mfc0 $t5, COP0_STATE_CAUSE
    mfc0 $t6, COP0_STATE_EPC
    bal sysmemStore8Words
    mfc0 $t7, COP0_STATE_PRID
    addu $t0, $zr, $zr
    addu $t1, $zr, $zr
    addu $t2, $zr, $zr
    addu $t3, $zr, $zr
    addu $t4, $zr, $zr
    mfc0 $t5, $21
    mfc0 $t6, $22
    bal sysmemStore8Words
    addu $t7, $zr, $zr
    mfc0 $t0, $24
    mfc0 $t1, COP0_STATE_EBASE
    addu $t2, $zr, $zr
    addu $t3, $zr, $zr
    mfc0 $t5, COP0_STATE_TAG_LO
    mfc0 $t6, COP0_STATE_TAG_HI
    bal sysmemStore8Words
    mfc0 $t7, COP0_STATE_ERROR_PC
    cfc0 $t0, $0
    cfc0 $t1, $1
    cfc0 $t2, $2
    cfc0 $t3, $3
    cfc0 $t4, $4
    cfc0 $t5, $5
    cfc0 $t6, $6
    bal sysmemStore8Words
    cfc0 $t7, $7
    cfc0 $t0, $8
    cfc0 $t1, $9
    cfc0 $t2, $10
    cfc0 $t3, $11
    cfc0 $t4, $12
    cfc0 $t5, $13
    cfc0 $t6, $14
    bal sysmemStore8Words
    cfc0 $t7, $15
    cfc0 $t0, $16
    cfc0 $t1, $17
    cfc0 $t2, $18
    cfc0 $t3, $19
    cfc0 $t4, $20
    cfc0 $t5, $21
    cfc0 $t6, $22
    bal sysmemStore8Words
    cfc0 $t7, $23
    cfc0 $t0, $24
    cfc0 $t1, $25
    cfc0 $t2, $26
    cfc0 $t3, $27
    cfc0 $t4, $28
    cfc0 $t5, $29
    cfc0 $t6, $30
    bal sysmemStore8Words
    cfc0 $t7, $31
    mfc0 $v0, COP0_STATE_STATUS
    lui $t1, 0x4000
    or $t1, $v0, $t1
    mtc0 $t1, COP0_STATE_STATUS
    nop
    nop
    mfvc $t0, VFPU_PFXS
    mfvc $t1, VFPU_PFXT
    mfvc $t2, VFPU_PFXD
    mfvc $t3, VFPU_CC
    mfvc $t4, VFPU_INF4
    addu $t5, $zr, $zr
    addu $t6, $zr, $zr
    bal sysmemStore8Words
    mfvc $t7, VFPU_REV
    mfvc $t0, VFPU_RCX0
    mfvc $t1, VFPU_RCX1
    mfvc $t2, VFPU_RCX2
    mfvc $t3, VFPU_RCX3
    mfvc $t4, VFPU_RCX4
    mfvc $t5, VFPU_RCX5
    mfvc $t6, VFPU_RCX6
    bal sysmemStore8Words
    mfvc $t7, VFPU_RCX7
    sv.q       C000, 0($t8)
    sv.q       C010, 16($t8)
    sv.q       C020, 32($t8)
    sv.q       C030, 48($t8)
    sv.q       C100, 64($t8)
    sv.q       C110, 80($t8)
    sv.q       C120, 96($t8)
    sv.q       C130, 112($t8)
    sv.q       C200, 128($t8)
    sv.q       C210, 144($t8)
    sv.q       C220, 160($t8)
    sv.q       C230, 176($t8)
    sv.q       C300, 192($t8)
    sv.q       C310, 208($t8)
    sv.q       C320, 224($t8)
    sv.q       C330, 240($t8)
    sv.q       C400, 256($t8)
    sv.q       C410, 272($t8)
    sv.q       C420, 288($t8)
    sv.q       C430, 304($t8)
    sv.q       C500, 320($t8)
    sv.q       C510, 336($t8)
    sv.q       C520, 352($t8)
    sv.q       C530, 368($t8)
    sv.q       C600, 384($t8)
    sv.q       C610, 400($t8)
    sv.q       C620, 416($t8)
    sv.q       C630, 432($t8)
    sv.q       C700, 448($t8)
    sv.q       C710, 464($t8)
    sv.q       C720, 480($t8)
    sv.q       C730, 496($t8)
    mtc0 $v0, COP0_STATE_STATUS
    nop
    nop
    jr $t9
    addu $v0, $zr, $zr

sysmemStore8Words:
    sw $t0, 0($t8)
    sw $t1, 4($t8)
    sw $t2, 8($t8)
    sw $t3, 12($t8)
    sw $t4, 16($t8)
    sw $t5, 20($t8)
    sw $t6, 24($t8)
    sw $t7, 28($t8)
    jr $ra
    addiu $t8, $t8, 32

sysmemRestoreCpuContext:
    addu $at, $zr, $zr
    addu $v0, $zr, $zr
    addu $v1, $zr, $zr
    addu $a2, $zr, $zr
    addu $a3, $zr, $zr
    lw $s0, 0($a0)
    lw $s1, 4($a0)
    lw $s2, 8($a0)
    lw $s3, 12($a0)
    lw $s4, 16($a0)
    lw $s5, 20($a0)
    lw $s6, 24($a0)
    lw $s7, 28($a0)
    lw $k0, 32($a0)
    lw $k1, 36($a0)
    lw $gp, 40($a0)
    lw $sp, 44($a0)
    lw $fp, 48($a0)
    lw $t9, 52($a0)
    mfc0 $t0, COP0_STATE_STATUS
    lui $t1, 0x2000
    or $t1, $t0, $t1
    mtc0 $t1, COP0_STATE_STATUS
    nop
    nop
    lw $t0, 60($a0)
    ctc1 $t0, $31
    lwc1 $0, 64($a0)
    lwc1 $1, 68($a0)
    lwc1 $2, 72($a0)
    lwc1 $3, 76($a0)
    lwc1 $4, 80($a0)
    lwc1 $5, 84($a0)
    lwc1 $6, 88($a0)
    lwc1 $7, 92($a0)
    lwc1 $8, 96($a0)
    lwc1 $9, 100($a0)
    lwc1 $10, 104($a0)
    lwc1 $11, 108($a0)
    lwc1 $12, 112($a0)
    lwc1 $13, 116($a0)
    lwc1 $14, 120($a0)
    lwc1 $15, 124($a0)
    lwc1 $16, 128($a0)
    lwc1 $17, 132($a0)
    lwc1 $18, 136($a0)
    lwc1 $19, 140($a0)
    lwc1 $20, 144($a0)
    lwc1 $21, 148($a0)
    lwc1 $22, 152($a0)
    lwc1 $23, 156($a0)
    lwc1 $24, 160($a0)
    lwc1 $25, 164($a0)
    lwc1 $26, 168($a0)
    lwc1 $27, 172($a0)
    lwc1 $28, 176($a0)
    lwc1 $29, 180($a0)
    lwc1 $30, 184($a0)
    lwc1 $31, 188($a0)
    addiu $t8, $a0, 192
    bal sysmemLoad8Words
    nop
    bal sysmemLoad8Words
    nop
    mtc0 $t4, COP0_STATE_STATUS
    mtc0 $t5, COP0_STATE_CAUSE
    mtc0 $t6, COP0_STATE_EPC
    bal sysmemLoad8Words
    nop
    bal sysmemLoad8Words
    nop
    mtc0 $t1, COP0_STATE_EBASE
    mtc0 $t4, COP0_STATE_TAG_LO
    mtc0 $t5, COP0_STATE_TAG_HI
    mtc0 $t6, COP0_STATE_ERROR_PC
    bal sysmemLoad8Words
    nop
    ctc0 $t0, $0
    ctc0 $t1, $1
    ctc0 $t2, $2
    ctc0 $t3, $3
    ctc0 $t4, $4
    ctc0 $t5, $5
    ctc0 $t6, $6
    bal sysmemLoad8Words
    ctc0 $t7, $7
    ctc0 $t0, $8
    ctc0 $t1, $9
    ctc0 $t2, $10
    ctc0 $t3, $11
    ctc0 $t4, $12
    ctc0 $t5, $13
    ctc0 $t6, $14
    bal sysmemLoad8Words
    ctc0 $t7, $15
    ctc0 $t0, $16
    ctc0 $t1, $17
    ctc0 $t2, $18
    ctc0 $t3, $19
    ctc0 $t4, $20
    ctc0 $t5, $21
    ctc0 $t6, $22
    bal sysmemLoad8Words
    ctc0 $t7, $23
    ctc0 $t0, $24
    ctc0 $t1, $25
    ctc0 $t2, $26
    ctc0 $t3, $27
    ctc0 $t4, $28
    ctc0 $t5, $29
    ctc0 $t6, $30
    ctc0 $t7, $31
    mfc0 $v0, COP0_STATE_STATUS
    lui $t1, 0x4000
    or $t1, $v0, $t1
    mtc0 $t1, COP0_STATE_STATUS
    nop
    nop
    bal sysmemLoad8Words
    nop
    mtvc $t0, VFPU_PFXS
    mtvc $t1, VFPU_PFXT
    mtvc $t2, VFPU_PFXD
    mtvc $t3, VFPU_CC
    bal sysmemLoad8Words
    mtvc $t4, VFPU_INF4
    mtvc $t0, VFPU_RCX0
    mtvc $t1, VFPU_RCX1
    mtvc $t2, VFPU_RCX2
    mtvc $t3, VFPU_RCX3
    mtvc $t4, VFPU_RCX4
    mtvc $t5, VFPU_RCX5
    mtvc $t6, VFPU_RCX6
    mtvc $t7, VFPU_RCX7
    lv.q       C000, 0($t8)
    lv.q       C010, 16($t8)
    lv.q       C020, 32($t8)
    lv.q       C030, 48($t8)
    lv.q       C100, 64($t8)
    lv.q       C110, 80($t8)
    lv.q       C120, 96($t8)
    lv.q       C130, 112($t8)
    lv.q       C200, 128($t8)
    lv.q       C210, 144($t8)
    lv.q       C220, 160($t8)
    lv.q       C230, 176($t8)
    lv.q       C300, 192($t8)
    lv.q       C310, 208($t8)
    lv.q       C320, 224($t8)
    lv.q       C330, 240($t8)
    lv.q       C400, 256($t8)
    lv.q       C410, 272($t8)
    lv.q       C420, 288($t8)
    lv.q       C430, 304($t8)
    lv.q       C500, 320($t8)
    lv.q       C510, 336($t8)
    lv.q       C520, 352($t8)
    lv.q       C530, 368($t8)
    lv.q       C600, 384($t8)
    lv.q       C610, 400($t8)
    lv.q       C620, 416($t8)
    lv.q       C630, 432($t8)
    lv.q       C700, 448($t8)
    lv.q       C710, 464($t8)
    lv.q       C720, 480($t8)
    lv.q       C730, 496($t8)
    mtc0 $v0, COP0_STATE_STATUS
    nop
    nop
    addu $ra, $t9, $zr
    addu $t0, $zr, $zr
    addu $t1, $zr, $zr
    addu $t2, $zr, $zr
    addu $t3, $zr, $zr
    addu $t4, $zr, $zr
    addu $t5, $zr, $zr
    addu $t6, $zr, $zr
    addu $t7, $zr, $zr
    addu $t8, $zr, $zr
    addu $t9, $zr, $zr
    jr $ra
    addu $v0, $a1, $zr

sysmemLoad8Words:
    lw $t0, 0($t8)
    lw $t1, 4($t8)
    lw $t2, 8($t8)
    lw $t3, 12($t8)
    lw $t4, 16($t8)
    lw $t5, 20($t8)
    lw $t6, 24($t8)
    lw $t7, 28($t8)
    jr $ra
    addiu $t8, $t8, 32

    .globl sceSuspendForKernel_67B59042
    .size sysmemSaveCpuContext, .-sysmemSaveCpuContext
    .type sceSuspendForKernel_67B59042, @function
sceSuspendForKernel_67B59042:
    lui $a0, %hi(g_suspendCtx)
    addiu $a0, $a0, %lo(g_suspendCtx)
    j sysmemSaveCpuContext
    nop

    .globl sceSuspendForKernel_B2C9640B
    .size sceSuspendForKernel_67B59042, .-sceSuspendForKernel_67B59042
    .type sceSuspendForKernel_B2C9640B, @function
sceSuspendForKernel_B2C9640B:
    addu $a1, $a0, $zr
    lui $a0, %hi(g_suspendCtx)
    addiu $a0, $a0, %lo(g_suspendCtx)
    j sysmemRestoreCpuContext
    nop

    # ?
    .size sceSuspendForKernel_B2C9640B, .-sceSuspendForKernel_B2C9640B

    .type l1DcacheWritebackAllEret, @function
l1DcacheWritebackAllEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)
    j sceKernelL1DcacheWritebackAll
    nop

    # ?
    .size l1DcacheWritebackAllEret, .-l1DcacheWritebackAllEret

    .type dcacheWritebackAllEret, @function
dcacheWritebackAllEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelDcacheWritebackAll
    .size dcacheWritebackAllEret, .-dcacheWritebackAllEret
    .type sceKernelDcacheWritebackAll, @function
sceKernelDcacheWritebackAll:
    mfc0 $t8, $24
    andi $t8, $t8, 0x1
    bnez $t8, .Ls_d
    addu $at, $ra, $zr

    .globl sceKernelL1DcacheWritebackAll
    .size sceKernelDcacheWritebackAll, .-sceKernelDcacheWritebackAll
    .type sceKernelL1DcacheWritebackAll, @function
sceKernelL1DcacheWritebackAll:
    mfic $v0, $0
    mtic $zr, $0
    mfc0 $t0, COP0_STATE_CONFIG
    li $t1, 2048
    ext $t0, $t0, 6, 3
    sllv $t1, $t1, $t0
    addu $t0, $zr, $zr
    li $t6, -1

.Ls_a:
    cache 0x10, 0($t0)
    mfc0 $t2, COP0_STATE_TAG_LO
    mfc0 $t3, COP0_STATE_TAG_HI
    addu $t4, $t2, $zr
    xor $t5, $t3, $t6
    ext $t5, $t5, 0, 19
    movz $t2, $t3, $t5
    movz $t3, $t4, $t5
    ext $t4, $t2, 20, 1
    beqz $t4, .Ls_b
    ext $t4, $t3, 20, 1
    sll $t5, $t2, 13
    or $t5, $t5, $t0
    cache 0x1A, 0($t5)
    addu $t6, $t2, $zr

.Ls_b:
    beqz $t4, .Ls_c
    sll $t5, $t3, 13
    or $t5, $t5, $t0
    cache 0x1A, 0($t5)
    addu $t6, $t3, $zr

.Ls_c:
    mtic $v0, $0
    addiu $t0, $t0, 64
    mtic $zr, $0
    bne $t0, $t1, .Ls_a
    nop
    mtic $v0, $0
    addu $v0, $zr, $zr
    jr $ra
    sync

.Ls_d:
    jal sceKernelL1DcacheWritebackAll
    nop
    j sceKernelL2cacheWritebackAll
    addu $ra, $at, $zr

    # ?
    .size sceKernelL1DcacheWritebackAll, .-sceKernelL1DcacheWritebackAll

    .type l1DcacheWritebackInvalidateAllEret, @function
l1DcacheWritebackInvalidateAllEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)
    j sceKernelL1DcacheWritebackInvalidateAll
    nop

    # ?
    .size l1DcacheWritebackInvalidateAllEret, .-l1DcacheWritebackInvalidateAllEret

    .type dcacheWritebackInvalidateAllEret, @function
dcacheWritebackInvalidateAllEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelDcacheWritebackInvalidateAll
    .size dcacheWritebackInvalidateAllEret, .-dcacheWritebackInvalidateAllEret
    .type sceKernelDcacheWritebackInvalidateAll, @function
sceKernelDcacheWritebackInvalidateAll:
    mfc0 $t8, $24
    andi $t8, $t8, 0x1
    bnez $t8, .Ls_f
    addu $at, $ra, $zr

    .globl sceKernelL1DcacheWritebackInvalidateAll
    .size sceKernelDcacheWritebackInvalidateAll, .-sceKernelDcacheWritebackInvalidateAll
    .type sceKernelL1DcacheWritebackInvalidateAll, @function
sceKernelL1DcacheWritebackInvalidateAll:
    mfc0 $t0, COP0_STATE_CONFIG
    li $t1, 2048
    ext $t0, $t0, 6, 3
    sllv $t1, $t1, $t0
    addu $t0, $zr, $zr

.Ls_e:
    cache 0x14, 0($t0)
    addiu $t0, $t0, 64
    bne $t0, $t1, .Ls_e
    cache 0x14, -64($t0)
    addu $v0, $zr, $zr
    jr $ra
    sync

.Ls_f:
    jal sceKernelL1DcacheWritebackInvalidateAll
    nop
    j sceKernelL2cacheWritebackInvalidateAll
    addu $ra, $at, $zr

    # ?
    .size sceKernelL1DcacheWritebackInvalidateAll, .-sceKernelL1DcacheWritebackInvalidateAll

    .type l1DcacheInvalidateAllEret, @function
l1DcacheInvalidateAllEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)
    j sceKernelL1DcacheInvalidateAll
    nop

    # ?
    .size l1DcacheInvalidateAllEret, .-l1DcacheInvalidateAllEret

    .type dcacheInvalidateAllEret, @function
dcacheInvalidateAllEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelDcacheInvalidateAll
    .size dcacheInvalidateAllEret, .-dcacheInvalidateAllEret
    .type sceKernelDcacheInvalidateAll, @function
sceKernelDcacheInvalidateAll:
    mfc0 $t8, $24
    andi $t8, $t8, 0x1
    bnez $t8, .Ls_h
    addu $at, $ra, $zr

    .globl sceKernelL1DcacheInvalidateAll
    .size sceKernelDcacheInvalidateAll, .-sceKernelDcacheInvalidateAll
    .type sceKernelL1DcacheInvalidateAll, @function
sceKernelL1DcacheInvalidateAll:
    mfc0 $t0, COP0_STATE_CONFIG
    li $t1, 4096
    ext $t0, $t0, 6, 3
    sllv $t1, $t1, $t0
    mfic $v0, $0
    mtic $zr, $0
    mtc0 $zr, COP0_STATE_TAG_LO
    mtc0 $zr, COP0_STATE_TAG_HI
    addu $t0, $zr, $zr

.Ls_g:
    addiu $t0, $t0, 64
    cache 0x11, -64($t0)
    bne $t0, $t1, .Ls_g
    cache 0x13, -64($t0)
    mtic $v0, $0
    jr $ra
    addu $v0, $zr, $zr

.Ls_h:
    jal sceKernelL1DcacheInvalidateAll
    nop
    j sceKernelL2cacheInvalidateAll
    addu $ra, $at, $zr

    # ?
    .size sceKernelL1DcacheInvalidateAll, .-sceKernelL1DcacheInvalidateAll

    .type l1DcacheWritebackRangeEret, @function
l1DcacheWritebackRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)
    j sceKernelL1DcacheWritebackRange
    nop

    # ?
    .size l1DcacheWritebackRangeEret, .-l1DcacheWritebackRangeEret

    .type dcacheWritebackRangeEret, @function
dcacheWritebackRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelDcacheWritebackRange
    .size dcacheWritebackRangeEret, .-dcacheWritebackRangeEret
    .type sceKernelDcacheWritebackRange, @function
sceKernelDcacheWritebackRange:
    mfc0 $t8, $24
    andi $t8, $t8, 0x1
    bnez $t8, .Ls_p
    addu $at, $ra, $zr

    .globl sceKernelL1DcacheWritebackRange
    .size sceKernelDcacheWritebackRange, .-sceKernelDcacheWritebackRange
    .type sceKernelL1DcacheWritebackRange, @function
sceKernelL1DcacheWritebackRange:
    srl $v0, $a1, 16
    bnez $v0, .Ls_l
    addu $a2, $a0, $a1
    beq $a2, $a0, .Ls_j
    addiu $a2, $a2, 63
    ins $a2, $a0, 0, 6

.Ls_i:
    addiu $a0, $a0, 64
    bne $a0, $a2, .Ls_i
    cache 0x1A, -64($a0)

.Ls_j:
    jr $ra
    sync

.Ls_k:
    lui $v0, 0x8000
    jr $ra
    ori $v0, $v0, 0x104

.Ls_l:
    ext $v0, $k1, 19, 3
    negu $v0, $v0
    and $v0, $v0, $a1
    bltz $v0, .Ls_k
    li $v0, -8192
    ins $a0, $zr, 0, 6
    subu $a1, $a1, $a0
    mfic $v1, $0
    addu $t0, $v0, $zr

.Ls_m:
    mtic $zr, $0
    addiu $v0, $v0, 64
    cache 0x10, 8128($v0)
    mfc0 $t2, COP0_STATE_TAG_LO
    mfc0 $t3, COP0_STATE_TAG_HI
    mtic $v1, $0
    ins $t0, $t2, 13, 19
    subu $t4, $t0, $a0
    sltu $t4, $t4, $a1
    bnezl $t4, .Ls_n
    cache 0x1A, 0($t0)

.Ls_n:
    ins $t0, $t3, 13, 19
    subu $t4, $t0, $a0
    sltu $t4, $t4, $a1
    bnezl $t4, .Ls_o
    cache 0x1A, 0($t0)

.Ls_o:
    bnez $v0, .Ls_m
    addu $t0, $v0, $zr
    jr $ra
    sync

.Ls_p:
    addu $t6, $a0, $zr
    jal sceKernelL1DcacheWritebackRange
    addu $t7, $a1, $zr
    bltz $v0, .Ls_k
    addu $ra, $at, $zr
    addu $a0, $t6, $zr
    j sceKernelL2cacheWritebackRange
    addu $a1, $t7, $zr

    # ?
    .size sceKernelL1DcacheWritebackRange, .-sceKernelL1DcacheWritebackRange

    .type l1DcacheWritebackInvalidateRangeEret, @function
l1DcacheWritebackInvalidateRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)
    j sceKernelL1DcacheWritebackInvalidateRange
    nop

    # ?
    .size l1DcacheWritebackInvalidateRangeEret, .-l1DcacheWritebackInvalidateRangeEret

    .type dcacheWritebackInvalidateRangeEret, @function
dcacheWritebackInvalidateRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelDcacheWritebackInvalidateRange
    .size dcacheWritebackInvalidateRangeEret, .-dcacheWritebackInvalidateRangeEret
    .type sceKernelDcacheWritebackInvalidateRange, @function
sceKernelDcacheWritebackInvalidateRange:
    mfc0 $t8, $24
    andi $t8, $t8, 0x1
    bnez $t8, .Ls_x
    addu $at, $ra, $zr

    .globl sceKernelL1DcacheWritebackInvalidateRange
    .size sceKernelDcacheWritebackInvalidateRange, .-sceKernelDcacheWritebackInvalidateRange
    .type sceKernelL1DcacheWritebackInvalidateRange, @function
sceKernelL1DcacheWritebackInvalidateRange:
    srl $v0, $a1, 16
    bnez $v0, .Ls_t
    addu $a2, $a0, $a1
    beq $a2, $a0, .Ls_r
    addiu $a2, $a2, 63
    ins $a2, $a0, 0, 6

.Ls_q:
    addiu $a0, $a0, 64
    bne $a0, $a2, .Ls_q
    cache 0x1B, -64($a0)

.Ls_r:
    jr $ra
    sync

.Ls_s:
    lui $v0, 0x8000
    jr $ra
    ori $v0, $v0, 0x104

.Ls_t:
    ext $v0, $k1, 19, 3
    negu $v0, $v0
    and $v0, $v0, $a1
    bltz $v0, .Ls_s
    li $v0, -8192
    ins $a0, $zr, 0, 6
    subu $a1, $a1, $a0
    mfic $v1, $0
    addu $t0, $v0, $zr

.Ls_u:
    mtic $zr, $0
    addiu $v0, $v0, 64
    cache 0x10, 8128($v0)
    mfc0 $t2, COP0_STATE_TAG_LO
    mfc0 $t3, COP0_STATE_TAG_HI
    mtic $v1, $0
    ins $t0, $t2, 13, 19
    subu $t4, $t0, $a0
    sltu $t4, $t4, $a1
    bnezl $t4, .Ls_v
    cache 0x1B, 0($t0)

.Ls_v:
    ins $t0, $t3, 13, 19
    subu $t4, $t0, $a0
    sltu $t4, $t4, $a1
    bnezl $t4, .Ls_w
    cache 0x1B, 0($t0)

.Ls_w:
    bnez $v0, .Ls_u
    addu $t0, $v0, $zr
    jr $ra
    sync

.Ls_x:
    addu $t6, $a0, $zr
    jal sceKernelL1DcacheWritebackInvalidateRange
    addu $t7, $a1, $zr
    bltz $v0, .Ls_s
    addu $ra, $at, $zr
    addu $a0, $t6, $zr
    j sceKernelL2cacheWritebackInvalidateRange
    addu $a1, $t7, $zr

    # ?
    .size sceKernelL1DcacheWritebackInvalidateRange, .-sceKernelL1DcacheWritebackInvalidateRange

    .type l1DcacheInvalidateRangeEret, @function
l1DcacheInvalidateRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)
    j sceKernelL1DcacheInvalidateRange
    nop

    # ?
    .size l1DcacheInvalidateRangeEret, .-l1DcacheInvalidateRangeEret

    .type dcacheInvalidateRangeEret, @function
dcacheInvalidateRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelDcacheInvalidateRange
    .size dcacheInvalidateRangeEret, .-dcacheInvalidateRangeEret
    .type sceKernelDcacheInvalidateRange, @function
sceKernelDcacheInvalidateRange:
    mfc0 $t8, $24
    andi $t8, $t8, 0x1
    bnez $t8, .Ls_ag
    addu $at, $ra, $zr

    .globl sceKernelL1DcacheInvalidateRange
    .size sceKernelDcacheInvalidateRange, .-sceKernelDcacheInvalidateRange
    .type sceKernelL1DcacheInvalidateRange, @function
sceKernelL1DcacheInvalidateRange:
    beqz $a1, .Ls_z
    or $t0, $a0, $a1
    andi $t0, $t0, 0x3F
    bnez $t0, .Ls_aa
    li $t0, 0xC000
    sltu $t0, $a1, $t0
    beqz $t0, .Ls_ab
    addu $a2, $a0, $a1

.Ls_y:
    addiu $a0, $a0, 64
    bne $a0, $a2, .Ls_y
    cache 0x19, -64($a0)

.Ls_z:
    jr $ra
    addu $v0, $zr, $zr

.Ls_aa:
    lui $v0, 0x8002
    jr $ra
    ori $v0, $v0, 0x44C

.Ls_ab:
    bltzl $a1, .Ls_ac
    break

.Ls_ac:
    li $v0, 8128
    mfic $v1, $0

.Ls_ad:
    mtic $zr, $0
    addu $t0, $v0, $zr
    cache 0x10, 0($v0)
    mfc0 $t2, COP0_STATE_TAG_LO
    mfc0 $t3, COP0_STATE_TAG_HI
    mtic $v1, $0
    ins $t0, $t2, 13, 19
    subu $t4, $t0, $a0
    sltu $t4, $t4, $a1
    bnezl $t4, .Ls_ae
    cache 0x19, 0($t0)

.Ls_ae:
    ins $t0, $t3, 13, 19
    subu $t4, $t0, $a0
    sltu $t4, $t4, $a1
    bnezl $t4, .Ls_af
    cache 0x19, 0($t0)

.Ls_af:
    bnez $v0, .Ls_ad
    addiu $v0, $v0, -64
    jr $ra
    addu $v0, $zr, $zr

.Ls_ag:
    addu $t6, $a0, $zr
    jal sceKernelL1DcacheInvalidateRange
    addu $t7, $a1, $zr
    bltz $v0, .Ls_aa
    addu $ra, $at, $zr
    addu $a0, $t6, $zr
    j sceKernelL2cacheInvalidateRange
    addu $a1, $t7, $zr

    # ?
    .size sceKernelL1DcacheInvalidateRange, .-sceKernelL1DcacheInvalidateRange

    .type dcachePurgeRangeEret, @function
dcachePurgeRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelDcachePurgeRange
    .size dcachePurgeRangeEret, .-dcachePurgeRangeEret
    .type sceKernelDcachePurgeRange, @function
sceKernelDcachePurgeRange:
    mfc0 $t0, COP0_STATE_CONFIG
    beqz $a1, .Ls_ai
    li $t1, 4096
    ext $t0, $t0, 6, 3
    sllv $t1, $t1, $t0
    or $t0, $a0, $a1
    andi $t0, $t0, 0x3F
    bnez $t0, .Ls_aj
    sltu $t0, $a1, $t1
    addu $a1, $a0, $a1
    beqzl $t0, .Ls_ak
    addiu $a1, $a1, -1
    mfic $v0, $0
    mtic $zr, $0

.Ls_ah:
    srl $t0, $a0, 13
    cache 0x10, 0($a0)
    sll $t0, $t0, 13
    mfc0 $t1, COP0_STATE_TAG_LO
    mfc0 $t2, COP0_STATE_TAG_HI
    ext $t3, $t1, 20, 1
    ext $t4, $t2, 20, 1
    sll $t5, $t1, 13
    sll $t6, $t2, 13
    xor $t5, $t5, $t0
    xor $t6, $t6, $t0
    movz $t3, $zr, $t5
    movz $t4, $zr, $t6
    ext $t1, $t3, 20, 1
    ext $t2, $t4, 20, 1
    mtc0 $t1, COP0_STATE_TAG_LO
    mtc0 $t2, COP0_STATE_TAG_HI
    cache 0x11, 0($a0)
    mtic $v0, $0
    addiu $a0, $a0, 64
    mtic $zr, $0
    bne $a0, $a1, .Ls_ah
    nop
    mtic $v0, $0

.Ls_ai:
    jr $ra
    addu $v0, $zr, $zr

.Ls_aj:
    lui $v0, 0x8002
    jr $ra
    ori $v0, $v0, 0x44C

.Ls_ak:
    mfic $v0, $0
    mtic $zr, $0
    srl $t1, $t1, 1
    addu $t0, $zr, $zr

.Ls_al:
    cache 0x10, 0($t0)
    mfc0 $t2, COP0_STATE_TAG_LO
    ext $t3, $t2, 19, 2
    xori $t3, $t3, 0x3
    bnez $t3, .Ls_am
    sll $t3, $t2, 13
    or $t3, $t3, $t0
    subu $t4, $a1, $t3
    subu $t5, $t3, $a0
    or $t4, $t4, $t5
    bltz $t4, .Ls_am
    ins $t2, $zr, 20, 1
    mtc0 $t2, COP0_STATE_TAG_LO

.Ls_am:
    mfc0 $t2, COP0_STATE_TAG_HI
    ext $t3, $t2, 19, 2
    xori $t3, $t3, 0x3
    beqz $t3, .Ls_an
    sll $t3, $t2, 13
    or $t3, $t3, $t0
    subu $t4, $a1, $t3
    subu $t5, $t3, $a0
    or $t4, $t4, $t5
    bltz $t4, .Ls_an
    ins $t2, $zr, 20, 1
    mtc0 $t2, COP0_STATE_TAG_HI

.Ls_an:
    cache 0x11, 0($t0)
    mtic $v0, $0
    addiu $t0, $t0, 64
    mtic $zr, $0
    bne $t0, $t1, .Ls_al
    nop
    mtic $v0, $0
    addu $v0, $zr, $zr
    jr $ra
    sync

    # ?
    .size sceKernelDcachePurgeRange, .-sceKernelDcachePurgeRange

    .type l1DcacheProbeEret, @function
l1DcacheProbeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)
    j sceKernelL1DcacheProbe
    nop

    # ?
    .size l1DcacheProbeEret, .-l1DcacheProbeEret

    .type dcacheProbeEret, @function
dcacheProbeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelDcacheProbe
    .size dcacheProbeEret, .-dcacheProbeEret
    .type sceKernelDcacheProbe, @function
sceKernelDcacheProbe:
    mfc0 $t8, $24
    andi $t8, $t8, 0x1
    bnez $t8, .Ls_ao
    addu $at, $ra, $zr

    .globl sceKernelL1DcacheProbe
    .size sceKernelDcacheProbe, .-sceKernelDcacheProbe
    .type sceKernelL1DcacheProbe, @function
sceKernelL1DcacheProbe:
    mfic $v0, $0
    mtic $zr, $0
    srl $t0, $a0, 13
    cache 0x10, 0($a0)
    sll $t0, $t0, 13
    mfc0 $t1, COP0_STATE_TAG_LO
    mfc0 $t2, COP0_STATE_TAG_HI
    mtic $v0, $0
    ext $t3, $t1, 19, 2
    ext $t4, $t2, 19, 2
    sll $t1, $t1, 13
    sll $t2, $t2, 13
    xor $t1, $t1, $t0
    xor $t2, $t2, $t0
    movn $t3, $zr, $t1
    movn $t4, $zr, $t2
    or $v0, $t3, $t4
    ext $t0, $v0, 1, 1
    jr $ra
    xor $v0, $v0, $t0

.Ls_ao:
    jal sceKernelL1DcacheProbe
    nop
    jal sceKernelL2cacheProbe
    addu $v1, $v0, $zr
    addu $ra, $at, $zr
    jr $ra
    max $v0, $v0, $v1

    # ?
    .size sceKernelL1DcacheProbe, .-sceKernelL1DcacheProbe

    .type dcacheReadTagEret, @function
dcacheReadTagEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelDcacheReadTagForUser
    .size dcacheReadTagEret, .-dcacheReadTagEret
    .type sceKernelDcacheReadTagForUser, @function
sceKernelDcacheReadTagForUser:
    mfic $t0, $0
    mtic $zr, $0
    cache 0x10, 0($a0)
    mfc0 $v0, COP0_STATE_TAG_LO
    mfc0 $v1, COP0_STATE_TAG_HI
    ext $at, $v0, 18, 1
    movn $v0, $zr, $at
    ext $at, $v1, 18, 1
    movn $v1, $zr, $at
    cache 0x12, 0($a0)
    mfc0 $at, COP0_STATE_TAG_LO
    mtic $t0, $0
    jr $ra
    nop

    .globl sceKernelDcacheReadTag
    .size sceKernelDcacheReadTagForUser, .-sceKernelDcacheReadTagForUser
    .type sceKernelDcacheReadTag, @function
sceKernelDcacheReadTag:
    mfic $t0, $0
    mtic $zr, $0
    cache 0x10, 0($a0)
    mfc0 $v0, COP0_STATE_TAG_LO
    mfc0 $v1, COP0_STATE_TAG_HI
    cache 0x12, 0($a0)
    mfc0 $at, COP0_STATE_TAG_LO
    mtic $t0, $0
    jr $ra
    nop

    # ?
    .size sceKernelDcacheReadTag, .-sceKernelDcacheReadTag

    .type l1DcacheProbeRangeEret, @function
l1DcacheProbeRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)
    j sceKernelL1DcacheProbeRange
    nop

    # ?
    .size l1DcacheProbeRangeEret, .-l1DcacheProbeRangeEret

    .type dcacheProbeRangeEret, @function
dcacheProbeRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelDcacheProbeRange
    .size dcacheProbeRangeEret, .-dcacheProbeRangeEret
    .type sceKernelDcacheProbeRange, @function
sceKernelDcacheProbeRange:
    mfc0 $t8, $24
    andi $t8, $t8, 0x1
    bnez $t8, .Ls_av
    addu $at, $ra, $zr

    .globl sceKernelL1DcacheProbeRange
    .size sceKernelDcacheProbeRange, .-sceKernelDcacheProbeRange
    .type sceKernelL1DcacheProbeRange, @function
sceKernelL1DcacheProbeRange:
    mfc0 $t0, COP0_STATE_CONFIG
    beqz $a1, .Ls_at
    addu $v0, $zr, $zr
    li $t1, 4096
    ext $t0, $t0, 6, 3
    sllv $t1, $t1, $t0
    addu $a1, $a0, $a1
    addiu $a1, $a1, -1
    ins $a0, $zr, 0, 6
    ori $a1, $a1, 0x3F
    mfic $v1, $0
    mtic $zr, $0
    srl $t1, $t1, 1
    addu $t0, $zr, $zr

.Ls_ap:
    cache 0x10, 0($t0)
    mfc0 $t2, COP0_STATE_TAG_LO
    ext $t3, $t2, 19, 1
    beqz $t3, .Ls_aq
    sll $t3, $t2, 13
    or $t3, $t3, $t0
    subu $t4, $a1, $t3
    subu $t5, $t3, $a0
    or $t4, $t4, $t5
    bltz $t4, .Ls_as
    ext $t3, $t2, 20, 1
    bnez $t3, .Ls_au
    nop

.Ls_aq:
    mfc0 $t2, COP0_STATE_TAG_HI
    ext $t3, $t2, 19, 1
    beqz $t3, .Ls_ar
    sll $t3, $t2, 13
    or $t3, $t3, $t0
    subu $t4, $a1, $t3
    subu $t5, $t3, $a0
    or $t4, $t4, $t5
    bltz $t4, .Ls_as
    ext $t3, $t2, 20, 1
    bnez $t3, .Ls_au
    nop

.Ls_ar:
    mtic $v1, $0
    addiu $t0, $t0, 64
    mtic $zr, $0
    bne $t0, $t1, .Ls_ap
    nop
    li $v0, 1

.Ls_as:
    mtic $v1, $0

.Ls_at:
    jr $ra
    nop

.Ls_au:
    b .Ls_as
    li $v0, 2

.Ls_av:
    addu $t6, $a0, $zr
    jal sceKernelL1DcacheProbeRange
    addu $t7, $a1, $zr
    addu $a0, $t6, $zr
    addu $a1, $t7, $zr
    jal sceKernelL2cacheProbeRange
    addu $v1, $v0, $zr
    addu $ra, $at, $zr
    jr $ra
    max $v0, $v0, $v1

    # ?
    .size sceKernelL1DcacheProbeRange, .-sceKernelL1DcacheProbeRange

    .type l1IcacheInvalidateAllEret, @function
l1IcacheInvalidateAllEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)
    j sceKernelL1IcacheInvalidateAll
    nop

    # ?
    .size l1IcacheInvalidateAllEret, .-l1IcacheInvalidateAllEret

    .type icacheInvalidateAllEret, @function
icacheInvalidateAllEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelIcacheInvalidateAll
    .size icacheInvalidateAllEret, .-icacheInvalidateAllEret
    .type sceKernelIcacheInvalidateAll, @function
sceKernelIcacheInvalidateAll:
    mfc0 $t8, $24
    andi $t8, $t8, 0x1
    bnez $t8, .Ls_ax
    addu $at, $ra, $zr

    .globl sceKernelL1IcacheInvalidateAll
    .size sceKernelIcacheInvalidateAll, .-sceKernelIcacheInvalidateAll
    .type sceKernelL1IcacheInvalidateAll, @function
sceKernelL1IcacheInvalidateAll:
    mfc0 $t0, COP0_STATE_CONFIG
    li $t1, 4096
    ext $t0, $t0, 9, 3
    sllv $t1, $t1, $t0
    mfic $v0, $0
    mtic $zr, $0
    mtc0 $zr, COP0_STATE_TAG_LO
    mtc0 $zr, COP0_STATE_TAG_HI
    addu $t0, $zr, $zr

.Ls_aw:
    addiu $t0, $t0, 64
    cache 0x1, -64($t0)
    bne $t0, $t1, .Ls_aw
    cache 0x3, -64($t0)
    mtic $v0, $0
    jr $ra
    addu $v0, $zr, $zr

.Ls_ax:
    jal sceKernelL2cacheWritebackInvalidateAll
    nop
    j sceKernelL1IcacheInvalidateAll
    addu $ra, $at, $zr

    # ?
    .size sceKernelL1IcacheInvalidateAll, .-sceKernelL1IcacheInvalidateAll

    .type l1IcacheInvalidateRangeEret, @function
l1IcacheInvalidateRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)
    j sceKernelL1IcacheInvalidateRange
    nop

    # ?
    .size l1IcacheInvalidateRangeEret, .-l1IcacheInvalidateRangeEret

    .type icacheInvalidateRangeEret, @function
icacheInvalidateRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelIcacheInvalidateRange
    .size icacheInvalidateRangeEret, .-icacheInvalidateRangeEret
    .type sceKernelIcacheInvalidateRange, @function
sceKernelIcacheInvalidateRange:
    mfc0 $t8, $24
    andi $t8, $t8, 0x1
    bnez $t8, .Ls_be
    addu $at, $ra, $zr

    .globl sceKernelL1IcacheInvalidateRange
    .size sceKernelIcacheInvalidateRange, .-sceKernelIcacheInvalidateRange
    .type sceKernelL1IcacheInvalidateRange, @function
sceKernelL1IcacheInvalidateRange:
    mfc0 $t0, COP0_STATE_CONFIG
    beqz $a1, .Ls_az
    li $t1, 4096
    ext $t0, $t0, 9, 3
    sllv $t1, $t1, $t0
    sltu $t0, $a1, $t1
    addu $a1, $a0, $a1
    addiu $a1, $a1, -1
    ins $a0, $zr, 0, 6
    beqz $t0, .Ls_ba
    ori $a1, $a1, 0x3F
    addiu $a1, $a1, 1

.Ls_ay:
    addiu $a0, $a0, 64
    bne $a0, $a1, .Ls_ay
    cache 0x8, -64($a0)

.Ls_az:
    jr $ra
    addu $v0, $zr, $zr

.Ls_ba:
    mfic $v0, $0
    mtic $zr, $0
    srl $t1, $t1, 1
    addu $t0, $zr, $zr

.Ls_bb:
    cache 0x0, 0($t0)
    mfc0 $t2, COP0_STATE_TAG_LO
    ext $t3, $t2, 20, 1
    beqz $t3, .Ls_bc
    sll $t2, $t2, 13
    or $t2, $t2, $t0
    subu $t4, $a1, $t2
    subu $t5, $t2, $a0
    or $t4, $t4, $t5
    bltz $t4, .Ls_bc
    nop
    cache 0x8, 0($t2)

.Ls_bc:
    mfc0 $t2, COP0_STATE_TAG_HI
    ext $t3, $t2, 20, 1
    beqz $t3, .Ls_bd
    sll $t2, $t2, 13
    or $t2, $t2, $t0
    subu $t4, $a1, $t2
    subu $t5, $t2, $a0
    or $t4, $t4, $t5
    bltz $t4, .Ls_bd
    nop
    cache 0x8, 0($t2)

.Ls_bd:
    mtic $v0, $0
    addiu $t0, $t0, 64
    mtic $zr, $0
    bne $t0, $t1, .Ls_bb
    nop
    mtic $v0, $0
    jr $ra
    addu $v0, $zr, $zr

.Ls_be:
    addu $a2, $a0, $zr
    jal sceKernelL2cacheWritebackInvalidateRange
    addu $a3, $a1, $zr
    addu $a0, $a2, $zr
    addu $a1, $a3, $zr
    j sceKernelL1IcacheInvalidateRange
    addu $ra, $at, $zr

    # ?
    .size sceKernelL1IcacheInvalidateRange, .-sceKernelL1IcacheInvalidateRange

    .type l1IcacheProbeEret, @function
l1IcacheProbeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)
    j sceKernelL1IcacheProbe
    nop

    # ?
    .size l1IcacheProbeEret, .-l1IcacheProbeEret

    .type icacheProbeEret, @function
icacheProbeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)
    lui $t0, 0xA000
    or $ra, $t0, $ra

    .globl sceKernelIcacheProbe
    .size icacheProbeEret, .-icacheProbeEret
    .type sceKernelIcacheProbe, @function
sceKernelIcacheProbe:
    mfc0 $t8, $24
    andi $t8, $t8, 0x1
    bnez $t8, .Ls_bf
    addu $at, $ra, $zr

    .globl sceKernelL1IcacheProbe
    .size sceKernelIcacheProbe, .-sceKernelIcacheProbe
    .type sceKernelL1IcacheProbe, @function
sceKernelL1IcacheProbe:
    mfic $v0, $0
    mtic $zr, $0
    nop
    cache 0x0, 0($a0)
    addu $t5, $a0, $zr
    ins $t5, $zr, 0, 13
    mfc0 $t1, COP0_STATE_TAG_LO
    mfc0 $t2, COP0_STATE_TAG_HI
    mtic $v0, $0
    ext $t3, $t1, 19, 1
    ext $t4, $t2, 19, 1
    sll $t1, $t1, 13
    sll $t2, $t2, 13
    xor $t1, $t1, $t5
    xor $t2, $t2, $t5
    movn $t3, $zr, $t1
    movn $t4, $zr, $t2
    jr $ra
    or $v0, $t3, $t4

.Ls_bf:
    jal sceKernelL1IcacheProbe
    nop
    jal sceKernelL2cacheProbe
    addu $v1, $v0, $zr
    addu $ra, $at, $zr
    jr $ra
    or $v0, $v0, $v1

    # ?
    .size sceKernelL1IcacheProbe, .-sceKernelL1IcacheProbe

    .type icacheReadTagEret, @function
icacheReadTagEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(dcacheEretReturn)
    addiu $ra, $ra, %lo(dcacheEretReturn)

    .globl sceKernelIcacheReadTagForUser
    .size icacheReadTagEret, .-icacheReadTagEret
    .type sceKernelIcacheReadTagForUser, @function
sceKernelIcacheReadTagForUser:
    mfic $t0, $0
    mtic $zr, $0
    cache 0x0, 0($a0)
    mfc0 $v0, COP0_STATE_TAG_LO
    mfc0 $v1, COP0_STATE_TAG_HI
    ext $at, $v0, 18, 1
    movn $v0, $zr, $at
    ext $at, $v1, 18, 1
    movn $v1, $zr, $at
    cache 0x2, 0($a0)
    mfc0 $at, COP0_STATE_TAG_LO
    mtic $t0, $0
    jr $ra
    nop

    .globl sceKernelIcacheReadTag
    .size sceKernelIcacheReadTagForUser, .-sceKernelIcacheReadTagForUser
    .type sceKernelIcacheReadTag, @function
sceKernelIcacheReadTag:
    mfic $t0, $0
    mtic $zr, $0
    cache 0x0, 0($a0)
    mfc0 $v0, COP0_STATE_TAG_LO
    mfc0 $v1, COP0_STATE_TAG_HI
    cache 0x2, 0($a0)
    mfc0 $at, COP0_STATE_TAG_LO
    mtic $t0, $0
    jr $ra
    nop

    .size sceKernelIcacheReadTag, .-sceKernelIcacheReadTag
    .type dcacheEretReturn, @function
dcacheEretReturn:
    addu $ra, $t9, $zr
    eret

    # ?
    addu $t9, $ra, $zr
    lui $ra, %hi(l2cacheEretReturn)
    addiu $ra, $ra, %lo(l2cacheEretReturn)

    .globl sceKernelL2cacheWritebackAll
    .size dcacheEretReturn, .-dcacheEretReturn
    .type sceKernelL2cacheWritebackAll, @function
sceKernelL2cacheWritebackAll:
    lui $t0, 0xA7F0
    lui $t1, 0xA000
    ori $t1, $t1, 0x3F
    lui $t2, 0xA7F8
    lui $t3, 0xFFF
    ori $t3, $t3, 0xFFC0
    li $t4, 8

.Ls_bg:
    li $t5, 4

.Ls_bh:
    lw $t6, 0($t2)
    and $t6, $t6, $t3
    or $t6, $t6, $t1
    sw $t6, 0($t0)
    addiu $t5, $t5, -1
    bnez $t5, .Ls_bh
    addiu $t2, $t2, 2048
    addiu $t4, $t4, -1
    bnez $t4, .Ls_bg
    addiu $t2, $t2, -8188
    lui $t1, 0x6000
    ori $t1, $t1, 0x3F
    lui $t2, 0x6000
    ori $t2, $t2, 0x103F
    lui $t3, 0x6000
    ori $t3, $t3, 0x203F
    lui $t4, 0x6000
    ori $t4, $t4, 0x303F
    li $t5, 8

.Ls_bi:
    sw $t1, 0($t0)
    sw $t2, 0($t0)
    sw $t3, 0($t0)
    sw $t4, 0($t0)
    addiu $t5, $t5, -1
    bnez $t5, .Ls_bi
    nop
    lw $zr, 0($t0)
    jr $ra
    addu $v0, $zr, $zr

    # ?
    .size sceKernelL2cacheWritebackAll, .-sceKernelL2cacheWritebackAll

    .type l2cacheWritebackInvalidateAllEret, @function
l2cacheWritebackInvalidateAllEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(l2cacheEretReturn)
    addiu $ra, $ra, %lo(l2cacheEretReturn)

    .globl sceKernelL2cacheWritebackInvalidateAll
    .size l2cacheWritebackInvalidateAllEret, .-l2cacheWritebackInvalidateAllEret
    .type sceKernelL2cacheWritebackInvalidateAll, @function
sceKernelL2cacheWritebackInvalidateAll:
    lui $t0, 0xA7F0
    lui $t1, 0xB000
    ori $t1, $t1, 0x3F
    lui $t2, 0xA7F8
    lui $t3, 0xFFF
    ori $t3, $t3, 0xFFC0
    li $t4, 8

.Ls_bj:
    li $t5, 4

.Ls_bk:
    lw $t6, 0($t2)
    and $t6, $t6, $t3
    or $t6, $t6, $t1
    sw $t6, 0($t0)
    addiu $t5, $t5, -1
    bnez $t5, .Ls_bk
    addiu $t2, $t2, 2048
    addiu $t4, $t4, -1
    bnez $t4, .Ls_bj
    addiu $t2, $t2, -8188
    lui $t1, 0x7000
    ori $t1, $t1, 0x3F
    lui $t2, 0x7000
    ori $t2, $t2, 0x103F
    lui $t3, 0x7000
    ori $t3, $t3, 0x203F
    lui $t4, 0x7000
    ori $t4, $t4, 0x303F
    li $t5, 8

.Ls_bl:
    sw $t1, 0($t0)
    sw $t2, 0($t0)
    sw $t3, 0($t0)
    sw $t4, 0($t0)
    addiu $t5, $t5, -1
    bnez $t5, .Ls_bl
    nop
    lw $zr, 0($t0)
    jr $ra
    addu $v0, $zr, $zr

    # ?
    .size sceKernelL2cacheWritebackInvalidateAll, .-sceKernelL2cacheWritebackInvalidateAll

    .type l2cacheInvalidateAllEret, @function
l2cacheInvalidateAllEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(l2cacheEretReturn)
    addiu $ra, $ra, %lo(l2cacheEretReturn)

    .globl sceKernelL2cacheInvalidateAll
    .size l2cacheInvalidateAllEret, .-l2cacheInvalidateAllEret
    .type sceKernelL2cacheInvalidateAll, @function
sceKernelL2cacheInvalidateAll:
    lui $t0, 0xA7F0
    lui $t1, 0x5000
    ori $t1, $t1, 0x3F
    lui $t2, 0x5000
    ori $t2, $t2, 0x103F
    lui $t3, 0x5000
    ori $t3, $t3, 0x203F
    lui $t4, 0x5000
    ori $t4, $t4, 0x303F
    li $t5, 8

.Ls_bm:
    sw $t1, 0($t0)
    sw $t2, 0($t0)
    sw $t3, 0($t0)
    sw $t4, 0($t0)
    addiu $t5, $t5, -1
    bnez $t5, .Ls_bm
    nop
    lw $zr, 0($t0)
    jr $ra
    addu $v0, $zr, $zr

    # ?
    .size sceKernelL2cacheInvalidateAll, .-sceKernelL2cacheInvalidateAll

    .type l2cacheWritebackRangeEret, @function
l2cacheWritebackRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(l2cacheEretReturn)
    addiu $ra, $ra, %lo(l2cacheEretReturn)

    .globl sceKernelL2cacheWritebackRange
    .size l2cacheWritebackRangeEret, .-l2cacheWritebackRangeEret
    .type sceKernelL2cacheWritebackRange, @function
sceKernelL2cacheWritebackRange:
    lui $v0, 0x2000
    lui $v1, 0xA000
    j .Ls_bo
    nop

    # ?
    .size sceKernelL2cacheWritebackRange, .-sceKernelL2cacheWritebackRange

    .type l2cacheWritebackInvalidateRangeEret, @function
l2cacheWritebackInvalidateRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(l2cacheEretReturn)
    addiu $ra, $ra, %lo(l2cacheEretReturn)

    .globl sceKernelL2cacheWritebackInvalidateRange
    .size l2cacheWritebackInvalidateRangeEret, .-l2cacheWritebackInvalidateRangeEret
    .type sceKernelL2cacheWritebackInvalidateRange, @function
sceKernelL2cacheWritebackInvalidateRange:
    lui $v0, 0x3000
    lui $v1, 0xB000
    j .Ls_bo
    nop

    # ?
    .size sceKernelL2cacheWritebackInvalidateRange, .-sceKernelL2cacheWritebackInvalidateRange

    .type l2cacheInvalidateRangeEret, @function
l2cacheInvalidateRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(l2cacheEretReturn)
    addiu $ra, $ra, %lo(l2cacheEretReturn)

    .globl sceKernelL2cacheInvalidateRange
    .size l2cacheInvalidateRangeEret, .-l2cacheInvalidateRangeEret
    .type sceKernelL2cacheInvalidateRange, @function
sceKernelL2cacheInvalidateRange:
    or $t0, $a0, $a1
    andi $t0, $t0, 0x3F
    bnez $t0, .Ls_bn
    nop
    lui $v0, 0x1000
    lui $v1, 0x9000
    j .Ls_bo
    nop

.Ls_bn:
    lui $v0, 0x8002
    jr $ra
    ori $v0, $v0, 0x44C

.Ls_bo:
    li $t0, 19532
    lui $t1, 0x4C
    ori $t1, $t1, 0x4C
    slt $t2, $zr, $a0
    movn $t0, $t1, $t2
    ext $t1, $a0, 26, 5
    srlv $t0, $t0, $t1
    andi $t0, $t0, 0x1
    beqz $t0, .Ls_bw
    addu $t0, $a0, $zr
    beqz $a1, .Ls_bw
    addu $t1, $a0, $a1
    ins $t0, $zr, 27, 5
    ins $t1, $zr, 27, 5
    ins $t0, $t2, 27, 1
    ins $t1, $t2, 27, 1
    lui $t8, 0xA7F0
    andi $t2, $t0, 0x3F
    beqz $t2, .Ls_bp
    ins $t0, $zr, 0, 6
    or $t3, $t0, $v1
    sw $t3, 0($t8)
    addu $a1, $a1, $t2
    addiu $a1, $a1, -64
    blez $a1, .Ls_bv
    addiu $t0, $t0, 64

.Ls_bp:
    andi $t2, $t1, 0x3F
    beqz $t2, .Ls_bq
    ins $t1, $zr, 0, 6
    or $t3, $t1, $v1
    sw $t3, 0($t8)
    subu $a1, $a1, $t2

.Ls_bq:
    srl $a1, $a1, 6
    beqz $a1, .Ls_bv
    sltiu $t2, $a1, 2048
    beqz $t2, .Ls_bs
    nop

.Ls_br:
    li $t2, 64
    min $t2, $t2, $a1
    addiu $t3, $t2, -1
    or $t3, $t3, $t0
    or $t3, $t3, $v1
    sw $t3, 0($t8)
    sll $t3, $t2, 6
    subu $a1, $a1, $t2
    bnez $a1, .Ls_br
    addu $t0, $t0, $t3
    lw $zr, 0($t8)
    jr $ra
    addu $v0, $zr, $zr

.Ls_bs:
    lui $t2, 0xA7F8
    li $t3, 2048
    lui $t4, 0xFFF
    ori $t4, $t4, 0xFFC0

.Ls_bt:
    lw $t5, 0($t2)
    and $t6, $t5, $v0
    beqz $t6, .Ls_bu
    and $t5, $t5, $t4
    sltu $t6, $t5, $t0
    bnez $t6, .Ls_bu
    subu $t6, $t1, $t5
    blez $t6, .Ls_bu
    srl $t6, $t6, 6
    li $t7, 64
    min $t6, $t6, $t7
    addiu $t6, $t6, -1
    or $t5, $t5, $v1
    ins $t5, $t6, 0, 6
    sw $t5, 0($t8)

.Ls_bu:
    addiu $t3, $t3, -1
    bnez $t3, .Ls_bt
    addiu $t2, $t2, 4

.Ls_bv:
    lw $zr, 0($t8)

.Ls_bw:
    jr $ra
    addu $v0, $zr, $zr

    # ?
    .size sceKernelL2cacheInvalidateRange, .-sceKernelL2cacheInvalidateRange

    .type l2cacheProbeEret, @function
l2cacheProbeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(l2cacheEretReturn)
    addiu $ra, $ra, %lo(l2cacheEretReturn)

    .globl sceKernelL2cacheProbe
    .size l2cacheProbeEret, .-l2cacheProbeEret
    .type sceKernelL2cacheProbe, @function
sceKernelL2cacheProbe:
    li $t0, 19532
    lui $t1, 0x4C
    ori $t1, $t1, 0x4C
    slt $t2, $zr, $a0
    movn $t0, $t1, $t2
    ext $t1, $a0, 26, 5
    srlv $t0, $t0, $t1
    andi $t0, $t0, 0x1
    beqz $t0, .Ls_by
    addu $v0, $zr, $zr
    ins $a0, $zr, 27, 5
    ins $a0, $t2, 27, 1
    ext $t0, $a0, 6, 8
    lui $t1, 0xA7F8
    ins $t1, $t0, 5, 8
    li $t0, 8
    lui $t2, 0xFFF
    ori $t2, $t2, 0xFFC0

.Ls_bx:
    lw $t3, 0($t1)
    ext $t4, $t3, 28, 1
    and $t5, $t3, $t2
    xor $t5, $t5, $a0
    movn $t4, $zr, $t5
    bnez $t4, .Ls_bz
    addiu $t0, $t0, -1
    bnez $t0, .Ls_bx
    addiu $t1, $t1, 4

.Ls_by:
    jr $ra
    li $v0, 0

.Ls_bz:
    ext $v0, $t3, 29, 1
    jr $ra
    addiu $v0, $v0, 1

    # ?
    .size sceKernelL2cacheProbe, .-sceKernelL2cacheProbe

    .type l2cacheProbeRangeEret, @function
l2cacheProbeRangeEret:
    addu $t9, $ra, $zr
    lui $ra, %hi(l2cacheEretReturn)
    addiu $ra, $ra, %lo(l2cacheEretReturn)

    .globl sceKernelL2cacheProbeRange
    .size l2cacheProbeRangeEret, .-l2cacheProbeRangeEret
    .type sceKernelL2cacheProbeRange, @function
sceKernelL2cacheProbeRange:
    li $t0, 19532
    lui $t1, 0x4C
    ori $t1, $t1, 0x4C
    slt $t2, $zr, $a0
    movn $t0, $t1, $t2
    ext $t1, $a0, 26, 5
    srlv $t0, $t0, $t1
    andi $t0, $t0, 0x1
    beqz $t0, .Ls_cb
    addu $v0, $zr, $zr
    beqz $a1, .Ls_cb
    addu $t0, $a0, $zr
    addu $t1, $a0, $a1
    lui $t3, 0xFFF
    ori $t3, $t3, 0xFFC0
    and $t0, $t0, $t3
    and $t1, $t1, $t3
    ins $t0, $t2, 27, 1
    ins $t1, $t2, 27, 1
    lui $t2, 0xA7F8
    li $t4, 2048
    li $v0, 0

.Ls_ca:
    lw $t5, 0($t2)
    ext $t6, $t5, 28, 1
    and $t7, $t5, $t3
    sltu $t8, $t7, $t0
    sltu $t7, $t1, $t7
    or $t7, $t7, $t8
    movn $t6, $zr, $t7
    ext $t5, $t5, 29, 1
    movn $t5, $zr, $t7
    addu $t6, $t5, $t6
    bnez $t5, .Ls_cb
    or $v0, $v0, $t6
    addiu $t4, $t4, -1
    bnez $t4, .Ls_ca
    addiu $t2, $t2, 4

.Ls_cb:
    jr $ra
    nop

    .size sceKernelL2cacheProbeRange, .-sceKernelL2cacheProbeRange
    .type l2cacheEretReturn, @function
l2cacheEretReturn:
    addu $ra, $t9, $zr
    eret

    .globl UtilsForKernel_A6B0A6B8 # return 1 if there's the additional cache?
    .size l2cacheEretReturn, .-l2cacheEretReturn
    .type UtilsForKernel_A6B0A6B8, @function
UtilsForKernel_A6B0A6B8:
    mfc0 $v0, $24
    jr $ra
    nop

    .globl UtilsForKernel_39FFB756 # copy a function doing cache stuff (DcacheWritebackInvalidateAll?) to 0xBFC00F00 and run it there
    .size UtilsForKernel_A6B0A6B8, .-UtilsForKernel_A6B0A6B8
    .type UtilsForKernel_39FFB756, @function
UtilsForKernel_39FFB756:
    lui $t0, 0xBFC0
    ori $t0, $t0, 0xF00
    lui $t1, %hi(dcacheInvalidateAllFromBootRam)
    addiu $t1, $t1, %lo(dcacheInvalidateAllFromBootRam)
    li $t2, 256

.Ls_cc:
    lw $t3, 0($t1)
    addiu $t1, $t1, 4
    addiu $t2, $t2, -4
    sw $t3, 0($t0)
    bnez $t2, .Ls_cc
    addiu $t0, $t0, 4
    sync
    lui $t0, 0xBFC0
    ori $t0, $t0, 0xF00
    jr $t0
    nop

dcacheInvalidateAllFromBootRam:
    mfic $t6, $0
    mtic $zr, $0
    mfc0 $t0, COP0_STATE_CONFIG
    li $t1, 2048
    ext $t0, $t0, 6, 3
    sllv $t1, $t1, $t0
    addu $t0, $zr, $zr

.Ls_cd:
    cache 0x14, 0($t0)
    cache 0x14, 0($t0)
    addiu $t0, $t0, 64
    bne $t0, $t1, .Ls_cd
    nop
    mfc0 $t0, $24
    sync
    andi $t0, $t0, 0x1
    beqz $t0, .Ls_cf
    nop
    lui $t0, 0xA7F0
    lui $t1, 0x7000
    ori $t1, $t1, 0x3F
    lui $t2, 0x7000
    ori $t2, $t2, 0x103F
    lui $t3, 0x7000
    ori $t3, $t3, 0x203F
    lui $t4, 0x7000
    ori $t4, $t4, 0x303F
    li $t5, 8

.Ls_ce:
    sw $t1, 0($t0)
    sw $t2, 0($t0)
    sw $t3, 0($t0)
    sw $t4, 0($t0)
    addiu $t5, $t5, -1
    bnez $t5, .Ls_ce
    nop
    lui $t1, 0xF000
    sw $t1, 0($t0)
    lw $zr, 0($t0)
    lui $t1, 0xFFFF
    ori $t1, $t1, 0xFFFF
    sw $t1, 32($t0)
    sync

.Ls_cf:
    mtc0 $zr, $24
    nop
    nop
    mtc0 $a0, $24
    nop
    nop
    nop
    addu $v0, $zr, $zr
    mfc0 $t0, $24
    xor $t0, $t0, $a0
    andi $t0, $t0, 0x1
    bnezl $t0, .Ls_cg
    li $v0, -1
    andi $t0, $a0, 0x1
    beqz $t0, .Ls_cg
    nop
    lui $t0, 0xA7F0
    lui $t1, 0xF000
    sw $t1, 0($t0)
    lw $zr, 0($t0)

.Ls_cg:
    sync
    jr $ra
    mtic $t6, $0

    .globl UtilsForKernel_66463BEC # while (*0xBD000004 & (mask & 5) != 0) {} (not called anywhere)
    .size UtilsForKernel_39FFB756, .-UtilsForKernel_39FFB756
    .type UtilsForKernel_66463BEC, @function
UtilsForKernel_66463BEC:
    sync
    lui $v0, 0xBD00
    andi $a0, $a0, 0x5
    sw $a0, 4($v0)

.Ls_ch:
    lw $v1, 4($v0)
    and $v1, $v1, $a0
    bnez $v1, .Ls_ch
    nop
    jr $ra
    addu $v0, $zr, $zr

# 0x1710
    .globl md5BlockUpdate
    .size UtilsForKernel_66463BEC, .-UtilsForKernel_66463BEC
    .type md5BlockUpdate, @function
md5BlockUpdate:
    addu $at, $ra, $zr
    lw $t0, 0($a0)
    lw $t1, 4($a0)
    lw $t2, 8($a0)
    lw $t3, 12($a0)
    lui $t7, %hi(md5RoundChoice)
    addiu $t7, $t7, %lo(md5RoundChoice)
    li $t8, 0
    lui $t9, %hi(g_md5Tbl)
    addiu $t9, $t9, %lo(g_md5Tbl)
    li $a2, 0
    li $a3, 4
    lui $t6, 0xA0F
    ori $t6, $t6, 0x1419

.Ls_ci:
    jalr $t7
    addu $t4, $a1, $a2
    lw $t4, 0($t4)
    addu $a2, $a2, $a3
    andi $a2, $a2, 0x3C
    lw $t5, 0($t9)
    addiu $t9, $t9, 4
    addu $v0, $v0, $t4
    addu $v0, $v0, $t5
    addu $t0, $t0, $v0
    andi $t4, $t6, 0x1F
    rotr $t6, $t6, 8
    rotrv $t0, $t0, $t4
    addu $t0, $t0, $t1
    addu $v0, $t3, $zr
    addu $t3, $t2, $zr
    addu $t2, $t1, $zr
    addu $t1, $t0, $zr
    addu $t0, $v0, $zr
    addiu $t8, $t8, 1
    andi $v0, $t8, 0xF
    bnez $v0, .Ls_ci
    nop
    addiu $t7, $t7, 16
    jr $t7
    nop

md5RoundChoice:
    xor $v0, $t2, $t3
    and $v0, $t1, $v0
    jr $ra
    xor $v0, $t3, $v0
    .size md5BlockUpdate, .-md5BlockUpdate

    .type md5RoundG, @function
md5RoundG:
    lui $t6, 0xC12
    ori $t6, $t6, 0x171B
    li $a2, 4
    li $a3, 20
    b .Ls_ci
    addiu $t7, $t7, 24
    xor $v0, $t1, $t2
    and $v0, $t3, $v0
    jr $ra
    xor $v0, $t2, $v0
    .size md5RoundG, .-md5RoundG

    .type md5RoundH, @function
md5RoundH:
    lui $t6, 0x910
    ori $t6, $t6, 0x151C
    li $a2, 20
    li $a3, 12
    b .Ls_ci
    addiu $t7, $t7, 24
    xor $v0, $t1, $t2
    jr $ra
    xor $v0, $v0, $t3
    .size md5RoundH, .-md5RoundH
    nop

    .type md5RoundI, @function
md5RoundI:
    lui $t6, 0xB11
    ori $t6, $t6, 0x161A
    li $a2, 0
    li $a3, 28
    b .Ls_ci
    addiu $t7, $t7, 24
    nor $v0, $zr, $t3
    or $v0, $t1, $v0
    jr $ra
    xor $v0, $t2, $v0
    .size md5RoundI, .-md5RoundI

    .type md5BlockFinish, @function
md5BlockFinish:
    lw $t4, 0($a0)
    lw $t5, 4($a0)
    lw $t6, 8($a0)
    lw $t7, 12($a0)
    addu $t4, $t4, $t0
    sw $t4, 0($a0)
    addu $t5, $t5, $t1
    addu $t6, $t6, $t2
    sw $t5, 4($a0)
    addu $t7, $t7, $t3
    addu $ra, $at, $zr
    sw $t6, 8($a0)
    jr $ra
    sw $t7, 12($a0)

# 0x1870
    .globl sha1BlockUpdate
    .size md5BlockFinish, .-md5BlockFinish
    .type sha1BlockUpdate, @function
sha1BlockUpdate:
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
    jal sha1CompressBlock
    addu $s5, $a1, $zr
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
    .size sha1BlockUpdate, .-sha1BlockUpdate
    nop

    .type sha1CompressBlock, @function
sha1CompressBlock:
    addu $t0, $s0, $zr
    addu $t1, $s1, $zr
    addu $t2, $s2, $zr
    addu $t3, $s3, $zr
    addu $t4, $s4, $zr
    addu $v0, $zr, $zr

.Ls_cj:
    andi $t5, $v0, 0x3C
    sltiu $t6, $v0, 64
    addu $t9, $a0, $t5
    bnez $t6, .Ls_ck
    lw $t5, 0($t9)
    addiu $t6, $v0, 8
    addiu $t7, $v0, 32
    addiu $t8, $v0, 52
    andi $t6, $t6, 0x3C
    andi $t7, $t7, 0x3C
    andi $t8, $t8, 0x3C
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

.Ls_ck:
    rotr $t6, $t0, 27
    addu $t5, $t5, $t4
    addu $t5, $t5, $t6
    sltiu $t6, $v0, 80
    bnez $t6, .Ls_cl
    sltiu $t6, $v0, 160
    bnez $t6, .Ls_cm
    sltiu $t6, $v0, 240
    bnez $t6, .Ls_cn
    xor $t6, $t1, $t2
    lui $t8, 0xCA62
    xor $t6, $t6, $t3
    ori $t8, $t8, 0xC1D6
    j .Ls_co
    addiu $v0, $v0, 4
    nop
    nop
    nop

.Ls_cl:
    and $t6, $t1, $t2
    nor $t7, $zr, $t1
    lui $t8, 0x5A82
    and $t7, $t7, $t3
    ori $t8, $t8, 0x7999
    or $t6, $t6, $t7
    j .Ls_co
    addiu $v0, $v0, 4

.Ls_cm:
    xor $t6, $t1, $t2
    lui $t8, 0x6ED9
    xor $t6, $t6, $t3
    ori $t8, $t8, 0xEBA1
    j .Ls_co
    addiu $v0, $v0, 4
    nop
    nop

.Ls_cn:
    and $t6, $t1, $t2
    and $t7, $t1, $t3
    lui $t8, 0x8F1B
    or $t6, $t6, $t7
    and $t7, $t2, $t3
    ori $t8, $t8, 0xBCDC
    or $t6, $t6, $t7
    addiu $v0, $v0, 4

.Ls_co:
    addu $t6, $t6, $t8
    addu $t4, $t3, $zr
    addu $t3, $t2, $zr
    sll $t7, $t1, 30
    srl $t1, $t1, 2
    sltiu $t8, $v0, 320
    or $t2, $t7, $t1
    addu $t1, $t0, $zr
    bnez $t8, .Ls_cj
    addu $t0, $t5, $t6
    addu $s0, $t0, $s0
    addu $s1, $t1, $s1
    addu $s2, $t2, $s2
    addu $s3, $t3, $s3
    jr $ra
    addu $s4, $t4, $s4

# 1A40
    .globl sha1Digest
    .size sha1CompressBlock, .-sha1CompressBlock
    .type sha1Digest, @function
sha1Digest:
    addiu $sp, $sp, -256
    sw $s0, 0($sp)
    sw $s1, 4($sp)
    sw $s2, 8($sp)
    sw $s3, 12($sp)
    sw $s4, 16($sp)
    sw $s5, 20($sp)
    sw $s6, 24($sp)
    sw $ra, 28($sp)
    addu $s5, $a0, $zr
    sll $s6, $a1, 3
    addiu $a0, $sp, 128
    lui $t0, %hi(g_sha1Tbl)
    addiu $t0, $t0, %lo(g_sha1Tbl)
    lw $s0, 0($t0)
    andi $t2, $s5, 0xF
    lw $s1, 4($t0)
    andi $t3, $a1, 0x3F
    lw $s2, 8($t0)
    or $t2, $t2, $t3
    lw $s3, 12($t0)
    lw $s4, 16($t0)
    bnez $t2, .Ls_ct
    nop
    beqz $a1, .Ls_cr
    nop
    nop
    nop

.Ls_cp:
    li $t0, 16
    addu $t1, $a0, $zr

.Ls_cq:
    lw $t2, 0($s5)
    addiu $t0, $t0, -1
    addiu $s5, $s5, 4
    wsbw $t2, $t2
    sw $t2, 0($t1)
    bnez $t0, .Ls_cq
    addiu $t1, $t1, 4
    addiu $a1, $a1, -64
    jal sha1CompressBlock
    nop
    bnez $a1, .Ls_cp
    nop

.Ls_cr:
    li $v0, 16
    addu $t0, $a0, $zr

.Ls_cs:
    addiu $v0, $v0, -4
    sw $zr, 0($t0)
    sw $zr, 4($t0)
    sw $zr, 8($t0)
    sw $zr, 12($t0)
    bnez $v0, .Ls_cs
    addiu $t0, $t0, 16
    sw $s6, 60($a0)
    lui $t0, 0x8000
    jal sha1CompressBlock
    sw $t0, 0($a0)
    j .Ls_df
    nop
    nop

.Ls_ct:
    sltiu $v0, $a1, 64
    bnez $v0, .Ls_cv
    nop
    li $t0, 16
    addu $t1, $a0, $zr

.Ls_cu:
    lwr $t2, 0($s5)
    lwl $t2, 3($s5)
    addiu $t0, $t0, -1
    addiu $s5, $s5, 4
    wsbw $t2, $t2
    sw $t2, 0($t1)
    bnez $t0, .Ls_cu
    addiu $t1, $t1, 4
    jal sha1CompressBlock
    nop
    j .Ls_ct
    addiu $a1, $a1, -64
    nop

.Ls_cv:
    li $v0, 16
    addu $t0, $a0, $zr

.Ls_cw:
    addiu $v0, $v0, -4
    sw $zr, 0($t0)
    sw $zr, 4($t0)
    sw $zr, 8($t0)
    sw $zr, 12($t0)
    bnez $v0, .Ls_cw
    addiu $t0, $t0, 16
    sltiu $v0, $a1, 56
    addu $t0, $s5, $zr
    addu $t2, $a1, $zr
    bnez $v0, .Ls_da
    nop
    addu $t1, $a0, $zr
    nop

.Ls_cx:
    lbu $t3, 0($t0)
    addiu $t2, $t2, -1
    addiu $t0, $t0, 1
    sb $t3, 0($t1)
    bnez $t2, .Ls_cx
    addiu $t1, $t1, 1
    li $t3, 128
    sb $t3, 0($t1)
    li $v0, 16
    addu $t0, $a0, $zr

.Ls_cy:
    lw $t1, 0($t0)
    addiu $v0, $v0, -1
    wsbw $t1, $t1
    sw $t1, 0($t0)
    bnez $v0, .Ls_cy
    addiu $t0, $t0, 4
    jal sha1CompressBlock
    nop
    addiu $a1, $a1, -64
    li $v0, 16
    addu $t0, $a0, $zr

.Ls_cz:
    addiu $v0, $v0, -4
    sw $zr, 0($t0)
    sw $zr, 4($t0)
    sw $zr, 8($t0)
    sw $zr, 12($t0)
    bnez $v0, .Ls_cz
    addiu $t0, $t0, 16

.Ls_da:
    bgtz $a1, .Ls_db
    sw $s6, 60($a0)
    sltiu $t0, $a1, 1
    sll $t0, $t0, 31
    j .Ls_de
    sw $t0, 0($a0)

.Ls_db:
    addu $t0, $s5, $zr
    addu $t1, $a0, $zr

.Ls_dc:
    lbu $t3, 0($t0)
    addiu $a1, $a1, -1
    addiu $t0, $t0, 1
    sb $t3, 0($t1)
    bnez $a1, .Ls_dc
    addiu $t1, $t1, 1
    li $t0, 128
    sb $t0, 0($t1)
    li $v0, 14
    addu $t0, $a0, $zr

.Ls_dd:
    lw $t1, 0($t0)
    addiu $v0, $v0, -1
    wsbw $t1, $t1
    sw $t1, 0($t0)
    bnez $v0, .Ls_dd
    addiu $t0, $t0, 4

.Ls_de:
    jal sha1CompressBlock
    nop

.Ls_df:
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
    addu $v0, $zr, $zr
    jr $ra
    addiu $sp, $sp, 256

# 1CF0
    .globl mt19937UInt
    .size sha1Digest, .-sha1Digest
    .type mt19937UInt, @function
mt19937UInt:
    lw $a1, 0($a0)
    addiu $a3, $a0, 4
    sltiu $t4, $a1, 624
    movz $a1, $zr, $t4
    sll $t0, $a1, 2
    sltiu $t4, $a1, 623
    addu $t0, $a3, $t0
    sltiu $t3, $a1, 227
    beqz $t4, .Ls_dh
    lw $t1, 0($t0)
    addiu $a1, $a1, 1
    lw $t2, 4($t0)
    lui $t4, 0x8000
    sw $a1, 0($a0)

.Ls_dg:
    srl $t6, $t1, 11
    nor $t5, $zr, $t4
    xor $v0, $t1, $t6
    and $t1, $t1, $t4
    lui $t7, 0x9D2C
    and $t5, $t2, $t5
    ori $t7, $t7, 0x5680
    sll $t6, $v0, 7
    lui $t4, 0x9908
    and $t6, $t6, $t7
    lui $t7, 0xEFC6
    xor $v0, $v0, $t6
    or $t1, $t1, $t5
    sll $t6, $v0, 15
    ori $t4, $t4, 0xB0DF
    and $t6, $t6, $t7
    andi $t2, $t2, 0x1
    xor $v0, $v0, $t6
    srl $t1, $t1, 1
    addiu $t5, $t0, -908
    addiu $t7, $t0, 1588
    xor $t4, $t1, $t4
    movn $t5, $t7, $t3
    movn $t1, $t4, $t2
    lw $t5, 0($t5)
    srl $t6, $v0, 18
    xor $t1, $t1, $t5
    xor $v0, $v0, $t6
    jr $ra
    sw $t1, 0($t0)

.Ls_dh:
    lw $t2, 4($a0)
    lui $t4, 0x8000
    b .Ls_dg
    sw $zr, 0($a0)
    .size mt19937UInt, .-mt19937UInt
    nop
    nop
    nop
    nop

    .globl sceKernelGetGPI
    .type sceKernelGetGPI, @function
sceKernelGetGPI:
    lui $v0, %hi(g_GetGPI)
    lw $v0, %lo(g_GetGPI)($v0)
    beqz $v0, .Ls_di
    lui $a0, %hi(g_GPIMask)
    jr $v0
    lw $a0, %lo(g_GPIMask)($a0)

.Ls_di:
    jr $ra
    addu $v0, $zr, $zr

    .globl sceKernelSetGPO
    .size sceKernelGetGPI, .-sceKernelGetGPI
    .type sceKernelSetGPO, @function
sceKernelSetGPO:
    lui $v0, %hi(g_SetGPO)
    lw $v0, %lo(g_SetGPO)($v0)
    beqz $v0, .Ls_di
    lui $v1, %hi(g_GPOMask)
    lw $v1, %lo(g_GPOMask)($v1)
    nor $a1, $zr, $a0
    and $a1, $a1, $v1
    jr $v0
    and $a0, $a0, $v1

    .globl sceKernelGetGPO
    .size sceKernelSetGPO, .-sceKernelSetGPO
    .type sceKernelGetGPO, @function
sceKernelGetGPO:
    lui $v0, %hi(g_SetGPO)
    lw $v0, %lo(g_SetGPO)($v0)
    beqz $v0, .Ls_di
    addu $a0, $zr, $zr
    jr $v0
    addu $a1, $zr, $zr

    .globl sceKernelSetGPIMask
    .size sceKernelGetGPO, .-sceKernelGetGPO
    .type sceKernelSetGPIMask, @function
sceKernelSetGPIMask:
    lui $t0, %hi(g_GPIMask)
    nor $t1, $zr, $a0
    lw $v0, %lo(g_GPIMask)($t0)
    and $v0, $v0, $t1
    or $v0, $v0, $a1
    jr $ra
    sw $v0, %lo(g_GPIMask)($t0)

    .globl sceKernelSetGPOMask
    .size sceKernelSetGPIMask, .-sceKernelSetGPIMask
    .type sceKernelSetGPOMask, @function
sceKernelSetGPOMask:
    lui $t0, %hi(g_GPOMask)
    nor $t1, $zr, $a0
    lw $v0, %lo(g_GPOMask)($t0)
    and $v0, $v0, $t1
    or $v0, $v0, $a1
    jr $ra
    sw $v0, %lo(g_GPOMask)($t0)
    .size sceKernelSetGPOMask, .-sceKernelSetGPOMask
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop

    .globl sceKernelSetPTRIG
    .type sceKernelSetPTRIG, @function
sceKernelSetPTRIG:
    lui $v0, %hi(g_SetPTRIG)
    lw $v0, %lo(g_SetPTRIG)($v0)
    beqz $v0, .Ls_dj
    lui $v1, %hi(g_PTRIGMask)
    lw $v1, %lo(g_PTRIGMask)($v1)
    jr $v0
    and $a0, $a0, $v1

.Ls_dj:
    jr $ra
    addu $v0, $zr, $zr

    .globl sceKernelGetPTRIG
    .size sceKernelSetPTRIG, .-sceKernelSetPTRIG
    .type sceKernelGetPTRIG, @function
sceKernelGetPTRIG:
    lui $v0, %hi(g_GetPTRIG)
    lw $v0, %lo(g_GetPTRIG)($v0)
    beqz $v0, .Ls_dj
    nop
    jr $v0
    nop

    .globl sceKernelSetPTRIGMask
    .size sceKernelGetPTRIG, .-sceKernelGetPTRIG
    .type sceKernelSetPTRIGMask, @function
sceKernelSetPTRIGMask:
    lui $v0, %hi(g_PTRIGMask)
    jr $ra
    sw $a0, %lo(g_PTRIGMask)($v0)

    .globl sceKernelPutUserLog
    .size sceKernelSetPTRIGMask, .-sceKernelSetPTRIGMask
    .type sceKernelPutUserLog, @function
sceKernelPutUserLog:
    lui $v0, %hi(g_UserLog)
    lw $v0, %lo(g_UserLog)($v0)
    beqz $v0, .Ls_dk
    nop
    jr $v0
    nop

.Ls_dk:
    jr $ra
    addu $v0, $zr, $zr

    .globl sceKernelRegisterUserLogHandler
    .size sceKernelPutUserLog, .-sceKernelPutUserLog
    .type sceKernelRegisterUserLogHandler, @function
sceKernelRegisterUserLogHandler:
    lui $v0, %hi(g_UserLog)
    jr $ra
    sw $a0, %lo(g_UserLog)($v0)

    .globl sceKernelDeflateDecompress
    .size sceKernelRegisterUserLogHandler, .-sceKernelRegisterUserLogHandler
    .type sceKernelDeflateDecompress, @function
sceKernelDeflateDecompress:
    addiu $sp, $sp, -1344
    sw $s4, 1228($sp)
    addu $a1, $a0, $a1
    addu $t6, $a0, $zr
    sw $s5, 1232($sp)
    addiu $s5, $a1, -64
    ins $s5, $zr, 0, 6
    sw $ra, 1236($sp)

.Ls_dl:
    andi $t9, $a2, 0x3
    subu $a2, $a2, $t9
    lw $t8, 0($a2)
    sll $t9, $t9, 3
    addiu $t9, $t9, -32

.Ls_dm:
    addiu $t9, $t9, 3
    blez $t9, .Ls_dn
    rotrv $v0, $t8, $t9
    addiu $a2, $a2, 4
    lw $t8, 0($a2)
    sllv $v0, $v0, $t9
    addiu $t9, $t9, -32
    ins $v0, $t8, 0, 16
    rotrv $v0, $v0, $t9

.Ls_dn:
    ext $v1, $v0, 30, 2
    beqz $v1, .Ls_fl
    ext $t2, $v0, 29, 1
    lui $t7, %hi(g_defltTbl)
    addiu $t0, $v1, -2
    beqz $t0, .Ls_ds
    sh $t2, 1240($sp)
    bgtz $t0, .Ls_fo
    addiu $t2, $sp, -108

.Ls_do:
    lw $t3, %lo(g_defltTbl)($t7)
    addiu $t2, $t2, 4
    addiu $t7, $t7, 4
    bne $t2, $sp, .Ls_do
    sw $t3, 104($t2)
    li $t1, 0
    li $s4, 144

.Ls_dp:
    sh $t1, 108($t2)
    addiu $t0, $t1, 144
    addiu $t1, $t1, 1
    sh $t0, 412($t2)
    bne $t1, $s4, .Ls_dp
    addiu $t2, $t2, 2
    addiu $s4, $t7, 16

.Ls_dq:
    lw $t3, %lo(g_defltTbl)($t7)
    addiu $t2, $t2, 4
    addiu $t7, $t7, 4
    bne $s4, $t7, .Ls_dq
    sw $t3, 104($t2)
    addiu $s4, $t7, 64

.Ls_dr:
    lw $t3, %lo(g_defltTbl)($t7)
    addiu $t2, $t2, 4
    addiu $t7, $t7, 4
    bne $s4, $t7, .Ls_dr
    sw $t3, 388($t2)
    sh $t3, 666($sp)
    j .Ls_dx
    sh $t3, 636($sp)

.Ls_ds:
    addiu $t9, $t9, 14
    blez $t9, .Ls_dt
    rotrv $v0, $t8, $t9
    addiu $a2, $a2, 4
    lw $t8, 0($a2)
    sllv $v0, $v0, $t9
    addiu $t9, $t9, -32
    ins $v0, $t8, 0, 16
    rotrv $v0, $v0, $t9

.Ls_dt:
    swl $v0, 1261($sp)
    ext $s4, $v0, 28, 4
    addiu $t3, $sp, 1212
    addiu $t0, $t7, -4
    addu $s4, $s4, $t7

.Ls_du:
    beq $t0, $s4, .Ls_dw
    addiu $t0, $t0, 1
    lb $t2, %lo(g_defltTbl + 9)($t0)
    addiu $t9, $t9, 3
    blez $t9, .Ls_dv
    rotrv $v0, $t8, $t9
    addiu $a2, $a2, 4
    lw $t8, 0($a2)
    sllv $v0, $v0, $t9
    addiu $t9, $t9, -32
    ins $v0, $t8, 0, 16
    rotrv $v0, $v0, $t9

.Ls_dv:
    srl $v0, $v0, 29
    beqz $v0, .Ls_du
    ins $t2, $v0, 9, 6
    addiu $t3, $t3, 2
    j .Ls_du
    sh $t2, 58($t3)

.Ls_dw:
    jal gzipBuildDistTable
    addiu $s4, $sp, 1212
    lh $t5, 1260($sp)
    beqz $t0, .Ls_fo
    addiu $s4, $sp, 0
    ext $t5, $t5, 2, 5
    li $at, 0
    jal gzipBuildHuffTable
    addiu $t5, $t5, 257
    beqz $t0, .Ls_fo
    lh $t5, 1260($sp)
    addiu $s4, $sp, 636
    ext $t5, $t5, 7, 5
    li $at, 288
    jal gzipBuildHuffTable
    addiu $t5, $t5, 289

.Ls_dx:
    lb $t5, 636($sp)
    lhu $t1, 30($sp)
    j .Ls_ea
    lb $t4, 0($sp)

.Ls_dy:
    beq $a0, $a1, .Ls_fq
    addiu $a0, $a0, 1

.Ls_dz:
    sb $v0, -1($a0)

.Ls_ea:
    jal gzipHuffDecodeSymbol
    addiu $t0, $sp, 2
    addiu $t0, $sp, 638
    bgez $v0, .Ls_dy
    bitrev $t2, $v0
    bltzal $t2, gzipReadBitsChecked
    sra $s4, $v0, 4
    lhu $t1, 666($sp)
    subu $at, $a0, $s4
    srlv $v0, $t8, $t9
    jal gzipHuffDecodeContinue
    subu $t9, $t9, $t5
    sltu $v1, $s5, $at
    bnez $v1, .Ls_fp
    clz $t2, $v0
    cache 0x18, 63($a0)

.Ls_eb:
    addiu $t2, $t2, -30
    bltzal $t2, gzipReadBitsOrError
    subu $s4, $a0, $v0
    sltu $t2, $t6, $s4
    beqz $t2, .Ls_fo
    lwr $v0, -1($s4)
    lhu $t1, 30($sp)
    bnez $v1, .Ls_ed
    subu $v1, $a0, $s4
    sltiu $v1, $v1, 3
    bnez $v1, .Ls_ed
    addiu $v1, $a0, -3
    addu $a0, $at, $zr
    ins $at, $v1, 0, 2

.Ls_ec:
    lwl $v0, 2($s4)
    swr $v0, 3($v1)
    addiu $v1, $v1, 4
    beq $v1, $at, .Ls_ea
    swl $v0, 2($v1)
    addiu $s4, $s4, 4
    j .Ls_ec
    lwr $v0, -1($s4)

.Ls_ed:
    addiu $a0, $a0, 1

.Ls_ee:
    sb $v0, -1($a0)
    addiu $a0, $a0, 1
    beq $at, $a0, .Ls_dz
    lbu $v0, 0($s4)
    j .Ls_ee
    addiu $s4, $s4, 1

.Ls_ef:
    addiu $a2, $a2, 4
    lw $t8, 0($a2)
    sllv $v0, $v0, $t9
    addiu $t9, $t9, -32
    ins $v0, $t8, 0, 16
    rotrv $v0, $v0, $t9
    srlv $v0, $v0, $t2
    jr $ra
    subu $s4, $s4, $v0

gzipReadBitsOrError:
    bltz $v0, .Ls_fo
    subu $t9, $t9, $t2
    bgtz $t9, .Ls_ef
    rotrv $v0, $t8, $t9
    srlv $v0, $v0, $t2
    jr $ra
    subu $s4, $s4, $v0

gzipReadBitsChecked:
    sra $t2, $t2, 28
    beq $s4, $t2, .Ls_eg
    subu $t9, $t9, $t2
    bgtz $t9, .Ls_ef
    rotrv $v0, $t8, $t9
    srlv $v0, $v0, $t2
    jr $ra
    subu $s4, $s4, $v0

.Ls_eg:
    addiu $v0, $v0, 1
    beqz $v0, .Ls_fo
    lh $t0, 1240($sp)
    beqz $t0, .Ls_dm
    addu $t9, $t9, $t2

.Ls_eh:
    beqz $a3, .Ls_ei
    subu $v0, $a0, $t6
    addiu $t9, $t9, 39
    srl $t1, $t9, 3
    addu $a2, $a2, $t1
    sw $a2, 0($a3)

.Ls_ei:
    lw $ra, 1236($sp)
    lw $s5, 1232($sp)
    lw $s4, 1228($sp)
    jr $ra
    addiu $sp, $sp, 1344

    .size sceKernelDeflateDecompress, .-sceKernelDeflateDecompress
    .type gzipHuffDecodeContinue, @function
gzipHuffDecodeContinue:
    blez $t9, .Ls_em
    sllv $v0, $v0, $t5

.Ls_ej:
    addiu $a2, $a2, 4
    lw $t8, 0($a2)
    sllv $v0, $v0, $t9
    addiu $t9, $t9, -32
    ins $v0, $t8, 0, 16
    rotrv $v0, $v0, $t9
    j .Ls_em
    ins $v0, $zr, 0, 16

.Ls_ek:
    addu $v0, $v0, $t0
    jr $ra
    lh $v0, 58($v0)

.Ls_el:
    lhu $t2, 0($t0)
    addu $v0, $v0, $t0
    subu $v0, $v0, $t2
    jr $ra
    lh $v0, 60($v0)

    .size gzipHuffDecodeContinue, .-gzipHuffDecodeContinue
    .type gzipHuffDecodeSymbol, @function
gzipHuffDecodeSymbol:
    srlv $v0, $t8, $t9
    subu $t9, $t9, $t4
    bgtz $t9, .Ls_ej
    sllv $v0, $v0, $t4

.Ls_em:
    bitrev $v0, $v0
    sltu $t2, $v0, $t1
    bnez $t2, .Ls_ek
    addu $v0, $v0, $v0

.Ls_en:
    beqz $t9, .Ls_ep
    lhu $v1, 30($t0)
    srlv $t2, $t8, $t9
    ins $v0, $t2, 0, 1
    addiu $t9, $t9, 1

.Ls_eo:
    sltu $v1, $v0, $v1
    bnez $v1, .Ls_el
    addu $v0, $v0, $v0
    j .Ls_en
    addiu $t0, $t0, 2

.Ls_ep:
    addiu $a2, $a2, 4
    lw $t8, 0($a2)
    li $t9, -31
    j .Ls_eo
    ins $v0, $t8, 0, 1

    .size gzipHuffDecodeSymbol, .-gzipHuffDecodeSymbol
    .type gzipBuildHuffTable, @function
gzipBuildHuffTable:
    sw $zr, 56($s4)
    sw $zr, 44($s4)
    addu $t3, $s4, $zr
    mthi $ra
    lb $t4, 1212($sp)
    sw $zr, 52($s4)

.Ls_eq:
    andi $t0, $at, 0x1FF

.Ls_er:
    subu $t0, $t0, $t5
    bgez $t0, .Ls_ey
    lhu $t1, 1242($sp)
    jal gzipHuffDecodeSymbol
    addiu $t0, $sp, 1214
    addiu $t0, $v0, -16
    blez $t0, .Ls_eu
    li $v1, 7
    min $t2, $t0, $v1
    addu $t9, $t9, $t2
    blez $t9, .Ls_es
    rotrv $v0, $t8, $t9
    addiu $a2, $a2, 4
    lw $t8, 0($a2)
    sllv $v0, $v0, $t9
    addiu $t9, $t9, -32
    ins $v0, $t8, 0, 16
    rotrv $v0, $v0, $t9

.Ls_es:
    negu $t2, $t2
    srlv $v0, $v0, $t2
    addu $t0, $t0, $v0

.Ls_et:
    j .Ls_eq
    addu $at, $at, $t0

.Ls_eu:
    beqz $t0, .Ls_ev
    addiu $t2, $t3, 2
    ins $at, $v0, 9, 6
    sh $at, 60($t3)
    addiu $at, $at, 1
    j .Ls_eq
    movn $t3, $t2, $v0

.Ls_ev:
    blez $at, .Ls_fo
    addiu $t9, $t9, 2
    blez $t9, .Ls_ew
    rotrv $v0, $t8, $t9
    addiu $a2, $a2, 4
    lw $t8, 0($a2)
    sllv $v0, $v0, $t9
    addiu $t9, $t9, -32
    ins $v0, $t8, 0, 16
    rotrv $v0, $v0, $t9

.Ls_ew:
    srl $v0, $v0, 30
    andi $t0, $at, 0x1FF
    beq $t0, $at, .Ls_et
    addiu $t0, $v0, 3
    addu $t0, $t0, $at

.Ls_ex:
    sh $at, 60($t3)
    addiu $at, $at, 1
    bne $at, $t0, .Ls_ex
    addiu $t3, $t3, 2
    j .Ls_er
    andi $t0, $at, 0x1FF

.Ls_ey:
    sw $zr, 48($s4)
    bnez $t0, .Ls_fo
    li $ra, 32767
    sw $zr, 32($s4)
    addiu $t1, $s4, 2
    li $at, -1
    sw $zr, 40($s4)
    addu $t4, $s4, $zr
    addu $t5, $s4, $zr

.Ls_ez:
    lh $v0, 60($t4)
    addiu $t4, $t4, 2
    sh $v0, 634($t4)
    bne $t4, $t3, .Ls_ez
    min $ra, $ra, $v0
    sw $zr, 36($s4)
    andi $ra, $ra, 0x7E00

.Ls_fa:
    addiu $ra, $ra, 512
    addu $v1, $s4, $zr

.Ls_fb:
    beq $t3, $t5, .Ls_ff
    li $t2, -1

.Ls_fc:
    lhu $v0, 636($v1)
    beq $t3, $v1, .Ls_fa
    min $v0, $v0, $ra
    beq $v0, $ra, .Ls_fc
    addiu $v1, $v1, 2
    addiu $at, $at, 1
    sh $t2, 634($v1)
    srl $t4, $v0, 9
    rotr $v0, $v0, 9
    bgtz $v0, .Ls_fd
    ext $v0, $v0, 23, 9
    ins $t7, $v0, 1, 6
    lh $v0, %lo(g_defltTbl + 60)($t7)

.Ls_fd:
    sh $v0, 60($t5)
    beq $t0, $t4, .Ls_fb
    addiu $t5, $t5, 2
    subu $t2, $t0, $t4
    beqz $t0, .Ls_fe
    addu $t0, $t4, $zr
    sh $at, 28($t1)
    rotrv $at, $at, $t2
    addu $t2, $t2, $t2
    subu $t1, $t1, $t2
    subu $t2, $t5, $at
    subu $t2, $t2, $at
    subu $t2, $t1, $t2

.Ls_fe:
    j .Ls_fb
    sh $t2, -2($t1)

.Ls_ff:
    mfhi $ra

.Ls_fg:
    srlv $v0, $at, $t0
    bnez $v0, .Ls_fo
    li $at, -1
    jr $ra
    sh $at, 28($t1)

    .size gzipBuildHuffTable, .-gzipBuildHuffTable
    .type gzipBuildDistTable, @function
gzipBuildDistTable:
    sw $zr, 32($s4)
    addu $t0, $zr, $zr
    addu $t5, $s4, $zr
    sw $zr, 36($s4)
    addiu $t1, $s4, 2
    li $at, -1
    sw $zr, 40($s4)

.Ls_fh:
    beq $t3, $t5, .Ls_fg
    addiu $t4, $t5, 2
    addiu $at, $at, 1
    lh $v0, 60($t5)

.Ls_fi:
    lh $v1, 60($t4)
    beq $t3, $t4, .Ls_fj
    addiu $t4, $t4, 2
    max $t2, $v1, $v0
    sh $t2, 58($t4)
    j .Ls_fi
    min $v0, $v1, $v0

.Ls_fj:
    srl $t4, $v0, 9
    andi $v0, $v0, 0x1FF
    sh $v0, 60($t5)
    beq $t0, $t4, .Ls_fh
    addiu $t5, $t5, 2
    subu $t2, $t0, $t4
    beqz $t0, .Ls_fk
    addu $t0, $t4, $zr
    sh $at, 28($t1)
    rotrv $at, $at, $t2
    addu $t2, $t2, $t2
    subu $t1, $t1, $t2
    subu $t2, $t5, $at
    subu $t2, $t2, $at
    subu $t2, $t1, $t2

.Ls_fk:
    j .Ls_fh
    sh $t2, -2($t1)

.Ls_fl:
    negu $t0, $t9
    srl $t0, $t0, 3
    subu $a2, $a2, $t0
    lwl $t0, 7($a2)
    lwr $t0, 4($a2)
    andi $v1, $t0, 0xFFFF
    addu $v1, $v1, $a0
    sltu $t1, $a1, $v1
    bnez $t1, .Ls_fq
    not $t1, $t0
    rotr $t1, $t1, 16
    bne $t1, $t0, .Ls_fo
    nop
    beq $v1, $a0, .Ls_fn
    nop

.Ls_fm:
    lbu $t1, 8($a2)
    addiu $a2, $a2, 1
    addiu $a0, $a0, 1
    bne $v1, $a0, .Ls_fm
    sb $t1, -1($a0)

.Ls_fn:
    bnez $t2, .Ls_eh
    li $t9, 32
    j .Ls_dl
    addiu $a2, $a2, 8

.Ls_fo:
    lui $v0, 0x8000
    j .Ls_ei
    ori $v0, $v0, 0x108

.Ls_fp:
    sltu $s4, $a1, $at
    beqz $s4, .Ls_eb
    nop

.Ls_fq:
    lui $v0, 0x8000
    j .Ls_ei
    ori $v0, $v0, 0x104

    .globl UtilsForKernel_6C6887EE
    .size gzipBuildDistTable, .-gzipBuildDistTable
    .type UtilsForKernel_6C6887EE, @function
UtilsForKernel_6C6887EE:
    addiu $sp, $sp, -2832
    sw $s2, 2816($sp)
    addu $t4, $a0, $a1
    addiu $t3, $a0, 0
    sw $s1, 2812($sp)
    li $t2, 5
    addiu $a1, $sp, -2808
    sw $s0, 2808($sp)
    addu $t9, $zr, $zr
    li $t0, 1
    sw $s3, 2820($sp)
    li $s3, -1
    addiu $at, $sp, 56
    sw $s4, 2824($sp)
    addiu $s4, $t4, -64
    ins $s4, $zr, 0, 6
    lb $s0, 0($a2)
    lwl $t1, 4($a2)
    lwr $t1, 1($a2)
    sw $s5, 2828($sp)
    bltz $s0, .Ls_iv
    wsbw $t1, $t1
    ext $s5, $s0, 3, 2
    sll $s5, $s5, 4
    li $s1, 128
    subu $s5, $s1, $s5
    ins $s5, $s5, 8, 8
    andi $s0, $s0, 0x7
    ins $s5, $s5, 16, 16

.Ls_fr:
    sw $s5, 2808($a1)
    addiu $a1, $a1, 8
    bne $a1, $sp, .Ls_fr
    sw $s5, 2804($a1)
    j .Ls_fz
    li $s5, 255

.Ls_fs:
    lbu $s2, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $s2
    mult $s3, $t6
    srl $s2, $t6, 3
    subu $t6, $t6, $s2
    mflo $t5
    sltu $t8, $t1, $t5
    bnez $t8, .Ls_fv
    srl $s2, $t9, 8
    sll $s3, $s3, 8

.Ls_ft:
    subu $t1, $t1, $t5
    sb $t6, -1($t7)
    bnez $s2, .Ls_fw
    subu $s3, $s3, $t5

.Ls_fu:
    addu $t7, $v1, $t9
    srl $t5, $s3, 24
    lbu $t6, -1($t7)
    beqz $t5, .Ls_fs
    sll $t9, $t9, 1
    srl $s2, $s3, 8
    mult $s2, $t6
    srl $s2, $t6, 3
    subu $t6, $t6, $s2
    mflo $t5
    sltu $t8, $t1, $t5
    beqz $t8, .Ls_ft
    srl $s2, $t9, 8

.Ls_fv:
    mflo $s3
    addiu $t6, $t6, 31
    sb $t6, -1($t7)
    beqz $s2, .Ls_fu
    addiu $t9, $t9, 1

.Ls_fw:
    srl $s1, $s3, 24
    lbu $t5, 2488($a1)
    beqz $s1, .Ls_gb
    sb $t9, 0($t3)

.Ls_fx:
    srl $v1, $s3, 8
    mult $v1, $t5
    addiu $t3, $t3, 1
    srl $v1, $t5, 4
    mflo $s1
    sltu $t6, $t1, $s1
    bnez $t6, .Ls_gc
    subu $v1, $t5, $v1

.Ls_fy:
    sb $v1, 2488($a1)
    addiu $a1, $a1, -1
    max $a1, $a1, $sp
    subu $t1, $t1, $s1
    beq $t3, $t4, .Ls_iu
    subu $s3, $s3, $s1

.Ls_fz:
    ins $t9, $t3, 8, 3
    srav $v1, $t9, $s0
    andi $v1, $v1, 0x7
    sll $t9, $v1, 8
    subu $t8, $t9, $v1
    addu $v1, $sp, $t8
    j .Ls_fu
    li $t9, 1

.Ls_ga:
    addu $t3, $v0, $zr
    lbu $t9, 0($t3)
    srl $s1, $s3, 24
    bnez $s1, .Ls_fx
    lbu $t5, 2488($a1)

.Ls_gb:
    lbu $t8, 5($a2)
    addiu $t3, $t3, 1
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $t8
    mult $s3, $t5
    sll $s3, $s3, 8
    srl $v1, $t5, 4
    mflo $s1
    sltu $t6, $t1, $s1
    beqz $t6, .Ls_fy
    subu $v1, $t5, $v1

.Ls_gc:
    li $t8, -1

.Ls_gd:
    addiu $v1, $v1, 15
    lbu $t5, 2496($a1)
    srl $t9, $s1, 24
    beqz $t9, .Ls_id
    mflo $s3
    sb $v1, 2488($a1)
    srl $s2, $s3, 8
    mult $s2, $t5
    addiu $a1, $a1, 8
    srl $s2, $t5, 4
    mflo $s1
    sltu $t9, $t1, $s1
    bnez $t9, .Ls_ie
    subu $v1, $t5, $s2

.Ls_ge:
    subu $s3, $s3, $s1
    bltz $t8, .Ls_gv
    subu $t1, $t1, $s1

.Ls_gf:
    sb $v1, 2488($a1)
    sllv $t9, $t3, $t8
    sll $s1, $t8, 5
    ins $s1, $t9, 3, 2
    ins $s1, $a1, 0, 3
    addu $s1, $sp, $s1
    addiu $t9, $t8, -3
    lbu $a1, 2552($s1)
    srl $v1, $s3, 24
    bltzl $t9, .Ls_gm
    li $t6, 1
    lbu $s2, 2576($s1)
    beqz $v1, .Ls_in
    srl $v0, $s3, 8
    mult $v0, $s2

.Ls_gg:
    srl $v0, $s2, 3
    subu $s2, $s2, $v0
    mflo $v1
    sltu $v0, $t1, $v1
    bnez $v0, .Ls_iq
    addiu $t6, $v0, 2
    subu $t1, $t1, $v1
    subu $s3, $s3, $v1
    blez $t9, .Ls_gl
    srl $v1, $s3, 24
    beqz $v1, .Ls_ir
    srl $v0, $s3, 8

.Ls_gh:
    mult $v0, $s2
    srl $v0, $s2, 3
    subu $s2, $s2, $v0
    mflo $t7
    sltu $v0, $t1, $t7
    bnez $v0, .Ls_io
    sll $t6, $t6, 1
    subu $s3, $s3, $t7
    srl $v1, $s3, 24
    beq $t9, $t0, .Ls_gl
    subu $t1, $t1, $t7
    beqz $v1, .Ls_ip

.Ls_gi:
    sll $t6, $t6, 1

.Ls_gj:
    srl $s3, $s3, 1

.Ls_gk:
    sltu $v0, $t1, $s3
    subu $v1, $t1, $s3
    movz $t1, $v1, $v0
    addiu $t9, $t9, -1
    bne $t9, $t0, .Ls_gi
    addu $t6, $t6, $v0
    srl $v1, $s3, 24

.Ls_gl:
    sb $s2, 2576($s1)

.Ls_gm:
    beqz $v1, .Ls_il
    srl $v0, $s3, 8
    mult $v0, $a1
    srl $v0, $a1, 3
    subu $a1, $a1, $v0
    mflo $v1
    sltu $v0, $t1, $v1
    bnez $v0, .Ls_im
    sll $t6, $t6, 1

.Ls_gn:
    sb $a1, 2552($s1)
    subu $t1, $t1, $v1
    blez $t8, .Ls_gw
    subu $s3, $s3, $v1
    lbu $a1, 2560($s1)

.Ls_go:
    srl $v1, $s3, 24
    beqz $v1, .Ls_ik
    srl $v0, $s3, 8
    mult $v0, $a1

.Ls_gp:
    srl $v0, $a1, 3
    subu $a1, $a1, $v0
    mflo $v1
    sltu $v0, $t1, $v1
    bnez $v0, .Ls_ij
    sll $t6, $t6, 1
    sb $a1, 2560($s1)
    subu $t1, $t1, $v1
    beq $t8, $t0, .Ls_gt
    subu $s3, $s3, $v1
    lbu $a1, 2568($s1)

.Ls_gq:
    srl $v1, $s3, 24
    beqz $v1, .Ls_if
    srl $v0, $s3, 8
    mult $v0, $a1

.Ls_gr:
    srl $v0, $a1, 3
    subu $a1, $a1, $v0
    mflo $v1
    sltu $v0, $t1, $v1
    bnez $v0, .Ls_ig
    sll $t6, $t6, 1
    subu $t1, $t1, $v1
    subu $s3, $s3, $v1

.Ls_gs:
    sb $a1, 2568($s1)

.Ls_gt:
    addu $t5, $at, $t8
    li $s2, 256
    li $s1, 8
    srl $v1, $s3, 24
    bnez $v1, .Ls_gy
    addu $t9, $t5, $s1

.Ls_gu:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    j .Ls_gy
    sll $s3, $s3, 8

.Ls_gv:
    sb $v1, 2488($a1)

.Ls_gw:
    li $s2, 64
    li $s1, 8
    addu $t5, $sp, $t8

.Ls_gx:
    srl $v1, $s3, 24
    beqz $v1, .Ls_gu
    addu $t9, $t5, $s1

.Ls_gy:
    lbu $t8, 2033($t9)
    srl $v0, $s3, 8
    sll $s1, $s1, 1
    mult $v0, $t8
    srl $v0, $t8, 3
    subu $t7, $t8, $v0
    mflo $v1
    sltu $t8, $t1, $v1
    beqz $t8, .Ls_is
    subu $t8, $s1, $s2
    mflo $s3
    addiu $t7, $t7, 31
    sb $t7, 2033($t9)
    bltz $t8, .Ls_gx
    addiu $s1, $s1, 8

.Ls_gz:
    addu $s1, $sp, $t8
    sra $t8, $t8, 3
    addiu $t9, $t8, -3
    lw $t5, 2344($s1)
    bltzl $t9, .Ls_hg
    li $t7, 1
    srl $v0, $s3, 24
    beqz $v0, .Ls_hy
    ext $s2, $t5, 24, 8
    srl $v0, $s3, 8
    mult $v0, $s2

.Ls_ha:
    srl $v0, $s2, 3
    subu $s2, $s2, $v0
    mflo $v1
    sltu $v0, $t1, $v1
    bnez $v0, .Ls_ib
    addiu $t7, $v0, 2
    subu $t1, $t1, $v1
    blez $t9, .Ls_hf
    subu $s3, $s3, $v1
    srl $v0, $s3, 24
    beqz $v0, .Ls_ic
    srl $v0, $s3, 8

.Ls_hb:
    mult $v0, $s2
    srl $v0, $s2, 3
    subu $s2, $s2, $v0
    mflo $v1
    sltu $v0, $t1, $v1
    bnez $v0, .Ls_hz
    sll $t7, $t7, 1
    subu $t1, $t1, $v1
    beq $t9, $t0, .Ls_hf
    subu $s3, $s3, $v1
    srl $v0, $s3, 24
    beqz $v0, .Ls_ia

.Ls_hc:
    sll $t7, $t7, 1

.Ls_hd:
    srl $s3, $s3, 1

.Ls_he:
    sltu $v0, $t1, $s3
    subu $v1, $t1, $s3
    addiu $t9, $t9, -1
    movz $t1, $v1, $v0
    bne $t9, $t0, .Ls_hc
    addu $t7, $t7, $v0

.Ls_hf:
    ins $t5, $s2, 24, 8

.Ls_hg:
    srl $v0, $s3, 24
    beqz $v0, .Ls_hx
    ext $s2, $t5, 0, 8
    srl $v0, $s3, 8
    mult $v0, $s2

.Ls_hh:
    srl $v0, $s2, 3
    subu $s2, $s2, $v0
    mflo $v1
    sltu $v0, $t1, $v1
    bnez $v0, .Ls_hv
    sll $t7, $t7, 1
    ins $t5, $s2, 0, 8
    subu $t1, $t1, $v1
    blez $t8, .Ls_hm
    subu $s3, $s3, $v1

.Ls_hi:
    srl $v0, $s3, 24
    beqz $v0, .Ls_hw
    ext $s2, $t5, 8, 8
    srl $v0, $s3, 8
    mult $v0, $s2

.Ls_hj:
    srl $v0, $s2, 3
    subu $s2, $s2, $v0
    mflo $v1
    sltu $v0, $t1, $v1
    bnez $v0, .Ls_hu
    sll $t7, $t7, 1
    ins $t5, $s2, 8, 8
    subu $t1, $t1, $v1
    beq $t8, $t0, .Ls_hm
    subu $s3, $s3, $v1

.Ls_hk:
    srl $v0, $s3, 24
    beqz $v0, .Ls_hs
    ext $s2, $t5, 16, 8
    srl $v0, $s3, 8
    mult $v0, $s2

.Ls_hl:
    srl $v0, $s2, 3
    subu $s2, $s2, $v0
    mflo $v1
    sltu $v0, $t1, $v1
    bnez $v0, .Ls_ht
    sll $t7, $t7, 1
    ins $t5, $s2, 16, 8
    subu $t1, $t1, $v1
    subu $s3, $s3, $v1

.Ls_hm:
    addiu $t7, $t7, -1

.Ls_hn:
    subu $t9, $t3, $a0
    sltu $t9, $t7, $t9
    beqz $t9, .Ls_it
    sw $t5, 2344($s1)

.Ls_ho:
    ori $a1, $sp, 0x6
    addu $v0, $t3, $t6
    sltu $s1, $s4, $v0
    bnez $s1, .Ls_hq
    subu $v1, $t3, $t7
    cache 0x18, 63($t3)
    slti $s1, $t7, 3
    bnez $s1, .Ls_hr
    ins $a1, $v0, 0, 1
    ins $t6, $zr, 0, 2
    addu $t6, $t3, $t6

.Ls_hp:
    lwl $t9, 2($v1)
    lwr $t9, -1($v1)
    swl $t9, 3($t3)
    addiu $v1, $v1, 4
    beq $t3, $t6, .Ls_ga
    swr $t9, 0($t3)
    j .Ls_hp
    addiu $t3, $t3, 4

.Ls_hq:
    sltu $s1, $v0, $t4
    beqz $s1, .Ls_iu
    ins $a1, $v0, 0, 1

.Ls_hr:
    beq $t3, $v0, .Ls_fw
    lbu $t9, -1($v1)
    sb $t9, 0($t3)
    addiu $t3, $t3, 1
    j .Ls_hr
    addiu $v1, $v1, 1

.Ls_hs:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    mult $s3, $s2
    j .Ls_hl
    sll $s3, $s3, 8

.Ls_ht:
    addiu $s2, $s2, 31
    ins $t5, $s2, 16, 8
    j .Ls_hn
    mflo $s3

.Ls_hu:
    addiu $s2, $s2, 31
    ins $t5, $s2, 8, 8
    beq $t8, $t0, .Ls_hn
    mflo $s3
    j .Ls_hk
    addiu $t7, $t7, 1

.Ls_hv:
    addiu $s2, $s2, 31
    ins $t5, $s2, 0, 8
    blez $t8, .Ls_hn
    mflo $s3
    j .Ls_hi
    addiu $t7, $t7, 1

.Ls_hw:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    mult $s3, $s2
    j .Ls_hj
    sll $s3, $s3, 8

.Ls_hx:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    mult $s3, $s2
    j .Ls_hh
    sll $s3, $s3, 8

.Ls_hy:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    mult $s3, $s2
    j .Ls_ha
    sll $s3, $s3, 8

.Ls_hz:
    mflo $s3
    addiu $t7, $t7, 1
    beq $t9, $t0, .Ls_hf
    addiu $s2, $s2, 31
    srl $v0, $s3, 24
    bnez $v0, .Ls_hd
    sll $t7, $t7, 1

.Ls_ia:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    j .Ls_he
    sll $s3, $s3, 7

.Ls_ib:
    mflo $s3
    blez $t9, .Ls_hf
    addiu $s2, $s2, 31
    srl $v0, $s3, 24
    bnez $v0, .Ls_hb
    srl $v0, $s3, 8

.Ls_ic:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    sll $s3, $s3, 8
    j .Ls_hb
    srl $v0, $s3, 8

.Ls_id:
    lbu $s2, 5($a2)
    sb $v1, 2488($a1)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $s2
    addiu $a1, $a1, 8
    mult $s3, $t5
    sll $s3, $s1, 8
    srl $s2, $t5, 4
    mflo $s1
    sltu $t9, $t1, $s1
    beqz $t9, .Ls_ge
    subu $v1, $t5, $s2

.Ls_ie:
    bne $t8, $t2, .Ls_gd
    addiu $t8, $t8, 1
    addiu $v1, $v1, 15
    j .Ls_gf
    mflo $s3

.Ls_if:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    mult $s3, $a1
    j .Ls_gr
    sll $s3, $s3, 8

.Ls_ig:
    addiu $a1, $a1, 31
    addiu $t6, $t6, 1
    bne $t6, $s5, .Ls_gs
    mflo $s3

.Ls_ih:
    subu $v0, $t3, $a0
    beqz $a3, .Ls_ii
    addiu $a2, $a2, 5
    sw $a2, 0($a3)

.Ls_ii:
    lw $s5, 2828($sp)
    lw $s4, 2824($sp)
    lw $s3, 2820($sp)
    lw $s2, 2816($sp)
    lw $s1, 2812($sp)
    lw $s0, 2808($sp)
    jr $ra
    addiu $sp, $sp, 2832

.Ls_ij:
    addiu $a1, $a1, 31
    sb $a1, 2560($s1)
    addiu $t6, $t6, 1
    beq $t8, $t0, .Ls_gt
    mflo $s3
    j .Ls_gq
    lbu $a1, 2568($s1)

.Ls_ik:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    mult $s3, $a1
    j .Ls_gp
    sll $s3, $s3, 8

.Ls_il:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    mult $s3, $a1
    sll $s3, $s3, 8
    srl $v0, $a1, 3
    subu $a1, $a1, $v0
    mflo $v1
    sltu $v0, $t1, $v1
    beqz $v0, .Ls_gn
    sll $t6, $t6, 1

.Ls_im:
    addiu $a1, $a1, 31
    sb $a1, 2552($s1)
    mflo $s3
    blez $t8, .Ls_gt
    addiu $t6, $t6, 1
    j .Ls_go
    lbu $a1, 2560($s1)

.Ls_in:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    mult $s3, $s2
    j .Ls_gg
    sll $s3, $s3, 8

.Ls_io:
    mflo $s3
    addiu $t6, $t6, 1
    addiu $s2, $s2, 31
    beq $t9, $t0, .Ls_gl
    srl $v1, $s3, 24
    bnez $v1, .Ls_gj
    sll $t6, $t6, 1

.Ls_ip:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    j .Ls_gk
    sll $s3, $s3, 7

.Ls_iq:
    mflo $s3
    addiu $s2, $s2, 31
    blez $t9, .Ls_gl
    srl $v1, $s3, 24
    bnez $v1, .Ls_gh
    srl $v0, $s3, 8

.Ls_ir:
    lbu $v1, 5($a2)
    sll $t1, $t1, 8
    addiu $a2, $a2, 1
    addu $t1, $t1, $v1
    sll $s3, $s3, 8
    j .Ls_gh
    srl $v0, $s3, 8

.Ls_is:
    subu $s3, $s3, $v1
    subu $t1, $t1, $v1
    bltz $t8, .Ls_gx
    sb $t7, 2033($t9)
    bnezl $t8, .Ls_gz
    addiu $t8, $t8, -8
    bne $t5, $t3, .Ls_ho
    addu $t7, $zr, $zr

.Ls_it:
    lui $v0, 0x8000
    j .Ls_ii
    ori $v0, $v0, 0x108

.Ls_iu:
    lui $v0, 0x8000
    j .Ls_ii
    ori $v0, $v0, 0x104

.Ls_iv:
    addu $t1, $t1, $a0
    sltu $v1, $t1, $t4
    beqz $v1, .Ls_iu
    nop

.Ls_iw:
    lbu $v1, 5($a2)
    addiu $t3, $t3, 1
    beq $t3, $t1, .Ls_ih
    sb $v1, 0($t3)
    j .Ls_iw
    addiu $a2, $a2, 1
    .size UtilsForKernel_6C6887EE, .-UtilsForKernel_6C6887EE

.data
# 13298
g_md5Tbl:
    .word 0xD76AA478
    .word 0xE8C7B756
    .word 0x242070DB
    .word 0xC1BDCEEE
    .word 0xF57C0FAF
    .word 0x4787C62A
    .word 0xA8304613
    .word 0xFD469501
    .word 0x698098D8
    .word 0x8B44F7AF
    .word 0xFFFF5BB1
    .word 0x895CD7BE
    .word 0x6B901122
    .word 0xFD987193
    .word 0xA679438E
    .word 0x49B40821
    .word 0xF61E2562
    .word 0xC040B340
    .word 0x265E5A51
    .word 0xE9B6C7AA
    .word 0xD62F105D
    .word 0x02441453
    .word 0xD8A1E681
    .word 0xE7D3FBC8
    .word 0x21E1CDE6
    .word 0xC33707D6
    .word 0xF4D50D87
    .word 0x455A14ED
    .word 0xA9E3E905
    .word 0xFCEFA3F8
    .word 0x676F02D9
    .word 0x8D2A4C8A
    .word 0xFFFA3942
    .word 0x8771F681
    .word 0x6D9D6122
    .word 0xFDE5380C
    .word 0xA4BEEA44
    .word 0x4BDECFA9
    .word 0xF6BB4B60
    .word 0xBEBFBC70
    .word 0x289B7EC6
    .word 0xEAA127FA
    .word 0xD4EF3085
    .word 0x04881D05
    .word 0xD9D4D039
    .word 0xE6DB99E5
    .word 0x1FA27CF8
    .word 0xC4AC5665
    .word 0xF4292244
    .word 0x432AFF97
    .word 0xAB9423A7
    .word 0xFC93A039
    .word 0x655B59C3
    .word 0x8F0CCC92
    .word 0xFFEFF47D
    .word 0x85845DD1
    .word 0x6FA87E4F
    .word 0xFE2CE6E0
    .word 0xA3014314
    .word 0x4E0811A1
    .word 0xF7537E82
    .word 0xBD3AF235
    .word 0x2AD7D2BB
    .word 0xEB86D391

# 13398
g_sha1Tbl:
    .word 0x67452301
    .word 0xEFCDAB89
    .word 0x98BADCFE
    .word 0x10325476
    .word 0xC3D2E1F0

# 133C0
g_defltTbl:
    .word 0x0032FFF9
    .word 0x131001C4
    .word 0x0708001B
    .word 0x050A0609
    .word 0x030C040B
    .word 0x010E020D
    .word 0x0000000F
    .word 0x00180000
    .word 0x020000C8
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0xFFD0FFE7
    .word 0xFFB0FFC0
    .word 0xFF90FFA0
    .word 0xFF70FF80
    .word 0xFF5FFF60
    .word 0xFF1FFF3F
    .word 0xFED7FEFF
    .word 0xFE57FE97
    .word 0xFDDBFE17
    .word 0xFCDBFD5B
    .word 0xFBD3FC5B
    .word 0xF9D3FAD3
    .word 0xF7DDF8D3
    .word 0xF3DDF5DD
    .word 0xEFE0F1DD
    .word 0xFFFFFFFF
    .word 0x00010000
    .word 0x00030002
    .word 0x00060004
    .word 0x000C0008
    .word 0x00180010
    .word 0x00300020
    .word 0x00600040
    .word 0x00C00080
    .word 0x01800100
    .word 0x03000200
    .word 0x06000400
    .word 0x0C000800
    .word 0x18001000
    .word 0x30002000
    .word 0x60004000
    .word 0xFFFBFFFB

# 13B40
    .globl g_GetGPI
g_GetGPI:
    .word 0
# 13B44
g_GPIMask:
    .word 0xFF
# 13B48
    .globl g_SetGPO
g_SetGPO:
    .word 0
# 13B4C
g_GPOMask:
    .word 0xFF

# Alignment..

# 13B80
    .globl g_GetPTRIG
g_GetPTRIG:
    .word 0
# 13B84
    .globl g_SetPTRIG
g_SetPTRIG:
    .word 0
# 13B88
g_PTRIGMask:
    .word 1

# Alignment..

# 13BC0
g_UserLog:
    .word 0

# 13BE0
g_suspendCtx:
    .space 1024

