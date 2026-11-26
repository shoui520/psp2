# Copyright (C) 2011, 2012, 2013 The uOFW team
# See the file COPYING for copying permission.

#include "common_asm.h"

    .text

    .global loadCoreCpuSuspendIntr
    .global loadCoreSync
    .global loadCoreCpuResumeIntr

    .ent loadCoreCpuSuspendIntr
loadCoreCpuSuspendIntr:
    mfic    $v0, INTR_CTRL_STATE
    mtic    $zero, INTR_CTRL_STATE
    beqz    $v0, .Lintr_done
    nop
    nop
.Lintr_resume_tail:
    nop
    nop
    nop
    nop
.Lintr_done:
    jr      $ra
    nop
    .end loadCoreCpuSuspendIntr

    .ent loadCoreSync
loadCoreSync:
    sync
    nop
    .end loadCoreSync

    .ent loadCoreCpuResumeIntr
loadCoreCpuResumeIntr:
    mtic    $a0, INTR_CTRL_STATE
    j       .Lintr_resume_tail
    nop
    .end loadCoreCpuResumeIntr
