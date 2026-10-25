; A C library function of CA78K0R's for the large model, one module of it: a program links
; the ones it calls. Written from the original.
;
; void *memmove(void *s1, const void *s2, size_t n): backwards when s1 is above s2 in
; the same segment

	NAME	memmove

	PUBLIC	_memmove
	EXTRN	@SEGDE

@@LCODE	CSEG	BASE
_memmove:
	MOVW	DE, AX
	MOVW	AX, SP
	PUSH	HL
	MOVW	HL, AX
	PUSH	DE
	PUSH	BC
	MOV	A, C
	MOV	@SEGDE, A		; s1's segment
	MOVW	AX, [HL+8]		; n
	MOVW	BC, AX
	MOV	A, [HL+6]
	MOV	@SEGDE+1, A		; s2's segment
	MOVW	AX, [HL+4]
	MOVW	HL, AX
	MOV	A, @SEGDE+1
	CMP	A, @SEGDE
	BNZ	$?forward
	MOVW	AX, DE
	CMPW	AX, HL
	SKZ
	BNC	$?backward
?forward:
	CLRW	AX
	CMPW	AX, BC
	BZ	$?moveEnd
	MOV	ES, @SEGDE+1
	MOV	A, ES:[HL]
	MOV	ES, @SEGDE
	MOV	ES:[DE], A
	INCW	HL
	INCW	DE
	DECW	BC
	BR	$?forward
?backward:
	XCHW	AX, DE
	ADDW	AX, BC
	DECW	AX
	XCHW	AX, DE
	XCHW	AX, HL
	ADDW	AX, BC
	DECW	AX
	XCHW	AX, HL
?backwardNext:
	CLRW	AX
	CMPW	AX, BC
	BZ	$?moveEnd
	MOV	ES, @SEGDE+1
	MOV	A, ES:[HL]
	MOV	ES, @SEGDE
	MOV	ES:[DE], A
	DECW	HL
	DECW	DE
	DECW	BC
	BR	$?backwardNext
?moveEnd:
	POP	DE
	POP	BC
	POP	HL
	RET

	END
