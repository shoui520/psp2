; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; @RTARG0..2 ^= AX..@RTARG4

	NAME	lubxor

	PUBLIC	@@lubxor
	EXTRN	@RTARG0, @RTARG2, @RTARG4

@@RTCODE	CSEG	BASE
@@lubxor:
	XOR	A, @RTARG2+1
	MOV	@RTARG2+1, A
	MOV	A, X
	XOR	A, @RTARG2
	MOV	@RTARG2, A
	MOVW	AX, @RTARG4
	XOR	A, @RTARG0+1
	XCH	A, X
	XOR	A, @RTARG0
	XCH	A, X
	MOVW	@RTARG0, AX
	RET

	END
