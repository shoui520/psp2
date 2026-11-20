; A runtime helper of CA78K0R's library (cl0rlf.lib, its RL78 form, module fdivr): a program
; links the modules it calls. Written from the original.
;
; the float quotient AX:BC / @RTARG2:@RTARG0

	NAME	fdivr

	PUBLIC	@@fdiv
	EXTRN	@@finfws, @@fret0s, @@funite, @@fx0div, @@fxnan1, @@fxnan2, @RTARG0, @RTARG2
	EXTRN	@RTARG4, @RTARG6

@@LCODE	CSEG	BASE
@@fdiv:
	PUSH	DE
	PUSH	BC
	MOVW	DE, AX
	MOVW	BC, @RTARG4
	MOVW	@RTARG4, #0
	MOVW	AX, @RTARG6
	PUSH	AX
	MOV	@RTARG6+1, #1
	PUSH	HL
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
	BZ	$?fdivr1
	CMP0	A
	BNZ	$?fdivr11
	BR	$?fdivr10
?fdivr1:
	BF	@RTARG6.0, $?fdivr3
	SET1	@RTARG6.4
	BR	$?fdivr11
?fdivr2:
	BT	@RTARG6.1, $?fdivr5
?fdivr3:
	BR	!@@fxnan1
?fdivr4:
	BR	!@@fxnan2
?fdivr5:
	BT	@RTARG6.4, $?fdivr4
?fdivr6:
	BR	!@@fret0s
	BT	@RTARG6.6, $?fdivr8
?fdivr7:
	SET1	@RTARG6.3
	INC	A
	BF	@RTARG6.1, $?fdivr12
	BT	@RTARG6.6, $?fdivr4
?fdivr8:
	BR	!@@fx0div
?fdivr9:
	BR	!@@finfws
?fdivr10:
	SET1	@RTARG6.2
	INC	A
	MOV1	CY, @RTARG6.0
	MOV1	@RTARG6.6, CY
?fdivr11:
	XCH	A, X
	CMP	A, #0FFH
	BZ	$?fdivr2
	CMP0	A
	BZ	$?fdivr7
?fdivr12:
	BT	@RTARG6.4, $?fdivr9
	BT	@RTARG6.6, $?fdivr6
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
	BF	@RTARG6.3, $?fdivr14
?fdivr13:
	INCW	DE
	SHLW	BC, 1
	ROLWC	AX, 1
	BF	A.7, $?fdivr13
?fdivr14:
	SET1	A.7
	XCH	A, X
	XCH	A, B
	MOV	C, A
	CLRB	A
	MOVW	@RTARG2, AX
	MOVW	AX, @RTARG0
	XCH	A, X
	XCHW	AX, BC
	MOVW	@RTARG0, AX
	MOV	A, H
	XCH	A, C
	MOV	X, A
	MOV	A, @RTARG4+1
	BF	@RTARG6.2, $?fdivr16
?fdivr15:
	DECW	DE
	SHLW	BC, 1
	ROLWC	AX, 1
	BF	A.7, $?fdivr15
?fdivr16:
	SET1	A.7
	XCHW	AX, DE
	MOVW	@RTARG4, AX
	MOVW	AX, DE
	XCHW	AX, BC
	MOVW	HL, @RTARG2
	MOVW	DE, @RTARG0
	DB	0CEH, 0FBH, 0BH	; DIVWU, written out: RA78K0R puts a NOP after its DIVWU, the library has none
	PUSH	AX
	MOV	A, L
	MOV	B, A
	MOV	A, D
	MOV	C, A
	MOV	A, E
	CLRB	X
	MOVW	HL, @RTARG2
	MOVW	DE, @RTARG0
	DB	0CEH, 0FBH, 0BH	; DIVWU, written out: RA78K0R puts a NOP after its DIVWU, the library has none
	PUSH	AX
	MOV	A, L
	MOV	B, A
	MOV	A, D
	MOV	C, A
	MOV	A, E
	CLRB	X
	MOVW	HL, @RTARG2
	MOVW	DE, @RTARG0
	DB	0CEH, 0FBH, 0BH	; DIVWU, written out: RA78K0R puts a NOP after its DIVWU, the library has none
	PUSH	AX
	MOV	A, L
	MOV	B, A
	MOV	A, D
	MOV	C, A
	MOV	A, E
	CLRB	X
	MOVW	HL, @RTARG2
	MOVW	DE, @RTARG0
	DB	0CEH, 0FBH, 0BH	; DIVWU, written out: RA78K0R puts a NOP after its DIVWU, the library has none
	POP	BC
	MOV	A, C
	MOVW	BC, AX
	POP	DE
	POP	AX
	SHLW	AX, 8
	MOV	D, A
	BC	$?fdivr17
	DECW	@RTARG4
	BR	$?fdivr18
?fdivr17:
	MOVW	AX, DE
	SHRW	AX, 1
	SET1	A.7
	MOVW	DE, AX
	MOVW	AX, BC
	RORC	A, 1
	XCH	A, X
	RORC	A, 1
	XCH	A, X
	MOVW	BC, AX
?fdivr18:
	MOVW	AX, @RTARG4
	BR	!@@funite

	END
