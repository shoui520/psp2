; The compiler's runtime helpers (CA78K0R's, which the code it generates calls by these
; names): long and int arithmetic too long to expand inline. A long operand is AX (low) with
; BC (high), or a pair of the short direct registers @RTARG0..@RTARG6; the result comes back
; in the same place. Written from the original.

	NAME	runtime

	PUBLIC	@@lucom, @@iumul, @@lumul, @@cudiv, @@iudiv, @@curem, @@luadd
	PUBLIC	@@lulsh, @@lursh, @@iscmp, @@luband, @@lubor, @@lubxor, @@divuw
	EXTRN	@RTARG0, @RTARG2, @RTARG4

@@RTCODE	CSEG	BASE

; ~@RTARG0..2
@@lucom:
	XOR	@RTARG0, #0FFH
	XOR	@RTARG0+1, #0FFH
	XOR	@RTARG2, #0FFH
	XOR	@RTARG2+1, #0FFH
	RET

; AX * @RTARG0, 16 bits
@@iumul:
	PUSH	BC
	PUSH	DE
	MOVW	BC, @RTARG0
	XCH	A, C
	MOVW	DE, AX
	XCH	A, B
	MULU	X
	XCHW	AX, BC
	MULU	X
	XCHW	AX, DE
	MULU	X
	ADD	A, C
	ADD	A, E
	POP	DE
	POP	BC
	RET

; @RTARG0..2 = AX..@RTARG4 * @RTARG0..2: shift and add, 32 rounds
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

; @RTARG0 / A, 8 bits (0xff for a divisor of 0)
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

; @RTARG0 / AX, 16 bits (0xffff for a divisor of 0)
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

; @RTARG0 % A, 8 bits (the dividend for a divisor of 0)
@@curem:
	CMP0	A
	BZ	$?curemZero
	PUSH	BC
	MOV	B, @RTARG0
	MOV	@RTARG0, A
	MOV	C, #8
	CLRB	A
?curem:
	SHL	B, 1
	ROLC	A, 1
	CMP	A, @RTARG0
	SKC
	SUB	A, @RTARG0
	DEC	C
	BNZ	$?curem
	POP	BC
	RET
?curemZero:
	MOV	A, @RTARG0
	RET

; @RTARG0..2 += AX..@RTARG4
@@luadd:
	ADDW	AX, @RTARG2
	MOVW	@RTARG2, AX
	MOVW	AX, @RTARG4
	ADDW	AX, @RTARG0
	MOVW	@RTARG0, AX
	SKNC
	INCW	@RTARG2
	RET

; @RTARG0..2 <<= A
@@lulsh:
	CMP0	A
	BZ	$?lulshEnd
	PUSH	AX
	PUSH	BC
	MOVW	BC, @RTARG0
	MOV	@RTARG0, A
	MOVW	AX, @RTARG2
?lulsh:
	SHLW	BC, 1
	ROLWC	AX, 1
	DEC	@RTARG0
	BNZ	$?lulsh
	MOVW	@RTARG2, AX
	MOVW	AX, BC
	MOVW	@RTARG0, AX
	POP	BC
	POP	AX
?lulshEnd:
	RET

; @RTARG0..2 >>= A
@@lursh:
	CMP0	A
	BZ	$?lurshEnd
	PUSH	AX
	PUSH	BC
	MOVW	BC, @RTARG2
	MOV	@RTARG2, A
	MOVW	AX, @RTARG0
?lursh:
	SHRW	AX, 1
	XCHW	AX, BC
	SHRW	AX, 1
	XCHW	AX, BC
	MOV1	A.7, CY
	DEC	@RTARG2
	BNZ	$?lursh
	MOVW	@RTARG0, AX
	MOVW	AX, BC
	MOVW	@RTARG2, AX
	POP	BC
	POP	AX
?lurshEnd:
	RET

; AX against @RTARG0, signed: CY set when AX is less
@@iscmp:
	CMPW	AX, @RTARG0
	BZ	$?iscmpEnd
	XOR1	CY, A.7
	XOR1	CY, @RTARG0+1.7
	NOT1	CY
?iscmpEnd:
	RET

; @RTARG0..2 &= AX..@RTARG4
@@luband:
	AND	A, @RTARG2+1
	MOV	@RTARG2+1, A
	MOV	A, X
	AND	A, @RTARG2
	MOV	@RTARG2, A
	MOVW	AX, @RTARG4
	AND	A, @RTARG0+1
	XCH	A, X
	AND	A, @RTARG0
	XCH	A, X
	MOVW	@RTARG0, AX
	RET

; @RTARG0..2 |= AX..@RTARG4
@@lubor:
	OR	A, @RTARG2+1
	MOV	@RTARG2+1, A
	MOV	A, X
	OR	A, @RTARG2
	MOV	@RTARG2, A
	MOVW	AX, @RTARG4
	OR	A, @RTARG0+1
	XCH	A, X
	OR	A, @RTARG0
	XCH	A, X
	MOVW	@RTARG0, AX
	RET

; @RTARG0..2 ^= AX..@RTARG4
@@lubxor:
	XOR	A, @RTARG2+1
	MOV	@RTARG2+1, A
	MOV	A, X
	XOR	A, @RTARG2
	MOV	@RTARG2, A
	MOVW	AX, @RTARG4
	XOR	A, @RTARG0+1
	XCH	A, X
	XOR	A, @RTARG0
	XCH	A, X
	MOVW	@RTARG0, AX
	RET

; AX / C, 16 by 8 bits: the quotient in AX, the remainder in C
@@divuw:
	PUSH	HL
	PUSH	DE
	PUSH	BC
	XCHW	AX, BC
	MOVW	HL, AX
	MOV	H, #10H
	CLRW	AX
	MOVW	DE, AX
?divuw:
	SHLW	BC, 1
	ROLC	A, 1
	BC	$?divuwSub
	CMP	A, L
	BC	$?divuwZero
?divuwSub:
	SUB	A, L
	SET1	CY
	SKC
?divuwZero:
	CLR1	CY
	XCHW	AX, DE
	ROLWC	AX, 1
	XCHW	AX, DE
	DEC	H
	BNZ	$?divuw
	POP	BC
	MOV	C, A
	MOVW	AX, DE
	POP	DE
	POP	HL
	RET

	END
