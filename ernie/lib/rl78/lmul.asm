; A runtime helper of CA78K0R's RL78 library (cl78l.lib), one module of it: the RL78 boards'
; (PCH-2000) programs link the ones they call. Written from the original.
;
; @RTARG0..3 * AX:@RTARG4 (AX the high word), 32 bits, through the RL78's MULHU

	NAME	lmul

	PUBLIC	@@lumul, @@lsmul	; the signed one the same
	EXTRN	@RTARG0, @RTARG2, @RTARG4

@@LCODE	CSEG	BASE
@@lsmul:
@@lumul:
	PUSH	DE
	PUSH	BC
	MOVW	BC, @RTARG0
	MULHU
	MOVW	DE, AX
	MOVW	BC, @RTARG2
	MOVW	AX, @RTARG4
	MULHU
	ADDW	AX, DE
	MOVW	DE, AX
	MOVW	BC, @RTARG0
	MOVW	AX, @RTARG4
	MULHU
	MOVW	@RTARG0, AX
	MOVW	AX, BC
	ADDW	AX, DE
	MOVW	@RTARG2, AX
	POP	BC
	POP	DE
	RET

	END
