; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; @RTARG0 % A, 8 bits (the dividend for a divisor of 0)

	NAME	curem

	PUBLIC	@@curem
	EXTRN	@RTARG0

@@RTCODE	CSEG	BASE
@@curem:
	CMP0	A
	BZ	$?curemZero
	PUSH	BC
	MOV	B, @RTARG0
	MOV	@RTARG0, A
	MOV	C, #8
	CLRB	A
?curem:
	SHL	B, 1
	ROLC	A, 1
	CMP	A, @RTARG0
	SKC
	SUB	A, @RTARG0
	DEC	C
	BNZ	$?curem
	POP	BC
	RET
?curemZero:
	MOV	A, @RTARG0
	RET

	END
