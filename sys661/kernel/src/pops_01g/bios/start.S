/*
 * PlayStation 1 R3000A ROM reset entry (0xBFC00000), cache flush,
 * exception/syscall vectors, and register-context switch primitives.
 */

    .set noreorder
    .set noat

    .section .text.boot, "ax"

    .globl _start
    .type _start, @function
_start:
    /* Configure PS1 memory controller & bus timings (matches SCPH-101 0xBFC00000) */
    lui     $t0, 0x1F80
    li      $t1, 0x0013243F
    sw      $t1, 0x1010($t0)    /* BIOS ROM delay */
    li      $t1, 0x00000B88
    sw      $t1, 0x1060($t0)    /* RAM_SIZE: 2 MB */
    li      $t1, 0x00031125
    sw      $t1, 0x1020($t0)    /* COM_DELAY */
    lui     $t1, 0x1F00
    sw      $t1, 0x1000($t0)    /* EXP1 base */
    lui     $t1, 0x1F80
    ori     $t1, $t1, 0x2000
    sw      $t1, 0x1004($t0)    /* EXP2 base */
    li      $t1, 0x0013243F
    sw      $t1, 0x1008($t0)    /* EXP1 delay */
    li      $t1, 0x200931E1
    sw      $t1, 0x1014($t0)    /* SPU delay */
    li      $t1, 0x00020843
    sw      $t1, 0x1018($t0)    /* CDROM delay */
    li      $t1, 0x00003022
    sw      $t1, 0x100C($t0)    /* EXP3 delay */
    li      $t1, 0x00070777
    sw      $t1, 0x101C($t0)    /* EXP2 delay */

    /* Clear CP0 debug/status/cause and jump past 0xBFC00108 ROM ID string */
    mtc0    $zero, $7
    mtc0    $zero, $3
    mtc0    $zero, $5
    mtc0    $zero, $6
    mtc0    $zero, $9
    mtc0    $zero, $11
    mtc0    $zero, $12
    mtc0    $zero, $13
    j       biosResetContinue
    nop
    .size _start, . - _start

    /* Fixed ROM identification strings at 0xBFC00108 / 0xBFC0012C */
    .section .rodata.romid, "a"
    .asciz "Sony Computer Entertainment Inc."
    .balign 4
    .asciz "CEX-3000/1001/1002 by K.S."

    .text

biosResetContinue:
    /* Isolate & clear 4 KB instruction cache via 0xFFFE0130 */
    lui     $t0, 0xFFFE
    li      $t1, 0x00000804
    sw      $t1, 0x0130($t0)
    li      $t1, 0x00010000     /* SR.IsC = 1 */
    mtc0    $t1, $12
    nop
    move    $t2, $zero
    li      $t3, 0x00000F80
1:
    sw      $zero, 0x00($t2)
    sw      $zero, 0x10($t2)
    sw      $zero, 0x20($t2)
    sw      $zero, 0x30($t2)
    sw      $zero, 0x40($t2)
    sw      $zero, 0x50($t2)
    sw      $zero, 0x60($t2)
    sw      $zero, 0x70($t2)
    bne     $t2, $t3, 1b
    addiu   $t2, $t2, 0x80
    mtc0    $zero, $12
    nop
    li      $t1, 0x0001E988     /* Enable I-cache + scratchpad */
    sw      $t1, 0x0130($t0)

    /* Mute SPU master & reverb outputs */
    lui     $t0, 0xBF80
    sh      $zero, 0x1D80($t0)
    sh      $zero, 0x1D82($t0)
    sh      $zero, 0x1D84($t0)
    sh      $zero, 0x1D86($t0)

    /* Initialize kernel stack at 0x8000FFF0 and jump into C boot */
    lui     $sp, 0x8001
    addiu   $sp, $sp, -16
    jal     biosMainBoot
    nop
2:
    b       2b
    nop

/* R3000 instruction cache flush (A0:44 FlushCache) */
    .globl biosFlushCache
    .type biosFlushCache, @function
biosFlushCache:
    mfc0    $t4, $12
    nop
    mtc0    $zero, $12
    nop
    lui     $t0, 0xFFFE
    li      $t1, 0x00000804
    sw      $t1, 0x0130($t0)
    li      $t1, 0x00010000
    mtc0    $t1, $12
    nop
    move    $t2, $zero
    li      $t3, 0x00000F80
1:
    sw      $zero, 0x00($t2)
    sw      $zero, 0x10($t2)
    sw      $zero, 0x20($t2)
    sw      $zero, 0x30($t2)
    sw      $zero, 0x40($t2)
    sw      $zero, 0x50($t2)
    sw      $zero, 0x60($t2)
    sw      $zero, 0x70($t2)
    bne     $t2, $t3, 1b
    addiu   $t2, $t2, 0x80
    mtc0    $zero, $12
    nop
    li      $t1, 0x0001E988
    sw      $t1, 0x0130($t0)
    mtc0    $t4, $12
    nop
    jr      $ra
    nop
    .size biosFlushCache, . - biosFlushCache

/* A0/B0/C0 syscall dispatch trampolines copied to 0x800000A0/B0/C0 */
    .globl biosA0Vector
biosA0Vector:
    lui     $t0, 0x8000
    sll     $t2, $t1, 2
    addu    $t0, $t0, $t2
    lw      $t0, 0x0200($t0)
    nop
    jr      $t0
    nop

    .globl biosB0Vector
biosB0Vector:
    lui     $t0, 0x8000
    sll     $t2, $t1, 2
    addu    $t0, $t0, $t2
    lw      $t0, 0x0874($t0)
    nop
    jr      $t0
    nop

    .globl biosC0Vector
biosC0Vector:
    lui     $t0, 0x8000
    sll     $t2, $t1, 2
    addu    $t0, $t0, $t2
    lw      $t0, 0x0674($t0)
    nop
    jr      $t0
    nop

    .globl biosBrkVector
biosBrkVector:
    jr      $ra
    nop

/* General exception vector installed at 0x80000080 (jumps to biosExceptionHandler) */
    .globl biosExceptionVector
biosExceptionVector:
    lui     $k0, %hi(biosExceptionHandler)
    addiu   $k0, $k0, %lo(biosExceptionHandler)
    jr      $k0
    nop

/* Full R3000 exception save/dispatch/restore into current TCB */
    .globl biosExceptionHandler
    .type biosExceptionHandler, @function
biosExceptionHandler:
    lui     $k0, 0x8000
    lw      $k0, 0x0108($k0)    /* PCB pointer */
    nop
    lw      $k0, 0x0000($k0)    /* current TCB */
    nop
    /* Save GPRs $1..$31 into tcb->regs[1..31] (offset 8 + 4*n) */
    sw      $1,  0x0C($k0)
    sw      $2,  0x10($k0)
    sw      $3,  0x14($k0)
    sw      $4,  0x18($k0)
    sw      $5,  0x1C($k0)
    sw      $6,  0x20($k0)
    sw      $7,  0x24($k0)
    sw      $8,  0x28($k0)
    sw      $9,  0x2C($k0)
    sw      $10, 0x30($k0)
    sw      $11, 0x34($k0)
    sw      $12, 0x38($k0)
    sw      $13, 0x3C($k0)
    sw      $14, 0x40($k0)
    sw      $15, 0x44($k0)
    sw      $16, 0x48($k0)
    sw      $17, 0x4C($k0)
    sw      $18, 0x50($k0)
    sw      $19, 0x54($k0)
    sw      $20, 0x58($k0)
    sw      $21, 0x5C($k0)
    sw      $22, 0x60($k0)
    sw      $23, 0x64($k0)
    sw      $24, 0x68($k0)
    sw      $25, 0x6C($k0)
    sw      $28, 0x78($k0)
    sw      $29, 0x7C($k0)
    sw      $30, 0x80($k0)
    sw      $31, 0x84($k0)
    mfc0    $k1, $14            /* EPC */
    mfhi    $t0
    mflo    $t1
    mfc0    $t2, $12            /* SR */
    mfc0    $t3, $13            /* CAUSE */
    sw      $k1, 0x88($k0)
    sw      $t0, 0x8C($k0)
    sw      $t1, 0x90($k0)
    sw      $t2, 0x94($k0)
    sw      $t3, 0x98($k0)

    /* Switch to kernel exception stack at 0x8000FFE0 and dispatch in C */
    move    $a0, $k0
    lui     $sp, 0x8001
    addiu   $sp, $sp, -32
    jal     biosExceptionDispatch
    nop

    /* Fall through to biosReturnFromException */
    .globl biosReturnFromException
    .type biosReturnFromException, @function
biosReturnFromException:
    lui     $k0, 0x8000
    lw      $k0, 0x0108($k0)
    nop
    lw      $k0, 0x0000($k0)
    nop
    lw      $t0, 0x8C($k0)      /* hi */
    lw      $t1, 0x90($k0)      /* lo */
    lw      $t2, 0x94($k0)      /* sr */
    mthi    $t0
    mtlo    $t1
    mtc0    $t2, $12
    lw      $1,  0x0C($k0)
    lw      $2,  0x10($k0)
    lw      $3,  0x14($k0)
    lw      $4,  0x18($k0)
    lw      $5,  0x1C($k0)
    lw      $6,  0x20($k0)
    lw      $7,  0x24($k0)
    lw      $8,  0x28($k0)
    lw      $9,  0x2C($k0)
    lw      $10, 0x30($k0)
    lw      $11, 0x34($k0)
    lw      $12, 0x38($k0)
    lw      $13, 0x3C($k0)
    lw      $14, 0x40($k0)
    lw      $15, 0x44($k0)
    lw      $16, 0x48($k0)
    lw      $17, 0x4C($k0)
    lw      $18, 0x50($k0)
    lw      $19, 0x54($k0)
    lw      $20, 0x58($k0)
    lw      $21, 0x5C($k0)
    lw      $22, 0x60($k0)
    lw      $23, 0x64($k0)
    lw      $24, 0x68($k0)
    lw      $25, 0x6C($k0)
    lw      $28, 0x78($k0)
    lw      $29, 0x7C($k0)
    lw      $30, 0x80($k0)
    lw      $31, 0x84($k0)
    lw      $k1, 0x88($k0)      /* EPC */
    nop
    jr      $k1
    rfe
    .size biosReturnFromException, . - biosReturnFromException

/* Syscall 1 / 2 wrappers for EnterCriticalSection / ExitCriticalSection */
    .globl biosEnterCriticalSection
biosEnterCriticalSection:
    li      $a0, 1
    syscall 0
    jr      $ra
    nop

    .globl biosExitCriticalSection
biosExitCriticalSection:
    li      $a0, 2
    syscall 0
    jr      $ra
    nop

/* Standard MIPS R3000 setjmp / longjmp (A0:13 / A0:14) */
    .globl biosSetjmp
biosSetjmp:
    sw      $ra, 0x00($a0)
    sw      $sp, 0x04($a0)
    sw      $fp, 0x08($a0)
    sw      $s0, 0x0C($a0)
    sw      $s1, 0x10($a0)
    sw      $s2, 0x14($a0)
    sw      $s3, 0x18($a0)
    sw      $s4, 0x1C($a0)
    sw      $s5, 0x20($a0)
    sw      $s6, 0x24($a0)
    sw      $s7, 0x28($a0)
    sw      $gp, 0x2C($a0)
    jr      $ra
    move    $v0, $zero

    .globl biosLongjmp
biosLongjmp:
    lw      $ra, 0x00($a0)
    lw      $sp, 0x04($a0)
    lw      $fp, 0x08($a0)
    lw      $s0, 0x0C($a0)
    lw      $s1, 0x10($a0)
    lw      $s2, 0x14($a0)
    lw      $s3, 0x18($a0)
    lw      $s4, 0x1C($a0)
    lw      $s5, 0x20($a0)
    lw      $s6, 0x24($a0)
    lw      $s7, 0x28($a0)
    lw      $gp, 0x2C($a0)
    bne     $a1, $zero, 1f
    move    $v0, $a1
    li      $v0, 1
1:
    jr      $ra
    nop

/* Enter a loaded PS-X EXE with its requested PC, GP, SP, FP */
    .globl biosJumpToExe
biosJumpToExe:
    move    $gp, $a1
    beq     $a2, $zero, 1f
    nop
    move    $sp, $a2
    move    $fp, $a3
1:
    jr      $a0
    nop
