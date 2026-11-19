; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; @RTARG0..3 / AX:@RTARG4 (AX the high word), signed 32 bits: the magnitudes divided by
; @@ludiv, the quotient negated when one of them is negative

	NAME	lsdiv

	PUBLIC	@@lsdiv
	EXTRN	@RTARG0, @RTARG2, @RTARG4, @@ludiv

@@RTCODE	CSEG	BASE
@@lsdiv:
	PUSH	BC
	CLRB	B
	BF	@RTARG2+1.7, $?lsdivDivisor
	INC	B
	PUSH	AX
	CLRW	AX
	SUBW	AX, @RTARG0
	MOVW	@RTARG0, AX
	SUBC	A, A
	MOV	X, A
	SUBW	AX, @RTARG2
	MOVW	@RTARG2, AX
	POP	AX
?lsdivDivisor:
	BF	A.7, $?lsdivDivide
	INC	B
	PUSH	BC
	MOVW	BC, AX
	CLRW	AX
	SUBW	AX, @RTARG4
	MOVW	@RTARG4, AX
	SUBC	A, A
	MOV	X, A
	SUBW	AX, BC
	POP	BC
?lsdivDivide:
	CALL	!@@ludiv
	DEC	B
	BNZ	$?lsdivEnd
	CLRW	AX
	SUBW	AX, @RTARG0
	MOVW	@RTARG0, AX
	SUBC	A, A
	MOV	X, A
	SUBW	AX, @RTARG2
	MOVW	@RTARG2, AX
?lsdivEnd:
	POP	BC
	RET

	END
