; USS-1001's flash extra area read (chipid.c's): the eight bytes at 0x850-0x857, read by the
; flash's sequencer. The sequencer cannot run while the CPU fetches from the code flash, so the
; read and its helpers are copied to RAM (0xf0800) and called there, with the interrupts off
; and the data flash on. The flash's registers have no names in the device file; the names
; here are ours.

	NAME	flashextra

	PUBLIC	_flashReadExtra, _flashExtraRam, _flashExtraDelay, _flashExtraProtect
	PUBLIC	_flashExtraSave

RAMCODE	EQU	0F0800H			; where _flashExtraRam runs

; the flash's registers
FLPMC	EQU	0FFF80H			; the flash's mode (written through _flashExtraProtect)
FLAPL	EQU	0FFF82H			; the sequencer's address, low word
FLAPH	EQU	0FFF84H			; and high byte
FSSQ	EQU	0FFF85H			; the sequencer's command
FLSEDL	EQU	0FFF86H			; its end address, low word
FLSEDH	EQU	0FFF88H			; and high byte
FSASTH	EQU	0FFF8BH			; its status: bit 1 the step done, bit 6 the sequence
FLRST	EQU	0FF89H			; the sequencer's reset (!addr16)
FLARS	EQU	0FF81H			; set: the extra area is addressed (!addr16)
FSSQW	EQU	0FF85H			; FSSQ (!addr16)
PFCMD	EQU	00C0H			; the protect key, before each FLPMC write (!addr16)
FSSET	EQU	00B6H			; the sequencer's clock setting (!addr16)
FLRL	EQU	00C2H			; the word read, low (!addr16)
FLRH	EQU	00C4H			; and high (!addr16)

@@CODEL	CSEG	UNITP		; at an even address (Sony's 0x01030505 pads before it)

; AX: the 8 bytes' buffer (near)
_flashReadExtra:
	PUSH	AX
	PUSH	BC
	PUSH	DE
	PUSH	HL
	MOVW	HL, AX
	PUSH	HL
	MOV	A, ES
	PUSH	AX
	MOV	ES, #HIGHW(_flashExtraRam)
	MOVW	HL, #LOWW(_flashExtraRam)
	MOVW	BC, #LOWW(?ramEnd)
	MOVW	DE, #LOWW(RAMCODE)
?copy:
	MOVW	AX, ES:[HL]
	MOVW	[DE], AX
	INCW	HL
	INCW	HL
	INCW	DE
	INCW	DE
	MOVW	AX, HL
	CMPW	AX, BC
	BC	$?copy
	POP	AX
	MOV	ES, A
	POP	HL
	PUSH	PSW
	DI
	MOV	A, FLPMC
	MOV	!LOWW(RAMCODE + ?flpmc - _flashExtraRam), A
	MOV	A, #82H
	SET1	A.6
	MOV	!LOWW(RAMCODE + ?flpmcRead - _flashExtraRam), A
	MOV	A, DFLCTL
	MOV	!LOWW(RAMCODE + ?dflctl - _flashExtraRam), A
	BT	A.0, $?dataFlashOn
	SET1	A.0
	MOV	DFLCTL, A
	MOV	A, #11H
	CALL	!!_flashExtraDelay
?dataFlashOn:
	MOV	A, RMC
	MOV	!LOWW(RAMCODE + ?rmc - _flashExtraRam), A
	CLR1	A.0
	MOV	RMC, A
	MOV	A, !FSSET
	AND	A, #0C0H
	OR	A, #7
	MOV	!LOWW(RAMCODE + ?fsset - _flashExtraRam), A
	ONEB	!FLRST
	CLRB	!FLRST
	CALL	!!RAMCODE
	ONEB	!FLRST
	CLRB	!FLRST
	MOV	A, !LOWW(RAMCODE + ?dflctl - _flashExtraRam)
	MOV	DFLCTL, A
	POP	PSW
	POP	HL
	POP	DE
	POP	BC
	POP	AX
	RET

; run from RAM: HL the buffer
_flashExtraRam:
	MOV	A, #12H
	CALL	$!_flashExtraProtect
	MOV	A, #3
	CALL	$!_flashExtraDelay
	MOV	A, #92H
	CALL	$!_flashExtraProtect
	MOV	A, !LOWW(RAMCODE + ?flpmcRead - _flashExtraRam)
	CALL	$!_flashExtraProtect
	MOV	A, #9
	CALL	$!_flashExtraDelay
	MOV	A, !LOWW(RAMCODE + ?fsset - _flashExtraRam)
	MOV	!FSSET, A
	ONEB	!FLARS
	CLRB	A
	MOV	FLAPH, A
	MOV	FLSEDH, A
	MOVW	FLAPL, #850H
	MOVW	FLSEDL, #857H
	MOV	C, #2
?read:
	MOV	FSSQ, #85H
?readWait:
	BF	FSASTH.1, $?readWait
	MOVW	AX, !FLRL
	MOV	[HL+1], A
	XCH	A, X
	MOV	[HL], A
	INCW	HL
	INCW	HL
	MOVW	AX, !FLRH
	MOV	[HL+1], A
	XCH	A, X
	MOV	[HL], A
	INCW	HL
	INCW	HL
	MOV	FSSQ, #95H
?readNext:
	BT	FSASTH.1, $?readNext
	DEC	C
	BNZ	$?read
	MOV	FSSQ, #85H
?endWait:
	BF	FSASTH.6, $?endWait
	CLRB	!FSSQW
?endClear:
	BT	FSASTH.6, $?endClear
	CLRB	!FLARS
	MOV	A, #92H
	CALL	$!_flashExtraProtect
	MOV	A, !LOWW(RAMCODE + ?rmc - _flashExtraRam)
	MOV	RMC, A
	MOV	A, #3
	CALL	$!_flashExtraDelay
	MOV	A, #12H
	CALL	$!_flashExtraProtect
	MOV	A, !LOWW(RAMCODE + ?flpmc - _flashExtraRam)
	CALL	$!_flashExtraProtect
	MOV	A, #9
	CALL	$!_flashExtraDelay
	RET
	NOP

; A rounds of a loop
_flashExtraDelay:
	DEC	A
	BNZ	$_flashExtraDelay
	RET

; A to FLPMC, by the protected write: the key, the value, its complement, the value
_flashExtraProtect:
	MOV	!PFCMD, #0A5H
	MOV	FLPMC, A
	MOV	X, A
	XOR	A, #0FFH
	MOV	FLPMC, A
	MOV	A, X
	MOV	FLPMC, A
	RET

; the registers kept while the sequencer reads, copied to RAM with the code
_flashExtraSave:
	DS	5
?flpmc:		DB	0		; FLPMC
?flpmcRead:	DB	0		; FLPMC for the read
?fsset:		DB	0		; FSSET for the read
?rmc:		DB	0		; RMC
?dflctl:	DB	0		; DFLCTL
	DB	0
?ramEnd:

	END
