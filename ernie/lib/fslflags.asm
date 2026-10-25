; A module of the flash self-programming library (Renesas's, for the 78K0R; fsl.asm has
; the core): the security flags read once and written back. A program links the modules it
; calls. Written from the original; the public names are the library's own.

	NAME	fslflags

	PUBLIC	_fslModifySecurityFlags, _fslSetFlags, _fslFlagsStatus, _fslMaskFlags
	EXTRN	_fslModeOn, _fslCommand, _fslExit, _fslParameterError, _fslFlagsRead

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

; the security flags (information 3 and 7) read into the descriptor once, masked by X
_fslModifySecurityFlags:
	CALL	$!_fslModeOn
	CMP0	!_fslFlagsRead
	BNZ	$?flagsRead
	PUSH	AX
	MOV	!FSLINFO, #3
	MOV	A, #9
	CALL	$!_fslCommand
	POP	AX
	CMP0	C
	BNZ	$?flagsError
	CALL	$!_fslMaskFlags
	INCW	!FSLDESC
	MOV	!FSLINFO, #7
	MOV	A, #9
	CALL	$!_fslCommand
	CMP0	C
	DECW	!FSLDESC
	BNZ	$?flagsError
?flagsRead:
	PUSH	HL
	MOVW	HL, !FSLDESC
	BT	[HL].0, $?flagsSet
	POP	HL
	RET
?flagsSet:
	POP	HL
_fslSetFlags:
	MOV	A, #0AH
	CALL	$!_fslCommand
_fslFlagsStatus:
	MOV	A, C
	CMP	A, #1FH
	BNZ	$?flagsDone
	MOV	!_fslFlagsRead, #1
	BR	$?flagsExit
?flagsDone:
	MOV	!_fslFlagsRead, #0
?flagsExit:
	POP	AX
	BR	$!_fslExit
?flagsError:
	POP	AX
	BR	$!_fslParameterError

; the descriptor's first byte in its set form, masked by X
_fslMaskFlags:
	PUSH	HL
	PUSH	AX
	MOVW	HL, !FSLDESC
	MOV	A, [HL]
	SHL	A, 1
	OR	A, #1
	AND	A, X
	MOV	[HL], A
	POP	AX
	POP	HL
	RET

	END
