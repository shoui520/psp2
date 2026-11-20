; A runtime helper of CA78K0R's library (cl0rlf.lib, its RL78 form, module faddr): a program
; links the modules it calls. Written from the original.
;
; the float sum AX:BC + @RTARG2:@RTARG0

	NAME	faddr

	PUBLIC	@@fadd, @@fadde, @@faddx
	EXTRN	@@fnor, @@fret, @@fret0, @@fxnan1, @@fxnan2, @@fxover, @RTARG0, @RTARG2
	EXTRN	@RTARG4, @RTARG6

@@LCODE	CSEG	BASE
@@fadd:
	PUSH	DE
	PUSH	BC
	MOVW	DE, AX
	MOVW	BC, @RTARG4
	MOVW	@RTARG4, #0
	MOVW	AX, @RTARG6
	PUSH	AX
	MOV	@RTARG6+1, #2
@@fadde:
	PUSH	HL
@@faddx:
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
	MOV1	@RTARG6.7, CY
	MOV	X, A
	MOV1	CY, @RTARG2.7
	MOV	A, @RTARG2+1
	ROLC	A, 1
	XOR1	CY, @RTARG6.7
	MOV1	@RTARG6.6, CY
	CMP	A, #0FFH
	BZ	$?faddr1
	CMP0	A
	BZ	$?faddr10
	SET1	@RTARG4+1.7
	BR	$?faddr11
?faddr1:
	BF	@RTARG6.1, $?faddr3
	SET1	@RTARG6.5
	BR	$?faddr11
?faddr2:
	BT	@RTARG6.0, $?faddr4
?faddr3:
	BR	!@@fxnan1
?faddr4:
	BF	@RTARG6.5, $?faddr8
	BF	@RTARG6.6, $?faddr8
	BR	!@@fxnan2
?faddr5:
	INC	A
	BF	@RTARG6.0, $?faddr12
	BT	@RTARG6.4, $?faddr7
?faddr6:
	MOVW	DE, @RTARG2
	MOVW	BC, @RTARG0
	MOV	A, H
	BR	$?faddr9
?faddr7:
	BF	@RTARG6.6, $?faddr8
	MOV	A, #07FH
	AND	D, A
?faddr8:
	MOV	A, L
?faddr9:
	MOV	@RTARG4, A
	CLRB	@RTARG6
	BR	!@@fret
?faddr10:
	INC	A
	MOV1	CY, @RTARG6.1
	MOV1	@RTARG6.4, CY
?faddr11:
	XCH	A, X
	CMP	A, #0FFH
	BZ	$?faddr2
	BT	@RTARG6.5, $?faddr6
	CMP0	A
	BZ	$?faddr5
	SET1	@RTARG4.7
?faddr12:
	BT	@RTARG6.4, $?faddr8
	SUB	A, X
	BZ	$?faddr14
	BNC	$?faddr13
	CMP	A, #0E1H
	BC	$?faddr6
	XOR	A, #0FFH
	INC	A
	SET1	CY
	BR	$?faddr14
?faddr13:
	CMP	A, #020H
	BNC	$?faddr8
	ADD	X, A
?faddr14:
	MOVW	@RTARG2, AX
	MOVW	AX, @RTARG0
	MOVW	DE, @RTARG4
	XCHW	AX, HL
	XCH	A, C
	XCHW	AX, BC
	XCH	A, L
	XCHW	AX, HL
	XCH	A, E
	BC	$?faddr16
	BZ	$?faddr18
	XCHW	AX, HL
?faddr15:
	SHRW	AX, 1
	XCHW	AX, DE
	SHRW	AX, 1
	XCHW	AX, DE
	MOV1	A.7, CY
	DEC	@RTARG2+1
	BNZ	$?faddr15
	XCHW	AX, HL
	BR	$?faddr18
?faddr16:
	XCHW	AX, BC
?faddr17:
	SHRW	AX, 1
	XCHW	AX, BC
	SHRW	AX, 1
	XCHW	AX, BC
	MOV1	A.7, CY
	DEC	@RTARG2+1
	BNZ	$?faddr17
	XCHW	AX, BC
?faddr18:
	XCHW	AX, DE
	BF	@RTARG6.6, $?faddr24
	CMPW	AX, DE
	BNZ	$?faddr19
	XCHW	AX, HL
	CMPW	AX, BC
	XCHW	AX, HL
	BZ	$?faddr22
?faddr19:
	BNC	$?faddr20
	XCHW	AX, DE
	SUBW	AX, DE
	XCHW	AX, BC
	SUBW	AX, HL
	XCHW	AX, BC
	SKNC
	DECW	AX
	BR	$?faddr21
?faddr20:
	XOR	@RTARG6, #080H
	SUBW	AX, DE
	XCHW	AX, HL
	SUBW	AX, BC
	MOVW	BC, AX
	MOVW	AX, HL
	SKNC
	DECW	AX
?faddr21:
	BR	!@@fnor
?faddr22:
	BR	!@@fret0
?faddr23:
	BR	!@@fxover
?faddr24:
	XCHW	AX, BC
	ADDW	AX, HL
	XCHW	AX, BC
	XCH	A, X
	ADDC	A, E
	XCH	A, X
	ADDC	A, D
	BNC	$?faddr21
	CMP	@RTARG2, #0FEH
	BZ	$?faddr23
	INC	@RTARG2
	XCHW	AX, BC
	SHRW	AX, 1
	XCHW	AX, BC
	SHRW	AX, 1
	SET1	A.7
	XCHW	AX, BC
	MOV1	A.7, CY
	XCHW	AX, BC
	BR	$?faddr21

	END
