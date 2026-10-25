; A module of the flash self-programming library (Renesas's, for the 78K0R; fsl.asm has
; the core): the security flags read (FSL_GetSecurityFlags). A program links the modules
; it calls. Written from the original; the public names are the library's own.

	NAME	fslinfo

	PUBLIC	_FSL_GetSecurityFlags
	EXTRN	_fslModeOn, _fslCommand, _fslExit

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

; AX: where the flags go (2 bytes of information 3); 0 or the firmware's status
_FSL_GetSecurityFlags:
	PUSH	DE
	PUSH	AX
	MOV	A, #3
	MOV	X, #2
	BR	$!?info
?info:
	CALL	$!_fslModeOn
	MOV	!FSLINFO, A
	MOV	A, #9
	CALL	$!_fslCommand
	POP	DE
	CMP0	C
	BNZ	$?infoDone
	XCH	A, X
	PUSH	BC
	XCH	A, C
	CLRB	B
	CALL	$!_fslCopyInfo
	POP	BC
?infoDone:
	POP	DE
	BR	$!_fslExit

; C bytes of the descriptor from byte B on, to DE
_fslCopyInfo:
	PUSH	AX
	PUSH	HL
	MOVW	HL, !FSLDESC
?copyInfo:
	MOV	A, [HL+B]
	MOV	[DE], A
	INCW	DE
	INC	B
	DEC	C
	BNZ	$?copyInfo
	POP	HL
	POP	AX
	RET

	END
