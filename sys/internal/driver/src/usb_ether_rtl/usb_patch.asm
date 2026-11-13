; USB MCU patch of the RTL8152B, uploaded by initializeHardware to USB 0xf800 (on the PS TV
; only). initializeHardware programs BP0 0x3019, BP2 0x085f and BP3 0x024f; entries 1 and
; 4-7 are not armed (entries 4-7 jump to 0).
;
; ocpRead and ocpWrite run a register access through the engine at 0xdc00, with the
; request (address, data low, data high, byte enables, result) in RAM at 0xcbe0.
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

; BP0 (0x3019): on bit 1 of 0xc2f8, count consecutive results of bit 5 of the register at
; 0xdc04 (read through ocpRead) in 0xc2f9/0xc2fa; after two, toggle bit 0 of 0xe028 and
; 0xe010 and, on the second path, restart the USB PHY (restartUsbPhy, copyLinkSettings).
bp0Hook:
	ldl r5, bp0Hook_regC2F8
	ld.b r3, [r5+0]
	btst r3, 1
	beq bp0Hook_1
	bclr r3, 1
	st.b r3, [r5+0]
	btst r3, 0
	beq bp0Hook_2
	ldl r5, ocpRequest
	ldl r3, bp0Hook_regDC04
	st.w r3, [r5+0]
	call ocpRead
	ld.w r3, [r5+10]
	btst r3, 5
	bne bp0Hook_2
	ld.w r3, [r5+8]
	btst r3, 5
	beq bp0Hook_3
	bra bp0Hook_5
bp0Hook_1:
	ldl r0, ocpEngine
	ldl r2, bp0Hook_regDF04
	ldl r5, bp0Hook_rom301A
	jmp r5
bp0Hook_rom301A:
	.word 0x301a
bp0Hook_2:
	ldl r5, bp0Hook_rom16DC
	jmp r5
bp0Hook_rom16DC:
	.word 0x16dc
bp0Hook_regE434:
	.word 0xe434
bp0Hook_regC2F8:
	.word 0xc2f8
ocpEngine:
	.word 0xdc00
ocpRequest:
	.word 0xcbe0
bp0Hook_regDC04:
	.word 0xdc04
bp0Hook_regE010:
	.word 0xe010
bp0Hook_regE028:
	.word 0xe028
bp0Hook_regDF04:
	.word 0xdf04

; ocpWrite: write the request at 0xcbe0 (address, data low, data high, byte enables).
ocpWrite:
	ldl r5, ocpRequest
	ldl r4, ocpEngine
	ld.w r7, [r5+0]
	st.w r7, [r4+12]
	ld.w r7, [r5+2]
	st.w r7, [r4+8]
	ld.w r7, [r5+4]
	st.w r7, [r4+10]
	ld.b r7, [r5+6]
	bset r7, 7
	st.b r7, [r4+14]
ocpWriteWait:
	ld.w r5, [r4+14]
	btst r5, 15
	bne ocpWriteWait
	ret

; ocpRead: read the register whose address is at 0xcbe0; the result goes to 0xcbe8/0xcbea.
ocpRead:
	ldl r5, ocpRequest
	ldl r4, ocpEngine
	ld.w r7, [r5+0]
	st.w r7, [r4+12]
	movi r7, 0x00
	st.b r7, [r4+14]
ocpReadWait:
	ld.w r7, [r4+14]
	btst r7, 15
	bne ocpReadWait
	ld.w r7, [r4+8]
	st.w r7, [r5+8]
	ld.w r7, [r4+10]
	st.w r7, [r5+10]
	ret
bp0Hook_3:
	ldl r5, bp0Hook_regC2F8
	movi r4, 0x00
	st.b r4, [r5+2]
	ld.b r4, [r5+1]
	addi r4, 0x01
	st.b r4, [r5+1]
	cmpi r4, 0x02
	b.d bp0Hook_2
	movi r4, 0x00
	st.b r4, [r5+1]
	ldl r5, bp0Hook_regB804
	ld.b r4, [r5+0]
	btst r4, 1
	bne bp0Hook_2
	ldl r5, bp0Hook_regB804
	ld.b r4, [r5+4]
	btst r4, 0
	beq bp0Hook_4
	ldl r3, bp0Hook_regBE00
	ld.b r4, [r3+0]
	bclr r4, 0
	bclr r4, 1
	st.b r4, [r3+0]
bp0Hook_4:
	ld.b r4, [r5+0]
	bset r4, 1
	st.b r4, [r5+0]
	ldl r5, ocpRequest
	ldl r3, bp0Hook_regE028
	st.w r3, [r5+0]
	call ocpRead
	ld.w r3, [r5+8]
	bclr r3, 0
	st.w r3, [r5+2]
	movi r3, 0x01
	st.b r3, [r5+6]
	call ocpWrite
	ldl r5, ocpRequest
	ldl r3, bp0Hook_regE010
	st.w r3, [r5+0]
	call ocpRead
	ld.w r3, [r5+8]
	bset r3, 0
	st.w r3, [r5+2]
	movi r7, 0x01
	st.b r7, [r5+6]
	call ocpWrite
	bra bp0Hook_2
bp0Hook_5:
	ldl r5, bp0Hook_regC2F8
	movi r4, 0x00
	st.b r4, [r5+1]
	ld.b r4, [r5+2]
	addi r4, 0x01
	st.b r4, [r5+2]
	cmpi r4, 0x02
	b.d bp0Hook_2
	movi r4, 0x00
	st.b r4, [r5+2]
	ldl r5, bp0Hook_regB804
	ld.b r4, [r5+0]
	btst r4, 1
	beq bp0Hook_2
	ldl r5, bp0Hook_regC2F8
	ld.b r3, [r5+0]
	bclr r3, 0
	st.b r3, [r5+0]
	ldl r5, bp0Hook_regE434
	ld.b r3, [r5+0]
	bclr r3, 0
	bset r3, 1
	bset r3, 4
	bclr r3, 3
	st.b r3, [r5+0]
	ldl r5, ocpRequest
	ldl r3, bp0Hook_regE028
	st.w r3, [r5+0]
	call ocpRead
	ld.w r3, [r5+8]
	bset r3, 0
	st.w r3, [r5+2]
	movi r3, 0x01
	st.b r3, [r5+6]
	call ocpWrite
	ldl r5, ocpRequest
	ldl r3, bp0Hook_regE010
	st.w r3, [r5+0]
	call ocpRead
	ld.w r3, [r5+8]
	bclr r3, 0
	st.w r3, [r5+2]
	movi r7, 0x01
	st.b r7, [r5+6]
	call ocpWrite
	call restartUsbPhy
	call copyLinkSettings
	movi r3, 0x03
	ldl r4, bp0Hook_regE420
	st.w r3, [r4+2]
	movi r3, 0x11
	st.w r3, [r4+0]
	bra bp0Hook_2
	.word 0xbe00
bp0Hook_regB000:
	.word 0xb000
bp0Hook_regB804:
	.word 0xb804
bp0Hook_regE420:
	.word 0xe420
bp0Hook_regBE00:
	.word 0xbe00
bp0Hook_regD802:
	.word 0xd802
bp0Hook_regD420:
	.word 0xd420
bp0Hook_regD41C:
	.word 0xd41c
bp0Hook_regB054:
	.word 0xb054

; restartUsbPhy: reset and reprogram the block at 0xb000 (name ours, from what it does).
restartUsbPhy:
	ldl r4, bp0Hook_regB000
	ld.b r3, [r4+4]
	bset r3, 1
	st.w r3, [r4+4]
	ldl r4, bp0Hook_regB000
	ld.w r3, [r4+8]
	bclr r3, 0
	st.w r3, [r4+8]
restartUsbPhyWaitReady:
	ld.w r3, [r4+18]
	btst r3, 15
	beq restartUsbPhyWaitReady
	ld.w r5, [r4+16]
	bset r5, 0
	st.w r5, [r4+16]
restartUsbPhyWaitReset:
	ld.w r5, [r4+16]
	btst r5, 0
	bne restartUsbPhyWaitReset
	ld.w r3, [r4+8]
	movi r5, 0x2e
	alu.41.2 r3, r5
	st.b r3, [r4+8]
	movi r5, 0x14
	alu.44.1 r7, r5
	st.w r7, [r4+12]
	movi r7, 0xff
	alu.44.1 r7, r7
	st.w r7, [r4+4]
	st.w r7, [r4+6]
	st.w r7, [r4+20]
	ld.w r3, [r4+24]
	bset r3, 1
	bset r3, 2
	st.w r3, [r4+24]
	ld.w r3, [r4+8]
	bset r3, 0
	st.w r3, [r4+8]
	movi r5, 0x80
	op.22 1, r5, 4
	op.a4 28, r5
	bclr r3, 0
	bclr r3, 1
	op.ae 28, r5
	movi r5, 0x20
	st.w r5, [r4+16]
restartUsbPhyWaitBit5:
	ld.w r5, [r4+16]
	btst r5, 5
	bne restartUsbPhyWaitBit5
	movi r3, 0x00
	movi r3, 0x00
	movi r3, 0x00
	movi r3, 0x00
	movi r5, 0x10
	st.w r5, [r4+16]
restartUsbPhyWaitBit4:
	ld.w r5, [r4+16]
	btst r5, 4
	bne restartUsbPhyWaitBit4
	movi r3, 0x00
	movi r3, 0x00
	movi r3, 0x00
	movi r3, 0x00
	movi r5, 0x81
	op.22 1, r5, 4
	op.ae 28, r5
	addi r5, 0x04
	op.ae 28, r5
	addi r5, 0x04
	op.af 28, r5
	addi r5, 0x02
	op.af 28, r5
	addi r5, 0x02
	op.ae 28, r5
	addi r5, 0x02
	op.ae 28, r5
	st.w r7, [r4+20]
	st.w r7, [r4+22]
	st.w r7, [r4+4]
	st.w r7, [r4+6]
	subi r5, 0x0e
	bclr r7, 13
	op.af 28, r5
	addi r5, 0x04
	op.af 28, r5
	bset r7, 13
	ld.w r5, [r4+24]
	ld.w r7, [r4+26]
	ldl r3, restartUsbPhyMaskHigh
	alu.41.2 r3, r7
	st.w r3, [r4+26]
	ldl r3, restartUsbPhyMaskLow
	alu.41.2 r3, r5
	st.w r3, [r4+24]
	ldl r7, bp0Hook_regD404
	ld.w r3, [r7+0]
	bclr r3, 8
	st.w r3, [r7+0]
	ret
restartUsbPhyMaskHigh:
	.word 0x888c
restartUsbPhyMaskLow:
	.word 0x3c10
	.word 0xb054
bp0Hook_regD404:
	.word 0xd404

; copyLinkSettings: build 0xb054/0xb055 from 0xd802, 0xd41c and 0xd420 (name ours).
copyLinkSettings:
	ldl r7, bp0Hook_regD802
	ld.w r4, [r7+0]
	btst r4, 1
	beq copyLinkSettingsMode
	ldl r7, bp0Hook_regB054
	ld.w r5, [r7+2]
	bset r5, 14
	st.w r5, [r7+2]
copyLinkSettingsMode:
	ldl r7, bp0Hook_regD41C
	ld.b r4, [r7+0]
	op.26 0, r4, 4
	ldl r7, bp0Hook_regB054
	st.b r4, [r7+1]
	ld.w r5, [r7+0]
	ldl r7, bp0Hook_regD420
	ld.b r4, [r7+0]
	btst r4, 4
	beq copyLinkSettingsDone
	bset r5, 0
	btst r4, 6
	beq copyLinkSettingsBit5
	bset r5, 1
copyLinkSettingsBit5:
	bset r5, 12
	ldl r7, bp0Hook_regB054
	st.w r5, [r7+0]
	btst r4, 5
	beq copyLinkSettingsDone
	ld.w r5, [r7+2]
	bset r5, 12
	bset r5, 13
	st.w r5, [r7+2]
copyLinkSettingsDone:
	ret

; BP1 (not armed): write 0x0fa0 and 1 to 0xe434.
bp1Hook:
	ldl r1, bp1Hook_timeout
	ldl r6, bp1Hook_regE434
	st.w r1, [r6+2]
	movi r1, 0x01
	st.w r1, [r6+0]
	st.w r4, [r0+4]
	ldl r6, bp1Hook_rom23EE
	jmp r6
bp1Hook_rom23EE:
	.word 0x23ee
bp1Hook_timeout:
	.word 0x0fa0
bp1Hook_regE434:
	.word 0xe434

; BP2 (0x085f): with bit 1 of 0xb804 set, clear bit 0 of the byte at r7 and resume at 0x0734.
bp2Hook:
	ldl r0, bp2Hook_regB804
	ld.b r2, [r0+0]
	btst r2, 1
	beq bp2Hook_1
	bclr r5, 0
	st.b r5, [r7+0]
	ldl r6, bp2Hook_rom0734
	jmp r6
bp2Hook_rom0734:
	.word 0x0734
bp2Hook_1:
	ldl r6, bp2Hook_rom0862
	jmp r6
bp2Hook_rom0862:
	.word 0x0862
bp2Hook_regB804:
	.word 0xb804

; BP3 (0x024f): write 4, then 1, to 0xe434, and set bit 0 of 0xd428 and bit 1 of 0xc2f8.
bp3Hook:
	ldl r7, bp3Hook_regE434
	movi r6, 0x04
	st.b r6, [r7+0]
	movi r6, 0x01
	st.b r6, [r7+0]
	ldl r7, bp3Hook_regD428
	ld.b r6, [r7+0]
	bset r6, 0
	st.b r6, [r7+0]
	ldl r7, bp3Hook_regC2F8
	ld.b r6, [r7+0]
	bset r6, 1
	st.b r6, [r7+0]
	ldl r6, bp3Hook_rom02B6
	jmp r6
bp3Hook_rom02B6:
	.word 0x02b6
bp3Hook_regD428:
	.word 0xd428
bp3Hook_regC2F8:
	.word 0xc2f8
bp3Hook_regE434:
	.word 0xe434
bp4Hook:
	ldl r6, bp4Hook_rom0000
	jmp r6
bp4Hook_rom0000:
	.word 0x0000
bp5Hook:
	ldl r6, bp5Hook_rom0000
	jmp r6
bp5Hook_rom0000:
	.word 0x0000
bp6Hook:
	ldl r6, bp6Hook_rom0000
	jmp r6
bp6Hook_rom0000:
	.word 0x0000
bp7Hook:
	ldl r6, bp7Hook_rom0000
	jmp r6
bp7Hook_rom0000:
	.word 0x0000
