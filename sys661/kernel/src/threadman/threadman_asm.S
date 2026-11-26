    .set noreorder
    .set noat
    .text

    .global tman_idle_loop
    .ent tman_idle_loop
tman_idle_loop:
    jal ExceptionManagerForKernel_96D47F7E
    nop
    lui $t1, 0xbc20
    mfic $t2, $0
    mtic $zero, $0
    lui $t3, 0x0001
    ori $t3, $t3, 0x0100
    lui $t4, 0x0000
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
1:
    lw $t6, 0($t4)
    mtc0 $v0, $25
    cache 0x0a, 0($v0)
    cache 0x0a, 64($v0)
    cache 0x0a, 0($t6)
    cache 0x0a, 64($t6)
    lw $t5, 0($t1)
    sw $t3, 0($t1)
    .word 0x70000000
    sw $t5, 0($t1)
    mtic $t2, $0
    mtic $zero, $0
    j 1b
    nop
    .end tman_idle_loop

    .p2align 4
    .global tman_vfpu_save
    .ent tman_vfpu_save
tman_vfpu_save:
    mfc0 $t0, $12
    lui $at, 0x4000
    or $t7, $t0, $at
    mtc0 $t7, $12
    nop
    cache 0x18, 0x000($a0)
    sv.q C000.q, 0x000($a0)
    sv.q C010.q, 0x010($a0)
    sv.q C020.q, 0x020($a0)
    sv.q C030.q, 0x030($a0)
    cache 0x18, 0x040($a0)
    sv.q C100.q, 0x040($a0)
    sv.q C110.q, 0x050($a0)
    sv.q C120.q, 0x060($a0)
    sv.q C130.q, 0x070($a0)
    cache 0x18, 0x080($a0)
    sv.q C200.q, 0x080($a0)
    sv.q C210.q, 0x090($a0)
    sv.q C220.q, 0x0A0($a0)
    sv.q C230.q, 0x0B0($a0)
    cache 0x18, 0x0C0($a0)
    sv.q C300.q, 0x0C0($a0)
    sv.q C310.q, 0x0D0($a0)
    sv.q C320.q, 0x0E0($a0)
    sv.q C330.q, 0x0F0($a0)
    cache 0x18, 0x100($a0)
    sv.q C400.q, 0x100($a0)
    sv.q C410.q, 0x110($a0)
    sv.q C420.q, 0x120($a0)
    sv.q C430.q, 0x130($a0)
    cache 0x18, 0x140($a0)
    sv.q C500.q, 0x140($a0)
    sv.q C510.q, 0x150($a0)
    sv.q C520.q, 0x160($a0)
    sv.q C530.q, 0x170($a0)
    cache 0x18, 0x180($a0)
    sv.q C600.q, 0x180($a0)
    sv.q C610.q, 0x190($a0)
    sv.q C620.q, 0x1A0($a0)
    sv.q C630.q, 0x1B0($a0)
    cache 0x18, 0x1C0($a0)
    sv.q C700.q, 0x1C0($a0)
    sv.q C710.q, 0x1D0($a0)
    sv.q C720.q, 0x1E0($a0)
    sv.q C730.q, 0x1F0($a0)
    cache 0x18, 0x200($a0)
    mfvc $t7, $128
    mfvc $t6, $129
    mfvc $t5, $130
    mfvc $t4, $131
    mfvc $t3, $132
    sw $t7, 0x200($a0)
    sw $t6, 0x204($a0)
    sw $t5, 0x208($a0)
    sw $t4, 0x20C($a0)
    sw $t3, 0x210($a0)
    mfvc $t7, $136
    mfvc $t6, $137
    mfvc $t5, $138
    mfvc $t4, $139
    sw $t7, 0x220($a0)
    sw $t6, 0x224($a0)
    sw $t5, 0x228($a0)
    sw $t4, 0x22C($a0)
    mfvc $t7, $140
    mfvc $t6, $141
    mfvc $t5, $142
    mfvc $t4, $143
    sw $t7, 0x230($a0)
    sw $t6, 0x234($a0)
    sw $t5, 0x238($a0)
    sw $t4, 0x23C($a0)
    vsync
    vnop
    mtc0 $t0, $12
    nop
    jr $ra
    nop
    .end tman_vfpu_save

    .global tman_vfpu_restore
    .ent tman_vfpu_restore
tman_vfpu_restore:
    mfc0 $t0, $12
    lui $at, 0x4000
    or $t7, $t0, $at
    mtc0 $t7, $12
    nop
    vsync
    lv.q C000.q, 0x000($a0)
    lv.q C010.q, 0x010($a0)
    lv.q C020.q, 0x020($a0)
    lv.q C030.q, 0x030($a0)
    lv.q C100.q, 0x040($a0)
    lv.q C110.q, 0x050($a0)
    lv.q C120.q, 0x060($a0)
    lv.q C130.q, 0x070($a0)
    lv.q C200.q, 0x080($a0)
    lv.q C210.q, 0x090($a0)
    lv.q C220.q, 0x0A0($a0)
    lv.q C230.q, 0x0B0($a0)
    lv.q C300.q, 0x0C0($a0)
    lv.q C310.q, 0x0D0($a0)
    lv.q C320.q, 0x0E0($a0)
    lv.q C330.q, 0x0F0($a0)
    lv.q C400.q, 0x100($a0)
    lv.q C410.q, 0x110($a0)
    lv.q C420.q, 0x120($a0)
    lv.q C430.q, 0x130($a0)
    lv.q C500.q, 0x140($a0)
    lv.q C510.q, 0x150($a0)
    lv.q C520.q, 0x160($a0)
    lv.q C530.q, 0x170($a0)
    lv.q C600.q, 0x180($a0)
    lv.q C610.q, 0x190($a0)
    lv.q C620.q, 0x1A0($a0)
    lv.q C630.q, 0x1B0($a0)
    lv.q C700.q, 0x1C0($a0)
    lv.q C710.q, 0x1D0($a0)
    lv.q C720.q, 0x1E0($a0)
    lv.q C730.q, 0x1F0($a0)
    lw $t7, 0x200($a0)
    lw $t6, 0x204($a0)
    lw $t5, 0x208($a0)
    lw $t4, 0x20C($a0)
    lw $t3, 0x210($a0)
    mtvc $t7, $128
    mtvc $t6, $129
    mtvc $t5, $130
    mtvc $t4, $131
    mtvc $t3, $132
    lw $t7, 0x220($a0)
    lw $t6, 0x224($a0)
    lw $t5, 0x228($a0)
    lw $t4, 0x22C($a0)
    mtvc $t7, $136
    mtvc $t6, $137
    mtvc $t5, $138
    mtvc $t4, $139
    lw $t7, 0x230($a0)
    lw $t6, 0x234($a0)
    lw $t5, 0x238($a0)
    lw $t4, 0x23C($a0)
    mtvc $t7, $140
    mtvc $t6, $141
    mtvc $t5, $142
    mtvc $t4, $143
    mtc0 $t0, $12
    nop
    jr $ra
    nop
    .end tman_vfpu_restore

    .global tman_vfpu_flush
    .ent tman_vfpu_flush
tman_vfpu_flush:
    mfc0 $t0, $12
    lui $at, 0x4000
    or $t7, $t0, $at
    mtc0 $t7, $12
    nop
    vsync
    vflush
    .word 0x486000ff /* mfvc $zero, VFPU_RCX7 */
    mtc0 $t0, $12
    nop
    jr $ra
    nop
    nop
    nop
.Lexc_deferred_write:
    lw $v0, 8($v1)
    beq $v0, $zero, .Lexc_return
    addiu $v0, $v0, -1
    bne $v0, $zero, .Lexc_return
    sw $v0, 8($v1)
    lw $v0, 4($v1)
    lw $v1, 0($v1)
    sw $v0, 0($v1)
    j .Lexc_return
    sync
.Lexc_chain_prev:
    lui $v0, %hi(tman_fpu_exc_handler)
    lw $v0, %lo(tman_fpu_exc_handler)($v0)
    jr $v0
    nop
    .global tman_fpu_exc_handler
tman_fpu_exc_handler:
    .word 0
    .word 0
    mfc0 $v1, $13
    ext $v1, $v1, 28, 2
    xori $v0, $v1, 1
    bne $v0, $zero, .Lexc_chain_prev
    cfc0 $v0, $13
    bne $v0, $zero, .Lexc_in_intr
    lui $v0, %hi(g_tman)
    lw $v0, %lo(g_tman)($v0)
    lw $v1, 20($v0)
    andi $v1, $v1, 0x2000
    bne $v1, $zero, .Lexc_chain_prev
    addiu $v1, $zero, 1
    sw $v1, 24($v0)
    cfc0 $v0, $2
    ins $v0, $v1, 29, 1
    mtc0 $v0, $12
    cfc0 $v1, $8
    bgez $v1, .Lexc_restore_fpu
    ctc0 $v0, $2
    lb $v0, 36($v1)
    xori $v0, $v0, 1
    beq $v0, $zero, .Lexc_save_callee_fpu
    cache 0x18, 256($v1)
    cache 0x18, 192($v1)
    swc1 $f0,  192($v1)
    swc1 $f1,  196($v1)
    swc1 $f2,  200($v1)
    swc1 $f3,  204($v1)
    swc1 $f4,  208($v1)
    swc1 $f5,  212($v1)
    swc1 $f6,  216($v1)
    swc1 $f7,  220($v1)
    swc1 $f8,  224($v1)
    swc1 $f9,  228($v1)
    swc1 $f10, 232($v1)
    swc1 $f11, 236($v1)
    swc1 $f12, 240($v1)
    swc1 $f13, 244($v1)
    swc1 $f14, 248($v1)
    swc1 $f15, 252($v1)
    swc1 $f16, 256($v1)
    swc1 $f17, 260($v1)
    swc1 $f18, 264($v1)
    swc1 $f19, 268($v1)
.Lexc_save_callee_fpu:
    swc1 $f20, 272($v1)
    swc1 $f21, 276($v1)
    swc1 $f22, 280($v1)
    swc1 $f23, 284($v1)
    swc1 $f24, 288($v1)
    swc1 $f25, 292($v1)
    swc1 $f26, 296($v1)
    swc1 $f27, 300($v1)
    swc1 $f28, 304($v1)
    swc1 $f29, 308($v1)
    swc1 $f30, 312($v1)
    swc1 $f31, 316($v1)
.Lexc_restore_fpu:
    cfc0 $v0, $14
    ctc0 $v0, $8
    lb $v1, 36($v0)
    lwc1 $f20, 272($v0)
    lwc1 $f21, 276($v0)
    lwc1 $f22, 280($v0)
    lwc1 $f23, 284($v0)
    lwc1 $f24, 288($v0)
    lwc1 $f25, 292($v0)
    lwc1 $f26, 296($v0)
    lwc1 $f27, 300($v0)
    lwc1 $f28, 304($v0)
    lwc1 $f29, 308($v0)
    lwc1 $f30, 312($v0)
    lwc1 $f31, 316($v0)
    xori $v1, $v1, 1
    beq $v1, $zero, .Lexc_set_fcr
    lw $v1, 40($v0)
    lwc1 $f0,  192($v0)
    lwc1 $f1,  196($v0)
    lwc1 $f2,  200($v0)
    lwc1 $f3,  204($v0)
    lwc1 $f4,  208($v0)
    lwc1 $f5,  212($v0)
    lwc1 $f6,  216($v0)
    lwc1 $f7,  220($v0)
    lwc1 $f8,  224($v0)
    lwc1 $f9,  228($v0)
    lwc1 $f10, 232($v0)
    lwc1 $f11, 236($v0)
    lwc1 $f12, 240($v0)
    lwc1 $f13, 244($v0)
    lwc1 $f14, 248($v0)
    lwc1 $f15, 252($v0)
    lwc1 $f16, 256($v0)
    lwc1 $f17, 260($v0)
    lwc1 $f18, 264($v0)
    lwc1 $f19, 268($v0)
.Lexc_set_fcr:
    ctc1 $zero, $31
    ctc1 $v1, $31
    cfc0 $v1, $25
    bne $v1, $zero, .Lexc_deferred_write
    nop
.Lexc_return:
    cfc0 $v1, $0
    mtc0 $v1, $14
    cfc0 $v0, $4
    cfc0 $v1, $5
    eret
.Lexc_in_intr:
    cfc0 $v0, $2
    ins $v0, $v1, 29, 1
    mtc0 $v0, $12
    ctc0 $v0, $2
    cfc0 $v0, $8
    bgezl $v0, .Lexc_set_fcr
    addiu $v1, $zero, 0x0e00
    sb $v1, 37($v0)
    ctc0 $zero, $8
    lb $v1, 36($v0)
    cache 0x18, 256($v0)
    beq $v1, $zero, .Lexc_chain_prev
    xori $v1, $v1, 1
    swc1 $f20, 272($v0)
    swc1 $f21, 276($v0)
    swc1 $f22, 280($v0)
    swc1 $f23, 284($v0)
    swc1 $f24, 288($v0)
    swc1 $f25, 292($v0)
    swc1 $f26, 296($v0)
    swc1 $f27, 300($v0)
    swc1 $f28, 304($v0)
    swc1 $f29, 308($v0)
    swc1 $f30, 312($v0)
    swc1 $f31, 316($v0)
    beq $v1, $zero, .Lexc_set_fcr
    addiu $v1, $zero, 0x0e00
    cache 0x18, 192($v0)
    swc1 $f0,  192($v0)
    swc1 $f1,  196($v0)
    swc1 $f2,  200($v0)
    swc1 $f3,  204($v0)
    swc1 $f4,  208($v0)
    swc1 $f5,  212($v0)
    swc1 $f6,  216($v0)
    swc1 $f7,  220($v0)
    swc1 $f8,  224($v0)
    swc1 $f9,  228($v0)
    swc1 $f10, 232($v0)
    swc1 $f11, 236($v0)
    swc1 $f12, 240($v0)
    swc1 $f13, 244($v0)
    swc1 $f14, 248($v0)
    swc1 $f15, 252($v0)
    swc1 $f16, 256($v0)
    swc1 $f17, 260($v0)
    swc1 $f18, 264($v0)
    j .Lexc_set_fcr
    swc1 $f19, 268($v0)
    .end tman_vfpu_flush

    .global tmanSaveFpuContext
    .ent tmanSaveFpuContext
tmanSaveFpuContext:
    cache 0x18, 256($a0)
    mfc0 $v0, $12
    lui $v1, 0x2000
    or $v1, $v0, $v1
    mtc0 $v1, $12
    lb $v1, 36($a0)
    swc1 $f20, 272($a0)
    swc1 $f21, 276($a0)
    swc1 $f22, 280($a0)
    swc1 $f23, 284($a0)
    swc1 $f24, 288($a0)
    swc1 $f25, 292($a0)
    swc1 $f26, 296($a0)
    swc1 $f27, 300($a0)
    swc1 $f28, 304($a0)
    swc1 $f29, 308($a0)
    swc1 $f30, 312($a0)
    xori $v1, $v1, 1
    beq $v1, $zero, 1f
    swc1 $f31, 316($a0)
    cache 0x18, 192($a0)
    swc1 $f0,  192($a0)
    swc1 $f1,  196($a0)
    swc1 $f2,  200($a0)
    swc1 $f3,  204($a0)
    swc1 $f4,  208($a0)
    swc1 $f5,  212($a0)
    swc1 $f6,  216($a0)
    swc1 $f7,  220($a0)
    swc1 $f8,  224($a0)
    swc1 $f9,  228($a0)
    swc1 $f10, 232($a0)
    swc1 $f11, 236($a0)
    swc1 $f12, 240($a0)
    swc1 $f13, 244($a0)
    swc1 $f14, 248($a0)
    swc1 $f15, 252($a0)
    swc1 $f16, 256($a0)
    swc1 $f17, 260($a0)
    swc1 $f18, 264($a0)
    swc1 $f19, 268($a0)
1:
    mtc0 $v0, $12
    jr $ra
    ctc0 $zero, $8
    .end tmanSaveFpuContext

    .global tman_systime_low_syscall
    .ent tman_systime_low_syscall
tman_systime_low_syscall:
    lui $v0, 0xbc60
    lw $v0, 0($v0)
    eret
    nop
    jr $ra
    nop
    .end tman_systime_low_syscall

    .global tman_idle_halt
    .ent tman_idle_halt
tman_idle_halt:
    mfc0 $v0, $25
1:
    cache 0x0a, 0($v0)
    .word 0x70000000
    j 1b
    nop
    .end tman_idle_halt
