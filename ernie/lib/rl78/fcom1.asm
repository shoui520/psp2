; A runtime helper of CA78K0R's library (cl0rlf.lib, its RL78 form, module fcom1r): a program
; links the modules it calls. Written from the original.
;
; the float routines' common ends: the normalization, the rounding and packing of a result,
; and the exceptions' results

	NAME	fcom1r

	PUBLIC	@@fnor, @@funite, @@fstore, @@fret, @@fxinex, @@fret0, @@fret0s, @@fret0x
	PUBLIC	@@fx0div, @@fxover, @@finfws, @@fxnan2, @@fxnan1
	EXTRN	@@fmthrr, @RTARG0, @RTARG2, @RTARG4, @RTARG6

@@LCODE	CSEG	BASE
@@fnor:
	MOVW	DE, @RTARG2
	BT	A.7, $?fcom1r2
?fcom1r1:
	DEC	E
	BZ	$?fcom1r2
	ADDW	AX, AX
	SHLW	BC, 1
	SKNC
	INCW	AX
	BF	A.7, $?fcom1r1
?fcom1r2:
	XCHW	AX, DE
	MOV	A, X
	BR	$@@fstore
?fcom1r3:
	CMPW	AX, #0FFEAH
	BC	$@@fxinex
	MOV	A, X
?fcom1r4:
	DEC	A
	XCHW	AX, BC
?fcom1r5:
	SHRW	AX, 1
	XCHW	AX, DE
	SHRW	AX, 1
	XCHW	AX, DE
	MOV1	A.7, CY
	INC	B
	BNZ	$?fcom1r5
	XCHW	AX, BC
	BR	$@@fstore
@@funite:
	MOV1	CY, A.7
	BC	$?fcom1r3
	CMPW	AX, #0FFH
	OR	A, X
	BNC	$@@fxover
	BZ	$?fcom1r4
@@fstore:
	BT	@RTARG6+1.7, $?fcom1r7
	XCH	A, C
	CMP	A, #080H
	XCH	A, C
	BC	$?fcom1r7
	BNZ	$?fcom1r6
	XCHW	AX, BC
	MOV1	CY, A.0
	XCHW	AX, BC
	BNC	$?fcom1r7
?fcom1r6:
	INC	B
	BNZ	$?fcom1r7
	XCHW	AX, DE
	SET1	A.7
	ADDW	AX, #1
	XCHW	AX, DE
	BNC	$?fcom1r7
	CMP	A, #0FEH
	BZ	$@@fxover
	INC	A
?fcom1r7:
	MOV1	CY, @RTARG6.7
	OR	A, #0
	RORC	A, 1
	XCH	A, D
	MOV1	A.7, CY
	XCH	A, E
	XCH	A, B
	XCH	A, C
	MOV	@RTARG4, A
	CLRB	@RTARG6
	SKNZ
	INC	@RTARG6
@@fret:
	MOV1	CY, @RTARG6+1.7
	SKNC
	RET
	POP	HL
	CMP0	@RTARG6
	SKZ
	CALL	!@@fmthrr
	MOVW	AX, DE
	MOVW	@RTARG2, AX
	MOVW	AX, BC
	MOVW	@RTARG0, AX
	POP	AX
	MOVW	@RTARG6, AX
	POP	BC
	POP	DE
	RET
@@fxinex:
	MOV	A, #2
	BR	$?fcom1r8
@@fret0:
	CLR1	@RTARG6.7
@@fret0s:
	CLRB	A
?fcom1r8:
	MOV1	CY, @RTARG6.7
	MOV	@RTARG6, A
	CLRW	AX
	BR	$?fcom1r10
@@fret0x:
	CLRB	@RTARG4
	BR	$@@fret
@@fx0div:
	MOV	A, #4
	BR	$?fcom1r9
@@fxover:
	MOV	A, #3
	BR	$?fcom1r9
@@finfws:
	CLRB	A
?fcom1r9:
	MOV1	CY, @RTARG6.7
	MOV	@RTARG6, A
	MOVW	AX, #0FE80H
?fcom1r10:
	RORC	A, 1
	MOVW	DE, AX
	CLRW	BC
	BT	@RTARG6+1.7, $@@fret0x
	BR	$@@fret
@@fxnan2:
	MOV	@RTARG6, #5
	BR	$?fcom1r11
@@fxnan1:
	CLRB	@RTARG6
?fcom1r11:
	MOVW	DE, #07FFFH
	BT	@RTARG6+1.7, $@@fret0x
	BR	$@@fret

	END
