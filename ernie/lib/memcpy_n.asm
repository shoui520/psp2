; A C library function of CA78K0R's for the large model, one module of it: a program links
; the ones it calls. Written from the original.
;
; void *memcpy_n(void __near *s1, const void __near *s2, size_t n)

	NAME	memcpy_n

	PUBLIC	_memcpy_n

@@LCODE	CSEG	BASE
_memcpy_n:
	MOVW	DE, AX
	MOVW	AX, SP
	PUSH	HL
	MOVW	HL, AX
	MOVW	AX, [HL+6]		; n
	MOVW	BC, AX
	PUSH	DE
	MOVW	AX, [HL+4]
	MOVW	HL, AX
?cpyN:
	CLRW	AX
	CMPW	AX, BC
	BZ	$?cpyNEnd
	MOV	A, [HL]
	MOV	[DE], A
	INCW	HL
	INCW	DE
	DECW	BC
	BR	$?cpyN
?cpyNEnd:
	POP	BC
	POP	HL
	RET

	END
