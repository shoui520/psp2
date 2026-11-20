; A runtime helper of CA78K0R's library (cl0rlf.lib, its RL78 form, module fdivxr): a program
; links the modules it calls. Written from the original.
;
; and the quotient

	NAME	fdivxr

	PUBLIC	@@fdivx
	EXTRN	@@finfws, @@fret0s, @@funite, @@fx0div, @@fxnan1, @@fxnan2, @RTARG0, @RTARG2
	EXTRN	@RTARG4, @RTARG6

@@LCODE	CSEG	BASE
@@fdivx:
	CLRB	@RTARG6
	MOVW	HL, @RTARG4
	MOV	A, E
	AND	A, #07FH
	MOV	@RTARG4, A
	OR	A, B
	OR	A, C
	SKNZ
	SET1	@RTARG6.1
	MOV	A, @RTARG2
	AND	A, #07FH
	MOV	@RTARG4+1, A
	OR	A, @RTARG0+1
	OR	A, @RTARG0
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
	BZ	$?fdivxr1
	CMP0	A
	BNZ	$?fdivxr11
	BR	$?fdivxr10
?fdivxr1:
	BF	@RTARG6.0, $?fdivxr3
	SET1	@RTARG6.4
	BR	$?fdivxr11
?fdivxr2:
	BT	@RTARG6.1, $?fdivxr5
?fdivxr3:
	BR	!@@fxnan1
?fdivxr4:
	BR	!@@fxnan2
?fdivxr5:
	BT	@RTARG6.4, $?fdivxr4
?fdivxr6:
	BR	!@@fret0s
	BT	@RTARG6.6, $?fdivxr8
?fdivxr7:
	SET1	@RTARG6.3
	INC	A
	BF	@RTARG6.1, $?fdivxr12
	BT	@RTARG6.6, $?fdivxr4
?fdivxr8:
	BR	!@@fx0div
?fdivxr9:
	BR	!@@finfws
?fdivxr10:
	SET1	@RTARG6.2
	INC	A
	MOV1	CY, @RTARG6.0
	MOV1	@RTARG6.6, CY
?fdivxr11:
	XCH	A, X
	CMP	A, #0FFH
	BZ	$?fdivxr2
	CMP0	A
	BZ	$?fdivxr7
?fdivxr12:
	BT	@RTARG6.4, $?fdivxr9
	BT	@RTARG6.6, $?fdivxr6
	SUB	X, A
	CLRB	A
	SUBC	A, #0
	ADDW	AX, #07FH
	MOVW	DE, AX
	MOV	A, B
	MOV	X, A
	SHLW	BC, 8
	MOV	A, L
	MOV	C, A
	MOV	A, @RTARG4
	BF	@RTARG6.3, $?fdivxr14
?fdivxr13:
	INCW	DE
	SHLW	BC, 1
	ROLWC	AX, 1
	BF	A.7, $?fdivxr13
?fdivxr14:
	SET1	A.7
	MOVW	@RTARG2, AX
	MOVW	AX, @RTARG0
	XCH	A, X
	XCHW	AX, BC
	MOVW	@RTARG0, AX
	MOV	A, H
	XCH	A, C
	MOV	X, A
	MOV	A, @RTARG4+1
	BF	@RTARG6.2, $?fdivxr16
?fdivxr15:
	DECW	DE
	SHLW	BC, 1
	ROLWC	AX, 1
	BF	A.7, $?fdivxr15
?fdivxr16:
	SET1	A.7
	XCHW	AX, DE
	MOVW	@RTARG4, AX
	MOVW	AX, #0100H
	MOVW	HL, #0
	XCHW	AX, DE
	CMPW	AX, @RTARG2
	XCHW	AX, DE
	BNZ	$?fdivxr17
	XCHW	AX, BC
	CMPW	AX, @RTARG0
	XCHW	AX, BC
?fdivxr17:
	BNC	$?fdivxr18
	DECW	@RTARG4
	BR	$?fdivxr20
?fdivxr18:
	MOV	A, #3
	SKNC
?fdivxr19:
	BF	A.0, $?fdivxr20
	XCHW	AX, DE
	SUBW	AX, @RTARG2
	XCHW	AX, DE
	XCHW	AX, BC
	SUBW	AX, @RTARG0
	XCHW	AX, BC
	SKNC
	DECW	DE
?fdivxr20:
	SHLW	BC, 1
	XCHW	AX, DE
	XCH	A, X
	ROLC	A, 1
	XCH	A, X
	ROLC	A, 1
	XCHW	AX, DE
	BC	$?fdivxr22
	XCHW	AX, DE
	CMPW	AX, @RTARG2
	XCHW	AX, DE
	BNZ	$?fdivxr21
	XCHW	AX, BC
	CMPW	AX, @RTARG0
	XCHW	AX, BC
?fdivxr21:
	NOT1	CY
?fdivxr22:
	ROLC	A, 1
	XCH	A, X
	ROLC	A, 1
	XCH	A, X
	XCHW	AX, HL
	XCH	A, X
	ROLC	A, 1
	XCH	A, X
	ROLC	A, 1
	XCHW	AX, HL
	BNC	$?fdivxr19
	XCH	A, X
	MOVW	BC, AX
	MOVW	AX, HL
	MOVW	DE, AX
	MOVW	AX, @RTARG4
	BR	!@@funite

	END
