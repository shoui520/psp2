; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; @RTARG0 / A, 8 bits (0xff for a divisor of 0)

	NAME	cudiv

	PUBLIC	@@cudiv
	EXTRN	@RTARG0

@@RTCODE	CSEG	BASE
@@cudiv:
	CMP0	A
	BZ	$?cudivZero
	PUSH	BC
	MOVW	BC, #8
	XCH	A, @RTARG0
?cudiv:
	ADD	A, A
	XCHW	AX, BC
	ROLC	A, 1
	CMP	A, @RTARG0
	BC	$?cudivNext
	SUB	A, @RTARG0
	INC	B
?cudivNext:
	XCHW	AX, BC
	DEC	C
	BNZ	$?cudiv
	POP	BC
	RET
?cudivZero:
	DEC	A
	RET

	END
