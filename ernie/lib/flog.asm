; A runtime helper of CA78K0R's library (cl0rlf.lib, module flog): a program links the modules
; it calls. Written from the original.
;
; the natural logarithm in the extended form: the exponent split off, the mantissa's
; logarithm from a series in ((m - 1) / (m + 1))^2

	NAME	flog

	PUBLIC	@@flogx
	EXTRN	@@faddx, @@fdivx, @@flds1, @@fldsd, @@fmulx, @@fply, @@fstrcp, @@fsubx
	EXTRN	@@fxcd1, @@lstof, @RTARG0, @RTARG2, @RTARG4, @RTARG6

@@LCODEL	CSEG	BASE
?flogx:
	INC	A
	BNZ	$?flog2
	ADDW	AX, BC
	SKC
	BZ	$?flog3
?flog1:
	CLRB	@RTARG6
	RET
?flog2:
	DEC	A
	ADDW	AX, BC
	SKC
	BZ	$?flog4
?flog3:
	MOV	@RTARG6, #5
	MOVW	DE, #07FFFH
	RET
?flog4:
	MOV	@RTARG6, #3
	MOVW	DE, #0FF80H
	RET
@@flogx:
	MOVW	AX, DE
	ADDW	AX, AX
	BC	$?flogx
	CMP	A, #0FFH
	BZ	$?flog1
	CMP0	A
	BNZ	$?flog6
	ADDW	AX, BC
	SKC
	BZ	$?flog4
	MOVW	HL, #1
	MOV	A, E
?flog5:
	DECW	HL
	SHLW	BC, 1
	ROLC	A, 1
	BF	A.7, $?flog5
	MOV	E, A
	MOVW	AX, HL
	SKNC
?flog6:
	SHRW	AX, 8
	MOV	D, #03FH
	SUBW	AX, #07FH
	MOVW	HL, AX
	MOV	A, E
	SET1	A.7
	CMP	A, #0B5H
	BC	$?flog7
	CLR1	A.7
	INCW	HL
?flog7:
	MOV	E, A
	PUSH	HL
	CLRB	@RTARG4
	MOVW	AX, @RTARG4
	PUSH	AX
	PUSH	DE
	PUSH	BC
	CALL	!!@@fstrcp
	CALL	!!@@fsubx
	CALL	!!@@fxcd1
	CALL	!!@@fstrcp
	CALL	!@@faddx
	CALL	!!@@flds1
	ADDW	SP, #6
	CALL	!@@fdivx
	PUSH	DE
	PUSH	BC
	MOV	A, @RTARG4
	PUSH	AX
	CALL	!!@@fldsd
	CALL	!@@fmulx
	CALL	!!@@fldsd
	POP	AX
	MOV	@RTARG4, A
	POP	BC
	POP	AX
	CMP0	A
	SKZ
	ADDW	AX, #080H
	MOVW	DE, AX
	MOV	ES, #HIGHW(?flogTable)
	MOVW	HL, #LOWW(?flogTable)
	MOV	A, #5
	CALL	!!@@fply
	POP	AX
	MOVW	@RTARG0, AX
	SARW	AX, 15
	MOVW	@RTARG2, AX
	CALL	!@@lstof
	PUSH	DE
	PUSH	BC
	MOV	A, @RTARG4
	PUSH	AX
	MOVW	DE, #03F31H
	MOVW	BC, #07217H
	MOVW	@RTARG4, #0F7H
	CALL	!@@fmulx
	CALL	!!@@fldsd
	POP	AX
	MOV	@RTARG4, A
	POP	BC
	POP	DE
	BR	!@@faddx


; the series' coefficients 1/3, 1/5, 1/7, 1/9 and 1/11, in the extended form: a pad byte, the
; mantissa's extra byte, the float's low word, its high word
@@CNSTL	CSEG	PAGE64KP
?flogTable:
	DB	0, 0AAH
	DW	0AAAAH, 3EAAH
	DB	0, 99H
	DW	9999H, 3F19H
	DB	0, 0B6H
	DW	0DB6DH, 3F36H
	DB	0, 0C7H
	DW	1C71H, 3F47H
	DB	0, 17H
	DW	745DH, 3F51H

	END
