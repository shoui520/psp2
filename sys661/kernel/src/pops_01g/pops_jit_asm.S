    .set noreorder
    .set noat
    .text


    .globl popsJitEnter
    .ent popsJitEnter
popsJitEnter:
    lui $sp,0x980
    move $s0,$zero
    move $s1,$zero
    move $s2,$zero
    move $s3,$zero
    move $s4,$zero
    move $s5,$zero
    move $s6,$zero
    move $s7,$zero
    move $k0,$zero
    move $k1,$zero
    move $fp,$zero
    mtc1 $zero,$f20
    mtc1 $zero,$f21
    mtc1 $zero,$f22
    mtc1 $zero,$f23
    mtc1 $zero,$f24
    mtc1 $zero,$f25
    mtc1 $zero,$f26
    mtc1 $zero,$f27
    mtc1 $zero,$f28
    mtc1 $zero,$f29
    mtc1 $zero,$f30
    mtc1 $zero,$f31
    j popsJitDispatchLookup
    lw $t9,432($gp)
    .end popsJitEnter

    .globl popsJitEventLoop
    .ent popsJitEventLoop
popsJitEventLoop:
    sw $v0,416($gp)
    sw $ra,436($gp)
    jal popsEventDispatch
    sw $t9,432($gp)
    jr $v0
    lw $t9,432($gp)
.Lbranch_aaa:
    jal popsEventDispatch
    sw $t9,432($gp)
    j popsJitDispatchLookup
    lw $t9,432($gp)
    .end popsJitEventLoop

    .globl popsMemReadByte
    .ent popsMemReadByte
popsMemReadByte:
    ext $v0,$a0,0x17,0x6
    bnez $v0,.Lbranch_aal
    li $a2,76
.Lbranch_aab:
    ins $a0,$a2,0x15,0xb
    jr $ra
    lb $v0,0($a0)
.Lbranch_aac:
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf800
    bne $v0,$a2,.Lbranch_aak
    li $a2,76
.Lbranch_aad:
    ins $a0,$a2,0xa,0x16
.Lbranch_aae:
    lb $v0,0($a0)
    jr $ra
    addiu $t9,$t9,4
.Lbranch_aaf:
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    bnez $v0,.Lbranch_aai
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
.Lbranch_aag:
    addu $v0,$v0,$a0
    jr $ra
    lb	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
.Lbranch_aah:
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_aao
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,0
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_aai:
    ori $a2,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    beq $v0,$a2,.Lbranch_aad
    li $a2,76
.Lbranch_aaj:
    ext $v0,$a0,0x17,0x6
    beqz $v0,.Lbranch_aab
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_aap
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,0
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_aak:
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    bnez $v0,.Lbranch_aaj
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
    addu $v0,$v0,$a0
    jr $ra
    lb	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
.Lbranch_aal:
    addiu $v0,$v0,-62
    blez $v0,.Lbranch_aap
    ext $v0,$a0,0xa,0xd
    bnez $v0,.Lbranch_aam
    cache 0x1a,-8($ra)
    lui $v0, %hi(.Lbranch_aac + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_aac + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    ins $a0,$a2,0xa,0x16
    lb $v0,0($a0)
    jr $ra
    addiu $t9,$t9,4
.Lbranch_aam:
    li $a2,6136
    ext $v0,$a0,0x13,0xd
    bnel $v0,$a2,.Lbranch_aan
    ext $v0,$a0,0xc,0x14
    lui $v0, %hi(.Lbranch_aaf + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_aaf + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
    addu $v0,$v0,$a0
    jr $ra
    lb	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
.Lbranch_aan:
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_aap
    lui $v0, %hi(.Lbranch_aah + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_aah + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,0
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_aao:
    ext $v0,$a0,0x17,0x6
    beqz $v0,.Lbranch_aab
    li $a2,76
    ori $a1,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    beql $v0,$a1,.Lbranch_aae
    ins $a0,$a2,0xa,0x16
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    beqz $v0,.Lbranch_aag
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
.Lbranch_aap:
    j .LioReadJitBridge
    li $a1,0
.Lbranch_aaq:
    addiu $v0,$v0,-62
    blez $v0,.Lbranch_aau
    ext $v0,$a0,0xa,0xd
    bnez $v0,.Lbranch_aar
    cache 0x1a,-8($ra)
    lui $v0, %hi(.Lbranch_aaw + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_aaw + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    ins $a0,$a2,0xa,0x16
    jr $ra
    sb $a1,0($a0)
.Lbranch_aar:
    ext $v0,$a0,0xd,0xa
    bnez $v0,.Lbranch_aau
    lui $v0, %hi(.Lbranch_aav + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_aav + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4100($a2)
    sw $ra,252($gp)
    li $a2,0
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_aas:
    ext $v0,$a0,0x17,0x6
    beqzl $v0,.Lbranch_aay
    ins $a0,$a2,0x15,0xb
    ext $v0,$a0,0xd,0xa
    bnez $v0,.Lbranch_aau
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4100($a2)
    sw $ra,252($gp)
    li $a2,0
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_aat:
    ori $a2,$gp,0xf800
    beq $v0,$a2,.Lbranch_aax
    li $a2,76
    ext $v0,$a0,0x17,0x6
    beqzl $v0,.Lbranch_aay
    ins $a0,$a2,0x15,0xb
.Lbranch_aau:
    j .LioWriteJitBridge
    li $a2,0
.Lbranch_aav:
    ori $a2,$gp,0xf801
    ext $v0,$a0,0xc,0x14
    bne $v0,$a2,.Lbranch_aat
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4100($a2)
    sw $ra,252($gp)
    li $a2,0
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_aaw:
    ori $a2,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    bne $v0,$a2,.Lbranch_aas
    li $a2,76
.Lbranch_aax:
    ins $a0,$a2,0xa,0x16
.Lbranch_aay:
    jr $ra
    sb $a1,0($a0)
    .end popsMemReadByte

    .globl popsMemWriteByte
    .ent popsMemWriteByte
popsMemWriteByte:
    ext $v0,$a0,0x17,0x6
    bnez $v0,.Lbranch_aaq
    li $a2,76
    ins $a0,$a2,0x15,0xb
    jr $ra
    sb $a1,0($a0)
    .end popsMemWriteByte

    .globl popsMemReadHalf
    .ent popsMemReadHalf
popsMemReadHalf:
    ext $v0,$a0,0x17,0x6
    bnez $v0,.Lbranch_abj
    li $a2,76
.Lbranch_aaz:
    ins $a0,$a2,0x15,0xb
    jr $ra
    lh $v0,0($a0)
.Lbranch_aba:
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf800
    bne $v0,$a2,.Lbranch_abi
    li $a2,76
.Lbranch_abb:
    ins $a0,$a2,0xa,0x16
.Lbranch_abc:
    lh $v0,0($a0)
    jr $ra
    addiu $t9,$t9,4
.Lbranch_abd:
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    bnez $v0,.Lbranch_abg
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
.Lbranch_abe:
    addu $v0,$v0,$a0
    lh	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
    jr $ra
    addiu $t9,$t9,-1
.Lbranch_abf:
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_abm
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,1
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_abg:
    ori $a2,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    beq $v0,$a2,.Lbranch_abb
    li $a2,76
.Lbranch_abh:
    ext $v0,$a0,0x17,0x6
    beqz $v0,.Lbranch_aaz
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_abn
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,1
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_abi:
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    bnez $v0,.Lbranch_abh
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
    addu $v0,$v0,$a0
    lh	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
    jr $ra
    addiu $t9,$t9,-1
.Lbranch_abj:
    addiu $v0,$v0,-62
    blez $v0,.Lbranch_abn
    ext $v0,$a0,0xa,0xd
    bnez $v0,.Lbranch_abk
    cache 0x1a,-8($ra)
    lui $v0, %hi(.Lbranch_aba + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_aba + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    ins $a0,$a2,0xa,0x16
    lh $v0,0($a0)
    jr $ra
    addiu $t9,$t9,4
.Lbranch_abk:
    li $a2,6136
    ext $v0,$a0,0x13,0xd
    bnel $v0,$a2,.Lbranch_abl
    ext $v0,$a0,0xc,0x14
    lui $v0, %hi(.Lbranch_abd + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_abd + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
    addu $v0,$v0,$a0
    lh	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
    jr $ra
    addiu $t9,$t9,-1
.Lbranch_abl:
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_abn
    lui $v0, %hi(.Lbranch_abf + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_abf + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,1
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_abm:
    ext $v0,$a0,0x17,0x6
    beqz $v0,.Lbranch_aaz
    li $a2,76
    ori $a1,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    beql $v0,$a1,.Lbranch_abc
    ins $a0,$a2,0xa,0x16
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    beqz $v0,.Lbranch_abe
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
.Lbranch_abn:
    j .LioReadJitBridge
    li $a1,1
.Lbranch_abo:
    addiu $v0,$v0,-62
    blez $v0,.Lbranch_abs
    ext $v0,$a0,0xa,0xd
    bnez $v0,.Lbranch_abp
    cache 0x1a,-8($ra)
    lui $v0, %hi(.Lbranch_abu + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_abu + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    ins $a0,$a2,0xa,0x16
    jr $ra
    sh $a1,0($a0)
.Lbranch_abp:
    ext $v0,$a0,0xd,0xa
    bnez $v0,.Lbranch_abs
    lui $v0, %hi(.Lbranch_abt + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_abt + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4100($a2)
    sw $ra,252($gp)
    li $a2,1
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_abq:
    ext $v0,$a0,0x17,0x6
    beqzl $v0,.Lbranch_abw
    ins $a0,$a2,0x15,0xb
    ext $v0,$a0,0xd,0xa
    bnez $v0,.Lbranch_abs
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4100($a2)
    sw $ra,252($gp)
    li $a2,1
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_abr:
    ori $a2,$gp,0xf800
    beq $v0,$a2,.Lbranch_abv
    li $a2,76
    ext $v0,$a0,0x17,0x6
    beqzl $v0,.Lbranch_abw
    ins $a0,$a2,0x15,0xb
.Lbranch_abs:
    j .LioWriteJitBridge
    li $a2,1
.Lbranch_abt:
    ori $a2,$gp,0xf801
    ext $v0,$a0,0xc,0x14
    bne $v0,$a2,.Lbranch_abr
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4100($a2)
    sw $ra,252($gp)
    li $a2,1
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_abu:
    ori $a2,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    bne $v0,$a2,.Lbranch_abq
    li $a2,76
.Lbranch_abv:
    ins $a0,$a2,0xa,0x16
.Lbranch_abw:
    jr $ra
    sh $a1,0($a0)
    .end popsMemReadHalf

    .globl popsMemWriteHalf
    .ent popsMemWriteHalf
popsMemWriteHalf:
    ext $v0,$a0,0x17,0x6
    bnez $v0,.Lbranch_abo
    li $a2,76
    ins $a0,$a2,0x15,0xb
    jr $ra
    sh $a1,0($a0)
    .end popsMemWriteHalf

    .globl popsMemReadWord
    .ent popsMemReadWord
popsMemReadWord:
    ext $v0,$a0,0x17,0x6
    bnez $v0,.Lbranch_ach
    li $a2,76
.Lbranch_abx:
    ins $a0,$a2,0x15,0xb
    jr $ra
    lw $v0,0($a0)
.Lbranch_aby:
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf800
    bne $v0,$a2,.Lbranch_acg
    li $a2,76
.Lbranch_abz:
    ins $a0,$a2,0xa,0x16
.Lbranch_aca:
    lw $v0,0($a0)
    jr $ra
    addiu $t9,$t9,4
.Lbranch_acb:
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    bnez $v0,.Lbranch_ace
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
.Lbranch_acc:
    addu $v0,$v0,$a0
    lw	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
    jr $ra
    addiu $t9,$t9,-3
.Lbranch_acd:
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_ack
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,2
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_ace:
    ori $a2,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    beq $v0,$a2,.Lbranch_abz
    li $a2,76
.Lbranch_acf:
    ext $v0,$a0,0x17,0x6
    beqz $v0,.Lbranch_abx
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_acl
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,2
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_acg:
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    bnez $v0,.Lbranch_acf
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
    addu $v0,$v0,$a0
    lw	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
    jr $ra
    addiu $t9,$t9,-3
.Lbranch_ach:
    addiu $v0,$v0,-62
    blez $v0,.Lbranch_acl
    ext $v0,$a0,0xa,0xd
    bnez $v0,.Lbranch_aci
    cache 0x1a,-8($ra)
    lui $v0, %hi(.Lbranch_aby + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_aby + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    ins $a0,$a2,0xa,0x16
    lw $v0,0($a0)
    jr $ra
    addiu $t9,$t9,4
.Lbranch_aci:
    li $a2,6136
    ext $v0,$a0,0x13,0xd
    bnel $v0,$a2,.Lbranch_acj
    ext $v0,$a0,0xc,0x14
    lui $v0, %hi(.Lbranch_acb + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_acb + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
    addu $v0,$v0,$a0
    lw	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
    jr $ra
    addiu $t9,$t9,-3
.Lbranch_acj:
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_acl
    lui $v0, %hi(.Lbranch_acd + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_acd + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,2
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_ack:
    ext $v0,$a0,0x17,0x6
    beqz $v0,.Lbranch_abx
    li $a2,76
    ori $a1,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    beql $v0,$a1,.Lbranch_aca
    ins $a0,$a2,0xa,0x16
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    beqz $v0,.Lbranch_acc
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
.Lbranch_acl:
    j .LioReadJitBridge
    li $a1,2
.Lbranch_acm:
    addiu $v0,$v0,-62
    blez $v0,.Lbranch_acq
    ext $v0,$a0,0xa,0xd
    bnez $v0,.Lbranch_acn
    cache 0x1a,-8($ra)
    lui $v0, %hi(.Lbranch_acs + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_acs + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    ins $a0,$a2,0xa,0x16
    jr $ra
    sw $a1,0($a0)
.Lbranch_acn:
    ext $v0,$a0,0xd,0xa
    bnez $v0,.Lbranch_acq
    lui $v0, %hi(.Lbranch_acr + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_acr + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4100($a2)
    sw $ra,252($gp)
    li $a2,2
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_aco:
    ext $v0,$a0,0x17,0x6
    beqzl $v0,.Lbranch_acu
    ins $a0,$a2,0x15,0xb
    ext $v0,$a0,0xd,0xa
    bnez $v0,.Lbranch_acq
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4100($a2)
    sw $ra,252($gp)
    li $a2,2
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_acp:
    ori $a2,$gp,0xf800
    beq $v0,$a2,.Lbranch_act
    li $a2,76
    ext $v0,$a0,0x17,0x6
    beqzl $v0,.Lbranch_acu
    ins $a0,$a2,0x15,0xb
.Lbranch_acq:
    j .LioWriteJitBridge
    li $a2,2
.Lbranch_acr:
    ori $a2,$gp,0xf801
    ext $v0,$a0,0xc,0x14
    bne $v0,$a2,.Lbranch_acp
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4100($a2)
    sw $ra,252($gp)
    li $a2,2
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_acs:
    ori $a2,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    bne $v0,$a2,.Lbranch_aco
    li $a2,76
.Lbranch_act:
    ins $a0,$a2,0xa,0x16
.Lbranch_acu:
    jr $ra
    sw $a1,0($a0)
    .end popsMemReadWord

    .globl popsMemWriteWord
    .ent popsMemWriteWord
popsMemWriteWord:
    ext $v0,$a0,0x17,0x6
    bnez $v0,.Lbranch_acm
    li $a2,76
    ins $a0,$a2,0x15,0xb
    jr $ra
    sw $a1,0($a0)
    .end popsMemWriteWord

    .globl popsMemReadByteUnsigned
    .ent popsMemReadByteUnsigned
popsMemReadByteUnsigned:
    ext $v0,$a0,0x17,0x6
    bnez $v0,.Lbranch_adf
    li $a2,76
.Lbranch_acv:
    ins $a0,$a2,0x15,0xb
    jr $ra
    lbu $v0,0($a0)
.Lbranch_acw:
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf800
    bne $v0,$a2,.Lbranch_ade
    li $a2,76
.Lbranch_acx:
    ins $a0,$a2,0xa,0x16
.Lbranch_acy:
    lbu $v0,0($a0)
    jr $ra
    addiu $t9,$t9,4
.Lbranch_acz:
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    bnez $v0,.Lbranch_adc
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
.Lbranch_ada:
    addu $v0,$v0,$a0
    jr $ra
    lbu	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
.Lbranch_adb:
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_adi
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,4
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_adc:
    ori $a2,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    beq $v0,$a2,.Lbranch_acx
    li $a2,76
.Lbranch_add:
    ext $v0,$a0,0x17,0x6
    beqz $v0,.Lbranch_acv
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_adj
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,4
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_ade:
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    bnez $v0,.Lbranch_add
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
    addu $v0,$v0,$a0
    jr $ra
    lbu	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
.Lbranch_adf:
    addiu $v0,$v0,-62
    blez $v0,.Lbranch_adj
    ext $v0,$a0,0xa,0xd
    bnez $v0,.Lbranch_adg
    cache 0x1a,-8($ra)
    lui $v0, %hi(.Lbranch_acw + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_acw + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    ins $a0,$a2,0xa,0x16
    lbu $v0,0($a0)
    jr $ra
    addiu $t9,$t9,4
.Lbranch_adg:
    li $a2,6136
    ext $v0,$a0,0x13,0xd
    bnel $v0,$a2,.Lbranch_adh
    ext $v0,$a0,0xc,0x14
    lui $v0, %hi(.Lbranch_acz + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_acz + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
    addu $v0,$v0,$a0
    jr $ra
    lbu	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
.Lbranch_adh:
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_adj
    lui $v0, %hi(.Lbranch_adb + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_adb + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,4
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_adi:
    ext $v0,$a0,0x17,0x6
    beqz $v0,.Lbranch_acv
    li $a2,76
    ori $a1,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    beql $v0,$a1,.Lbranch_acy
    ins $a0,$a2,0xa,0x16
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    beqz $v0,.Lbranch_ada
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
.Lbranch_adj:
    j .LioReadJitBridge
    li $a1,4
    .end popsMemReadByteUnsigned

    .globl popsJitDispatch
    .ent popsJitDispatch
popsJitDispatch:
    blez $t9,.Lbranch_aaa
    sw $t9,432($gp)
    .end popsJitDispatch

    .globl popsJitDispatchLookup
    .ent popsJitDispatchLookup
popsJitDispatchLookup:
    lw $a0,416($gp)
    lui $a2,0x9c0
    ins $a2,$a0,0x0,0x15
    ext $v1,$a0,0x17,0x1
    ins $a2,$v1,0x15,0x1
    lw $a2,0($a2)
    lui $v1, %hi(popsJitCompileEntry)
    addiu $v1, $v1, %lo(popsJitCompileEntry)
    movz $a2,$v1,$a2
    jr $a2
    nop
    .end popsJitDispatchLookup

    .globl popsJitDispatchAlt
    .ent popsJitDispatchAlt
popsJitDispatchAlt:
    blez $t9,.Lbranch_aaa
    sw $t9,432($gp)
    lw $a0,416($gp)
    lui $a2,0x9c0
    ext $v1,$a0,0x17,0x1
    beqzl $v1,.Lbranch_adk
    ins $a2,$a0,0x0,0x15
    ins $a2,$a0,0x0,0x13
.Lbranch_adk:
    ins $a2,$v1,0x15,0x1
    lw $a2,0($a2)
    lui $v1, %hi(popsJitCompileEntry)
    addiu $v1, $v1, %lo(popsJitCompileEntry)
    movz $a2,$v1,$a2
    jr $a2
    nop
    .end popsJitDispatchAlt

    .globl popsMemReadHalfUnsigned
    .ent popsMemReadHalfUnsigned
popsMemReadHalfUnsigned:
    ext $v0,$a0,0x17,0x6
    bnez $v0,.Lbranch_adv
    li $a2,76
.Lbranch_adl:
    ins $a0,$a2,0x15,0xb
    jr $ra
    lhu $v0,0($a0)
.Lbranch_adm:
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf800
    bne $v0,$a2,.Lbranch_adu
    li $a2,76
.Lbranch_adn:
    ins $a0,$a2,0xa,0x16
.Lbranch_ado:
    lhu $v0,0($a0)
    jr $ra
    addiu $t9,$t9,4
.Lbranch_adp:
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    bnez $v0,.Lbranch_ads
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
.Lbranch_adq:
    addu $v0,$v0,$a0
    lhu	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
    jr $ra
    addiu $t9,$t9,-1
.Lbranch_adr:
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_ady
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,5
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_ads:
    ori $a2,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    beq $v0,$a2,.Lbranch_adn
    li $a2,76
.Lbranch_adt:
    ext $v0,$a0,0x17,0x6
    beqz $v0,.Lbranch_adl
    ext $v0,$a0,0xc,0x14
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_adz
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,5
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_adu:
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    bnez $v0,.Lbranch_adt
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
    addu $v0,$v0,$a0
    lhu	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
    jr $ra
    addiu $t9,$t9,-1
.Lbranch_adv:
    addiu $v0,$v0,-62
    blez $v0,.Lbranch_adz
    ext $v0,$a0,0xa,0xd
    bnez $v0,.Lbranch_adw
    cache 0x1a,-8($ra)
    lui $v0, %hi(.Lbranch_adm + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_adm + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    ins $a0,$a2,0xa,0x16
    lhu $v0,0($a0)
    jr $ra
    addiu $t9,$t9,4
.Lbranch_adw:
    li $a2,6136
    ext $v0,$a0,0x13,0xd
    bnel $v0,$a2,.Lbranch_adx
    ext $v0,$a0,0xc,0x14
    lui $v0, %hi(.Lbranch_adp + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_adp + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
    addu $v0,$v0,$a0
    lhu	$v0,%lo(g_psxBiosRom + 0x40400000)($v0)
    jr $ra
    addiu $t9,$t9,-1
.Lbranch_adx:
    ori $a2,$gp,0xf801
    bne $v0,$a2,.Lbranch_adz
    lui $v0, %hi(.Lbranch_adr + 0x30000000)
    addiu $v0, $v0, %lo(.Lbranch_adr + 0x30000000)
    srl $v0,$v0,0x2
    sw $v0,-8($ra)
    cache 0x8,-8($ra)
    sync
    andi $v0,$a0,0xff8
    addu $a2,$gp,$v0
    lw $v0,4096($a2)
    sw $ra,252($gp)
    li $a1,5
    jalr $v0
    sw $t9,432($gp)
    lw $ra,252($gp)
    jr $ra
    lw $t9,432($gp)
.Lbranch_ady:
    ext $v0,$a0,0x17,0x6
    beqz $v0,.Lbranch_adl
    li $a2,76
    ori $a1,$gp,0xf800
    ext $v0,$a0,0xc,0x14
    beql $v0,$a1,.Lbranch_ado
    ins $a0,$a2,0xa,0x16
    ext $v0,$a0,0x13,0xd
    xori $v0,$v0,0x17f8
    beqz $v0,.Lbranch_adq
    lui $v0, %hi(g_psxBiosRom + 0x40400000)
.Lbranch_adz:
    j .LioReadJitBridge
    li $a1,5
    .end popsMemReadHalfUnsigned

    .globl popsJitCompileEntry
    .ent popsJitCompileEntry
popsJitCompileEntry:
    jal popsJitRecompile
    sw $t9,432($gp)
    jr $v0
    lw $t9,432($gp)
    .end popsJitCompileEntry

    .globl popsJitInvalidateEntry
    .ent popsJitInvalidateEntry
popsJitInvalidateEntry:
    jal popsJitInvalidateBlock
    sw $t9,432($gp)
    jr $v0
    lw $t9,432($gp)
    .end popsJitInvalidateEntry

    .globl popsJitLinkPatchEntry
    .ent popsJitLinkPatchEntry
popsJitLinkPatchEntry:
    lui $v0,0x9c0
    ins $v0,$a0,0x0,0x15
    ext $a1,$a0,0x17,0x1
    ins $v0,$a1,0x15,0x1
    lw $v0,0($v0)
    bnez $v0,.Lbranch_aea
    sw $ra,252($gp)
    jal popsJitRecompile
    sw $t9,432($gp)
    lw $ra,252($gp)
    lw $t9,432($gp)
.Lbranch_aea:
    sll $a0,$v0,0x6
    swl $a0,-6($ra)
    cache 0x1b,-8($ra)
    sync
    cache 0x8,-8($ra)
    jr $v0
    nop
    .end popsJitLinkPatchEntry

    .globl popsMemTranslateUnaligned
    .ent popsMemTranslateUnaligned
popsMemTranslateUnaligned:
    ext $v0,$a0,0x17,0x6
    bnez $v0,.Lbranch_aeb
    li $a2,76
    jr $ra
    ins $a0,$a2,0x15,0xb
.Lbranch_aeb:
    ext $v0,$a0,0xa,0xd
    bnez $v0,.Lbranch_aec
    addiu $t9,$t9,4
    jr $ra
    ins $a0,$a2,0xa,0x16
.Lbranch_aec:
    lui $a2, %hi(g_psxBiosRom + 0x40400000)
    addiu $a2, $a2, %lo(g_psxBiosRom + 0x40400000)
    ins $a0,$zero,0x13,0xd
    addiu $t9,$t9,-7
    jr $ra
    addu $a0,$a0,$a2
.Lbranch_aed:
    j .Lbranch_aed
    nop
    .end popsMemTranslateUnaligned

    .globl popsJitVerifyChecksum
    .ent popsJitVerifyChecksum
popsJitVerifyChecksum:
    lw $t0,0($a0)
    lw $t1,4($a0)
    lw $t2,8($a0)
    lw $t3,12($a0)
    subu $a2,$a2,$t0
    subu $a2,$a2,$t1
    subu $a2,$a2,$t2
    addiu $a0,$a0,16
    bne $a1,$a0,popsJitVerifyChecksum
    subu $a2,$a2,$t3
    bnez $a2,popsJitInvalidateEntry
    nop
    jr $ra
    nop
    .end popsJitVerifyChecksum

    .globl popsIoReadDefault
    .ent popsIoReadDefault
popsIoReadDefault:
    lui $v0, %hi(.LioReadByte)
    ins $v0, $a1, 3, 3
    addiu $v0, $v0, %lo(.LioReadByte)
    move $at, $gp
    jr $v0
    ins $at, $a0, 0, 12
.LioReadByte:
    jr $ra
    lb $v0,8192($at)
    .end popsIoReadDefault

    .globl popsIoReadHalf
    .ent popsIoReadHalf
popsIoReadHalf:
    jr $ra
    lh $v0,8192($at)
    .end popsIoReadHalf

    .globl popsIoReadWord
    .ent popsIoReadWord
popsIoReadWord:
    jr $ra
    lw $v0,8192($at)
    .end popsIoReadWord

    .globl popsIoReadZero
    .ent popsIoReadZero
popsIoReadZero:
    jr $ra
    li $v0,0
    .end popsIoReadZero

    .globl popsIoReadByteUnsigned
    .ent popsIoReadByteUnsigned
popsIoReadByteUnsigned:
    jr $ra
    lbu $v0,8192($at)
    .end popsIoReadByteUnsigned

    .globl popsIoReadHalfUnsigned
    .ent popsIoReadHalfUnsigned
popsIoReadHalfUnsigned:
    jr $ra
    lhu $v0,8192($at)
    .end popsIoReadHalfUnsigned

    .globl popsIoReadZeroAlt
    .ent popsIoReadZeroAlt
popsIoReadZeroAlt:
    jr $ra
    move $v0,$zero
    .end popsIoReadZeroAlt

    .globl popsIoWriteDefault
    .ent popsIoWriteDefault
popsIoWriteDefault:
    lui $v0, %hi(.LioWriteByte)
    ins $v0, $a2, 3, 2
    addiu $v0, $v0, %lo(.LioWriteByte)
    move $at, $gp
    jr $v0
    ins $at, $a0, 0, 12
.LioWriteByte:
    jr $ra
    sb $a1,8192($at)
    .end popsIoWriteDefault

    .globl popsIoWriteHalf
    .ent popsIoWriteHalf
popsIoWriteHalf:
    jr $ra
    sh $a1,8192($at)
    .end popsIoWriteHalf

    .globl popsIoWriteWord
    .ent popsIoWriteWord
popsIoWriteWord:
    jr $ra
    sw $a1,8192($at)
    .end popsIoWriteWord

    .globl popsIoWriteNop
    .ent popsIoWriteNop
popsIoWriteNop:
    jr $ra
    nop
.LioReadJitBridge:
    addiu $sp,$sp,-16
    sw $ra,0($sp)
    jal popsIoReadSlow
    sw $t9,432($gp)
    lw $t9,432($gp)
    lw $ra,0($sp)
    jr $ra
    addiu $sp,$sp,16
.LioWriteJitBridge:
    addiu $sp,$sp,-16
    sw $ra,0($sp)
    jal popsIoWriteSlow
    sw $t9,432($gp)
    lw $t9,432($gp)
    lw $ra,0($sp)
    jr $ra
    addiu $sp,$sp,16
    .end popsIoWriteNop

    .globl popsSyscallPatchTrampoline
    .ent popsSyscallPatchTrampoline
popsSyscallPatchTrampoline:
    mfic $t7,$0
    mtic $zero,$0
    lui $at, %hi(g_syscallStubBase)
    lw $at, %lo(g_syscallStubBase)($at)
    addu $v0,$at,$v0
    lui $at,0x3000
    ins $at,$v0,0x0,0x1c
    lui $v1, %hi(g_syscallTargetDelta)
    lw $v1, %lo(g_syscallTargetDelta)($v1)
    addu $v0,$v0,$v1
    lw $v1,-8($ra)
    srl $at,$at,0x2
    beql $v1,$at,.Lbranch_aee
    lw $v1,-4($ra)
    jr $v0
    mtic $t7,$0
.Lbranch_aee:
    lw $v0,4($v0)
    sw $v1,-8($ra)
    sw $v0,-4($ra)
    cache 0x1b,-4($ra)
    cache 0x8,-4($ra)
    cache 0x1b,-8($ra)
    cache 0x8,-8($ra)
    addiu $ra,$ra,-4
    sync
    jr $ra
    mtic $t7,$0
    .end popsSyscallPatchTrampoline

    .globl popsSyscallPatchRet
    .ent popsSyscallPatchRet
popsSyscallPatchRet:
    jr $ra
    .end popsSyscallPatchRet
