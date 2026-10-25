; A C library function of CA78K0R's for the large model, one module of it: a program links
; the ones it calls. Written from the original.
;
; void *memset_n(void __near *s, int c, size_t n)

	NAME	memset_n

	PUBLIC	_memset_n

@@LCODE	CSEG	BASE
_memset_n:
	MOVW	DE, AX
	MOVW	AX, SP
	PUSH	HL
	MOVW	HL, AX
	MOV	A, [HL+4]		; c
	MOV	C, A
	MOVW	AX, [HL+6]		; n
	MOVW	HL, AX
	PUSH	DE
?setN:
	CLRW	AX
	CMPW	AX, HL
	BZ	$?setNEnd
	MOV	A, C
	MOV	[DE], A
	INCW	DE
	DECW	HL
	BR	$?setN
?setNEnd:
	POP	BC
	POP	HL
	RET

	END
