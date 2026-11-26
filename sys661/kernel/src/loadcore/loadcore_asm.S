# Copyright (C) 2011, 2012, 2013 The uOFW team
# See the file COPYING for copying permission.

#include "common_asm.h"

    .text

    .globl loadCoreClearMem
    .globl sceLoadCorePrimarySyscallHandler

    .global g_ToolBreakMode
    .global g_SyscallIntrRegSave

# loadCoreClearMem(u32 *baseAddr, u32 size)
# zeroes out memory range starting at "baseAddr"
# for "size" bytes, twice, then halts via break.

    .ent loadCoreClearMem
loadCoreClearMem:
    addu    $v0, $a0, $zr
    addu    $t0, $a0, $a1
    addiu   $t0, $t0, -4
.Lclear_first:
    sw      $zr, 0($a0)
    bne     $t0, $a0, .Lclear_first
    addiu   $a0, $a0, 4
.Lclear_second:
    sw      $zr, 0($v0)
    bne     $t0, $v0, .Lclear_second
    addiu   $v0, $v0, 4
    break
    .end loadCoreClearMem

    .ent sceLoadCorePrimarySyscallHandler
sceLoadCorePrimarySyscallHandler:
    lui     $v0, %hi(g_ToolBreakMode)
    lw      $v0, %lo(g_ToolBreakMode)($v0)
    beq     $zr, $v0, .Lnot_linked_error
    nop
    lui     $v0, %hi(g_SyscallIntrRegSave)
    addiu   $v0, $v0, %lo(g_SyscallIntrRegSave)
    cfc0    $v1, COP0_CTRL_TCB
    sw      $v0, 0($v1)
    cfc0    $v1, COP0_CTRL_STATUS
    sw      $v1, 0($v0)
    mfc0    $v1, COP0_STATE_EPC
    addiu   $v1, $v1, -4
    mtc0    $v1, COP0_STATE_EPC
    sw      $v1, 4($v0)
    sw      $sp, 8($v0)
    sw      $ra, 12($v0)
    sw      $k1, 16($v0)
    cfc1    $v1, COP1_CTRL_FCSR
    sw      $v1, 20($v0)
    cfc0    $v1, COP0_CTRL_V0
    sw      $v1, 24($v0)
    break   0x40, 0x000
    nop
.Lnot_linked_error:
    lui     $v0, %hi(SCE_ERROR_KERNEL_LIBRARY_IS_NOT_LINKED)
    ori     $v0, $v0, %lo(SCE_ERROR_KERNEL_LIBRARY_IS_NOT_LINKED)
    mtc0    $ra, COP0_STATE_EPC
    nop
    nop
    eret
    .end sceLoadCorePrimarySyscallHandler
