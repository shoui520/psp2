; A runtime helper of CA78K0R's RL78 library (cl78l.lib), one module of it: the RL78 boards'
; (PCH-2000) programs link the ones they call. Written from the original.
;
; AX / C, 16 by 8 bits, through the RL78's DIVHU: the quotient in AX, the remainder in C
; (0xffff, and the dividend's low byte, for a divisor of 0)

	NAME	divuw

	PUBLIC	@@divuw

@@LCODE	CSEG	BASE
@@divuw:
	CMP0	C
	BZ	$?divuwZero
	PUSH	DE
	PUSH	BC
	POP	DE
	MOV	D, #0
	DB	0CEH, 0FBH, 03H	; DIVHU, written out: RA78K0R puts a NOP after its DIVHU, Sony's library has none
	XCH	A, E
	MOV	C, A
	XCH	A, E
	POP	DE
	RET
?divuwZero:
	MOV	A, X
	MOV	C, A
	CLRW	AX
	DECW	AX
	RET

	END
