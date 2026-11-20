; A runtime helper of CA78K0R's library (cl0rlf.lib, its RL78 form, module lutofr): a program
; links the modules it calls. Written from the original.
;
; an unsigned long to a float

	NAME	lutofr

	PUBLIC	@@lutof, @@lutofe
	EXTRN	@RTARG0, @RTARG2

@@LCODE	CSEG	BASE
@@lutof:
	PUSH	DE
	PUSH	BC
	PUSH	AX
	MOVW	AX, @RTARG2
	MOVW	BC, @RTARG0
	CLRB	@RTARG2+1
@@lutofe:
	ADDW	AX, BC
	SKC
	BZ	$?lutofr5
	SUBW	AX, BC
	MOV	D, #09EH
	BT	A.7, $?lutofr2
?lutofr1:
	DEC	D
	ADDW	AX, AX
	SHLW	BC, 1
	SKNC
	INCW	AX
	BF	A.7, $?lutofr1
?lutofr2:
	XCH	A, C
	CMP	A, #080H
	XCH	A, C
	BC	$?lutofr4
	BNZ	$?lutofr3
	XCHW	AX, BC
	MOV1	CY, A.0
	XCHW	AX, BC
	BNC	$?lutofr4
?lutofr3:
	INC	B
	BNZ	$?lutofr4
	ADDW	AX, #1
	SKNC
	INC	D
?lutofr4:
	MOV1	CY, @RTARG2+1.7
	XCHW	AX, DE
	RORC	A, 1
	MOV	@RTARG2+1, A
	MOV	A, D
	MOV1	A.7, CY
	MOV	@RTARG2, A
	MOV	A, B
	MOV	X, A
	MOV	A, E
	MOVW	@RTARG0, AX
?lutofr5:
	POP	AX
	POP	BC
	POP	DE
	RET

	END
