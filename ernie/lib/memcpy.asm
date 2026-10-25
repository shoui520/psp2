; A C library function of CA78K0R's for the large model, one module of it: a program links
; the ones it calls. Written from the original.
;
; void *memcpy(void *s1, const void *s2, size_t n)

	NAME	memcpy

	PUBLIC	_memcpy
	EXTRN	@RTARG0

@@LCODE	CSEG	BASE
_memcpy:
	MOVW	DE, AX
	MOVW	AX, SP
	PUSH	HL
	MOVW	HL, AX
	MOVW	AX, @RTARG0
	PUSH	AX
	MOVW	AX, [HL+8]		; n
	MOVW	@RTARG0, AX
	PUSH	DE
	PUSH	BC
	MOV	A, [HL+6]		; s2's segment
	MOV	B, A
	MOVW	AX, [HL+4]
	MOVW	HL, AX
?cpy:
	CLRW	AX
	CMPW	AX, @RTARG0
	BZ	$?cpyEnd
	MOV	A, B
	MOV	ES, A
	MOV	A, ES:[HL]
	MOV	X, A
	MOV	A, C
	MOV	ES, A
	MOV	A, X
	MOV	ES:[DE], A
	INCW	HL
	INCW	DE
	DECW	@RTARG0
	BR	$?cpy
?cpyEnd:
	POP	DE
	POP	BC
	POP	AX
	MOVW	@RTARG0, AX
	POP	HL
	RET

	END
