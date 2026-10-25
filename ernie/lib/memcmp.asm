; A C library function of CA78K0R's for the large model, one module of it: a program links
; the ones it calls. Written from the original.
;
; int memcmp(const void *s1, const void *s2, size_t n)

	NAME	memcmp

	PUBLIC	_memcmp
	EXTRN	@RTARG0

@@LCODE	CSEG	BASE
_memcmp:
	MOVW	DE, AX
	MOVW	AX, SP
	PUSH	HL
	MOVW	HL, AX
	MOVW	AX, @RTARG0
	PUSH	AX
	MOVW	AX, [HL+8]		; n
	MOVW	@RTARG0, AX
	MOV	A, [HL+6]		; s2's segment
	MOV	B, A
	MOVW	AX, [HL+4]
	MOVW	HL, AX
	INCW	@RTARG0
?cmp:
	DECW	@RTARG0
	CLRW	AX
	CMPW	AX, @RTARG0
	BZ	$?cmpEqual
	MOV	A, C
	MOV	ES, A
	MOV	A, ES:[DE]
	MOV	X, A
	MOV	A, B
	MOV	ES, A
	MOV	A, ES:[HL]
	INCW	DE
	INCW	HL
	SUB	X, A
	BZ	$?cmp
	SUBC	A, A			; the difference, sign-extended by the borrow
	MOVW	BC, AX
	BR	$?cmpEnd
?cmpEqual:
	CLRW	BC
?cmpEnd:
	POP	AX
	MOVW	@RTARG0, AX
	POP	HL
	RET

	END
