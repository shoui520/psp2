; A runtime helper of CA78K0R's library (cl0rlf.lib, module fcom3): a program links the
; modules it calls. Written from the original.
;
; the extended form's loads, stores and exchanges between the registers, @RTARG0..5 and the
; stack

	NAME	fcom3

	PUBLIC	@@fldsd, @@fstrcp, @@fldsk_f, @@fldsk_n, @@flds1, @@fldd2, @@flds2, @@fxcd1
	PUBLIC	@@fst1d
	EXTRN	@RTARG0, @RTARG2, @RTARG4

@@LCODEL	CSEG	BASE
@@fldsd:
	MOVW	AX, DE
	MOVW	@RTARG2, AX
	MOVW	AX, BC
	MOVW	@RTARG0, AX
	MOV	A, @RTARG4
	MOV	@RTARG4+1, A
	RET
@@fstrcp:
	MOVW	@RTARG2, #03F80H
	MOVW	@RTARG0, #0
	CLRB	@RTARG4+1
	RET
@@fldsk_f:
	MOV	A, ES:[HL+1]
	MOV	@RTARG4+1, A
	MOVW	AX, ES:[HL+2]
	MOVW	@RTARG0, AX
	MOVW	AX, ES:[HL+4]
	MOVW	@RTARG2, AX
	MOVW	AX, HL
	ADDW	AX, #6
	MOVW	HL, AX
	RET
@@fldsk_n:
	MOV	A, [HL+1]
	MOV	@RTARG4+1, A
	MOVW	AX, [HL+2]
	MOVW	@RTARG0, AX
	MOVW	AX, [HL+4]
	MOVW	@RTARG2, AX
	MOVW	AX, HL
	ADDW	AX, #6
	MOVW	HL, AX
	RET
@@flds1:
	MOVW	AX, [SP+6]
	MOVW	@RTARG2, AX
	MOVW	AX, [SP+4]
	MOVW	@RTARG0, AX
	MOV	A, [SP+8]
	MOV	@RTARG4+1, A
	RET
@@fldd2:
	MOVW	AX, [SP+12]
	MOVW	DE, AX
	MOVW	AX, [SP+10]
	MOVW	BC, AX
	MOV	A, [SP+9]
	MOV	@RTARG4, A
	RET
@@flds2:
	MOVW	AX, [SP+12]
	MOVW	@RTARG2, AX
	MOVW	AX, [SP+10]
	MOVW	@RTARG0, AX
	MOV	A, [SP+9]
	MOV	@RTARG4+1, A
	RET
@@fxcd1:
	MOVW	AX, [SP+6]
	XCHW	AX, DE
	MOVW	[SP+6], AX
	MOVW	AX, [SP+4]
	XCHW	AX, BC
	MOVW	[SP+4], AX
	MOV	A, [SP+8]
	XCH	A, @RTARG4
	MOV	[SP+8], A
	RET
@@fst1d:
	MOVW	AX, DE
	MOVW	[SP+6], AX
	MOVW	AX, BC
	MOVW	[SP+4], AX
	MOV	A, @RTARG4
	MOV	[SP+8], A
	RET

	END
