; A runtime helper of CA78K0R's RL78 library (cl78l.lib), one module of it: the RL78 boards'
; (PCH-2000) programs link the ones they call. Written from the original.
;
; @RTARG0..3 / AX:@RTARG4 (AX the high word), 32 bits, through the RL78's DIVWU
; (0xffffffff for a divisor of 0)

	NAME	ludiv

	PUBLIC	@@ludiv
	EXTRN	@RTARG0, @RTARG2, @RTARG4

@@LCODE	CSEG	BASE
@@ludiv:
	PUSH	AX
	OR	A, X
	OR	A, @RTARG4
	OR	A, @RTARG4+1
	POP	AX
	BZ	$?ludivZero
	PUSH	HL
	PUSH	DE
	PUSH	BC
	MOVW	HL, AX
	MOVW	DE, @RTARG4
	MOVW	BC, @RTARG2
	MOVW	AX, @RTARG0
	DB	0CEH, 0FBH, 0BH	; DIVWU, written out: RA78K0R puts a NOP after its DIVWU, Sony's library has none
	MOVW	@RTARG0, AX
	MOVW	AX, BC
	MOVW	@RTARG2, AX
	POP	BC
	POP	DE
	POP	HL
	RET
?ludivZero:
	CLRW	AX
	DECW	AX
	MOVW	@RTARG0, AX
	MOVW	@RTARG2, AX
	RET

	END
