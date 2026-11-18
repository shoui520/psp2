; A runtime helper of CA78K0R's RL78 library (cl78l.lib), one module of it: the RL78 boards'
; (PCH-2000) programs link the ones they call. Written from the original.
;
; @RTARG0 % A, 8 bits, through the RL78's DIVHU (the dividend for a divisor of 0)

	NAME	curem

	PUBLIC	@@curem
	EXTRN	@RTARG0

@@LCODE	CSEG	BASE
@@curem:
	CMP0	A
	BZ	$?curemZero
	PUSH	DE
	PUSH	BC
	MOVW	BC, AX
	SHRW	AX, 8
	MOVW	DE, AX
	MOV	X, @RTARG0
	DIVHU
	MOV	A, E
	MOV	B, A
	MOVW	AX, BC
	POP	BC
	POP	DE
	RET
?curemZero:
	MOV	A, @RTARG0
	RET

	END
