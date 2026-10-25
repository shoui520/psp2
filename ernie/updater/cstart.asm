; The updater's C startup: the application's (app/cstart.asm) with the updater's memory: its
; stack below 0xd500, the RAM from there kept on a warm start, and no initialised data. Then
; startHook and main, which does not return. Written from the original.

	NAME	cstart

	PUBLIC	_cstart
	PUBLIC	_heapUsed, _heapBreak, _heapArea
	EXTRN	_hdwinit, _startHook, _main, _keepRam

STACKTOP	EQU	0D500H		; the stack grows down from here, to RAMBOTTOM
SADDRAREA	EQU	0FE20H		; the short direct area
RAMBOTTOM	EQU	0CF00H		; the RAM's start (0xfcf00)
KEEPEND		EQU	0D900H + 4 + 0FCH	; the kept RAM's end: its header and the bytes summed
KEEPCLEAR	EQU	0D500H		; what a warm start clears from
SADDRSIZE	EQU	0C0H		; its size
ROMDATA		EQU	29F1H		; the initialised data's values (none)
ROMDATAEND	EQU	29F1H
INITDATA	EQU	0D8F8H		; and their place
NEARBSS		EQU	0D534H		; the uninitialised near data
NEARBSSEND	EQU	0D7E8H
FARBSS		EQU	0DA90H		; the uninitialised data of page 0xf
FARBSSEND	EQU	0F168H
SADDRBSS	EQU	0FE20H		; the uninitialised short direct data
SADDRBSSEND	EQU	0FE42H
NOINIT		EQU	29F1H		; the empty initialised sections' values
NOINIT1		EQU	0CF00H
NOINIT2		EQU	0D7F8H

; the C library's memory pool: what is used and the next free byte
@@HEAP	DSEG	UNITP
_heapUsed:	DS	2
_heapBreak:	DS	2
_heapArea:	DS	32

@@CSTART	CSEG	BASE
_cstart:
	SEL	RB0
	MOVW	SP, #STACKTOP
	CALL	!!_hdwinit
	; the short direct area
	MOV	B, #SADDRSIZE
	CLRW	AX
?saddr:
	DEC	B
	DEC	B
	MOVW	SADDRAREA[B], AX
	BNZ	$?saddr
	; the stack
	MOV	ES, #0FH
	MOVW	AX, #0D900H
	SUBW	AX, #RAMBOTTOM
	MOVW	BC, AX
	CLRW	AX
?stack:
	DECW	BC
	DECW	BC
	MOVW	ES:RAMBOTTOM[BC], AX
	CMPW	AX, BC
	BNZ	$?stack
	; the kept RAM: its magic, then the complement of the sum of its bytes
	MOV	ES, #0FH
	MOVW	AX, ES:!_keepRam
	CMPW	AX, #0A55AH
	BNZ	$?cold
	MOV	B, #KEEPEND - 0D904H
	MOVW	DE, #0
?sum:
	DEC	B
	CLRW	AX
	MOV	A, ES:0D904H[B]
	XCH	A, X
	ADDW	AX, DE
	MOVW	DE, AX
	CMP0	B
	BNZ	$?sum
	XOR	A, #0FFH
	XCH	A, X
	XOR	A, #0FFH
	XCH	A, X
	MOVW	DE, AX
	MOVW	AX, ES:!_keepRam + 2
	CMPW	AX, DE
	BNZ	$?cold
	; a warm start: the RAM above the kept block cleared, its header too
	MOV	ES, #0FH
	MOVW	AX, #SADDRAREA
	SUBW	AX, #KEEPCLEAR
	MOVW	BC, AX
	CLRW	AX
?warm:
	DECW	BC
	DECW	BC
	MOVW	ES:KEEPCLEAR[BC], AX
	CMPW	AX, BC
	BNZ	$?warm
	MOVW	ES:!_keepRam, AX
	MOVW	ES:!_keepRam + 2, AX
	BR	$?data
	; a cold start: the RAM from the kept block on cleared
?cold:
	MOV	ES, #0FH
	MOVW	AX, #SADDRAREA
	SUBW	AX, #0D900H
	MOVW	BC, AX
	CLRW	AX
?coldNext:
	DECW	BC
	DECW	BC
	MOVW	ES:0D900H[BC], AX
	CMPW	AX, BC
	BNZ	$?coldNext
?data:
	CLRW	AX
	MOVW	!_heapUsed, AX
	MOVW	AX, #LOWW(_heapArea)
	MOVW	!_heapBreak, AX
	; the initialised data
	MOV	ES, #0
	MOVW	HL, #NOINIT
	MOVW	DE, #NOINIT1
	BR	$?copy1End
?copy1:
	MOV	A, ES:[HL]
	MOV	[DE], A
	INCW	HL
	INCW	DE
?copy1End:
	MOVW	AX, HL
	CMPW	AX, #NOINIT
	BNZ	$?copy1
	MOVW	HL, #ROMDATA
	MOVW	DE, #INITDATA
	BR	$?copy2End
?copy2:
	MOV	ES, #0
	MOV	A, ES:[HL]
	MOV	ES, #0FH
	MOV	ES:[DE], A
	INCW	HL
	INCW	DE
?copy2End:
	MOVW	AX, HL
	CMPW	AX, #ROMDATAEND
	BNZ	$?copy2
	; the uninitialised data
	MOVW	HL, #NEARBSS
	MOVW	AX, #NEARBSSEND
	BR	$?bss1End
?bss1:
	MOV	[HL+0], #0
	INCW	HL
?bss1End:
	CMPW	AX, HL
	BNZ	$?bss1
	MOV	ES, #0FH
	MOVW	HL, #FARBSS
	MOVW	AX, #FARBSSEND
	BR	$?bss2End
?bss2:
	MOV	ES:[HL+0], #0
	INCW	HL
?bss2End:
	CMPW	AX, HL
	BNZ	$?bss2
	MOV	ES, #0
	MOVW	HL, #NOINIT
	MOVW	DE, #NOINIT2
	BR	$?copy3End
?copy3:
	MOV	A, ES:[HL]
	MOV	[DE], A
	INCW	HL
	INCW	DE
?copy3End:
	MOVW	AX, HL
	CMPW	AX, #NOINIT
	BNZ	$?copy3
	MOVW	HL, #SADDRBSS
	MOVW	AX, #SADDRBSSEND
	BR	$?bss3End
?bss3:
	MOV	[HL+0], #0
	INCW	HL
?bss3End:
	CMPW	AX, HL
	BNZ	$?bss3
	CALL	!!_startHook
	CALL	!!_main
?halt:
	BR	$?halt

	END
