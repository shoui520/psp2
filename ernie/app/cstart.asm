; The application's C startup: register bank 0, the stack, hdwinit, then the RAM: the RAM kept
; over a reset (keepram.h) is kept while its magic and checksum hold, the rest is cleared,
; the initialised data copied from ROM and the uninitialised cleared; then main, and the
; kernel. Written from the original (CA78K0R's startup, with Sony's kept RAM); what it copies and
; clears runs between its segments' start labels and cstartend.asm's end labels.

	NAME	cstart

	PUBLIC	_cstart, _cstartHalt
	PUBLIC	_errno, _@BRKADR, _@MEMTOP
	EXTRN	_?R_INIT, _?R_INIS, _?RLINIT, _?DATA, _?DATAL, _?DATS
	EXTRN	_hdwinit, _main, _kernelStart, _keepRam

STACKTOP	EQU	0FE20H		; the stack grows down from here, to RAMBOTTOM
$_IF (ERNIE_HW>=0x00800000)	; CHIP_RL78: the RL78/G13's larger RAM
RAMBOTTOM	EQU	0AF00H		; the stack's bottom (0xfaf00)
DATASTART	EQU	0BC00H		; the RAM a cold start clears, from the stack's top
KEEPRAM		EQU	0CC00H		; the kept RAM (keepRam)
KEEPCLEAR	EQU	0CD90H		; what a warm start clears from
$ELSE
RAMBOTTOM	EQU	0CF00H		; the RAM's start (0xfcf00)
DATASTART	EQU	0D900H		; the RAM a cold start clears: the kept RAM on
KEEPRAM		EQU	0D900H		; the kept RAM (keepRam)
$_IF (ERNIE_FW>=0x00090009)
KEEPCLEAR	EQU	0DA90H		; what a warm start clears from
$ELSE
KEEPCLEAR	EQU	0DA8EH		; (0x00080504)
$ENDIF
$ENDIF
KEEPEND		EQU	KEEPRAM + 4 + 0FCH	; the kept RAM's end: its header and the bytes summed
SADDRSIZE	EQU	0C0H		; the short direct area above the stack (0xffe20)

; the starts of the segments copied and cleared (cstartend.asm has their ends): the
; initialised data's values in ROM and their places in RAM, and the uninitialised data
@@R_INIT	CSEG	UNIT64KP
_@R_INIT:
@@R_INIS	CSEG	UNIT64KP
_@R_INIS:
@@RLINIT	CSEG	UNIT64KP
_@RLINIT:
@@INIT	DSEG	BASEP
_@INIT:
@@INIS	DSEG	BASEP
_@INIS:
@@INITL	DSEG	UNIT64KP
_@INITL:
@@DATA	DSEG	BASEP
_@DATA:
@@DATAL	DSEG	UNIT64KP
_@DATAL:
@@DATS	DSEG	SADDRP
_@DATS:

; the C library's errno, and its memory pool's break value and area (the library's names)
@@HEAP	DSEG	UNITP
_errno:	DS	2
_@BRKADR:	DS	2
_@MEMTOP:	DS	32

@@CSTART	CSEG	BASE
_cstart:
	SEL	RB0
	MOVW	SP, #STACKTOP
$_IF (ERNIE_HW>=0x00800000)	; CHIP_RL78: the RL78 template's CRC, RAM parity and access guards off
	CLRB	!CRC0CTL
	MOV	RPECTL, #80H
	CLRB	!IAWCTL
$ENDIF
	CALL	!!_hdwinit
	; the short direct area
$_IF (ERNIE_HW>=0x00800000)	; the RL78 template computes the size
	MOV	A, #0E0H
	SUB	A, #20H
	MOV	B, A
$ELSE
	MOV	B, #SADDRSIZE
$ENDIF
	CLRW	AX
?saddr:
	DEC	B
	DEC	B
	MOVW	STACKTOP[B], AX
	BNZ	$?saddr
	; the stack
	MOV	ES, #0FH
	MOVW	AX, #DATASTART
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
	MOV	B, #KEEPEND - (KEEPRAM + 4)
	MOVW	DE, #0
?sum:
	DEC	B
	CLRW	AX
	MOV	A, ES:KEEPRAM + 4[B]
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
	SUBW	AX, #DATASTART
	MOVW	BC, AX
	CLRW	AX
?coldNext:
	DECW	BC
	DECW	BC
	MOVW	ES:DATASTART[BC], AX
	CMPW	AX, BC
	BNZ	$?coldNext
?data:
	CLRW	AX
	MOVW	!_errno, AX
	MOVW	AX, #LOWW(_@MEMTOP)
	MOVW	!_@BRKADR, AX
	; the initialised data
	MOV	ES, #HIGHW(_@R_INIT)
	MOVW	HL, #LOWW(_@R_INIT)
	MOVW	DE, #LOWW(_@INIT)
	BR	$?copy1End
?copy1:
	MOV	A, ES:[HL]
	MOV	[DE], A
	INCW	HL
	INCW	DE
?copy1End:
	MOVW	AX, HL
	CMPW	AX, #LOWW(_?R_INIT)
	BNZ	$?copy1
	MOVW	HL, #LOWW(_@RLINIT)
	MOVW	DE, #LOWW(_@INITL)
	BR	$?copy2End
?copy2:
	MOV	ES, #HIGHW(_@RLINIT)
	MOV	A, ES:[HL]
	MOV	ES, #0FH
	MOV	ES:[DE], A
	INCW	HL
	INCW	DE
?copy2End:
	MOVW	AX, HL
	CMPW	AX, #LOWW(_?RLINIT)
	BNZ	$?copy2
	; the uninitialised data
	MOVW	HL, #LOWW(_@DATA)
	MOVW	AX, #LOWW(_?DATA)
	BR	$?bss1End
?bss1:
	MOV	[HL+0], #0
	INCW	HL
?bss1End:
	CMPW	AX, HL
	BNZ	$?bss1
	MOV	ES, #0FH
	MOVW	HL, #LOWW(_@DATAL)
	MOVW	AX, #LOWW(_?DATAL)
	BR	$?bss2End
?bss2:
	MOV	ES:[HL+0], #0
	INCW	HL
?bss2End:
	CMPW	AX, HL
	BNZ	$?bss2
	MOV	ES, #HIGHW(_@R_INIS)
	MOVW	HL, #LOWW(_@R_INIS)
	MOVW	DE, #LOWW(_@INIS)
	BR	$?copy3End
?copy3:
	MOV	A, ES:[HL]
	MOV	[DE], A
	INCW	HL
	INCW	DE
?copy3End:
	MOVW	AX, HL
	CMPW	AX, #LOWW(_?R_INIS)
	BNZ	$?copy3
	MOVW	HL, #LOWW(_@DATS)
	MOVW	AX, #LOWW(_?DATS)
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
