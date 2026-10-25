; The flash self-programming library (Renesas's, for the 78K0R; Sony linked it): commands to
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
	PUBLIC	_FSL_Init, _FSL_Init_cont, _FSL_GetSecurityFlags, _FSL_Write, _FSL_EEPROMWrite
	PUBLIC	_FSL_Open, _FSL_Close
	PUBLIC	_fslSetSecurityFlag, _fslSetSecurityFlagSwap, _fslSwapBootCluster, _fslModeReset
	PUBLIC	_fslVersion, _fslConfig

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

; the security flags with bit 0 of their set form cleared, written back (command 0x0a)
_fslSetSecurityFlag:
	PUSH	AX
	MOV	X, #0FEH
	CALL	$!_fslModifySecurityFlags
	BR	$!?setFlags

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
?setFlags:
	MOV	A, #0AH
	CALL	$!_fslCommand
?flagsStatus:
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
	BR	$!?flagsStatus

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

@@FSLOPN	CSEG	BASE

; FLMD0's pull-up on, for the commands, and off
_FSL_Open:
	SET1	BECTL.7
	RET

_FSL_Close:
	CLR1	BECTL.7
	RET

	END
