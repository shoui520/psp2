; The C library's memory functions (CA78K0R's, for the large model; Sony linked them):
; memcpy, memmove, memcmp and memset on far pointers, memcpy_n and memset_n their near
; variants. Written from the original.
;
; The first argument comes in AX (with its segment in C for a far pointer), the others on
; the stack after the return address; a far source's segment is set in ES byte by byte.

	NAME	string

	PUBLIC	_memcpy, _memcpy_n, _memmove, _memcmp, _memset, _memset_n
	EXTRN	@SEGDE, @RTARG0

@@LCODE	CSEG	BASE

; void *memcpy(void *s1, const void *s2, size_t n)
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

; void *memcpy_n(void __near *s1, const void __near *s2, size_t n)
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

; void *memmove(void *s1, const void *s2, size_t n): backwards when s1 is above s2 in
; the same segment
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

; int memcmp(const void *s1, const void *s2, size_t n)
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

; void *memset(void *s, int c, size_t n)
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

; void *memset_n(void __near *s, int c, size_t n)
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
