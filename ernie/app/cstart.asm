; The application's C startup: register bank 0, the stack, hdwinit, then the RAM: the RAM kept
; over a reset (keepram.h) is kept while its magic and checksum hold, the rest is cleared,
; the initialised data copied from ROM and the uninitialised cleared; then main, and the
; kernel. Written from the original (CA78K0R's startup, with Sony's kept RAM).

	NAME	cstart

	PUBLIC	_cstart, _cstartHalt
	PUBLIC	_heapUsed, _heapBreak, _heapArea
	EXTRN	_hdwinit, _main, _kernelStart, _keepRam

STACKTOP	EQU	0FE20H		; the stack grows down from here, to RAMBOTTOM
RAMBOTTOM	EQU	0CF00H		; the RAM's start (0xfcf00)
KEEPEND		EQU	0D900H + 4 + 0FCH	; the kept RAM's end: its header and the bytes summed
KEEPCLEAR	EQU	0DA90H		; what a warm start clears from
SADDRSIZE	EQU	0C0H		; the short direct area above the stack (0xffe20)
ROMDATA		EQU	0728H		; the initialised data's values (in page 1)
ROMDATAEND	EQU	072EH
INITDATA	EQU	0E16CH		; and their place (in page 0xf)
NEARBSS		EQU	0DAC4H		; the uninitialised near data
NEARBSSEND	EQU	0DD36H
FARBSS		EQU	0E172H		; the uninitialised data of page 0xf
FARBSSEND	EQU	0FCEAH
SADDRBSS	EQU	0FE20H		; the uninitialised short direct data
SADDRBSSEND	EQU	0FE74H
NOINIT		EQU	0FFFEH		; the empty initialised sections' values
NOINIT1		EQU	0CF00H
NOINIT2		EQU	0DD46H

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
	MOVW	STACKTOP[B], AX
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
	MOVW	AX, #STACKTOP
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
	MOVW	AX, #STACKTOP
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
	MOV	ES, #1
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
	CALL	!!_main
	BR	!!_kernelStart

; never reached
_cstartHalt:
	BR	$_cstartHalt

	END
