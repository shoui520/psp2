; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; AX * @RTARG0, 16 bits

	NAME	iumul

	PUBLIC	@@iumul
	EXTRN	@RTARG0

@@RTCODE	CSEG	BASE
@@iumul:
	PUSH	BC
	PUSH	DE
	MOVW	BC, @RTARG0
	XCH	A, C
	MOVW	DE, AX
	XCH	A, B
	MULU	X
	XCHW	AX, BC
	MULU	X
	XCHW	AX, DE
	MULU	X
	ADD	A, C
	ADD	A, E
	POP	DE
	POP	BC
	RET

	END
