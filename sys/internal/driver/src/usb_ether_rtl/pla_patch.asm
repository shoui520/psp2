; PLA MCU patch of the RTL8152B (revision 0x4c00), uploaded by plaPatch to PLA 0xf800.
;
; The ROM jumps to entry N of the table when it reaches breakpoint N; plaPatch programs
; BP0 0x170b, BP1 0x06e9, BP2 0x0989, BP3 0x1387, BP4 0x01b7, BP5 0x061d, BP7 0x047b
; (BP6 unused). Each hook redoes the ROM code it replaces and resumes in the ROM.
; Realtek's r8152b_pla_patch_a (U-Boot drivers/usb/eth/r8152_fw.c) is a neighbouring
; revision of the same program, not this one.
;
; NOTES on the processor (tools/rtlmcuasm.py has the encoding). Realtek documents neither
; the MCU nor these programs; everything here was read from the program words. Settled:
; the instruction formats and fields; branches, calls, ret and register jumps (each entry
; of the table at the start branches to a hook that ends in a jump back into the ROM, and
; the jump targets sit next to the breakpoint addresses the driver programs); PC-relative
; literal loads (ldl), whose pools hold register addresses and ROM resume addresses;
; ld/st with byte and word width and st.bh, which stores a register's high byte to an odd
; address; addi, subi, cmpi, movi; btst, bset, bclr; beq/bne after btst (equal: the bit
; is clear) and after cmpi; push and pop; and/or with a mode field.
; Open: what op.2x (a register and a 4-bit count; op.21 then op.25 extract a field that the
; following cmpi switches on), op.ax (an access through a pointer register), alu.40,
; alu.41, alu.44 and the and/or modes 1 and 3 (byte-lane variants: the driver's masks are
; built with movi 0xff and both modes) compute exactly, and what b.d tests (a loop counter
; below its limit). The registers the programs touch are named by address: no source
; names them.
;

; Entry N is taken at breakpoint N.
entryTable:
	bra bp0Hook
	bra bp1Hook
	bra bp2Hook
	bra bp3Hook
	bra bp4Hook
	bra bp5Hook
	bra bp6Hook
	bra bp7Hook

; BP0 (0x170b): with bits 0 and 4 of 0xc0c2 set, when bit 0 of r1+6 is set, set bit 15 of
; 0xd23a and copy the word at r1+4 to 0xd238; record r1 in 0xd2c6;
; otherwise toggle bit 0 of 0xd1a2 for the field values 1-4 of r1+12 with bit 4 set.
bp0Hook:
	ldl r3, bp0Hook_regC0C2
	ld.w r2, [r3+0]
	btst r2, 0
	beq bp0Hook_2
	btst r2, 4
	beq bp0Hook_2
	ldl r3, bp0Hook_regD238
	ld.w r2, [r3+2]
	ld.w r0, [r1+6]
	btst r0, 0
	beq bp0Hook_1
	bset r2, 15
	st.w r2, [r3+2]
	ld.w r0, [r1+4]
	st.w r0, [r3+0]
bp0Hook_1:
	ldl r3, bp0Hook_regD2C6
	st.w r1, [r3+0]
	ldl r3, bp0Hook_rom1784
	jmp r3
bp0Hook_2:
	ld.w r5, [r1+12]
	op.21 1, r5, 12
	op.25 1, r3, 12
	cmpi r3, 0x04
	beq bp0Hook_3
	cmpi r3, 0x03
	beq bp0Hook_3
	cmpi r3, 0x02
	beq bp0Hook_3
	cmpi r3, 0x01
	beq bp0Hook_3
	bra bp0Hook_4
bp0Hook_3:
	btst r5, 4
	bne bp0Hook_5
bp0Hook_4:
	ldl r2, bp0Hook_regC0BC
	ldl r3, bp0Hook_rom170C
	jmp r3
bp0Hook_5:
	ldl r3, bp0Hook_regD1A2
	ld.w r5, [r3+0]
	btst r5, 0
	bne bp0Hook_6
	bset r5, 0
	st.w r5, [r3+0]
	ldl r6, bp0Hook_rom13A2
	jmp r6
bp0Hook_6:
	bclr r5, 0
	st.w r5, [r3+0]
	bra bp0Hook_4
bp0Hook_regC0C2:
	.word 0xc0c2
bp0Hook_regD238:
	.word 0xd238
bp0Hook_regD2C6:
	.word 0xd2c6
bp0Hook_rom1784:
	.word 0x1784
bp0Hook_rom13A2:
	.word 0x13a2
bp0Hook_rom170C:
	.word 0x170c
bp0Hook_regC0BC:
	.word 0xc0bc
bp0Hook_regD1A2:
	.word 0xd1a2

; BP1 (0x06e9): copy two words from r2 to r1, from r2+0 or r2+2 depending on r4 & r2.
bp1Hook:
	ldl r4, bp1Hook_regD038
	alu.40.0 r4, r2
	beq bp1Hook_1
	ld.w r3, [r2+0]
	st.w r3, [r1+0]
	ld.w r3, [r2+2]
	st.w r3, [r1+2]
	bra bp1Hook_2
bp1Hook_1:
	ld.w r3, [r2+2]
	st.w r3, [r1+0]
	ld.w r3, [r2+4]
	st.w r3, [r1+2]
bp1Hook_2:
	ldl r4, bp1Hook_rom06D6
	jmp r4
bp1Hook_rom06D6:
	.word 0x06d6
bp1Hook_regD038:
	.word 0xd038

; BP2 (0x0989): clear bit 3 of r1 (read from 0xe400) when bits 0 and 4 of 0xc0c2 are set.
bp2Hook:
	ldl r0, bp2Hook_regE400
	ld.w r1, [r0+0]
	ldl r0, bp2Hook_regC0C2
	ld.w r2, [r0+0]
	btst r2, 0
	beq bp2Hook_1
	btst r2, 4
	beq bp2Hook_1
	bclr r1, 3
bp2Hook_1:
	ldl r0, bp2Hook_rom098C
	jmp r0
bp2Hook_regE400:
	.word 0xe400
bp2Hook_regC0C2:
	.word 0xc0c2
bp2Hook_rom098C:
	.word 0x098c

; BP3 (0x1387): pick the ROM continuation from the field in r1+12 and the matching bit of
; 0xc0b6 (wake-on-LAN configuration, PLA_CFG_WOL), unless bit 1 of 0xd1a0 is set.
bp3Hook:
	ld.w r5, [r1+12]
	ldl r3, bp3Hook_regD1A0
	ld.w r3, [r3+0]
	btst r3, 1
	bne bp3Hook_1
	op.21 1, r5, 12
	op.25 1, r3, 12
	ldl r6, bp3Hook_regC0B6
	ld.w r7, [r6+0]
	cmpi r3, 0x04
	beq bp3Hook_5
	cmpi r3, 0x03
	beq bp3Hook_6
	cmpi r3, 0x02
	beq bp3Hook_7
	cmpi r3, 0x01
	beq bp3Hook_8
bp3Hook_1:
	btst r5, 4
	bne bp3Hook_2
	ldl r5, bp3Hook_rom146A
	jmp r5
bp3Hook_2:
	ldl r5, bp3Hook_rom13A2
	jmp r5
bp3Hook_3:
	ldl r5, bp3Hook_rom13D6
	jmp r5
bp3Hook_4:
	ldl r5, bp3Hook_rom14FA
	jmp r5
bp3Hook_5:
	btst r7, 1
	bne bp3Hook_3
	ldl r5, bp3Hook_rom146A
	jmp r5
bp3Hook_6:
	btst r7, 4
	bne bp3Hook_4
	ldl r5, bp3Hook_rom146A
	jmp r5
bp3Hook_7:
	btst r7, 3
	bne bp3Hook_4
	ldl r5, bp3Hook_rom146A
	jmp r5
bp3Hook_8:
	btst r7, 2
	bne bp3Hook_4
	ldl r5, bp3Hook_rom146A
	jmp r5
bp3Hook_regC0B6:
	.word 0xc0b6
bp3Hook_rom146A:
	.word 0x146a
bp3Hook_rom13A2:
	.word 0x13a2
bp3Hook_rom13D6:
	.word 0x13d6
bp3Hook_rom14FA:
	.word 0x14fa
bp3Hook_regD1A0:
	.word 0xd1a0

; BP4 (0x01b7): when bit 4 of r5 is set, reprogram the USB-side registers 0xb804 and 0xb034
; through the indirect access engine, then write 4 to 0xe420.
bp4Hook:
	btst r5, 4
	beq bp4Hook_4
	push r2
	ldl r4, bp4Hook_regB804
	movi r5, 0x00
	call indirectAccess
	ld.w r3, [r7+0]
	op.21 1, r3, 9
	op.25 1, r3, 13
	cmpi r3, 0x04
	beq bp4Hook_1
	bra bp4Hook_3
bp4Hook_1:
	ldl r4, bp4Hook_regB034
	ldl r3, bp4Hook_rom0300
	call indirectWriteSequence
	ldl r3, bp4Hook_rom0C00
	call indirectWriteSequence
bp4Hook_2:
	movi r5, 0x00
	call indirectAccess
	ld.w r3, [r7+2]
	btst r3, 11
	beq bp4Hook_2
	ld.w r3, [r7+0]
	bclr r3, 7
	push r3
	movi r5, 0x81
	call indirectAccess
	movi r2, 0x40
	movi r5, 0x84
	call indirectAccess
	ldl r3, bp4Hook_rom0300
	call indirectWriteSequence
	pop r3
	movi r5, 0x81
	call indirectAccess
	ldl r3, bp4Hook_rom0E00
	call indirectWriteSequence
bp4Hook_3:
	pop r2
	ldl r7, bp4Hook_regE420
	movi r6, 0x04
	st.w r6, [r7+0]
bp4Hook_4:
	ldl r6, bp4Hook_rom0222
	jmp r6
bp4Hook_rom0222:
	.word 0x0222
bp4Hook_regE420:
	.word 0xe420
bp4Hook_regB804:
	.word 0xb804
bp4Hook_regB034:
	.word 0xb034
bp4Hook_rom0200:
	.word 0x0200
bp4Hook_rom0300:
	.word 0x0300
bp4Hook_rom0E00:
	.word 0x0e00
bp4Hook_rom0C00:
	.word 0x0c00

; indirectAccess: run one access through the engine at 0xea08: r3/r2 data, r4 address,
; r5 command; waits until bit 15 of the command word clears.
indirectAccess:
	ldl r7, indirectAccessRegs
	st.w r3, [r7+0]
	st.w r2, [r7+2]
	st.w r4, [r7+4]
	st.b r5, [r7+6]
indirectAccessWait:
	ld.w r6, [r7+6]
	btst r6, 15
	bne indirectAccessWait
	ret
indirectAccessRegs:
	.word 0xea08

; indirectWriteSequence: the three accesses (commands 0x82, 0x88, 0x88) that write r3 to the
; address in r4.
indirectWriteSequence:
	movi r5, 0x82
	call indirectAccess
	movi r2, 0x00
	movi r5, 0x88
	call indirectAccess
	ldl r2, bp4Hook_rom0200
	call indirectAccess
	ret

; BP5 (0x061d): resume at 0x0646 unchanged.
bp5Hook:
	ldl r6, bp5Hook_rom0646
	jmp r6
bp5Hook_rom0646:
	.word 0x0646

; BP6: not armed.
bp6Hook:
	ldl r6, bp6Hook_rom0000
	jmp r6
bp6Hook_rom0000:
	.word 0x0000

; BP7 (0x047b): update the receive descriptor at r7 (bits of its flag bytes) from 0xe84e and
; 0xe812, and clear bit 1 of 0xd1a0 on the path that resumes at 0x04e6.
bp7Hook:
	beq bp7Hook_6
	movi r4, 0x08
	st.b r4, [r7+10]
	ld.b r4, [r7+3]
	btst r4, 7
	bne bp7Hook_5
	ld.w r5, [r7+0]
	movi r3, 0xff
	or.3 r5, r3
	movi r3, 0xff
	or.1 r5, r3
	st.w r5, [r7+0]
	ldl r3, bp7Hook_regE84E
	ld.w r5, [r3+0]
	btst r5, 8
	beq bp7Hook_1
	movi r5, 0x81
	st.b r5, [r7+2]
	bra bp7Hook_2
bp7Hook_1:
	ld.b r3, [r7+2]
	movi r5, 0x81
	or.3 r3, r5
	st.b r3, [r7+2]
bp7Hook_2:
	ldl r3, bp7Hook_regE812
	movi r5, 0x00
	st.b r5, [r3+1]
	addi r3, 0x3c
	ld.w r5, [r3+0]
	btst r5, 8
	bne bp7Hook_3
	bclr r5, 15
	st.bh r5, [r3+1]
	bra bp7Hook_6
bp7Hook_regE84E:
	.word 0xe84e
bp7Hook_regE812:
	.word 0xe812
bp7Hook_3:
	ldl r5, bp7Hook_regD1A0
	ld.w r3, [r5+0]
	btst r3, 0
	beq bp7Hook_4
	bset r3, 1
	st.w r3, [r5+0]
bp7Hook_4:
	bra bp7Hook_6
bp7Hook_5:
	btst r4, 0
	bne bp7Hook_6
	ld.b r3, [r7+2]
	movi r5, 0x7e
	and.3 r3, r5
	st.b r3, [r7+2]
	ld.w r5, [r7+0]
	movi r3, 0x83
	and.3 r5, r3
	movi r3, 0xfe
	and.1 r5, r3
	st.w r5, [r7+0]
bp7Hook_6:
	btst r6, 4
	beq bp7Hook_8
	movi r5, 0x10
	st.b r5, [r7+10]
	ld.b r4, [r7+3]
	btst r4, 6
	bne bp7Hook_7
	ldl r5, bp7Hook_regD1A0
	ld.w r3, [r5+0]
	bclr r3, 1
	st.w r3, [r5+0]
	ldl r5, bp7Hook_rom04E6
	jmp r5
bp7Hook_rom04E6:
	.word 0x04e6
bp7Hook_regD1A0:
	.word 0xd1a0
bp7Hook_7:
	ldl r5, bp7Hook_rom04FE
	jmp r5
bp7Hook_rom04FE:
	.word 0x04fe
bp7Hook_8:
	ldl r5, bp7Hook_rom0530
	jmp r5
bp7Hook_rom0530:
	.word 0x0530
