/*
 * Position-independent MIPS assembly copied to ME boot RAM (0xBFC00C00)
 * during DDR frequency transitions so reclocking executes without any DDR
 * instruction fetch, stack access, or data load.
 *
 * Evidence Sony handwrote this in assembly:
 * - Zero stack usage; preserves return address in $t9 and $at across PIC bal calls.
 * - Uses `bal .+8; addiu $t0, $ra, 0xd4` to locate the inline timing table
 *   immediately following the routines inside the 0x240-byte copied block.
 * - Uses Allegrex `rotr` and `cache 0x19` for post-reclock DDR verification.
 */

	.set noreorder
	.set noat
	.text

	.globl ddrChangeClockBootRam
	.ent ddrChangeClockBootRam
ddrChangeClockBootRam:
	addu	$t4, $a0, $zero
	addu	$t5, $a2, $zero
	addu	$t6, $a3, $zero
	addu	$t7, $t0, $zero
	addu	$t8, $t1, $zero
	addu	$t9, $ra, $zero
	bal	sceDdr_driver_6DE74D80
	nop
	bltz	$t5, .Lset_pll
	nop
	bal	sceDdr_driver_4F30BFE8
	addu	$a0, $t5, $zero
.Lset_pll:
	lui	$t0, 0xbc10
	ori	$t1, $t4, 0x80
	sw	$t1, 0x68($t0)
	sync
	bltz	$t6, .Ldone
	addu	$a0, $t6, $zero
	bal	sceDdr_driver_F1005384
	addu	$a1, $t7, $zero
	lui	$t0, 0xbd00
	addiu	$t2, $zero, 2
	addiu	$t3, $zero, 0x15e
.Lwait_high:
	lw	$t1, 0x40($t0)
	addiu	$t3, $t3, -1
	bltz	$t3, .Lcheck_pattern
	andi	$t1, $t1, 0x100
	beqz	$t1, .Lwait_high
	nop
.Lwait_low:
	lw	$t1, 0x40($t0)
	addiu	$t3, $t3, -1
	bltz	$t3, .Lcheck_pattern
	andi	$t1, $t1, 0x100
	bnez	$t1, .Lwait_low
	nop
	addiu	$t2, $t2, -1
	bgtz	$t2, .Lwait_high
	nop
.Lcheck_pattern:
	beqz	$t8, .Ldone
	nop
.Lverify_loop:
	cache	0x19, 0($t8)
	lw	$t0, 0($t8)
	lw	$t1, 4($t8)
	lw	$t2, 8($t8)
	lw	$t3, 12($t8)
	or	$v0, $t2, $t3
	and	$v0, $t1, $v0
	nor	$v0, $zero, $v0
	rotr	$t2, $t2, 1
	xor	$t2, $t2, $t3
	or	$t0, $t0, $t2
	or	$v0, $v0, $t0
	bnez	$v0, .Lverify_loop
	nop
.Ldone:
	jr	$t9
	addu	$v0, $zero, $zero
	.end ddrChangeClockBootRam

	.globl sceDdr_driver_6DE74D80
	.ent sceDdr_driver_6DE74D80
sceDdr_driver_6DE74D80:
	addu	$at, $ra, $zero
	bal	.Lpic_base
	nop
.Lpic_base:
	addiu	$t0, $ra, (ddrBootRamTimingTable - .Lpic_base)
	sll	$t1, $a0, 3
	addu	$t0, $t0, $t1
	lw	$v0, 0($t0)
	lw	$v1, 4($t0)
	addiu	$t0, $a1, -3
	sll	$t0, $t0, 12
	or	$v0, $v0, $t0
	lui	$t0, 0xbd00
	lw	$t1, 0x30($t0)
	andi	$t1, $t1, 0x31
	or	$t1, $t1, $v0
	sw	$t1, 0x30($t0)
	andi	$t1, $v1, 0xffff
	sw	$t1, 0x44($t0)
	srl	$t1, $v1, 16
	sw	$t1, 0x34($t0)
	jr	$at
	sync
	.end sceDdr_driver_6DE74D80

	.globl sceDdr_driver_4F30BFE8
	.ent sceDdr_driver_4F30BFE8
sceDdr_driver_4F30BFE8:
	lui	$t0, 0xbd00
	addiu	$t1, $zero, 0x40
	addiu	$t2, $zero, 0x2b
.Lstep_down:
	sw	$t2, 0x40($t0)
	lw	$zero, 0x40($t0)
	addiu	$t1, $t1, -1
	bnez	$t1, .Lstep_down
	nop
	addiu	$t1, $a0, 1
	addiu	$t2, $zero, 0x2a
.Lstep_up:
	sw	$t2, 0x40($t0)
	lw	$zero, 0x40($t0)
	addiu	$t1, $t1, -1
	bnez	$t1, .Lstep_up
	nop
	jr	$ra
	addu	$v0, $zero, $zero
	.end sceDdr_driver_4F30BFE8

	.globl sceDdr_driver_F1005384
	.ent sceDdr_driver_F1005384
sceDdr_driver_F1005384:
	lui	$t0, 0xbd00
	ori	$t1, $a1, 0x10
	sw	$t1, 0x40($t0)
	lw	$zero, 0x40($t0)
	ori	$t1, $a0, 0x10
	sw	$t1, 0x40($t0)
	lw	$zero, 0x40($t0)
	addiu	$t1, $zero, 0x20
	sw	$t1, 0x40($t0)
	sync
	jr	$ra
	addu	$v0, $zero, $zero
	.end sceDdr_driver_F1005384

	nop
	nop
	nop
	nop
	nop

	.globl ddrBootRamTimingTable
	.ent ddrBootRamTimingTable
ddrBootRamTimingTable:
	.word	0x11110000, 0x0006000f
	.word	0x11110000, 0x0007000f
	.word	0x11110000, 0x0007000f
	.word	0x11110400, 0x00070000
	.word	0x11110400, 0x00070008
	.word	0x11110400, 0x0007000a
	.word	0x00000000, 0x00000000
	.word	0x00000000, 0x00000000
	.word	0x11110000, 0x0005000f
	.word	0x11110000, 0x0007000f
	.word	0x11110000, 0x0007000f
	.word	0x11110000, 0x0007000f
	.word	0x11110000, 0x0007000f
	.word	0x11110000, 0x0007000f
	.end ddrBootRamTimingTable
	.word	0x00000000, 0x00000000
	.word	0x00000000, 0x00000000

	.globl ddrBootRamEnd
ddrBootRamEnd:
