# Copyright (C) 2011, 2012, 2013 The uOFW team
# See the file COPYING for copying permission.

#include "common_asm.h"

    .text

    .global loadCoreCpuSuspendIntr
    .global loadCoreCpuResumeIntr
    .global loadCoreBreakHalt

    .ent loadCoreCpuSuspendIntr
loadCoreCpuSuspendIntr:
    mfic       $v0, INTR_CTRL_STATE
	mtic       $zero, INTR_CTRL_STATE
	beqz       $v0, loc_000076A4
	nop        
	nop        
	nop        
	nop        
	nop        
	nop        
loc_000076A4: 
	jr         $ra
	nop    
    .end loadCoreCpuSuspendIntr

    .ent loadCoreCpuResumeIntr
loadCoreCpuResumeIntr:
    mtic       $a0, INTR_CTRL_STATE
	nop        
	nop        
	nop        
	nop        
	jr         $ra
	nop     
    .end loadCoreCpuResumeIntr

    .ent loadCoreBreakHalt
loadCoreBreakHalt:
    break
    j          loadCoreBreakHalt
    nop
    .end loadCoreBreakHalt


