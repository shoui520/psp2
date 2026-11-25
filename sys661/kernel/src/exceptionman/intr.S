#include "common_asm.h"

    .globl suspendIntr
    .type suspendIntr, @function
suspendIntr:
    mfic $v0, $0
    mtic $zr, $0
    beqz $v0, ret
    nop
    nop

retWithNop:
    nop
    nop
    nop
    nop

ret:
    jr $ra
    nop
    .size suspendIntr, . - suspendIntr

    .globl intrSyncBarrier
    .type intrSyncBarrier, @function
intrSyncBarrier:
    sync
    nop
    .size intrSyncBarrier, . - intrSyncBarrier

    .globl resumeIntr
    .type resumeIntr, @function
resumeIntr:
    mtic $a0, $0
    j    retWithNop
    nop
    .size resumeIntr, . - resumeIntr
