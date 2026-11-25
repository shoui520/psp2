# Copyright (C) 2011, 2012, 2013 The uOFW team
# See the file COPYING for copying permission.

#include "common_asm.h"

    .text

    .globl initClearMem

# initClearMem(u32 *baseAddr, u32 size)
# Copied to the reset vector (0xBFC00000) to zero all RAM (including stack) twice and halt.
    .type initClearMem, @function
    .ent initClearMem
initClearMem:
    addu       $v0, $a0, $zr
    addu       $t0, $a0, $a1
    addiu      $t0, $t0, -4
clearPassFirst:
    sw         $zr, 0($a0)
    bne        $t0, $a0, clearPassFirst
    addiu      $a0, $a0, 4
clearPassSecond:
    sw         $zr, 0($v0)
    bne        $t0, $v0, clearPassSecond
    addiu      $v0, $v0, 4
    break
    .end initClearMem
    .size initClearMem, . - initClearMem
