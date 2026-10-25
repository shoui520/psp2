; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; AX / C, 16 by 8 bits: the quotient in AX, the remainder in C

	NAME	divuw

	PUBLIC	@@divuw

@@RTCODE	CSEG	BASE
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
