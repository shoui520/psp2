; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; @RTARG0..3 / AX:@RTARG4 (AX the high word), 32 bits, shifted and subtracted bit by bit
; (0 for a divisor of 0)

	NAME	ludiv

	PUBLIC	@@ludiv
	EXTRN	@RTARG0, @RTARG2, @RTARG4

@@RTCODE	CSEG	BASE
@@ludiv:
	PUSH	AX
	OR	A, X
	OR	A, @RTARG4
	OR	A, @RTARG4+1
	POP	AX
	BZ	$?ludivZero
	PUSH	BC
	PUSH	DE
	PUSH	HL
	MOVW	DE, @RTARG0
	MOVW	HL, @RTARG2
	MOVW	@RTARG2, AX
	CLRW	AX
	CLRW	BC
	MOV	@RTARG0, #20H
?ludivBit:
	XCHW	AX, DE
	ADDW	AX, AX
	XCHW	AX, DE
	XCHW	AX, HL
	ROLWC	AX, 1
	XCHW	AX, HL
	ROLWC	AX, 1
	ROLWC	BC, 1
	XCHW	AX, BC
	CMPW	AX, @RTARG2
	XCHW	AX, BC
	BC	$?ludivNext
	BNZ	$?ludivSubtract
	CMPW	AX, @RTARG4
	BC	$?ludivNext
?ludivSubtract:
	SUBW	AX, @RTARG4
	XCH	A, C
	SUBC	A, @RTARG2
	XCH	A, C
	XCHW	AX, BC
	SUBC	A, @RTARG2+1
	XCHW	AX, BC
	INC	E
?ludivNext:
	DEC	@RTARG0
	BNZ	$?ludivBit
	MOVW	AX, DE
	MOVW	@RTARG0, AX
	MOVW	AX, HL
	MOVW	@RTARG2, AX
	POP	HL
	POP	DE
	POP	BC
	RET
?ludivZero:
	CLRW	AX
	MOVW	@RTARG0, AX
	MOVW	@RTARG2, AX
	RET

	END
