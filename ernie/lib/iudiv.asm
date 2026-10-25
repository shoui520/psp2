; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; @RTARG0 / AX, 16 bits (0xffff for a divisor of 0)

	NAME	iudiv

	PUBLIC	@@iudiv
	EXTRN	@RTARG0

@@RTCODE	CSEG	BASE
@@iudiv:
	CMPW	AX, #0
	BZ	$?iudivZero
	PUSH	BC
	PUSH	DE
	PUSH	HL
	MOVW	BC, @RTARG0
	MOVW	DE, AX
	CLRW	AX
	MOV	L, #10H
?iudiv:
	SHLW	BC, 1
	ROLWC	AX, 1
	CMPW	AX, DE
	BC	$?iudivNext
	SUBW	AX, DE
	INC	C
?iudivNext:
	DEC	L
	BNZ	$?iudiv
	MOVW	AX, BC
	POP	HL
	POP	DE
	POP	BC
	RET
?iudivZero:
	DECW	AX
	RET

	END
