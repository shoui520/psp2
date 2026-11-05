; The program SceAudio uploads to each of the three SrcMix processors (srcmix.c,
; audioInitializeSrcMix), one image in the processors' halfword-addressed program space.
; Its first 256 words are uploaded with command 0x000060 (audioSrcMixProgram), the other
; 1414 with 0x120060 (audioSrcMixSecondaryProgram); the command table at 0x1ff straddles
; the two parts. The image is the same in every firmware.
;
; No document describes the processor: the syntax is tools/srcmixasm.py's, and the meaning
; of each instruction was read from this program and from how srcmix.c drives it.
; Sure: the program flow (branches, calls, the repeat loops, the interrupt vectors), the
; moves and operand space, loads, stores and immediates, the paired address-register loads
; (ldi2 r12/r1a, r13/r1b, r16/r1e), ld2, bset/bclr, and the ALU's add, sub, cmp and and.
; Read from use, not yet certain: ldi2's other pairs (r14/r1c, r15/r1d, r17/r1f), ldi24 r08,
; the multiplier unit (mu.XX; mpy and mac in the filter chains), the other ALU ops (aluC.O),
; ef.XX, ee.XX, the branch conditions (b.CC) and the memory updates mN.
;
; Memory: x and y data memories of 4096 24-bit words. The ARM writes y memory from 0x10
; with command 0x20AANN (command_y_transfer); y:0x19 is the run flag, y:0x0d the channel
; flags (bit 1 channel 0, bit 2 channel 1).

	.equ RUN, 0x19
	.equ CHANNEL_FLAGS, 0x0d

; The vectors: reset and the three interrupts.
	jmp reset
	jmp input0_interrupt
	jmp input1_interrupt
	jmp frame_interrupt

; Reset: clear x and y memory, set the current parameters to 'none' (0xfff).
reset:
	move r58, r00
	ldi r62, 0x0
	jmp L00d
L00d:
	ldi2 r16, 0xfff, r1e, 0xfff
	ldi2 r17, 0xfff, r1f, 0xfff
	move r42, r00
	move r42, r00
	move r42, r00
	move r42, r00
	ldi2 r12, 0x000, r1a, 0x000
	ldi r08, 0x10
L019:
	rep 128, L01c
	move [r12]+, r00
L01c:
	move [r12]+, r00
	sub r08, 1
	b.08 L019
	ldi r08, 0x4
L022:
	rep 128, L025
	move [r1a]+, r00
L025:
	move [r1a]+, r00
	sub r08, 1
	b.08 L022
	move r60, r00
	ldi r1a, 0xb
	ldi r02, 0xfff
	move [r1a]+, r02
	move [r1a]+, r02
	move [r1a]+, r02
	ldi r1a, 0x10
	move [r1a]+, r00
	move [r1a]+, r00
	jmp main_loop

; The main loop: take a command, else run the enabled channels.
main_loop:
	jmp command_poll

; Nothing to do until the ARM sets the run flag; then apply changed parameters.
run_check:
	ld r03, y:RUN
	cmp r03, 0
	b.88 main_loop
	bset sr, 0x400
	ldi r1a, 0xb
	ldi r1b, 0x10
	move r03, [r1a]
	move r08, [r1b]+
	move [r1a]+, r08
	nop
	move r00, [r1a]+
	cmp r08, r03, 3
	b.88 L04e
	bclr sr, 0x400
L04e:
	move r02, [r1b]
	cmp r02, r03
	b.88 L054
	bclr sr, 0x400
L054:
	b.50 L086
	move r58, r00
	ldi r03, 0xf
	and r08, r03
	move r00, [r1a]-
	move r03, [r1a]+
	move [r1a]+, r08
	move [r1a]+, r02
	st x:0xfc6, r00
	st x:0xfc7, r00
	ldi r08, 0xf0
	and r08, r03
	b.88 L06e
	alu9.0 1, r02
	st y:CHANNEL_FLAGS, r08
	st y:0x11, r08
	jmp L075
L06e:
	and 1, r02
	b.08 L075
	move [r1a]+, r01
	move [r1a], r01
	jmp L079
L075:
	ldi r02, 0x3
	move [r1a]+, r02
	move [r1a]+, r00
L079:
	jmp configure
L07b:
	ld r03, y:CHANNEL_FLAGS
	and 1, r03
	b.88 L084
	ldi r58, 0x55
	jmp L086
L084:
	ldi r58, 0x77
L086:
	ldi r02, 0x2
	and r02, r03
	b.88 channel0_main
L08b:
	ld r03, y:CHANNEL_FLAGS
	and 1, r03
	b.08 main_loop
	ldi r02, 0x4
	and r02, r03
	b.88 channel1_main
L095:
	jmp main_loop

; Input interrupts: two samples of the channel's input port into its ring buffer.
input0_interrupt:
	st x:0xf80, r12
	st x:0xf81, r16
	ldi r12, 0xf82
	ldi r16, 0xfff
	move [r12]+, r02
	move [r12]+, r03
	move [r12]+, sr
	move [r12]+, r0b
	move [r12]+, r0a
	move [r12], r09
	ld r03, y:CHANNEL_FLAGS
	ldi r02, 0x2
	and r02, r03
	b.88 L0b2
	ldi r12, 0xfc0
	move [r12]+, r40
	move [r12], r40
	jmp L0c8
L0b2:
	ld2 r02, x:0x38b, r03, y:0x20c
	add 1, r02
	st x:0x38b, r08
	cmp r03, 0
	b.88 L0c8
	ld r12, x:0x381
	ldi r16, 0x7f
	move [r12]+, r40
	move [r12]+, r40
	move r03, r12
	ld r02, x:0x380
	cmp r02, r03
	b.88 L0c8
	st x:0x381, r12
L0c8:
	ldi r12, 0xf87
	ldi r16, 0xfff
	move r09, [r12]-
	move r0a, [r12]-
	move r0b, [r12]-
	move sr, [r12]-
	move r03, [r12]-
	move r02, [r12]
	ld r16, x:0xf81
	ld r12, x:0xf80
	reti

input1_interrupt:
	st x:0xf90, r12
	st x:0xf91, r16
	ldi r12, 0xf92
	ldi r16, 0xfff
	move [r12]+, r02
	move [r12]+, r03
	move [r12]+, sr
	move [r12]+, r0b
	move [r12]+, r0a
	move [r12], r09
	ld r03, y:CHANNEL_FLAGS
	ldi r02, 0x4
	and r02, r03
	b.88 L0f2
	ldi r12, 0xfc2
	move [r12]+, r41
	move [r12], r41
	jmp L10b
L0f2:
	and 1, r03
	b.08 L10b
	ld2 r02, x:0x3cb, r03, y:0x20e
	add 1, r02
	st x:0x3cb, r08
	cmp r03, 0
	b.88 L10b
	ld r12, x:0x3c1
	ldi r16, 0x7f
	move [r12]+, r41
	move [r12]+, r41
	move r03, r12
	ld r02, x:0x3c0
	cmp r02, r03
	b.88 L10b
	st x:0x3c1, r12
L10b:
	ldi r12, 0xf97
	ldi r16, 0xfff
	move r09, [r12]-
	move r0a, [r12]-
	move r0b, [r12]-
	move sr, [r12]-
	move r03, [r12]-
	move r02, [r12]
	ld r16, x:0xf91
	ld r12, x:0xf90
	reti

; The frame interrupt: save the whole context, mix and output a frame, restore.
frame_interrupt:
	st x:0xfa0, r12
	st x:0xfa1, r16
	st y:0x3e8, r1a
	st y:0x3e9, r1e
	ldi2 r12, 0xfa2, r1a, 0x3ea
	ldi2 r16, 0xfff, r1e, 0xfff
	move [r12]+, r60
	move r60, r00
	move [r12]+, sr
	move [r12]+, r02
	move [r12]+, r14
	move [r12]+, r15
	move [r12]+, r13
	move [r12]+, r17
	move [r12]+, r0b
	move [r12]+, r0a
	move [r12]+, r09
	move [r12]+, r0f
	move [r12]+, r0e
	mu.60 r00
	move [r12]+, r0d
	move [r12]+, r0b
	move [r12]+, r0a
	move [r12]+, r09
	move [r12]+, r04
	move [r12]+, r05
	move [r12]+, r06
	move [r12]+, r07
	move [r12], r5c
	move [r1a]+, r03
	move [r1a]+, r1b
	move [r1a]+, r1f
	move [r1a]+, r1c
	move [r1a]+, r1d
	move [r1a]+, r5d
	move [r1a], r5e
	ldi2 r17, 0xfff, r1f, 0xfff
	jmp frame_process
L148:
	jmp frame_mix

; Output the frame's samples to r42; then each enabled channel's frame work.
frame_output:
	ld2 r02, x:0xfcb, r03, y:0x02e
	ldi24 r08, 0x400000
	and r08, r03
	b.88 L153
	call fetch_tagged
L153:
	move r42, r02
	ld2 r02, x:0xfcc, r03, y:0x02f
	ldi24 r08, 0x400000
	and r08, r03
	b.88 L15d
	call fetch_tagged
L15d:
	move r42, r02
	ldi r1a, 0x2
	move r02, [r1a]+
	move r42, r02
	move r02, [r1a]
	move r42, r02
	ld r03, y:CHANNEL_FLAGS
	ldi r02, 0x2
	and r02, r03
	b.88 channel0_frame
	ldi r60, 0xf5
	ldi2 r12, 0xfc0, r1a, 0x01a
	mu.00 [r12]
	mu.38 r10
	mu.38 r00
	move r02, r08
	move r03, r08
	ldi r60, 0x0
	move r08, r02
	move r0c, r03
L178:
	ldi r1a, 0x0
	ld r03, y:CHANNEL_FLAGS
	move [r1a]+, r08
	move [r1a], r0c
	and 1, r03
	move r0c, r00
	bd.08 frame_return
	move r08, r00
	ldi r02, 0x4
	and r02, r03
	b.88 channel1_frame
	ldi r60, 0xf5
	ldi2 r12, 0xfc2, r1a, 0x01b
	mu.00 [r12]
	mu.38 r10
	mu.38 r00
	move r02, r08
	move r03, r08
	ldi r60, 0x0
	move r08, r02
	move r0c, r03

; Restore the context saved by frame_interrupt.
frame_return:
	ldi2 r12, 0xfc5, r1a, 0x002
	move [r1a]+, r08
	move [r1a], r0c
	move r02, [r12]
	add 1, r02
	move [r12], r08
	ldi2 r12, 0xfb6, r1a, 0x3f0
	ldi2 r16, 0xfff, r1e, 0xfff
	move r5e, [r1a]-
	move r5d, [r1a]-
	move r1d, [r1a]-
	move r1c, [r1a]-
	move r1f, [r1a]-
	move r1b, [r1a]-
	move r5c, [r12]-
	move r07, [r12]-
	move r06, [r12]-
	move r05, [r12]-
	move r04, [r12]-
	move r09, [r12]-
	move r0a, [r12]-
	move r0b, [r12]-
	move r03, r00
	mu.90 r01
	move r09, [r12]-
	move r0a, [r12]-
	move r0b, [r12]-
	alu6.1 r08, 0
	move r09, [r12]-
	move r0a, [r12]-
	move r0b, [r12]-
	move r17, [r12]-
	move r13, [r12]-
	move r15, [r12]-
	move r14, [r12]-
	move r02, [r12]-
	move sr, [r12]-
	move r60, [r12]
	ld r16, x:0xfa1
	ld r12, x:0xfa0
	ld r1e, y:0x3e9
	ld r1a, y:0x3e8
	reti

; r02 = the word r03 addresses; r03's top nibble selects where it is read.
fetch_tagged:
	move r12, r03
	move r1a, r03
	ldi r02, 0xf000
	and r02, r03
	ldi r02, 0x0
	cmp r08, r02
	move r02, [r1a]
	ee.89 | move r08, r02
	ldi24 r02, 0x0f0000
	alu8.3 r02, r03
	b.88 L1e4
	mu.7d r00
	ldi24 r03, 0x080000
	cmp r02, r03
	b.88 L1e4
	mu.70 r00
	ldi24 r03, 0x0c0000
	cmp r02, r03
	b.88 L1e4
	mu.70 r00
L1e4:
	ret | move r02, r08

; Commands: the top nibble of the command word indexes command_table; the
; transfers move NN + 1 words from address AA (0x?0AANN) through the data port.
command_poll:
	b.30 command_none
	move r02, cmd
	ldi2 r13, 0x008, r1b, 0x00f
	ldi r03, 0x8000
	mu.10 r00
	move r03, r1b
	move r12, r08
	move r1a, r08
	and r02, r03
	add 1, r08
	move r5c, r08
	move r03, r13
	mu.10 r00
	move r03, r1b
	and r08, r03
	move r03, r08
	add r03, r03
	ldi r03, 0x1ff
	add r08, r03
	move jt, r08
	jmp (jt)

command_table:
	jmp command_next
	jmp command_x_transfer
	jmp command_y_transfer
	jmp command_next
	jmp command_next
	jmp command_next
	jmp command_next
	jmp command_next
	jmp command_next
	jmp command_next
	jmp command_next
	jmp command_next
	jmp command_next
	jmp command_next
	jmp command_next
	jmp command_next

command_done:
	bset sr, 0x40

command_none:
	jmp run_check

command_next:
	jmp command_done

command_x_transfer:
	b.b8 L22d
	repr.00 L22a
	move r02, port
L22a:
	move [r12]+, r02
	jmp command_next
L22d:
	repr.00 L230
	move r02, [r12]+
L230:
	move port, r02
	jmp command_next

command_y_transfer:
	b.b8 L23b
	repr.00 L238
	move r02, port
L238:
	move [r1a]+, r02
	jmp command_next
L23b:
	repr.00 L23e
	move r02, [r1a]+
L23e:
	move port, r02
	jmp command_next

; Configure for the rate in y:0x0c: copy its five-word table to y:0x203.
configure:
	move r07, r60
	ldi2 r16, 0xfff, r1e, 0xfff
	ld r03, y:0xc
	ldi r1b, 0xe49
	ldi r1a, 0x203
	rep 5, L24e
	move r08, [r1b]+
	move [r1a]+, r08
L24e:
	nop
	ldi2 r12, 0x399, r1a, 0xe55
	cmp r03, 0
	b.88 L25d
	cmp 1, r03
	b.88 L25b
	ldi r1a, 0xe4e
	jmp L25d
L25b:
	ldi r1a, 0xe5c
L25d:
	rep 7, L260
	move r08, [r1a]+
L260:
	move [r12]+, r08
	ldi r12, 0xb00
	ldi2 r13, 0xb00, r1b, 0xe63
	st x:0x380, r13
	ldi r14, 0x40
	move r00, [r13]m4
	st x:0x381, r13
	rep 64, L26f
	move [r12]+, r00
L26f:
	move [r12]+, r00
	ldi2 r12, 0x391, r1a, 0x208
	move [r12]+, r00
	move [r12]+, r00
	move [r12]+, r00
	rep 4, L279
	move r08, [r1b]+
	move [r1a]+, r08
L279:
	nop
	ldi r1a, 0x210
	rep 8, L280
	move r08, [r1b]+
	move [r1a]+, r08
L280:
	nop
	st x:0x3b3, r01
	ldi2 r12, 0x384, r1a, 0x20c
	ldi r1b, 0xe6f
	move [r1a]+, r00
	ldi r31, 0x2
	rep 13, L28d
	move r08, [r1b]+
L28d:
	move [r12]+, r08
	ldi r12, 0x400
	st x:0x3a4, r12
	ldi r14, 0x180
	move r00, [r12]m4
	st x:0x3a3, r12
	ldi r12, 0x400
	rep 180, L29e
	move [r12]+, r00
	move [r12]+, r00
	move [r12]+, r00
L29e:
	move [r12]+, r00
	ldi r12, 0xc00
	st x:0x3a5, r12
	st x:0x3a6, r12
	rep 192, L2a8
	move [r12]+, r00
L2a8:
	move [r12]+, r00
	ldi r12, 0x360
	ldi r13, 0x370
	rep 14, L2b0
	move [r12]+, r00
L2b0:
	move [r13]+, r00
	ldi r12, 0x0
	st x:0x3af, r12
	ldi r14, 0x180
	move r00, [r12]m4
	st x:0x3ae, r12
	ldi r12, 0x0
	rep 196, L2c1
	move [r12]+, r00
	move [r12]+, r00
	move [r12]+, r00
L2c1:
	move [r12]+, r00
	ldi r12, 0x700
	st x:0x3b0, r12
	st x:0x3b1, r12
	rep 96, L2cb
	move [r12]+, r00
L2cb:
	move [r12]+, r00
	ldi r12, 0xb80
	ldi r13, 0xb80
	st x:0x3c0, r13
	ldi r14, 0x40
	move r00, [r13]m4
	st x:0x3c1, r13
	rep 64, L2da
	move [r12]+, r00
L2da:
	move [r12]+, r00
	ldi r12, 0x3d1
	move [r12]+, r00
	move [r12]+, r00
	move [r12]+, r00
	ldi2 r12, 0x3c4, r1a, 0x20e
	ldi r1b, 0xe6f
	move [r1a]+, r00
	ldi r31, 0x2
	rep 13, L2ea
	move r08, [r1b]+
L2ea:
	move [r12]+, r08
	ldi r12, 0x800
	st x:0x3dd, r12
	ldi r14, 0x180
	move r00, [r12]m4
	st x:0x3dc, r12
	ldi r12, 0x800
	rep 180, L2fb
	move [r12]+, r00
	move [r12]+, r00
	move [r12]+, r00
L2fb:
	move [r12]+, r00
	ldi r12, 0xe00
	st x:0x3de, r12
	st x:0x3df, r12
	rep 192, L305
	move [r12]+, r00
L305:
	move [r12]+, r00
	ldi r12, 0xd80
	ldi r13, 0xd90
	rep 14, L30d
	move [r12]+, r00
L30d:
	move [r13]+, r00
	move r60, r07
	jmp L07b

; Per-channel work in the frame interrupt (channel 0 at x:0x38x, channel 1 at x:0x3cx).
channel0_frame:
	move r07, r60
	ldi2 r16, 0xfff, r1e, 0xfff
	ldi2 r17, 0xfff, r1f, 0xfff
	ldi2 r13, 0x38a, r1b, 0x20c
	ldi2 r12, 0x38e, r1a, 0x203
	ldi2 r15, 0x002, r1d, 0x002
	ld r02, y:0xe
	cmp r02, 0
	b.08 L325
	move [r1b], r00
	move [r12], r00
	jmp L4d2
L325:
	move r02, [r13]
	add 1, r02
	mu.03 [r12]+
	cmp r08, r03
	b.08 L4d2
	move r05, r58
	bset r58, 0x70
	mu.02 r12
	mu.10 [r1a]m0
	ldi r03, 0x4
	move r08, r09
	move [r13], r08
	cmp r02, r03
	b.08 L33a
	mu.68 r00
	jmp L33e
L33a:
	move r03, r1d
	cmp r02, r03
	b.08 L33f
L33e:
	mu.68 r00
L33f:
	move r03, [r1b]
	cmp r03, 0, 45
	b.8e L352
	add r03, r02
	alu6.3 r03, r02, 4
	cmp r08, r02
	b.10 L352
	move r08, r0c
	cmp r08, r02
	b.88 L352
	b.90 L352
	ldi r39, 0x3
	jmp L4a8
L352:
	move r03, [r13]
	move r04, r03
	ldi r08, 0x2
	ld r02, y:0xc
	cmp r02, r08
	b.08 L389
	ldi2 r12, 0x39e, r1a, 0xe7c
	ldi2 r13, 0x3a0, r1b, 0x400
	ldi r1c, 0x4
	move r02, [r12]-
	cmp r02, r03
	b.93 L3b6
	ldi r1a, 0xe94
	ldi r1b, 0x57d
	cmp r02, r03
	b.93 L3b6
	move r00, [r1a]m4
	ldi r1b, 0x5fd
	cmp r02, r03
	b.93 L3b6
	move r00, [r1a]m4
	ldi r1b, 0x681
	cmp r02, r03
	b.93 L3b6
	move r00, [r1a]m4
	ldi r1b, 0x6b6
	cmp r02, r03
	b.93 L3b6
	move r00, [r1a]m4
	ldi r1b, 0x6cc
	cmp r02, r03
	b.90 L3b6
	move r00, [r1a]m4
	ldi r1b, 0x6b6
	jmp L3b6
L389:
	ldi2 r12, 0x39d, r1a, 0xe7c
	ldi2 r13, 0x3a0, r1b, 0x400
	ldi r1c, 0x4
	move r02, [r12]-
	cmp r02, r03
	b.93 L3b6
	cmp r02, r03
	b.13 L3a0
	ld r08, y:0xc
	cmp r08, 0
	b.08 L3b6
	move r00, [r1a]m4
	ldi r1b, 0x464
	jmp L3b6
L3a0:
	move r00, [r1a]m4
	move r00, [r1a]m4
	ldi r1b, 0x512
	cmp r02, r03
	b.93 L3b6
	move r00, [r1a]m4
	ldi r1b, 0x567
	cmp r02, r03
	b.93 L3b6
	move r00, [r1a]m4
	ldi r1b, 0x547
	cmp r02, r03
	b.90 L3b6
	move r00, [r1a]m4
	ldi r1b, 0x567
L3b6:
	ldi r08, 0x3
L3b8:
	move r03, [r1a]+
	sub r08, 1
	bd.08 L3b8
	move [r13]+, r03
	move r02, [r1a]+
	ldi r1a, 0x30
	move [r1a]+, r02
	move r08, r03
	mu.68 r00
	move r03, [r1b]+
	move r02, r08
L3c5:
	move [r1a]+, r03
	sub r08, 1
	bd.08 L3c5
	move r03, [r1b]+
	mu.04 r0b
	move r00, [r1b]-
	move r03, [r1b]-
L3cd:
	move [r1a]+, r03
	sub r08, 1
	bd.08 L3cd
	move r03, [r1b]-
	ldi2 r12, 0x3a7, r1a, 0x139
	ldi r32, 0x100
	ldi r22, 0xdff
	ldi r22, 0x6
	ldi r22, 0xe
	move [r12]+, r00
	move r02, r04
	ld r08, x:0x39a
	cmp r08, r02
	b.90 L42a
	st x:0x3b3, r00
	ldi r08, 0x3
	ld r03, y:0xe
	cmp r08, r03
	b.90 L483
	ld r03, y:0xc
	ldi r08, 0x1
	cmp r03, r08
	b.88 L404
	ldi r08, 0x2
	cmp r03, r08
	b.88 L3fe
	ldi2 r12, 0x3ab, r1a, 0xeac
	ldi r1b, 0x400
	jmp L408
L3fe:
	ldi2 r12, 0x3ab, r1a, 0xeb4
	ldi r1b, 0x6ec
	jmp L408
L404:
	ldi2 r12, 0x3ab, r1a, 0xeb0
	ldi r1b, 0x4d2
L408:
	ldi r08, 0x3
L40a:
	move r03, [r1a]+
	sub r08, 1
	bd.08 L40a
	move [r12]+, r03
	move r08, [r1a]+
	ldi r1a, 0x13a
	mu.68 r00
	move [r1a]+, r01
	move r02, r08
	move r03, [r1b]+
L416:
	move [r1a]+, r03
	sub r08, 1
	bd.08 L416
	move r03, [r1b]+
	mu.04 r0b
	move r00, [r1b]-
	move r03, [r1b]-
L41e:
	move [r1a]+, r03
	sub r08, 1
	bd.08 L41e
	move r03, [r1b]-
	ldi r08, 0x4
	move r0c, r15
	ldi24 r03, 0x200000
	jmp L473
L42a:
	ld r08, x:0x39c
	st x:0x3b3, r01
	cmp r08, r02
	b.90 L48a
	ldi r08, 0x2
	ld r03, y:0xe
	cmp r08, r03
	b.90 L483
	ld r03, y:0xc
	ldi r08, 0x1
	cmp r03, r08
	b.88 L450
	ldi r08, 0x2
	cmp r03, r08
	b.88 L44a
	ldi2 r12, 0x3ab, r1a, 0xeac
	ldi r1b, 0x400
	jmp L454
L44a:
	ldi2 r12, 0x3ab, r1a, 0xeb4
	ldi r1b, 0x6ec
	jmp L454
L450:
	ldi2 r12, 0x3ab, r1a, 0xeb0
	ldi r1b, 0x4d2
L454:
	ldi r08, 0x3
L456:
	move r03, [r1a]+
	sub r08, 1
	bd.08 L456
	move [r12]+, r03
	move r08, [r1a]+
	ldi r1a, 0x13a
	mu.68 r00
	move [r1a]+, r01
	move r02, r08
	move r03, [r1b]+
L462:
	move [r1a]+, r03
	sub r08, 1
	bd.08 L462
	move r03, [r1b]+
	mu.04 r0b
	move r00, [r1b]-
	move r03, [r1b]-
L46a:
	move [r1a]+, r03
	sub r08, 1
	bd.08 L46a
	move r03, [r1b]-
	move r08, r15
	move r0c, r15
	ldi24 r03, 0x400000
L473:
	ldi2 r12, 0x38e, r1a, 0x20d
	ldi2 r13, 0x38b, r1b, 0x20c
	move [r12]+, r08
	move [r12]+, r0c
	move r02, r15
	move [r1b], r02
	move r02, r04
	mu.10 r00
	ldi r03, 0x2
	move [r13], r08
	move [r1a], r03
	b.06 L493
L483:
	ldi2 r13, 0x38b, r1b, 0x20c
	ldi2 r12, 0x390, r1a, 0x20d
	move [r1b], r01
	jmp L493
L48a:
	ldi2 r12, 0x38e, r1a, 0x20d
	ldi2 r13, 0x38b, r1b, 0x20c
	move [r12]+, r01
	move [r12]+, r01
	move [r1a], r01
	ldi r39, 0x2
L493:
	move [r12]+, r00
	move [r12]+, r02
	move [r12]+, r00
	ldi2 r12, 0xb00, r1a, 0x214
	st x:0x380, r12
	ldi r14, 0x40
	move r00, [r12]m4
	st x:0x381, r12
	move [r1a], r00
	move r00, [r12]m5
	ldi r08, 0x40
L4a3:
	move [r12]+, r00
	sub r08, 1
	bd.08 L4a3
	move [r12]+, r00
L4a8:
	ldi24 r03, 0xffffdf
	ld r02, x:0x386
	cmp r02, r03
	b.10 L4b6
	ldi r03, 0x1f
	cmp r03, r02
	b.10 L4b6
	jmp L4cf
L4b6:
	move r02, [r13]
	ldi r12, 0x391
	move [r12]+, r02
	move [r12]+, r00
	ldi2 r12, 0xb00, r1a, 0x214
	st x:0x380, r12
	ldi r14, 0x40
	move r00, [r12]m4
	st x:0x381, r12
	move [r1a], r00
	move r00, [r12]m5
	ldi r08, 0x40
L4c8:
	move [r12]+, r00
	sub r08, 1
	bd.08 L4c8
	move [r12]+, r00
	ldi r39, 0x2
L4cf:
	move [r13]-, r00
	move [r13], r00
	move r58, r05
L4d2:
	move r02, r01
	move r03, [r1b]
	cmp r03, r02
	b.10 L6a1
	ld r02, x:0x381
	ld r08, x:0x380
	sub r02, r08
	b.10 L4e1
	ldi r02, 0x80
	add r08, r02
L4e1:
	ldi24 r02, 0xffffc0
	add r08, r02
	mu.68 r00
	nop
	st x:0x386, r08
	move r02, r08
	ldi r1a, 0x214
	move r03, [r1a]+
	add 1, r03, 3
	move [r1a], r08
	cmp r03, r08
	b.90 L4f6
	move [r1a], r03
	ldi2 r12, 0x391, r1a, 0x213
	jmp L4f8
L4f6:
	ldi2 r12, 0x391, r1a, 0x20b
L4f8:
	ldi r1c, 0x3
	move r0a, [r12]+
	move r09, [r12]-
	move r03, [r1a]m5
	mu.90 r00
	mu.60 r02
	nop
	move [r12]+, r08
	move [r12]+, r09
	move r04, r08
	move r08, r02
	mu.78 r00
	nop
	mu.78 r00
	nop
	mu.a0 r12
	mu.20 r00
	move r03, [r1a]
	move [r12], r08
	move r02, r08
	move r08, r04
	mu.90 r00
	mu.60 r00
	ld r03, x:0x38e
	move r02, r08
	mu.10 r00
	st x:0x38c, r02
	st x:0x389, r09
	ldi r60, 0xff
	move r03, r09
	ld r02, x:0x39f
	mu.10 r00
	ldi r60, 0x0
	ld r02, x:0x38f
	st x:0x3b2, r08
L524:
	st y:0x219, r02
	ld r02, x:0x385
	ld r03, x:0x38c
	add r02, r03
	ld r03, x:0x384
	st x:0x385, r08
	move r02, r08
L530:
	cmp r03, r02
	b.90 L590
	ld r02, x:0x390
	add 1, r02
	ld r02, x:0x3b3
	and r02, r08
	st x:0x390, r08
	b.08 L566
	ldi r13, 0x395
	ld r12, x:0x380
	ldi r16, 0x7f
	move r08, [r12]+
	move r0c, [r12]+
	move r03, r12
	ld r02, x:0x381
	cmp r02, r03
	b.88 L54d
	st x:0x380, r12
L54d:
	move [r13]+, r08
	move [r13], r0c
	ldi r16, 0xfff
	ldi r60, 0xf5
	ld r03, y:0x1a
	move r02, r08
	mu.3d r00
	mu.38 r00
	move r02, r08
	move r03, r08
	ldi r60, 0x0
	ld r12, x:0x3a4
	ldi r16, 0x2cf
	move [r12]+, r02
	move [r12]+, r03
	st x:0x3a4, r12
	ldi r16, 0xfff
L566:
	ldi r13, 0x395
	ld r12, x:0x3a5
	ldi r16, 0x17f
	move r08, [r12]+
	move r0c, [r12]+
	move [r13]+, r08
	move [r13]+, r0c
	st x:0x3a5, r12
	ldi r16, 0xfff
	ld r12, x:0x3aa
	ldi r14, 0x360
	move r00, [r12]m4
	move [r12]m5, r08
	ldi r14, 0x370
	move r00, [r12]m4
	move [r12]m5, r0c
	nop
	ld r16, x:0x3a9
	nop
	move r00, [r12]+
	st x:0x3aa, r12
	ldi r16, 0xfff
	ld r02, x:0x385
	ld r03, x:0x384
	sub r02, r03
	st x:0x385, r08
	b.06 L530
L590:
	ld2 r02, x:0x385, r03, y:0x206
	mu.10 r00
	ldi r13, 0x396
	st x:0x383, r08
	ldi2 r14, 0x360, r1c, 0x749
	ld r12, x:0x3aa
	ld2 r02, x:0x3a9, r03, y:0x139
	move r16, r02
	move r1d, r03
	move r02, r08
	sub r03, r02
	sub r08, 1
	b.10 L5a5
	add r08, r03, 8
L5a5:
	move r04, r12
	move r05, r08
	ldi r60, 0xff
	move r1a, r05
	ldi r16, 0xfff
	mu.00 r44
	ld r16, x:0x3a9
	nop
	mu.00 [r12]m6
	mpy [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac r05
	move r03, r1a
	ld r02, x:0x3a7
	sub r02, r03
	move r1a, r08
	nop
	mu.02 [r1a]m4
	move [r13]+, r02
	nop
	mu.00 [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mu.20 r00
	ldi r14, 0x370
	move [r13], r08
	move r1a, r05
	move r12, r04
	ldi r16, 0xfff
	mu.00 r44
	ld r16, x:0x3a9
	nop
	mu.00 [r12]m6
	mpy [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac r05
	move r03, r1a
	ld r02, x:0x3a7
	sub r02, r03
	move r1a, r08
	nop
	mu.02 [r1a]m4
	move [r13]+, r02
	nop
	mu.00 [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mu.20 r00
	nop
	move [r13], r08
	ldi r60, 0x0
	ldi r16, 0xfff
	ld r02, x:0x383
	add 1, r02
	ldi r13, 0x398
	ldi2 r14, 0x360, r1c, 0x749
	ld r12, x:0x3aa
	ld2 r02, x:0x3a9, r03, y:0x139
	move r16, r02
	move r1d, r03
	move r02, r08
	sub r03, r02
	sub r08, 1
	b.10 L606
	add r08, r03, 8
L606:
	move r04, r12
	move r05, r08
	ldi r60, 0xff
	move r1a, r05
	ldi r16, 0xfff
	mu.00 r44
	ld r16, x:0x3a9
	nop
	mu.00 [r12]m6
	mpy [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac r05
	move r03, r1a
	ld r02, x:0x3a7
	sub r02, r03
	move r1a, r08
	nop
	mu.02 [r1a]m4
	move [r13]+, r02
	nop
	mu.00 [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mu.20 r00
	ldi r14, 0x370
	move [r13], r08
	move r1a, r05
	move r12, r04
	ldi r16, 0xfff
	mu.00 r44
	ld r16, x:0x3a9
	nop
	mu.00 [r12]m6
	mpy [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac r05
	move r03, r1a
	ld r02, x:0x3a7
	sub r02, r03
	move r1a, r08
	nop
	mu.02 [r1a]m4
	move [r13]+, r02
	nop
	mu.00 [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mu.20 r00
	nop
	move [r13], r08
	ldi r60, 0x0
	ldi r16, 0xfff
	ld2 r02, x:0x385, r03, y:0x207
	and r02, r03
	st x:0x383, r08
	ldi r13, 0x395
	ldi r60, 0xff
	move r12, r13
	mu.70 r00
	ldi r14, 0x2
	mu.78 r00
	ldi24 r03, 0x800000
	sub 0, r08
	move r02, r08
	alu6.3 r02, r03
	move r05, r08
	move r03, [r12]m4
	mu.a8 [r1b]m0
	mu.20 r00
	move r03, [r12]m4
	sub 0, r08, 4
	mu.ab [r13]m0
	mu.20 r00
	nop
	sub 0, r08
	move [r13]+, r08
	ldi r60, 0x0
	ld r02, x:0x38e
	move r03, r01
	cmp r02, r03
	b.10 L688
	ldi r13, 0x395
	ldi r16, 0x30f
	ld r12, x:0x3af
	move r02, [r13]+
	move r03, [r13]+
	move [r12]+, r02
	move [r12]+, r03
	st x:0x3af, r12
	ldi r16, 0xfff
L688:
	ld r02, y:0x219
	sub r02, 1
	b.0e L524
	ld r02, x:0x38e
	move r03, r01
	cmp r02, r03
	b.10 L6a1
	ldi r13, 0x395
	ld r12, x:0x3b0
	ldi r16, 0xbf
	move r08, [r12]+
	move r0c, [r12]+
	move [r13]+, r08
	move [r13]+, r0c
	st x:0x3b0, r12
	ldi r16, 0xfff
L6a1:
	ldi2 r12, 0x394, r1a, 0x20c
	mu.00 r11
	add 1, r02
	move [r12]+, r08
	ldi r08, 0x3
	cmp r03, r08
	b.88 L6ad
	move [r12]+, r00
	move [r12]-, r00
L6ad:
	ldi2 r16, 0xfff, r1e, 0xfff
	ldi2 r17, 0xfff, r1f, 0xfff
	move r08, [r12]+
	move r0c, [r12]+
	jd L178
	move r60, r07

; Per-channel work in the main loop: the filter chains over the input ring buffers.
channel0_main:
	move r07, r60
	ldi2 r16, 0xfff, r1e, 0xfff
	ldi2 r17, 0xfff, r1f, 0xfff
	ld r03, y:0x20d
	ldi r02, 0x0
	cmp r02, r03
	b.88 L709
	ldi r02, 0x1
	cmp r02, r03
	b.88 L6e2
	ldi r12, 0x0
	st x:0x3af, r12
	ldi r14, 0x180
	move r00, [r12]m4
	st x:0x3ae, r12
	ldi r12, 0x0
	rep 196, L6d7
	move [r12]+, r00
	move [r12]+, r00
	move [r12]+, r00
L6d7:
	move [r12]+, r00
	ldi r12, 0x700
	st x:0x3b0, r12
	st x:0x3b1, r12
	rep 96, L6e1
	move [r12]+, r00
L6e1:
	move [r12]+, r00
L6e2:
	ldi r12, 0x400
	st x:0x3a4, r12
	ldi r14, 0x180
	move r00, [r12]m4
	st x:0x3a3, r12
	ldi r12, 0x400
	rep 180, L6f2
	move [r12]+, r00
	move [r12]+, r00
	move [r12]+, r00
L6f2:
	move [r12]+, r00
	ldi r12, 0xc00
	st x:0x3a5, r12
	st x:0x3a6, r12
	rep 192, L6fc
	move [r12]+, r00
L6fc:
	move [r12]+, r00
	ldi r12, 0x360
	ldi r13, 0x370
	rep 14, L704
	move [r12]+, r00
L704:
	move [r13]+, r00
	st y:0x20d, r00
	jmp L861
L709:
	move r02, r01
	ld r03, y:0x20c
	cmp r03, r02
	b.10 L861
	ld r02, x:0x3a5
	ld r03, x:0x3a6
	sub r02, r03
	b.10 L719
	ldi r02, 0x180
	add r08, r02
L719:
	move r02, r08
	ldi r03, 0x80
	cmp r03, r02
	b.90 L807
	bset sr, 0x200
	bset sr, 0x400
	ldi2 r16, 0x2cf, r1e, 0xfff
	ldi2 r17, 0x17f, r1f, 0xfff
	ldi2 r14, 0x002, r1c, 0x031
	ld r13, x:0x3a6
	ld r15, x:0x3a3
	ld2 r02, x:0x3a1, r03, y:0x030
	sub r02, 1
	bset sr, 0x100
	alu8.3 1, r08
	b.88 L738
	sub r08, 1
	bclr sr, 0x100
L738:
	mu.68 r00
	move r1d, r03
	move r5c, r08
	sub r03, 1
	move r04, r08
	ldi r60, 0xf5
	cmp 1, r08
	b.08 L798
	ldi r06, 0x20
L744:
	move r12, r15
	move r1a, r04
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 L74c
	mac r46
L74c:
	repr.00 L74f
	mac r46
L74f:
	mac r46
	mu.20 r00
	move r12, r15
	move [r13]+, r08
	b.18 L757
	bclr sr, 0x200
L757:
	move r1a, r04
	move r00, [r12]+
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 L75f
	mac r46
L75f:
	repr.00 L762
	mac r46
L762:
	mac r46
	mu.20 r00
	move r12, r15
	move [r13]+, r08
	b.18 L76a
	bclr sr, 0x400
L76a:
	move r1a, r00
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 L771
	mac r46
L771:
	repr.00 L774
	mac r46
L774:
	mac r46
	mu.20 r00
	move r12, r15
	move [r13]+, r08
	b.18 L77c
	bclr sr, 0x200
L77c:
	move r1a, r00
	move r00, [r12]+
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 L784
	mac r46
L784:
	repr.00 L787
	mac r46
L787:
	mac r46
	mu.27 r00
	move r12, r15
	move [r13]+, r08
	b.18 L78f
	bclr sr, 0x400
L78f:
	nop
	move r00, [r12]m4
	move r15, r12
	sub r02, 1
	bd.08 L744
	move r06, r08
	jmp L7f2
L798:
	ldi r06, 0x20
L79a:
	move r12, r15
	move r1a, r00
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 L7a2
	mac r46
L7a2:
	repr.00 L7a5
	mac r46
L7a5:
	mac r46
	mu.20 r00
	move r12, r15
	mu.68 r00
	move r1a, r00
	move r00, [r12]+
	move [r13]+, r08
	b.18 L7b0
	bclr sr, 0x200
L7b0:
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 L7b6
	mac r46
L7b6:
	repr.00 L7b9
	mac r46
L7b9:
	mac r46
	mu.20 r00
	move r12, r15
	mu.68 r00
	move r1a, r00
	move r00, [r12]m4
	move [r13]+, r08
	b.18 L7c4
	bclr sr, 0x400
L7c4:
	move r15, r12
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 L7cb
	mac r46
L7cb:
	repr.00 L7ce
	mac r46
L7ce:
	mac r46
	mu.20 r00
	move r12, r15
	mu.68 r00
	move r1a, r00
	move r00, [r12]+
	move [r13]+, r08
	b.18 L7d9
	bclr sr, 0x200
L7d9:
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 L7df
	mac r46
L7df:
	repr.00 L7e2
	mac r46
L7e2:
	mac r46
	mu.27 r00
	move r12, r15
	mu.68 r00
	nop
	move [r13]+, r08
	b.18 L7ec
	bclr sr, 0x400
L7ec:
	move r00, [r12]m4
	move r15, r12
	sub r02, 1
	bd.08 L79a
	move r06, r08
L7f2:
	st x:0x3a3, r12
	st x:0x3a6, r13
	ldi r60, 0x0
	ld r08, x:0xfc6
	b.48 L7ff
	ldi24 r02, 0x200000
	alu9.0 r08, r02
L7ff:
	b.50 L804
	ldi24 r02, 0x100000
	alu9.0 r08, r02
L804:
	st x:0xfc6, r08
	nop
L807:
	ld r02, x:0x3b0
	ld r03, x:0x3b1
	sub r02, r03
	b.10 L811
	ldi r02, 0xc0
	add r08, r02
L811:
	move r02, r08
	ldi r03, 0x40
	cmp r03, r02
	b.90 L861
	ldi2 r16, 0x30f, r1e, 0xfff
	ldi r17, 0xbf
	ldi2 r14, 0x002, r1c, 0x13b
	ld r13, x:0x3b1
	ld r15, x:0x3ae
	ld2 r02, x:0x3ac, r03, y:0x13a
	sub r02, 1
	bset sr, 0x100
	alu8.3 1, r08
	b.88 L82c
	sub r08, 1
	bclr sr, 0x100
L82c:
	mu.68 r00
	move r1d, r03
	move r5c, r08
	ldi r60, 0xf5
	ldi r06, 0x20
L833:
	move r12, r15
	move r1a, r00
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 L83b
	mac r46
L83b:
	repr.00 L83e
	mac r46
L83e:
	mac r46
	mu.20 r00
	move r12, r15
	mu.68 r00
	move r1a, r00
	move r00, [r12]+
	move [r13]+, r08
	move r15, r12
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 L84c
	mac r46
L84c:
	repr.00 L84f
	mac r46
L84f:
	mac r46
	mu.27 r00
	move r12, r15
	mu.68 r00
	move r00, [r12]+
	move r00, [r12]m4
	move [r13]+, r08
	move r15, r12
	sub r02, 1
	bd.08 L833
	move r06, r08
	st x:0x3ae, r12
	st x:0x3b1, r13
	ldi r60, 0x0
L861:
	ldi2 r16, 0xfff, r1e, 0xfff
	ldi2 r17, 0xfff, r1f, 0xfff
	jd L08b
	move r60, r07

channel1_frame:
	ld r02, y:0xf
	cmp r02, 0
	b.08 L879
	st y:0x20e, r00
	st x:0x3ce, r00
	st x:0x3d5, r00
	st x:0x3d6, r00
	move r08, r00
	jd frame_return
	move r0c, r00
L879:
	move r07, r60
	ldi2 r16, 0xfff, r1e, 0xfff
	ldi2 r17, 0xfff, r1f, 0xfff
	ldi2 r13, 0x3ca, r1b, 0x20e
	ldi2 r12, 0x3ce, r1a, 0x203
	ldi2 r15, 0x002, r1d, 0x002
	move r02, [r13]
	add 1, r02
	mu.03 [r12]+
	cmp r08, r03
	b.08 L985
	move r05, r58
	bset r58, 0x70
	mu.02 r12
	mu.10 [r1a]m0
	ldi r03, 0x4
	move r08, r09
	move [r13], r08
	cmp r02, r03
	b.08 L899
	mu.68 r00
	jmp L89d
L899:
	move r03, r1d
	cmp r02, r03
	b.08 L89e
L89d:
	mu.68 r00
L89e:
	move r03, [r1b]
	cmp r03, 0, 45
	b.8e L8b1
	add r03, r02
	alu6.3 r03, r02, 4
	cmp r08, r02
	b.10 L8b1
	move r08, r0c
	cmp r08, r02
	b.88 L8b1
	b.90 L8b1
	ldi r39, 0x3
	jmp L95b
L8b1:
	move r03, [r13]
	move r04, r03
	ldi r08, 0x2
	ld r02, y:0xc
	cmp r02, r08
	b.08 L8e8
	ldi2 r12, 0x39e, r1a, 0xe7c
	ldi2 r13, 0x3d9, r1b, 0x400
	ldi r1c, 0x4
	move r02, [r12]-
	cmp r02, r03
	b.93 L915
	ldi r1a, 0xe94
	ldi r1b, 0x57d
	cmp r02, r03
	b.93 L915
	move r00, [r1a]m4
	ldi r1b, 0x5fd
	cmp r02, r03
	b.93 L915
	move r00, [r1a]m4
	ldi r1b, 0x681
	cmp r02, r03
	b.93 L915
	move r00, [r1a]m4
	ldi r1b, 0x6b6
	cmp r02, r03
	b.93 L915
	move r00, [r1a]m4
	ldi r1b, 0x6cc
	cmp r02, r03
	b.90 L915
	move r00, [r1a]m4
	ldi r1b, 0x6b6
	jmp L915
L8e8:
	ldi2 r12, 0x39d, r1a, 0xe7c
	ldi2 r13, 0x3d9, r1b, 0x400
	ldi r1c, 0x4
	move r02, [r12]-
	cmp r02, r03
	b.93 L915
	cmp r02, r03
	b.13 L8ff
	ld r08, y:0xc
	cmp r08, 0
	b.08 L915
	move r00, [r1a]m4
	ldi r1b, 0x464
	jmp L915
L8ff:
	move r00, [r1a]m4
	move r00, [r1a]m4
	ldi r1b, 0x512
	cmp r02, r03
	b.93 L915
	move r00, [r1a]m4
	ldi r1b, 0x567
	cmp r02, r03
	b.93 L915
	move r00, [r1a]m4
	ldi r1b, 0x547
	cmp r02, r03
	b.90 L915
	move r00, [r1a]m4
	ldi r1b, 0x567
L915:
	ldi r08, 0x3
L917:
	move r03, [r1a]+
	sub r08, 1
	bd.08 L917
	move [r13]+, r03
	move r02, [r1a]+
	ldi r1a, 0x220
	move [r1a]+, r02
	move r08, r03
	mu.68 r00
	move r03, [r1b]+
	move r02, r08
L924:
	move [r1a]+, r03
	sub r08, 1
	bd.08 L924
	move r03, [r1b]+
	mu.04 r0b
	move r00, [r1b]-
	move r03, [r1b]-
L92c:
	move [r1a]+, r03
	sub r08, 1
	bd.08 L92c
	move r03, [r1b]-
	ldi2 r12, 0x3e0, r1a, 0x139
	ldi r32, 0x100
	ldi r22, 0xdff
	ldi r22, 0x6
	ldi r22, 0xe
	move [r12]+, r00
	move r02, r04
	ldi2 r12, 0x3ce, r1a, 0x20f
	ldi2 r13, 0x3cb, r1b, 0x20e
	move [r12]+, r01
	move [r12]+, r01
	move [r1a], r01
	ldi r39, 0x2
	move [r12]+, r00
	move [r12]+, r02
	move [r12]+, r00
	ldi2 r12, 0xb80, r1a, 0x216
	st x:0x3c0, r12
	ldi r14, 0x40
	move r00, [r12]m4
	st x:0x3c1, r12
	move [r1a], r00
	move r00, [r12]m5
	ldi r08, 0x40
L956:
	move [r12]+, r00
	sub r08, 1
	bd.08 L956
	move [r12]+, r00
L95b:
	ldi24 r03, 0xffffdf
	ld r02, x:0x3c6
	cmp r02, r03
	b.10 L969
	ldi r03, 0x1f
	cmp r03, r02
	b.10 L969
	jmp L982
L969:
	move r02, [r13]
	ldi r12, 0x3d1
	move [r12]+, r02
	move [r12]+, r00
	ldi2 r12, 0xb80, r1a, 0x216
	st x:0x3c0, r12
	ldi r14, 0x40
	move r00, [r12]m4
	st x:0x3c1, r12
	move [r1a], r00
	move r00, [r12]m5
	ldi r08, 0x40
L97b:
	move [r12]+, r00
	sub r08, 1
	bd.08 L97b
	move [r12]+, r00
	ldi r39, 0x2
L982:
	move [r13]-, r00
	move [r13], r00
	move r58, r05
L985:
	move r02, r01
	move r03, [r1b]
	cmp r03, r02
	b.10 Lb2b
	ld r02, x:0x3c1
	ld r08, x:0x3c0
	sub r02, r08
	b.10 L994
	ldi r02, 0x80
	add r08, r02
L994:
	ldi24 r02, 0xffffc0
	add r08, r02
	mu.68 r00
	nop
	st x:0x3c6, r08
	move r02, r08
	ldi r1a, 0x216
	move r03, [r1a]+
	add 1, r03, 3
	move [r1a], r08
	cmp r03, r08
	b.90 L9a9
	move [r1a], r03
	ldi2 r12, 0x3d1, r1a, 0x213
	jmp L9ab
L9a9:
	ldi2 r12, 0x3d1, r1a, 0x20b
L9ab:
	ldi r1c, 0x3
	move r0a, [r12]+
	move r09, [r12]-
	move r03, [r1a]m5
	mu.90 r00
	mu.60 r02
	nop
	move [r12]+, r08
	move [r12]+, r09
	move r04, r08
	move r08, r02
	mu.78 r00
	nop
	mu.78 r00
	nop
	mu.a0 r12
	mu.20 r00
	move r03, [r1a]
	move [r12], r08
	move r02, r08
	move r08, r04
	mu.90 r00
	mu.60 r00
	ld r03, x:0x3ce
	move r02, r08
	mu.10 r00
	st x:0x3cc, r02
	st x:0x3c9, r09
	ldi r60, 0xff
	move r03, r09
	ld r02, x:0x39f
	mu.10 r00
	ldi r60, 0x0
	st x:0x3e4, r08
	nop
	ld r02, x:0x3cf
L9d8:
	st y:0x219, r02
	ld r02, x:0x3c5
	ld r03, x:0x3cc
	add r02, r03
	ld r03, x:0x3c4
	st x:0x3c5, r08
	move r02, r08
L9e4:
	cmp r03, r02
	b.90 La42
	ld r02, x:0x3d0
	add 1, r02
	and 1, r08
	st x:0x3d0, r08
	b.08 La18
	ldi r13, 0x3d5
	ld r12, x:0x3c0
	ldi r16, 0x7f
	move r08, [r12]+
	move r0c, [r12]+
	move r03, r12
	ld r02, x:0x3c1
	cmp r02, r03
	b.88 L9ff
	st x:0x3c0, r12
L9ff:
	move [r13]+, r08
	move [r13], r0c
	ldi r16, 0xfff
	ldi r60, 0xf5
	ld r03, y:0x1b
	move r02, r08
	mu.3d r00
	mu.38 r00
	move r02, r08
	move r03, r08
	ldi r60, 0x0
	ld r12, x:0x3dd
	ldi r16, 0x2cf
	move [r12]+, r02
	move [r12]+, r03
	st x:0x3dd, r12
	ldi r16, 0xfff
La18:
	ldi r13, 0x3d5
	ld r12, x:0x3de
	ldi r16, 0x17f
	move r08, [r12]+
	move r0c, [r12]+
	move [r13]+, r08
	move [r13]+, r0c
	st x:0x3de, r12
	ldi r16, 0xfff
	ld r12, x:0x3e3
	ldi r14, 0xd80
	move r00, [r12]m4
	move [r12]m5, r08
	ldi r14, 0xd90
	move r00, [r12]m4
	move [r12]m5, r0c
	nop
	ld r16, x:0x3e2
	nop
	move r00, [r12]+
	st x:0x3e3, r12
	ldi r16, 0xfff
	ld r02, x:0x3c5
	ld r03, x:0x3c4
	sub r02, r03
	st x:0x3c5, r08
	b.06 L9e4
La42:
	ld2 r02, x:0x3c5, r03, y:0x206
	mu.10 r00
	ldi r13, 0x3d6
	st x:0x3c3, r08
	ldi2 r14, 0xd80, r1c, 0x749
	ld r12, x:0x3e3
	ld2 r02, x:0x3e2, r03, y:0x139
	move r16, r02
	move r1d, r03
	move r02, r08
	sub r03, r02
	sub r08, 1
	b.10 La57
	add r08, r03, 8
La57:
	move r04, r12
	move r05, r08
	ldi r60, 0xff
	move r1a, r05
	ldi r16, 0xfff
	mu.00 r44
	ld r16, x:0x3e2
	nop
	mu.00 [r12]m6
	mpy [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac r05
	move r03, r1a
	ld r02, x:0x3e0
	sub r02, r03
	move r1a, r08
	nop
	mu.02 [r1a]m4
	move [r13]+, r02
	nop
	mu.00 [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mu.20 r00
	ldi r14, 0xd90
	move [r13], r08
	move r1a, r05
	move r12, r04
	ldi r16, 0xfff
	mu.00 r44
	ld r16, x:0x3e2
	nop
	mu.00 [r12]m6
	mpy [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac r05
	move r03, r1a
	ld r02, x:0x3e0
	sub r02, r03
	move r1a, r08
	nop
	mu.02 [r1a]m4
	move [r13]+, r02
	nop
	mu.00 [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mu.20 r00
	nop
	move [r13], r08
	ldi r60, 0x0
	ldi r16, 0xfff
	ld r02, x:0x3c3
	add 1, r02
	ldi r13, 0x3d8
	ldi2 r14, 0xd80, r1c, 0x749
	ld r12, x:0x3e3
	ld2 r02, x:0x3e2, r03, y:0x139
	move r16, r02
	move r1d, r03
	move r02, r08
	sub r03, r02
	sub r08, 1
	b.10 Lab8
	add r08, r03, 8
Lab8:
	move r04, r12
	move r05, r08
	ldi r60, 0xff
	move r1a, r05
	ldi r16, 0xfff
	mu.00 r44
	ld r16, x:0x3e2
	nop
	mu.00 [r12]m6
	mpy [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac r05
	move r03, r1a
	ld r02, x:0x3e0
	sub r02, r03
	move r1a, r08
	nop
	mu.02 [r1a]m4
	move [r13]+, r02
	nop
	mu.00 [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mu.20 r00
	ldi r14, 0xd90
	move [r13], r08
	move r1a, r05
	move r12, r04
	ldi r16, 0xfff
	mu.00 r44
	ld r16, x:0x3e2
	nop
	mu.00 [r12]m6
	mpy [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac [r12]m6
	mac r05
	move r03, r1a
	ld r02, x:0x3e0
	sub r02, r03
	move r1a, r08
	nop
	mu.02 [r1a]m4
	move [r13]+, r02
	nop
	mu.00 [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mac [r12]m7
	mu.20 r00
	nop
	move [r13], r08
	ldi r60, 0x0
	ldi r16, 0xfff
	ld2 r02, x:0x3c5, r03, y:0x207
	and r02, r03
	st x:0x3c3, r08
	ldi r13, 0x3d5
	ldi r60, 0xff
	move r12, r13
	mu.70 r00
	ldi r14, 0x2
	mu.78 r00
	ldi24 r03, 0x800000
	sub 0, r08
	move r02, r08
	alu6.3 r02, r03
	move r05, r08
	move r03, [r12]m4
	mu.a8 [r1b]m0
	mu.20 r00
	move r03, [r12]m4
	sub 0, r08, 4
	mu.ab [r13]m0
	mu.20 r00
	nop
	sub 0, r08
	move [r13]+, r08
	ldi r60, 0x0
	ld r02, y:0x219
	sub r02, 1
	b.0e L9d8
Lb2b:
	ldi2 r12, 0x3d4, r1a, 0x20e
	mu.00 r11
	add 1, r02
	move [r12]+, r08
	ldi r08, 0x3
	cmp r03, r08
	b.88 Lb37
	move [r12]+, r00
	move [r12]-, r00
Lb37:
	ldi2 r16, 0xfff, r1e, 0xfff
	ldi2 r17, 0xfff, r1f, 0xfff
	move r08, [r12]+
	move r0c, [r12]+
	jd frame_return
	move r60, r07

channel1_main:
	move r07, r60
	ldi2 r16, 0xfff, r1e, 0xfff
	ldi2 r17, 0xfff, r1f, 0xfff
	ld r03, y:0x20f
	ldi r02, 0x0
	cmp r02, r03
	b.88 Lb73
	ldi r12, 0x800
	st x:0x3dd, r12
	ldi r14, 0x180
	move r00, [r12]m4
	st x:0x3dc, r12
	ldi r12, 0x800
	rep 180, Lb5c
	move [r12]+, r00
	move [r12]+, r00
	move [r12]+, r00
Lb5c:
	move [r12]+, r00
	ldi r12, 0xe00
	st x:0x3de, r12
	st x:0x3df, r12
	rep 192, Lb66
	move [r12]+, r00
Lb66:
	move [r12]+, r00
	ldi r12, 0xd80
	ldi r13, 0xd90
	rep 14, Lb6e
	move [r12]+, r00
Lb6e:
	move [r13]+, r00
	st y:0x20f, r00
	jmp Lc71
Lb73:
	move r02, r01
	ld r03, y:0x20e
	cmp r03, r02
	b.10 Lc71
	ld r02, x:0x3de
	ld r03, x:0x3df
	sub r02, r03
	b.10 Lb83
	ldi r02, 0x180
	add r08, r02
Lb83:
	move r02, r08
	ldi r03, 0x80
	cmp r03, r02
	b.90 Lc71
	bset sr, 0x200
	bset sr, 0x400
	ldi2 r16, 0x2cf, r1e, 0xfff
	ldi2 r17, 0x17f, r1f, 0xfff
	ldi2 r14, 0x002, r1c, 0x221
	ld r13, x:0x3df
	ld r15, x:0x3dc
	ld2 r02, x:0x3da, r03, y:0x220
	sub r02, 1
	bset sr, 0x100
	alu8.3 1, r08
	b.88 Lba2
	sub r08, 1
	bclr sr, 0x100
Lba2:
	mu.68 r00
	move r1d, r03
	move r5c, r08
	sub r03, 1
	move r04, r08
	ldi r60, 0xf5
	cmp 1, r08
	b.08 Lc02
	ldi r06, 0x20
Lbae:
	move r12, r15
	move r1a, r04
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 Lbb6
	mac r46
Lbb6:
	repr.00 Lbb9
	mac r46
Lbb9:
	mac r46
	mu.20 r00
	move r12, r15
	move [r13]+, r08
	b.18 Lbc1
	bclr sr, 0x200
Lbc1:
	move r1a, r04
	move r00, [r12]+
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 Lbc9
	mac r46
Lbc9:
	repr.00 Lbcc
	mac r46
Lbcc:
	mac r46
	mu.20 r00
	move r12, r15
	move [r13]+, r08
	b.18 Lbd4
	bclr sr, 0x400
Lbd4:
	move r1a, r00
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 Lbdb
	mac r46
Lbdb:
	repr.00 Lbde
	mac r46
Lbde:
	mac r46
	mu.20 r00
	move r12, r15
	move [r13]+, r08
	b.18 Lbe6
	bclr sr, 0x200
Lbe6:
	move r1a, r00
	move r00, [r12]+
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 Lbee
	mac r46
Lbee:
	repr.00 Lbf1
	mac r46
Lbf1:
	mac r46
	mu.27 r00
	move r12, r15
	move [r13]+, r08
	b.18 Lbf9
	bclr sr, 0x400
Lbf9:
	nop
	move r00, [r12]m4
	move r15, r12
	sub r02, 1
	bd.08 Lbae
	move r06, r08
	jmp Lc5c
Lc02:
	ldi r06, 0x20
Lc04:
	move r12, r15
	move r1a, r00
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 Lc0c
	mac r46
Lc0c:
	repr.00 Lc0f
	mac r46
Lc0f:
	mac r46
	mu.20 r00
	move r12, r15
	mu.68 r00
	move r1a, r00
	move r00, [r12]+
	move [r13]+, r08
	b.18 Lc1a
	bclr sr, 0x200
Lc1a:
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 Lc20
	mac r46
Lc20:
	repr.00 Lc23
	mac r46
Lc23:
	mac r46
	mu.20 r00
	move r12, r15
	mu.68 r00
	move r1a, r00
	move r00, [r12]m4
	move [r13]+, r08
	b.18 Lc2e
	bclr sr, 0x400
Lc2e:
	move r15, r12
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 Lc35
	mac r46
Lc35:
	repr.00 Lc38
	mac r46
Lc38:
	mac r46
	mu.20 r00
	move r12, r15
	mu.68 r00
	move r1a, r00
	move r00, [r12]+
	move [r13]+, r08
	b.18 Lc43
	bclr sr, 0x200
Lc43:
	move r02, r00
	move r00, [r1a]m4
	mpy r46
	b.40 Lc49
	mac r46
Lc49:
	repr.00 Lc4c
	mac r46
Lc4c:
	mac r46
	mu.27 r00
	move r12, r15
	mu.68 r00
	nop
	move [r13]+, r08
	b.18 Lc56
	bclr sr, 0x400
Lc56:
	move r00, [r12]m4
	move r15, r12
	sub r02, 1
	bd.08 Lc04
	move r06, r08
Lc5c:
	st x:0x3dc, r12
	st x:0x3df, r13
	ldi r60, 0x0
	ld r08, x:0xfc7
	b.48 Lc69
	ldi24 r02, 0x200000
	alu9.0 r08, r02
Lc69:
	b.50 Lc6e
	ldi24 r02, 0x100000
	alu9.0 r08, r02
Lc6e:
	st x:0xfc7, r08
	nop
Lc71:
	ldi2 r16, 0xfff, r1e, 0xfff
	ldi2 r17, 0xfff, r1f, 0xfff
	jd L095
	move r60, r07

frame_mix:
	ldi r60, 0x35
	ldi2 r15, 0x002, r1d, 0x002
	ldi2 r12, 0xfc8, r1a, 0x000
	mu.00 [r12]m6
	mpy [r1a]-
	mu.30 [r12]m6
	mpy [r12]
	mu.31 r00
	move r03, [r12]+
	mu.41 r00
	nop
	mu.41 [r12]m0
	ldi r1b, 0x14
	move [r12], r08
	ldi2 r12, 0xfc8, r1a, 0x012
	move r04, r01
Lc8d:
	mu.00 r11
	cmp r02, r03
	b.90 Lc98
	move r00, [r1b]+
	move r03, [r1b]-
	add r03, r02, 2
	cmp r08, r03
	jd Lc9d
	ef.16
Lc98:
	move r03, [r1b]
	sub r02, r03, 2
	cmp r08, r03
	nop
	ef.96
Lc9d:
	move [r12]+, r03
	move r02, r04
	cmp r02, 0
	bd.08 Lc8d
	move r04, r00
	ldi r1b, 0x17
	ldi r1a, 0x16
	mu.00 r11
	cmp r02, r03
	b.90 Lcb2
	move r00, [r1b]+
	move r03, [r1b]-
	add r03, r02, 2
	cmp r08, r03
	jd Lcb7
	ef.16
Lcb2:
	move r03, [r1b]
	sub r02, r03, 2
	cmp r08, r03
	nop
	ef.96
Lcb7:
	move [r12]+, r03
	ldi r60, 0x0
	jmp frame_output

; The frame's mix: scale and combine the channels for output.
frame_process:
	ldi2 r14, 0x003, r1c, 0x003
	ldi2 r15, 0x002, r1d, 0x002
	ldi2 r12, 0xfd0, r1a, 0x01c
	ldi r1b, 0x0
	move r02, [r1a]+
	move r03, [r1b]m6
	mpy r0b
	mu.20 r0e
	mpy r09
	mu.20 r00
	alu7.3 r08, 0
	move r03, r0c
	alu7.2 r08, 0
	cmp r03, r08, 4
	nop
	ef.16
	move r08, r03
	cmp r02, r08
	nop
	ef.92
	move r03, [r1a]m6
	mu.a0 r01
	mu.20 r00
	ldi r1a, 0x21
	mu.01 r02
	cmp r02, r03, 42
	b.10 Lcfd
	mu.10 r00
	ldi r02, 0xe78
	alu6.1 r08, r02
	move r1b, r0c
	move r03, r09
	move r08, [r1b]+
	move r02, [r1b]
	alu6.3 r08, r02
	move r02, r0c
	mu.90 r02
	mu.60 r00
	move r00, [r12]+
	add r08, r03, 2
	sub r03, r08, 42
	add r08, r03
	mu.01 r02
	mu.11 r71
	ldi r02, 0xe9d
	add r08, r03
	alu6.1 r08, r02
	move r1b, r0c
	move r03, r09
	move r08, [r1b]+
	move r02, [r1b]
	alu6.3 r08, r02
	move r02, r0c
	mu.90 r00
	jd Lcff
	mu.60 r00
Lcfd:
	ld r08, y:0x28
Lcff:
	ldi r1b, 0x0
	mu.01 r0e
	mu.11 r1b
	mu.08 r0e
	mu.10 r0d
	move [r1b]m6, r08
	mu.08 r00
	move [r1b]-, r0c
	move [r1b]m6, r08
	jd L148
	move [r1b], r0c
