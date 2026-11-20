; A runtime helper of CA78K0R's library (cl0rlf.lib, module fcom4): a program links the
; modules it calls. Written from the original.
;
; a math function's extended result rounded to a float, its exceptions reported

	NAME	fcom4

	PUBLIC	@@frnd
	EXTRN	@@fmthrr, @RTARG4, @RTARG6

@@LCODEL	CSEG	BASE
@@frnd:
	POP	HL
	CMP	@RTARG6, #2
	BNC	$?fcom46
	MOVW	AX, DE
	ADDW	AX, AX
	CMP	A, #0FFH
	BZ	$?fcom43
	ADDW	AX, BC
	SKC
	BZ	$?fcom43
	MOVW	AX, DE
	CLR1	A.7
	CMP	@RTARG4, #080H
	BC	$?fcom42
	BNZ	$?fcom41
	XCH	A, C
	MOV1	CY, A.0
	XCH	A, C
	BNC	$?fcom42
?fcom41:
	INC	C
	BNZ	$?fcom42
	INC	B
	BNZ	$?fcom42
	INCW	DE
	INCW	AX
	CMPW	AX, #07F80H
	BNC	$?fcom45
?fcom42:
	CMPW	AX, #080H
	BC	$?fcom44
?fcom43:
	RET
?fcom44:
	MOV	@RTARG6, #1
	BR	$?fcom46
?fcom45:
	MOV	@RTARG6, #3
?fcom46:
	BR	!@@fmthrr

	END
