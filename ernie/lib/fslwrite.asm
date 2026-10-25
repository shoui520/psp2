; A module of the flash self-programming library (Renesas's, for the 78K0R; fsl.asm has
; the core): the writes (FSL_Write, FSL_EEPROMWrite). A program links the modules it
; calls. Written from the original; the public names are the library's own.

	NAME	fslwrite

	PUBLIC	_FSL_Write, _FSL_EEPROMWrite
	EXTRN	_fslModeOn, _fslCommand, _fslExit, _fslParameterError

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

; AX, C: the flash address; on the stack, the words (1 to 0x40, from the descriptor); 4 a
; write, 0x17 a write and verify
_FSL_Write:
	PUSH	DE
	MOV	E, #4
	BR	$?write
_FSL_EEPROMWrite:
	PUSH	DE
	MOV	E, #17H
?write:
	CALL	$!_fslModeOn
	PUSH	HL
	MOVW	HL, #FSLREG
	MOVW	[HL], AX
	MOV	A, C
	MOV	[HL+2], A
	MOV	A, [SP+8]
	MOV	[HL+3], A
	CMP0	A
	BZ	$?writeError
	CMP	A, #41H
	BNC	$?writeError
	MOV	X, A
	CLRB	A
	SHLW	AX, 2
	DECW	AX
	ADDW	AX, [HL+0]
	XCHW	AX, BC
	XCH	A, X
	ADDC	A, #0
	XCH	A, X
	MOV	A, #0FFH
	SUB	A, C
	MOV	ES, #HIGHW (FIRMWARE)
	MOV	A, ES:!BLOCKS
	SUBC	A, B
	MOV	A, ES:!BLOCKS + 1
	SUBC	A, X
	BC	$?writeError
	MOV	A, [HL]
	AND	A, #3
	BNZ	$?writeError
	MOV	A, E
	POP	HL
	POP	DE
	CALL	$!_fslCommand
	BR	$!_fslExit
?writeError:
	POP	HL
	POP	DE
	BR	$!_fslParameterError

	END
