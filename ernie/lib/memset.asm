; A C library function of CA78K0R's for the large model, one module of it: a program links
; the ones it calls. Written from the original.
;
; void *memset(void *s, int c, size_t n)

	NAME	memset

	PUBLIC	_memset

@@LCODE	CSEG	BASE
_memset:
	MOVW	DE, AX
	MOVW	AX, SP
	PUSH	HL
	MOVW	HL, AX
	PUSH	BC
	MOV	A, C
	MOV	ES, A
	MOV	A, [HL+4]		; c
	MOV	C, A
	MOVW	AX, [HL+6]		; n
	MOVW	HL, AX
	PUSH	DE
?set:
	CLRW	AX
	CMPW	AX, HL
	BZ	$?setEnd
	MOV	A, C
	MOV	ES:[DE], A
	INCW	DE
	DECW	HL
	BR	$?set
?setEnd:
	POP	BC
	POP	DE
	POP	HL
	RET

	END
