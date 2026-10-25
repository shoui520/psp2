; The flash self-programming library's core (Renesas's, for the 78K0R; Sony linked it; the
; other modules are fsl*.asm, a program links the ones it calls): commands to
; the boot ROM's self-programming firmware (0xefff8) through the flash controller's
; registers at 0xf0800 (command 0x0800, data 0x0801, info 0x0803, the descriptor's address
; 0x0804, 0x0806 the interrupts' state), with FLPMC switched to self-programming mode
; around each. The descriptor (FSL_Init's argument) is a near buffer the firmware writes
; its results to. Written from the original; the public names are the library's own.
;
; Each command returns its status in C: 0 done, 5 a parameter out of range, 0x1f with the
; interrupts left enabled a command interrupted (FSL_* then reports it).

	NAME	fsl

	PUBLIC	_FSL_BlankCheck, _FSL_Erase, _FSL_IVerify, _FSL_ModeCheck
	PUBLIC	_FSL_Init, _FSL_Init_cont, _fslModeReset, _fslVersion, _fslConfig
	PUBLIC	_fslCheckBlock, _fslCommand, _fslParameterError, _fslExit, _fslModeOn
	PUBLIC	_fslSetFlpmc, _fslMaskInterrupts, _fslRestoreInterrupts
	PUBLIC	_fslMk0, _fslMk1, _fslMk2, _fslFlagsRead, _fslCommandCode, _fslStatus

FSLREG	EQU	0800H			; the flash controller's registers (0xf0800)
FSLCMD	EQU	FSLREG + 0		; the command
FSLARG	EQU	FSLREG + 1
FSLINFO	EQU	FSLREG + 3		; what an information command reads
FSLDESC	EQU	FSLREG + 4		; the descriptor's address
FSLINT	EQU	FSLREG + 6		; set: the command may be interrupted
FLBTS	EQU	00C0H			; the boot swap register (0xf00c0)
FIRMWARE	EQU	0EFFF8H		; the self-programming firmware's entry
BLOCKS	EQU	0FFD0H			; its block count (in page 0xe), times 4

; the library's state: the interrupt masks while a command runs, the caller's registers,
; the descriptor's byte 5 (FLBTS) and whether the security flags were read
@@FSLDAT	DSEG	UNITP
_fslMk0:	DS	2
_fslMk1:	DS	2
_fslMk2:	DS	2
_fslAx:		DS	2
_fslBc:		DS	2
_fslDe:		DS	2
_fslHl:		DS	2
_fslFlagsRead:	DS	1
_fslInterrupt:	DS	1			; FSLINT across an interrupted command

@@FSLSAD	DSEG	SADDR
_fslCommandCode:	DS	1		; the firmware's command
_fslStatus:	DS	1			; and its status

; the user's settings (the library's fsl_user file): the clock in MHz and the voltage mode
@@FSLCNS	CSEG	UNIT64KP
_fslConfig:	DB	08H, 01H

@@FSLCOD	CSEG	BASE

; C = 8, 3, 6: AX is the block
_FSL_BlankCheck:
	MOV	C, #8
	BR	$?blockCommand
_FSL_Erase:
	MOV	C, #3
	BR	$?blockCommand
_FSL_IVerify:
	MOV	C, #6
?blockCommand:
	CALL	$!_fslModeOn
	CALL	$!_fslCheckBlock
	BC	$?blockError
	XCH	A, X
	MOV	!FSLINFO, A
	MOV	A, !FSLDESC
	XCH	A, X
	MOV	!FSLDESC, A
	MOV	A, C
	CALL	$!_fslCommand
	XCH	A, X
	MOV	!FSLDESC, A
	BR	$!_fslExit
?blockError:
	BR	$!_fslParameterError

; CY: the block in AX is past the last
_fslCheckBlock:
	PUSH	AX
	PUSH	DE
	MOVW	DE, AX
	MOV	ES, #HIGHW (FIRMWARE)
	MOV	X, ES:!BLOCKS
	MOV	A, ES:!BLOCKS + 1
	SHRW	AX, 2
	CMPW	AX, DE
	POP	DE
	POP	AX
	RET

; 1 when FLMD0 is low (no self-programming)
_FSL_ModeCheck:
	CLRB	C
	BT	FLPMC.2, $?modeOk
	ONEB	C
?modeOk:
	RET

; runs the firmware's command A with the interrupts masked (9: an information command, run
; with them disabled); the status in C
_fslCommand:
	MOV	_fslCommandCode, A
	PUSH	PSW
	CMP	A, #9
	BNZ	$?notInfo
	DI
	PUSH	AX
	MOV	A, !FSLINT
	MOV	!_fslInterrupt, A
	POP	AX
	MOV	!FSLINT, #0
	BR	$!?run
?notInfo:
	BT	PSW.7, $?enabled
	MOV	!FSLINT, #0
?enabled:
	CMP	A, #0
	BNZ	$?resume
	CMP0	!FSLCMD
	BZ	$?run
?resume:
	PUSH	PSW
	DI
	CMP0	!FSLINT
	BZ	$?noResume
	MOV	_fslStatus, #1FH
	POP	PSW
	POP	PSW
	BR	$!?done
?noResume:
	POP	PSW
?run:
	CALL	$!_fslSaveRegisters
	SEL	RB3
	CALL	$!_fslMaskInterrupts
	CALL	!!FIRMWARE
	CALL	$!_fslRestoreInterrupts
	POP	PSW
	CALL	$!_fslRestoreRegisters
?done:
	CMP	A, #9
	BNZ	$?clear
	PUSH	AX
	MOV	A, !FSLINT
	OR	A, !_fslInterrupt
	MOV	!FSLINT, A
	POP	AX
	BR	$!?status
?clear:
	MOV	!FSLINT, #0
?status:
	MOV	C, _fslStatus
	RET

; AX: the descriptor (a near buffer); C = 0, or 2 to continue
_FSL_Init:
	MOV	C, #0
	BR	$?init
_FSL_Init_cont:
	MOV	C, #2
?init:
	CALL	$!_fslModeOn
	MOV	!_fslFlagsRead, #0
	MOV	!FSLINT, #0
	MOVW	!FSLDESC, AX
	PUSH	HL
	MOVW	HL, AX
	MOV	A, C
	MOV	!FSLCMD, A
	MOV	ES, #HIGHW (_fslConfig)
	MOV	A, ES:!LOWW (_fslConfig)
	MOV	[HL], A
	MOV	ES, #HIGHW (_fslConfig + 1)
	MOV	A, ES:!LOWW (_fslConfig + 1)
	MOV	!FSLARG, A
	POP	HL
	MOV	A, #0
	CALL	$!_fslCommand
	BR	$_fslExit

_fslParameterError:
	MOV	C, #5
_fslExit:
	MOV	!FSLINT, #0
	PUSH	AX
	MOV	A, #0CH
	CALL	$!_fslSetFlpmc
	POP	AX
	RET

; self-programming mode on
_fslModeOn:
	PUSH	AX
	MOV	A, #1DH
	CALL	$!_fslSetFlpmc
	POP	AX
	RET

; FLPMC = A, by its protected write sequence
_fslSetFlpmc:
	PUSH	PSW
	DI
	MOV	X, A
	MOV	PFCMD, #0A5H
	MOV	FLPMC, A
	XOR	A, #0FFH
	MOV	FLPMC, A
	MOV	A, X
	MOV	FLPMC, A
	POP	PSW
	RET

; nothing calls it: self-programming mode set again from FLPMC's bits, interrupts allowed
_fslModeReset:
	PUSH	PSW
	DI
	PUSH	AX
	MOV	A, FLPMC
	AND	A, #1BH
	CALL	$!_fslModeOn
	MOV	!FSLINT, #1
	CALL	$!_fslSetFlpmc
	POP	AX
	POP	PSW
	RET

; with the interrupts enabled, every interrupt masked while the firmware runs
_fslMaskInterrupts:
	BT	PSW.7, $?maskDone
	PUSH	AX
	MOVW	AX, MK0
	MOVW	!_fslMk0, AX
	MOVW	AX, MK1
	MOVW	!_fslMk1, AX
	MOVW	AX, MK2
	MOVW	!_fslMk2, AX
	MOVW	AX, #0FFFFH
	MOVW	MK0, AX
	MOVW	MK1, AX
	MOVW	MK2, AX
	POP	AX
?maskDone:
	RET

_fslRestoreInterrupts:
	BT	PSW.7, $?restoreDone
	PUSH	AX
	MOVW	AX, !_fslMk0
	MOVW	MK0, AX
	MOVW	AX, !_fslMk1
	MOVW	MK1, AX
	MOVW	AX, !_fslMk2
	MOVW	MK2, AX
	POP	AX
?restoreDone:
	RET

_fslSaveRegisters:
	PUSH	AX
	MOVW	!_fslAx, AX
	MOVW	AX, BC
	MOVW	!_fslBc, AX
	MOVW	AX, DE
	MOVW	!_fslDe, AX
	MOVW	AX, HL
	MOVW	!_fslHl, AX
	POP	AX
	RET

_fslRestoreRegisters:
	MOVW	AX, !_fslAx
	MOVW	BC, !_fslBc
	MOVW	DE, !_fslDe
	MOVW	HL, !_fslHl
	RET

_fslVersion:
	DB	'SK0RT02N200GV120', 0

	END
