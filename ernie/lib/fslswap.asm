; A module of the flash self-programming library (Renesas's, for the 78K0R; fsl.asm has
; the core): the boot swap: a security flag set from RAM, FLBTS, the boot cluster swapped.
; A program links the modules it calls. Written from the original; the public names are
; the library's own.

	NAME	fslswap

	PUBLIC	_fslSetSecurityFlagSwap, _fslRunFromRam, _fslRamSetFlbts, _fslSetFlbts
	PUBLIC	_fslSwapBootCluster, _fslCopy
	EXTRN	_fslModifySecurityFlags, _fslFlagsStatus, _fslRestoreInterrupts, _fslMaskInterrupts
	EXTRN	_fslMk0, _fslMk1, _fslStatus

FSLREG	EQU	0800H			; the flash controller's registers (0xf0800)
FSLCMD	EQU	FSLREG + 0		; the command
FSLARG	EQU	FSLREG + 1
FSLINFO	EQU	FSLREG + 3		; what an information command reads
FSLDESC	EQU	FSLREG + 4		; the descriptor's address
FSLINT	EQU	FSLREG + 6		; set: the command may be interrupted
FLBTS	EQU	00C0H			; the boot swap register (0xf00c0)
FIRMWARE	EQU	0EFFF8H		; the self-programming firmware's entry
BLOCKS	EQU	0FFD0H			; its block count (in page 0xe), times 4

@@FSLCOD	CSEG	BASE

; nothing calls it: as fslSetSecurityFlag, the command run from RAM with FLBTS kept
_fslSetSecurityFlagSwap:
	PUSH	AX
	MOV	X, #0FEH
	CALL	$!_fslModifySecurityFlags
	PUSH	PSW
	DI
	MOV	!FSLINT, #0
	CALL	$!_fslRunFromRam
?ramReturn:
	CALL	$!_fslRestoreInterrupts
	POP	PSW
	MOV	C, _fslStatus
	BR	$!_fslFlagsStatus

; command 0x0a, returning through a copy of fslRamSetFlbts at descriptor + 6 (FLBTS as it
; was, saved at descriptor + 5), then to ?ramReturn
_fslRunFromRam:
	PUSH	HL
	MOVW	HL, !FSLDESC
	MOVW	!_fslMk0, AX
	MOV	A, !FLBTS
	MOV	[HL+5], A
	PUSH	DE
	PUSH	BC
	XCHW	AX, HL
	ADDW	AX, #6
	MOVW	!_fslMk1, AX
	XCHW	AX, DE
	MOVW	HL, #LOWW (_fslRamSetFlbts)
	MOVW	AX, #HIGHW (_fslRamSetFlbts)
	MOV	C, #_fslRamEnd - _fslRamSetFlbts
	CALL	$!_fslCopy
	POP	BC
	POP	DE
	POP	HL
	MOVW	AX, #LOWW (?ramReturn)
	MOVW	[SP+0], AX
	MOVW	AX, #HIGHW (?ramReturn)
	MOVW	[SP+2], AX
	SUBW	SP, #4
	MOVW	AX, !_fslMk1
	MOVW	[SP+0], AX
	MOVW	AX, #0FH
	MOVW	[SP+2], AX
	MOVW	AX, !_fslMk0
	SEL	RB3
	MOV	C, #0AH
	CALL	$!_fslMaskInterrupts
	BR	!!FIRMWARE

; run from RAM: FLBTS's bit 7 put back from descriptor + 5
_fslRamSetFlbts:
	DI
	PUSH	AX
	PUSH	HL
	MOVW	HL, !FSLDESC
	MOV	A, [HL+5]
	POP	HL
	AND	A, #80H
	CALL	$!_fslSetFlbts
	POP	AX
	RET

; FLBTS = A, by its protected write sequence
_fslSetFlbts:
	PUSH	PSW
	DI
	MOV	X, A
	MOV	!PFCMD, #0A5H
	MOV	!FLBTS, A
	XOR	A, #0FFH
	MOV	!FLBTS, A
	MOV	A, X
	MOV	!FLBTS, A
	POP	PSW
	RET
_fslRamEnd:

; nothing calls it: FLBTS = B, FLMD0's pull-up off, and the reset vector run
_fslSwapBootCluster:
	XCH	A, B
	CALL	$!_fslSetFlbts
	CLR1	BECTL.7
	MOV	ES, #0
	MOVW	HL, ES:!0
	MOVW	AX, HL
	MOV	CS, #0
	BR	AX

; C bytes from A:HL (far) to DE
_fslCopy:
	XCH	A, X
	MOV	ES, A
	CLRB	B
?copy:
	MOV	A, ES:[HL+B]
	MOV	[DE], A
	INCW	DE
	INC	B
	DEC	C
	BNZ	$?copy
	RET

	END
