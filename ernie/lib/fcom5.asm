; A runtime helper of CA78K0R's library (cl0rlf.lib, module fcom5): a program links the
; modules it calls. Written from the original.
;
; an exception's report: _matherr called with its type and the routine's name

	NAME	fcom5

	PUBLIC	@@fmthrr
	EXTRN	@RTARG6, _matherr

@@LCODE	CSEG	BASE
@@fmthrr:
	MOV	A, CS
	MOV	X, A
	MOV	A, ES
	PUSH	AX
	PUSH	DE
	PUSH	BC
	MOVW	AX, @RTARG6
	SHRW	AX, 5
	ADDW	AX, #LOWW(?fnames)	; its name: the high byte of @RTARG6 is 8 times its index
	MOVW	BC, #HIGHW(?fnames)
	PUSH	BC
	PUSH	AX
	MOV	X, @RTARG6
	CLRB	A
	PUSH	AX
	MOVW	AX, SP
	CALL	!!_matherr
	POP	AX
	POP	AX
	POP	AX
	POP	BC
	POP	DE
	POP	AX
	MOV	ES, A
	MOV	A, X
	MOV	CS, A
	RET


; the routines' names, 8 bytes each (the last 6)
@@CNSTL	CSEG	PAGE64KP
?fnames:
	DB	'@@fmul', 0, 0
	DB	'@@fdiv', 0, 0
	DB	'@@fadd', 0, 0
	DB	'@@fsub', 0, 0
	DB	'@@fcmp', 0, 0
	DB	'acos', 0, 0, 0, 0
	DB	'asin', 0, 0, 0, 0
	DB	'atan', 0, 0, 0, 0
	DB	'atan2', 0, 0, 0
	DB	'cos', 0, 0, 0, 0, 0
	DB	'sin', 0, 0, 0, 0, 0
	DB	'tan', 0, 0, 0, 0, 0
	DB	'cosh', 0, 0, 0, 0
	DB	'sinh', 0, 0, 0, 0
	DB	'tanh', 0, 0, 0, 0
	DB	'exp', 0, 0, 0, 0, 0
	DB	'frexp', 0, 0, 0
	DB	'ldexp', 0, 0, 0
	DB	'log', 0, 0, 0, 0, 0
	DB	'log10', 0, 0, 0
	DB	'modf', 0, 0, 0, 0
	DB	'pow', 0, 0, 0, 0, 0
	DB	'sqrt', 0, 0, 0, 0
	DB	'ceil', 0, 0, 0, 0
	DB	'floor', 0, 0, 0
	DB	'fmod', 0, 0

	END
