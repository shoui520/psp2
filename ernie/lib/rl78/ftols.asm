; A runtime helper of CA78K0R's library (cl0rlf.lib, its RL78 form, module ftolsr): a program
; links the modules it calls. Written from the original.
;
; a float to a signed long

	NAME	ftolsr

	PUBLIC	@@ftols, @@ftolse
	EXTRN	@RTARG0, @RTARG2

@@LCODE	CSEG	BASE
@@ftols:
	PUSH	DE
	PUSH	BC
	PUSH	AX
	MOV	A, @RTARG2
	SET1	A.7
	MOV	B, A
	MOV	C, @RTARG0+1
	MOVW	AX, @RTARG2
	ADDW	AX, AX
	CLRB	X
	CMP	A, #09FH
	BNC	$?ftolsr10
@@ftolse:
	SUB	A, #07FH
	BC	$?ftolsr10
	SUB	A, #010H
	BNC	$?ftolsr4
	INC	A
	XCHW	AX, BC
	BZ	$?ftolsr2
?ftolsr1:
	SHRW	AX, 1
	INC	B
	BNZ	$?ftolsr1
?ftolsr2:
	BF	@RTARG2+1.7, $?ftolsr3
	MOVW	DE, AX
	CLRW	AX
	SUBW	AX, BC
	MOVW	BC, AX
	CLRW	AX
	SUBW	AX, DE
	SKNC
	DECW	BC
?ftolsr3:
	MOVW	@RTARG0, AX
	MOVW	AX, BC
	MOVW	@RTARG2, AX
	POP	AX
	POP	BC
	POP	DE
	RET
?ftolsr4:
	SUB	A, #8
	BNC	$?ftolsr7
	XCHW	AX, BC
	INC	B
	MOV	C, @RTARG0
	BZ	$?ftolsr6
?ftolsr5:
	SHRW	AX, 1
	XCH	A, C
	RORC	A, 1
	XCH	A, C
	INC	B
	BNZ	$?ftolsr5
?ftolsr6:
	XCH	A, C
	XCH	A, X
	BR	$?ftolsr2
?ftolsr7:
	SUB	A, #7
	MOV	D, A
	MOV	A, @RTARG0
	BZ	$?ftolsr9
?ftolsr8:
	SHRW	AX, 1
	XCHW	AX, BC
	SHRW	AX, 1
	XCHW	AX, BC
	MOV1	A.7, CY
	INC	D
	BNZ	$?ftolsr8
	BR	$?ftolsr2
?ftolsr9:
	BF	@RTARG2+1.7, $?ftolsr10
	SHLW	BC, 1
	ADDW	AX, BC
	BC	$?ftolsr10
	MOV	B, #080H
	BZ	$?ftolsr3
?ftolsr10:
	CLRW	BC
	CLRW	AX
	BR	$?ftolsr3

	END
