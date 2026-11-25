    .set noreorder
    .set noat
    .text

    .global tman_idle_loop
    .ent tman_idle_loop
tman_idle_loop:
    mtic $zero, $0
1:
    .word 0x70000000
    nop
    b 1b
    nop
    .end tman_idle_loop

    .global tman_vfpu_save
    .ent tman_vfpu_save
tman_vfpu_save:
    sv.q C000.q, 0x000($a0)
    sv.q C010.q, 0x010($a0)
    sv.q C020.q, 0x020($a0)
    sv.q C030.q, 0x030($a0)
    sv.q C100.q, 0x040($a0)
    sv.q C110.q, 0x050($a0)
    sv.q C120.q, 0x060($a0)
    sv.q C130.q, 0x070($a0)
    sv.q C200.q, 0x080($a0)
    sv.q C210.q, 0x090($a0)
    sv.q C220.q, 0x0A0($a0)
    sv.q C230.q, 0x0B0($a0)
    sv.q C300.q, 0x0C0($a0)
    sv.q C310.q, 0x0D0($a0)
    sv.q C320.q, 0x0E0($a0)
    sv.q C330.q, 0x0F0($a0)
    sv.q C400.q, 0x100($a0)
    sv.q C410.q, 0x110($a0)
    sv.q C420.q, 0x120($a0)
    sv.q C430.q, 0x130($a0)
    sv.q C500.q, 0x140($a0)
    sv.q C510.q, 0x150($a0)
    sv.q C520.q, 0x160($a0)
    sv.q C530.q, 0x170($a0)
    sv.q C600.q, 0x180($a0)
    sv.q C610.q, 0x190($a0)
    sv.q C620.q, 0x1A0($a0)
    sv.q C630.q, 0x1B0($a0)
    sv.q C700.q, 0x1C0($a0)
    sv.q C710.q, 0x1D0($a0)
    sv.q C720.q, 0x1E0($a0)
    sv.q C730.q, 0x1F0($a0)
    mfvc $t7, $128
    mfvc $t6, $129
    mfvc $t5, $130
    mfvc $t4, $131
    sw $t7, 0x200($a0)
    sw $t6, 0x204($a0)
    sw $t5, 0x208($a0)
    sw $t4, 0x20C($a0)
    mfvc $t7, $136
    mfvc $t6, $137
    mfvc $t5, $138
    mfvc $t4, $139
    sw $t7, 0x210($a0)
    sw $t6, 0x214($a0)
    sw $t5, 0x218($a0)
    sw $t4, 0x21C($a0)
    mfvc $t7, $140
    mfvc $t6, $141
    mfvc $t5, $142
    mfvc $t4, $143
    sw $t7, 0x220($a0)
    sw $t6, 0x224($a0)
    sw $t5, 0x228($a0)
    sw $t4, 0x22C($a0)
    vsync
    vnop
    jr $ra
    nop
    .end tman_vfpu_save

    .global tman_vfpu_restore
    .ent tman_vfpu_restore
tman_vfpu_restore:
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
    mtvc $t7, $128
    mtvc $t6, $129
    mtvc $t5, $130
    mtvc $t4, $131
    lw $t7, 0x210($a0)
    lw $t6, 0x214($a0)
    lw $t5, 0x218($a0)
    lw $t4, 0x21C($a0)
    mtvc $t7, $136
    mtvc $t6, $137
    mtvc $t5, $138
    mtvc $t4, $139
    lw $t7, 0x220($a0)
    lw $t6, 0x224($a0)
    lw $t5, 0x228($a0)
    lw $t4, 0x22C($a0)
    mtvc $t7, $140
    mtvc $t6, $141
    mtvc $t5, $142
    mtvc $t4, $143
    jr $ra
    nop
    .end tman_vfpu_restore

    .global tman_vfpu_flush
    .ent tman_vfpu_flush
tman_vfpu_flush:
    vsync
    vflush
    mfvc $zero, $143
    jr $ra
    nop
    .end tman_vfpu_flush

    .global tman_vfpu_exc_handler
    .ent tman_vfpu_exc_handler
tman_vfpu_exc_handler:
    mfc0 $v1, $13
    ext $v1, $v1, 28, 2
    xori $v0, $v1, 1
    bnez $v0, 1f
    nop
    mfc0 $v0, $12
    lui $v1, 0x2000
    or $v0, $v0, $v1
    mtc0 $v0, $12
1:
    eret
    nop
    .end tman_vfpu_exc_handler

    .global tman_eret
    .ent tman_eret
tman_eret:
    eret
    nop
    .end tman_eret

    .global tman_idle_halt
    .ent tman_idle_halt
tman_idle_halt:
    mfc0 $v0, $25
1:
    cache 0x0a, 0($v0)
    .word 0x70000000
    b 1b
    nop
    .end tman_idle_halt
