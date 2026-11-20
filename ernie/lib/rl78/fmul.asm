; A runtime helper of CA78K0R's library (cl0rlf.lib, its RL78 form, module fmulr): a program
; links the modules it calls. Written from the original.
;
; the float product AX:BC * @RTARG2:@RTARG0

	NAME	fmulr

	PUBLIC	@@fmul
	EXTRN	@@finfws, @@fret0s, @@funite, @@fxnan1, @@fxnan2, @RTARG0, @RTARG2, @RTARG4
	EXTRN	@RTARG6

@@LCODE	CSEG	BASE
@@fmul:
	PUSH	DE
	PUSH	BC
	MOVW	DE, AX
	MOVW	BC, @RTARG4
	MOVW	@RTARG4, #0
	MOVW	AX, @RTARG6
	PUSH	AX
	MOV	@RTARG6+1, #0
	PUSH	HL
	CLRB	@RTARG6
	MOVW	HL, @RTARG4
	MOV	A, @RTARG2
	AND	A, #07FH
	MOV	@RTARG4+1, A
	OR	A, @RTARG0+1
	OR	A, @RTARG0
	SKNZ
	SET1	@RTARG6.1
	MOV	A, E
	AND	A, #07FH
	MOV	@RTARG4, A
	OR	A, B
	OR	A, C
	SKNZ
	SET1	@RTARG6.0
	MOVW	AX, DE
	ADDW	AX, AX
	MOV	X, A
	XOR1	CY, @RTARG2+1.7
	MOV1	@RTARG6.7, CY
	MOV1	CY, @RTARG2.7
	MOV	A, @RTARG2+1
	ROLC	A, 1
	CMP	A, #0FFH
	BZ	$?fmulr1
	CMP0	A
	BNZ	$?fmulr10
	BR	$?fmulr9
?fmulr1:
	BF	@RTARG6.1, $?fmulr3
	SET1	@RTARG6.5
	BR	$?fmulr10
?fmulr2:
	BT	@RTARG6.0, $?fmulr5
?fmulr3:
	BR	!@@fxnan1
?fmulr4:
	BR	!@@fxnan2
?fmulr5:
	BT	@RTARG6.6, $?fmulr4
?fmulr6:
	BR	!@@finfws
?fmulr7:
	SET1	@RTARG6.2
	INC	A
	BF	@RTARG6.0, $?fmulr11
	BT	@RTARG6.5, $?fmulr4
?fmulr8:
	BR	!@@fret0s
?fmulr9:
	SET1	@RTARG6.3
	INC	A
	MOV1	CY, @RTARG6.1
	MOV1	@RTARG6.6, CY
?fmulr10:
	XCH	A, X
	CMP	A, #0FFH
	BZ	$?fmulr2
	CMP0	A
	BZ	$?fmulr7
?fmulr11:
	BT	@RTARG6.5, $?fmulr6
	BT	@RTARG6.6, $?fmulr8
	ADD	X, A
	CLRB	A
	ROLC	A, 1
	SUBW	AX, #07FH
	MOVW	DE, AX
	MOV	A, B
	MOV	X, A
	SHLW	BC, 8
	MOV	A, L
	MOV	C, A
	MOV	A, @RTARG4
	BF	@RTARG6.2, $?fmulr13
?fmulr12:
	DECW	DE
	SHLW	BC, 1
	ROLWC	AX, 1
	BF	A.7, $?fmulr12
?fmulr13:
	SET1	A.7
	MOVW	@RTARG2, AX
	MOVW	AX, BC
	XCHW	AX, HL
	MOV	X, @RTARG0+1
	MOV	B, @RTARG0
	MOV	C, @RTARG4+1
	XCH	A, C
	BF	@RTARG6.3, $?fmulr15
?fmulr14:
	DECW	DE
	SHLW	BC, 1
	ROLWC	AX, 1
	BF	A.7, $?fmulr14
?fmulr15:
	SET1	A.7
	XCHW	AX, DE
	MOVW	@RTARG0, AX
	PUSH	BC
	MOVW	AX, HL
	MULHU
	MOVW	AX, BC
	MOVW	@RTARG4, AX
	POP	BC
	MOVW	AX, @RTARG2
	MULHU
	ADDW	AX, @RTARG4
	MOVW	@RTARG4, AX
	SKNC
	INCW	BC
	MOVW	AX, BC
	XCHW	AX, HL
	MOVW	BC, AX
	MOVW	AX, DE
	MULHU
	ADDW	AX, @RTARG4
	MOV	@RTARG4, A
	SKNC
	INCW	BC
	MOVW	AX, BC
	XCHW	AX, DE
	MOVW	BC, @RTARG2
	MULHU
	ADDW	AX, HL
	SKNC
	INCW	BC
	ADDW	AX, DE
	SKNC
	INCW	BC
	XCHW	AX, BC
	BT	A.7, $?fmulr16
	MOV1	CY, @RTARG4.7
	ROLWC	BC, 1
	ROLWC	AX, 1
	MOVW	DE, AX
	MOVW	AX, @RTARG0
	BR	!@@funite
?fmulr16:
	MOVW	DE, AX
	MOVW	AX, @RTARG0
	INCW	AX
	BR	!@@funite

	END
