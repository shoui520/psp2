; A runtime helper of CA78K0R's library (cl0rlf.lib, its RL78 form, module fmulxr): a program
; links the modules it calls. Written from the original.
;
; the product in the library's extended form (the math functions')

	NAME	fmulxr

	PUBLIC	@@fmulx
	EXTRN	@@finfws, @@fret0s, @@funite, @@fxnan1, @@fxnan2, @RTARG0, @RTARG2, @RTARG4
	EXTRN	@RTARG6

@@LCODE	CSEG	BASE
@@fmulx:
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
	BZ	$?fmulxr1
	CMP0	A
	BNZ	$?fmulxr10
	BR	$?fmulxr9
?fmulxr1:
	BF	@RTARG6.1, $?fmulxr3
	SET1	@RTARG6.5
	BR	$?fmulxr10
?fmulxr2:
	BT	@RTARG6.0, $?fmulxr5
?fmulxr3:
	BR	!@@fxnan1
?fmulxr4:
	BR	!@@fxnan2
?fmulxr5:
	BT	@RTARG6.6, $?fmulxr4
?fmulxr6:
	BR	!@@finfws
?fmulxr7:
	SET1	@RTARG6.2
	INC	A
	BF	@RTARG6.0, $?fmulxr11
	BT	@RTARG6.5, $?fmulxr4
?fmulxr8:
	BR	!@@fret0s
?fmulxr9:
	SET1	@RTARG6.3
	INC	A
	MOV1	CY, @RTARG6.1
	MOV1	@RTARG6.6, CY
?fmulxr10:
	XCH	A, X
	CMP	A, #0FFH
	BZ	$?fmulxr2
	CMP0	A
	BZ	$?fmulxr7
?fmulxr11:
	BT	@RTARG6.5, $?fmulxr6
	BT	@RTARG6.6, $?fmulxr8
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
	BF	@RTARG6.2, $?fmulxr13
?fmulxr12:
	DECW	DE
	SHLW	BC, 1
	ROLWC	AX, 1
	BF	A.7, $?fmulxr12
?fmulxr13:
	SET1	A.7
	MOVW	@RTARG2, AX
	MOVW	AX, BC
	XCHW	AX, HL
	MOV	X, @RTARG0+1
	MOV	B, @RTARG0
	MOV	C, @RTARG4+1
	XCH	A, C
	BF	@RTARG6.3, $?fmulxr15
?fmulxr14:
	DECW	DE
	SHLW	BC, 1
	ROLWC	AX, 1
	BF	A.7, $?fmulxr14
?fmulxr15:
	SET1	A.7
	XCHW	AX, DE
	MOVW	@RTARG0, AX
	MOVW	AX, HL
	MOV	A, D
	MULU	X
	MOVW	@RTARG4, AX
	MOVW	AX, DE
	MOV	A, H
	MULU	X
	ADDW	AX, @RTARG4
	XCHW	AX, HL
	MOV	X, A
	MOV	A, D
	MULU	X
	ADDC	A, #0
	MOVW	@RTARG4, AX
	MOVW	AX, BC
	MOV	A, @RTARG2+1
	MULU	X
	ADDW	AX, HL
	MOVW	HL, AX
	MOV1	@RTARG6.0, CY
	MOV	A, B
	MOV	X, @RTARG2
	MULU	X
	ADDW	AX, HL
	MOVW	HL, AX
	MOV	X, A
	CLRB	A
	ADDC	A, #0
	MOV1	CY, @RTARG6.0
	ADDC	A, #0
	ADDW	AX, @RTARG4
	MOV	H, #0
	SKNC
	INC	H
	XCHW	AX, BC
	MOV	X, @RTARG2+1
	MULU	X
	ADDW	AX, BC
	MOVW	BC, AX
	SKNC
	INC	H
	MOVW	AX, DE
	MOV	A, @RTARG2
	MULU	X
	ADDW	AX, BC
	MOVW	BC, AX
	SKNC
	INC	H
	MOV	A, D
	MOV	X, @RTARG2
	MULU	X
	MOVW	@RTARG4, AX
	MOV	A, E
	MOV	X, @RTARG2+1
	MULU	X
	ADDW	AX, @RTARG4
	MOVW	@RTARG4, AX
	MOV	A, D
	MOV	X, @RTARG2+1
	MULU	X
	ADDC	A, #0
	XCH	A, @RTARG4
	ADD	B, A
	MOV	A, H
	ADDC	A, @RTARG4+1
	ADDC	@RTARG4, #0
	ADD	X, A
	MOV	A, @RTARG4
	ADDC	A, #0
	BT	A.7, $?fmulxr16
	ADDW	AX, AX
	MOVW	DE, AX
	MOV	A, L
	ROLC	A, 1
	ROLWC	BC, 1
	SKNC
	INCW	DE
	MOVW	AX, @RTARG0
	BR	!@@funite
?fmulxr16:
	MOVW	DE, AX
	MOVW	AX, @RTARG0
	INCW	AX
	BR	!@@funite

	END
