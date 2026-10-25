; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; @RTARG0..2 += AX..@RTARG4

	NAME	luadd

	PUBLIC	@@luadd
	EXTRN	@RTARG0, @RTARG2, @RTARG4

@@RTCODE	CSEG	BASE
@@luadd:
	ADDW	AX, @RTARG2
	MOVW	@RTARG2, AX
	MOVW	AX, @RTARG4
	ADDW	AX, @RTARG0
	MOVW	@RTARG0, AX
	SKNC
	INCW	@RTARG2
	RET

	END
