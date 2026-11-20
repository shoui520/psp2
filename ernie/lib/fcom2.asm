; A runtime helper of CA78K0R's library (cl0rlf.lib, module fcom2): a program links the
; modules it calls. Written from the original.
;
; a polynomial of A coefficients at ES:HL, by Horner's rule

	NAME	fcom2

	PUBLIC	@@fply
	EXTRN	@@faddx, @@flds2, @@fldsd, @@fldsk_f, @@fmulx, @@fxcd1, @RTARG0, @RTARG2
	EXTRN	@RTARG4

@@LCODEL	CSEG	BASE
@@fply:
	PUSH	HL
	PUSH	AX
	MOVW	AX, @RTARG2
	PUSH	AX
	MOVW	AX, @RTARG0
	PUSH	AX
	MOVW	AX, @RTARG4
	PUSH	AX
	PUSH	DE
	PUSH	BC
	BR	$?fcom22
?fcom21:
	CALL	!!@@fxcd1
	CALL	!!@@flds2
?fcom22:
	CALL	!@@fmulx
	MOVW	AX, [SP+12]
	MOVW	HL, AX
	CALL	!!@@fldsk_f
	MOVW	AX, HL
	MOVW	[SP+12], AX
	CALL	!@@fmulx
	CALL	!!@@fldsd
	CALL	!!@@fxcd1
	CALL	!@@faddx
	MOV	A, [SP+11]
	DEC	A
	MOV	[SP+11], A
	BNZ	$?fcom21
	ADDW	SP, #0EH
	RET

	END
