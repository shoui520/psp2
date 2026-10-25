; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; @RTARG0..2 = AX..@RTARG4 * @RTARG0..2: shift and add, 32 rounds

	NAME	lumul

	PUBLIC	@@lumul
	EXTRN	@RTARG0, @RTARG2, @RTARG4

@@RTCODE	CSEG	BASE
@@lumul:
	PUSH	BC
	PUSH	DE
	PUSH	HL
	MOVW	HL, @RTARG2
	MOVW	BC, @RTARG0
	MOVW	DE, AX
	CLRW	AX
	MOVW	@RTARG0, AX
	MOVW	@RTARG2, AX
	MOVW	AX, @RTARG4
	MOV	@RTARG4, #20H
?lumul:
	XCHW	AX, DE
	SHRW	AX, 1
	XCHW	AX, DE
	RORC	A, 1
	XCH	A, X
	RORC	A, 1
	XCH	A, X
	BNC	$?lumulNext
	PUSH	AX
	MOVW	AX, @RTARG0
	ADDW	AX, BC
	MOVW	@RTARG0, AX
	MOVW	AX, @RTARG2
	SKNC
	INCW	AX
	ADDW	AX, HL
	MOVW	@RTARG2, AX
	POP	AX
?lumulNext:
	SHLW	BC, 1
	XCHW	AX, HL
	ROLWC	AX, 1
	XCHW	AX, HL
	DEC	@RTARG4
	BNZ	$?lumul
	POP	HL
	POP	DE
	POP	BC
	RET

	END
