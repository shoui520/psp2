; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; @RTARG0..2 <<= A

	NAME	lulsh

	PUBLIC	@@lulsh
	EXTRN	@RTARG0, @RTARG2

@@RTCODE	CSEG	BASE
@@lulsh:
	CMP0	A
	BZ	$?lulshEnd
	PUSH	AX
	PUSH	BC
	MOVW	BC, @RTARG0
	MOV	@RTARG0, A
	MOVW	AX, @RTARG2
?lulsh:
	SHLW	BC, 1
	ROLWC	AX, 1
	DEC	@RTARG0
	BNZ	$?lulsh
	MOVW	@RTARG2, AX
	MOVW	AX, BC
	MOVW	@RTARG0, AX
	POP	BC
	POP	AX
?lulshEnd:
	RET

	END
