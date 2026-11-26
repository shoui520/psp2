#include "common_asm.h"

    .globl suspendIntr
    .type suspendIntr, @function
suspendIntr:
    mfic $v0, $0
    mtic $zr, $0
    beqz $v0, .Lintr_end
    nop
    nop

.Lintr_wait:
    nop
    nop
    nop
    nop

.Lintr_end:
    jr $ra
    nop
    .size suspendIntr, .-suspendIntr

    .type suspendIntrSyncPad, @function
suspendIntrSyncPad:
    sync
    nop
    .size suspendIntrSyncPad, .-suspendIntrSyncPad

    .globl resumeIntr
    .type resumeIntr, @function
resumeIntr:
    mtic $a0, $0
    j .Lintr_wait
    nop
    .size resumeIntr, .-resumeIntr



