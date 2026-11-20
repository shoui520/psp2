; Renesas' RL78 flash self-programming library (FSL, type 01) and data flash library (PFDL), as
; USS-1001's (PCH-2000) application links them; Sony linked Renesas' library. The commands go to
; the boot ROM's firmware (0xefff8, the data flash's 0xef004/0xef008) through the library's
; work area at 0xf0800 (FSLCMD..FSLSIZE the command's arguments, FSLDESC the descriptor's
; address; FSLSTATE the state bits, FSLRESUME where a boot swap goes on). Written from the
; original; the public names are the library's own, the labels ours.
;
; FSL_PrepareFunctions copies ?blockA to 0xf0a80 and FSL_PrepareExtFunctions ?blockB to
; 0xf0b80: each runs from there, its jump table (by the command's index in C) holding RAM
; addresses. The commands the program calls (FSL_BlankCheck..FSL_SwapBootCluster) run from RAM
; at 0xfd090, where _rcopy (lib/rl78/rcopy.asm) copies them from _rcopyTable's entry.
;
; Each command returns its status in C: 0 done, 5 a parameter out of range, 0x1f the library
; not ready for it.

	NAME	fsl

	PUBLIC	_FSL_Init, _FSL_Open, _FSL_Close, _FSL_PrepareFunctions, _FSL_PrepareExtFunctions
	PUBLIC	_rcopyTable, _fslRamCommands
	PUBLIC	_fslBlockA, _fslBlockB
	PUBLIC	_PFDL_Open, _PFDL_Execute, _PFDL_Handler, _PFDL_Close

FSLCMD	EQU	0800H			; the command's address, low word (0xf0800)
FSLADDR	EQU	0801H			; its address's middle byte (and a byte argument)
FSLHIGH	EQU	0802H			; its address's high byte
FSLINFO	EQU	0803H			; 1: the data flash's
FSLDESC	EQU	0804H			; the descriptor's (the buffer's) address
FSLSIZE	EQU	0806H			; the bytes it covers
FSLBUF	EQU	0880H			; the descriptor's copy (the CPU clock), and PFDL's state
FSLPCMD	EQU	0881H			; PFDL's command
FSLSTATE EQU	0890H			; bit 0 initialised, 1 open, 2 and 3 prepared, 4 busy,
					; 5 a boot swap pending, 7 a write's last block
FSLOPT	EQU	0891H			; bit 0: each command runs to its end (auto status check)
FSLRESUME EQU	0892H			; where FSL_SwapBootCluster goes on (in RAM)
FSLCHECK EQU	0F08C4H			; the firmware's status check
FIRMWARE EQU	0EFFF8H			; the self-programming firmware's entry
PFDLRUN	EQU	0EF004H			; the data flash firmware's handler
PFDLEND	EQU	0EF008H			; and its close
RAMA	EQU	0A80H			; where FSL_PrepareFunctions copies ?blockA
RAMB	EQU	0B80H			; and FSL_PrepareExtFunctions ?blockB
$_IF (ERNIE_FW>=0x01050000)
RAMCMD	EQU	0D136H			; USS-1002's (0xfd136: more near data before them)
$ELSE
$_IF (ERNIE_FW>=0x01030511)
RAMCMD	EQU	0D090H			; where _rcopy copies the commands (0xfd090)
$ELSE
RAMCMD	EQU	0D038H			; 0x01030505's (0xfd038)
$ENDIF
$ENDIF
; the jump tables' RAM addresses (word[C] takes no relocated operand): RAMA + ?tableA -
; ?blockA, RAMB + ?tableB - ?blockB
TABLEA	EQU	RAMA + 0F4H
TABLEB	EQU	RAMB + 78H
; the commands the program calls, at their RAM addresses (RAMCMD + their place after
; _fslRamCommands: the link places code where it is stored, not where it runs); the program
; gets them from the link file's SYMBOLs (app/uss1001.lk), the link taking no absolute publics
_FSL_BlankCheck	EQU	0F0000H + RAMCMD
_FSL_Erase	EQU	0F0000H + RAMCMD + 4
_FSL_IVerify	EQU	0F0000H + RAMCMD + 8
_FSL_Write	EQU	0F0000H + RAMCMD + 0CH
_FSL_InvertBootFlag EQU	0F0000H + RAMCMD + 42H
_FSL_SwapBootCluster EQU 0F0000H + RAMCMD + 4AH

; the library's ROM part
@@FSLCOD	CSEG	BASE
_FSL_Init:
	PUSH	HL
	PUSH	DE
	PUSH	AX
	MOVW	HL, AX
	MOVW	DE, #FSLBUF
	CLRW	AX
?clear:
	MOVW	[DE], AX
	INCW	DE
	INCW	DE
	XCHW	AX, DE
	CMPW	AX, #08C0H
	XCHW	AX, DE
	BNZ	$?clear
	MOV	A, ES
	PUSH	AX
	MOV	A, C
	MOV	ES, A
	BT	CSC.0, $?initError
	CLRB	A
	CMP	A, ES:[HL+2]
	SKZ
	SET1	!FSLOPT.0
	MOV	!FSLINFO, A
	CMP	A, ES:[HL+0]
	SKZ
	ONEB	A
	MOV	!FSLADDR, A
	MOVW	AX, #FSLBUF
	MOVW	DE, AX
	MOVW	!FSLDESC, AX
	MOV	A, ES:[HL+1]
	MOV	[DE], A
	MOV	ES, #0EH
	CMP	A, ES:!0FFE8H		; the firmware's lowest clock
	BC	$?initError
	MOV	C, ES:!0FFE9H		; and its highest
	CMP	C, A
	BC	$?initError
	MOV	C, #0
	CALL	!!FIRMWARE
	SET1	!FSLSTATE.0
	BR	$?initEnd
?initError:
	MOV	C, #5
?initEnd:
	POP	AX
	MOV	ES, A
	POP	AX
	POP	DE
	POP	HL
	RET

_FSL_Open:
	SET1	!FSLSTATE.1
	RET
	NOP

_FSL_Close:
	CLR1	!FSLSTATE.1
	RET
	NOP

_FSL_PrepareFunctions:
	PUSH	HL
	PUSH	DE
	PUSH	BC
	PUSH	AX
	SET1	!FSLSTATE.2
	MOV	A, ES
	PUSH	AX
	MOV	ES, #0
	MOVW	HL, #LOWW(?blockA)
	MOVW	BC, #LOWW(?blockB)
	MOVW	DE, #RAMA
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
	POP	AX
	POP	BC
	POP	DE
	POP	HL
	RET
	NOP

_FSL_PrepareExtFunctions:
	PUSH	HL
	PUSH	DE
	PUSH	BC
	PUSH	AX
	SET1	!FSLSTATE.3
	MOV	A, ES
	PUSH	AX
	MOV	ES, #0
	MOVW	HL, #LOWW(?blockB)
	MOVW	BC, #LOWW(?blockEnd)
	MOVW	DE, #RAMB
	BR	$!?copy
	NOP

; the commands, run from RAMA: C the command's index into ?tableA
?blockA:
_fslBlockA:
	PUSH	HL
	PUSH	DE
	PUSH	AX
	MOV	A, CS
	MOV	D, A
	MOV	A, ES
	MOV	E, A
	MOV	CS, #0FH
	MOVW	AX, TABLEA[C]
	BR	AX
?status:
	CALL	!!FSLCHECK
	MOV	A, C
	CMP	A, #0FFH
	BZ	$?exitA
	CLR1	!FSLSTATE.4
	CLR1	!FSLSTATE.7
	MOV	A, !FSLSTATE
	BF	A.5, $?exitA
	MOV	A, #0FH
	MOV	CS, A
	MOVW	AX, !FSLRESUME
	BR	AX
?erase:
	MOV	C, #3
	BR	$?block
?iverify:
	MOV	C, #6
	BR	$?block
?blank:
	MOV	C, #8
; the block's number on the stack: its address, a block of 0x400 bytes
?block:
	MOVW	AX, [SP+0]
	MOVW	HL, AX
	MOV	ES, #0EH
	MOVW	AX, ES:!0FFE6H		; the firmware's block count
	CMPW	AX, HL
	BC	$?range
	MOVW	AX, HL
	SHLW	AX, 2
	MOV	!FSLHIGH, A
	MOV	A, X
	MOV	!FSLADDR, A
	CLRB	!FSLCMD
	CLRB	!FSLINFO
	MOVW	AX, #0400H
	MOVW	!FSLSIZE, AX
	BR	$?run
?range:
	MOV	C, #5
?exitA:
	MOV	A, D
	MOV	CS, A
	MOV	A, E
	MOV	ES, A
	POP	AX
	POP	DE
	POP	HL
	RET
; fsl_write_t on the stack: the buffer, the address (in 4 bytes), the words (1-0x40)
?write:
	MOVW	AX, [SP+0]
	MOVW	HL, AX
	MOV	A, [HL+0]
	MOV	X, A
	MOV	A, [HL+1]
	MOVW	!FSLDESC, AX
	MOV	A, [HL+2]
	AND	A, #3
	BNZ	$?range
	MOV	A, [HL+5]
	CMP0	A
	BNZ	$?range
	MOV	A, [HL+6]
	CMP0	A
	BZ	$?range
	CMP	A, #41H
	BNC	$?range
	MOV	X, A
	CLRB	A
	SHLW	AX, 2
	MOVW	!FSLSIZE, AX
	DECW	AX
	DECW	AX
	PUSH	DE
	MOVW	DE, AX
	MOV	A, [HL+2]
	MOV	X, A
	MOV	A, [HL+3]
	MOVW	!FSLCMD, AX
	ADDW	AX, DE
	XCHW	AX, DE
	MOV	A, [HL+4]
	MOV	!FSLHIGH, A
	ADDC	A, #0
	XCHW	AX, DE
	MOVW	HL, AX
	MOVW	AX, DE
	MOV	X, A
	POP	DE
	BC	$?range
	MOV	ES, #0EH
	MOV	A, L
	SUBC	A, ES:!0FFDFH		; the flash's end
	MOV	A, H
	SUBC	A, ES:!0FFE0H
	MOV	A, X
	SUBC	A, ES:!0FFE1H
	BNC	$?range
	MOV	C, #4
?run:
	MOVW	AX, BC
	CALL	!!FIRMWARE
?runCheck:
	MOV	A, C
	CMP	A, #0FFH
	BNZ	$?exitA
	MOV	A, !FSLOPT
	BT	A.0, $?runWait
	SET1	!FSLSTATE.4
	MOV	A, X
	CMP	A, #3
	SKNZ
	SET1	!FSLSTATE.7
	BR	$?exitA
?runWait:
	CALL	!!FSLCHECK
	BR	$?runCheck
	DB	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
?tableA:
	DW	LOWW(RAMA + ?erase - ?blockA)
	DW	LOWW(RAMA + ?iverify - ?blockA)
	DW	LOWW(RAMA + ?blank - ?blockA)
	DW	LOWW(RAMA + ?write - ?blockA)
	DW	LOWW(RAMA + ?status - ?blockA)
	DW	0

; the extended commands, run from RAMB: C the command's index into ?tableB
?blockB:
_fslBlockB:
	PUSH	DE
	PUSH	AX
	MOV	A, CS
	MOV	D, A
	MOV	A, ES
	MOV	E, A
	MOV	CS, #0FH
	MOVW	AX, TABLEB[C]
	BR	AX
; the boot flag: read the security flags, H's bit 0 inverted into them, set them
?bootFlag:
	MOVW	AX, #FSLBUF
	MOVW	!FSLDESC, AX
	MOV	C, #10H
	CALL	!!FIRMWARE
	CMP0	C
	BNZ	$?flagsError
	MOV	A, H
	AND	A, !FSLBUF
	MOV	!FSLBUF, A
	MOV	A, H
	AND	A, #1
	SKZ
	SET1	!FSLBUF.0
	MOV	C, #11H
	BR	$?setFlags
?flagsError:
	CLR1	!FSLSTATE.5
?exitB:
	MOV	A, D
	MOV	CS, A
	MOV	A, E
	MOV	ES, A
	POP	AX
	POP	DE
	POP	HL
	RET
?swap:
	MOV	C, #13H
?setFlags:
	CALL	!!FIRMWARE
?setCheck:
	MOV	A, C
	CMP	A, #0FFH
	BNZ	$?setDone
	MOV	A, !FSLOPT
	BT	A.0, $?setWait
	SET1	!FSLSTATE.4
	BR	$?exitB
?setDone:
	MOV	A, !FSLSTATE
	BF	A.5, $?exitB
	MOV	A, #0FH
	MOV	CS, A
	MOVW	AX, !FSLRESUME
	BR	AX
?setWait:
	CALL	!!FSLCHECK
	BR	$?setCheck
	DB	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
?tableB:
	DW	LOWW(RAMB + ?bootFlag - ?blockB)
	DW	LOWW(RAMB + ?swap - ?blockB)
	DW	0
	DW	0
?blockEnd:

; the data flash library: AX the descriptor (its byte 1 the CPU clock)
@@PFDLCOD	CSEG	BASE
_PFDL_Open:
	SET1	DFLCTL.0
	PUSH	HL
	MOVW	HL, AX
	MOVW	!FSLDESC, AX
	MOV	A, [HL+1]
	MOV	!FSLADDR, A
	ONEB	!FSLINFO
	CLRB	C
	CALL	!!FIRMWARE
	POP	HL
	MOV	!FSLBUF, #4
	RET

; AX the request: its index (the byte or block), the buffer, the bytes, the command
_PFDL_Execute:
	PUSH	DE
	PUSH	HL
	MOVW	HL, AX
	PUSH	BC
	ONEB	!FSLINFO
	MOV	A, [HL+2]
	XCH	A, X
	MOV	A, [HL+3]
	MOVW	!FSLDESC, AX
	MOVW	DE, #1000H
	MOV	!FSLHIGH, #0FH
	MOV	A, [HL+6]
	MOV	!FSLPCMD, A
	CMP	A, #3
	BZ	$?pfdlErase
	MOVW	AX, DE
	XCH	A, X
	ADD	A, [HL+0]
	XCH	A, X
	ADDC	A, [HL+1]
	MOVW	!FSLCMD, AX
	MOVW	DE, AX
	MOV	A, [HL+4]
	XCH	A, X
	MOV	A, [HL+5]
	MOVW	!FSLSIZE, AX
	MOVW	BC, AX
	MOV	A, #0
	CMP	A, [HL+6]
	BZ	$?pfdlRead
?pfdlRun:
	MOV	C, !FSLPCMD
	CALL	!!FIRMWARE
	MOV	A, C
	BR	$?pfdlEnd
?pfdlErase:
	MOVW	AX, #0400H
	MOVW	!FSLSIZE, AX
	MOV	A, [HL+0]
	SHL	A, 2
	CLRB	X
	ADDW	AX, DE
	MOVW	!FSLCMD, AX
	BR	$?pfdlRun
?pfdlRead:
	MOVW	HL, !FSLDESC
?pfdlByte:
	MOV	A, [DE]
	MOV	[HL], A
	INCW	HL
	INCW	DE
	DECW	BC
	CLRW	AX
	CMPW	AX, BC
	BNZ	$?pfdlByte
	MOV	A, #0
?pfdlEnd:
	POP	BC
	MOV	C, A
	POP	HL
	POP	DE
	RET

_PFDL_Handler:
	CALL	!!PFDLRUN
	RET

_PFDL_Close:
	PUSH	BC
	CALL	!!PFDLEND
	POP	BC
	CLR1	DFLCTL.0
	CLRB	!FSLBUF
	RET

; _rcopy's table: the commands below, copied to RAMCMD
@@FSLRAM	CSEG	BASE
_rcopyTable:
	DW	1
	DW	LOWW(?ramEnd - ?ramStart)
	DW	RAMCMD
	DW	LOWW(?ramStart)
	DB	0FH, 0

; the commands, run from RAMCMD (relative branches only; ?swapResume's RAM address is FSLRESUME)
?ramStart:
_fslRamCommands:
?blankCheck:
	MOV	C, #4
	BR	$?command
?erase0:
	MOV	C, #0
	BR	$?command
?iverify0:
	MOV	C, #2
	BR	$?command
?write0:
	MOV	C, #6
?command:
	CALL	$!?readyCommand
	SKNZ
	BR	!!0F0000H + RAMA
	MOV	C, #1FH
	RET
; Z: the state bits (FSLSTATE under A's mask) are X
?readyExt:
	PUSH	AX
	MOV	A, #0FFH
	MOV	X, #0FH
	BR	$?ready
?ready1:
	PUSH	AX
	MOV	A, #47H
	MOV	X, #7
	BR	$?ready
?ready2:
	PUSH	AX
	MOV	A, #47H
	MOV	X, #7
	BR	$?ready
?ready3:
	PUSH	AX
	MOV	A, #47H
	MOV	X, #47H
	BR	$?ready
?readyCommand:
	PUSH	AX
	MOV	A, #0F7H
	MOV	X, #7
?ready:
	AND	A, !FSLSTATE
	CMP	A, X
	POP	AX
	RET
?invertBootFlag:
	PUSH	HL
	MOV	H, #0FEH
	MOV	C, #0
	BR	$!?extCommand
?swapBootCluster:
	CALL	$!?readyExt
	BNZ	$?swapBusy
	PUSH	AX
	SET1	!FSLSTATE.5
	MOVW	AX, #LOWW(RAMCMD + ?swapResume - ?ramStart)
	MOVW	!FSLRESUME, AX
	POP	AX
	PUSH	HL
	MOV	H, #0FEH
	MOV	C, #0
	BR	!!0F0000H + RAMB
?swapBusy:
	MOV	C, #1FH
	RET
?swapResume:
	CLR1	!FSLSTATE.5
	CMP0	C
	BNZ	$?swapEnd
	MOV	C, #15H
	CALL	!!FIRMWARE
?swapEnd:
	MOV	A, D
	MOV	CS, A
	MOV	A, E
	MOV	ES, A
	POP	AX
	POP	DE
	POP	HL
	RET
?extCommand:
	CALL	$!?readyExt
	SKNZ
	BR	!!0F0000H + RAMB
	MOV	C, #1FH
	POP	HL
	RET
?ramEnd:

	END
