; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; @RTARG0..2 >>= A

	NAME	lursh

	PUBLIC	@@lursh
	EXTRN	@RTARG0, @RTARG2

@@RTCODE	CSEG	BASE
@@lursh:
	CMP0	A
	BZ	$?lurshEnd
	PUSH	AX
	PUSH	BC
	MOVW	BC, @RTARG2
	MOV	@RTARG2, A
	MOVW	AX, @RTARG0
?lursh:
	SHRW	AX, 1
	XCHW	AX, BC
	SHRW	AX, 1
	XCHW	AX, BC
	MOV1	A.7, CY
	DEC	@RTARG2
	BNZ	$?lursh
	MOVW	@RTARG0, AX
	MOVW	AX, BC
	MOVW	@RTARG2, AX
	POP	BC
	POP	AX
?lurshEnd:
	RET

	END
