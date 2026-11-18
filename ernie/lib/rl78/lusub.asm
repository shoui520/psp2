; A runtime helper of CA78K0R's RL78 library (cl78l.lib), one module of it: the RL78 boards'
; (PCH-2000) programs link the ones they call. Written from the original.
;
; @RTARG0..3 minus AX (high word) and @RTARG4 (low word), 32 bits

	NAME	lusub

	PUBLIC	@@lusub
	EXTRN	@RTARG0, @RTARG2, @RTARG4

@@LCODE	CSEG	BASE
@@lusub:
	PUSH	BC
	MOVW	BC, AX
	MOVW	AX, @RTARG0
	SUBW	AX, @RTARG4
	MOVW	@RTARG0, AX
	SUBC	A, A
	MOV	X, A
	ADDW	AX, @RTARG2
	SUBW	AX, BC
	MOVW	@RTARG2, AX
	POP	BC
	RET

	END
