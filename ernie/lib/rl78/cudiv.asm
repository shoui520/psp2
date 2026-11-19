; A runtime helper of CA78K0R's RL78 library (cl78l.lib), one module of it: the RL78 boards'
; (PCH-2000) programs link the ones they call. Written from the original.
;
; @RTARG0 / A, 8 bits, through the RL78's DIVHU (0xff for a divisor of 0)

	NAME	cudiv

	PUBLIC	@@cudiv
	EXTRN	@RTARG0

@@LCODE	CSEG	BASE
@@cudiv:
	CMP0	A
	BZ	$?cudivZero
	PUSH	DE
	PUSH	BC
	MOVW	BC, AX
	SHRW	AX, 8
	MOVW	DE, AX
	MOV	X, @RTARG0
	DB	0CEH, 0FBH, 03H	; DIVHU, written out: RA78K0R puts a NOP after its DIVHU, Sony's library has none
	MOV	A, X
	MOV	B, A
	MOVW	AX, BC
	POP	BC
	POP	DE
	RET
?cudivZero:
	DEC	A
	RET

	END
