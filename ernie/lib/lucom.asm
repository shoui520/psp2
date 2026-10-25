; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; ~@RTARG0..2

	NAME	lucom

	PUBLIC	@@lucom
	EXTRN	@RTARG0, @RTARG2

@@RTCODE	CSEG	BASE
@@lucom:
	XOR	@RTARG0, #0FFH
	XOR	@RTARG0+1, #0FFH
	XOR	@RTARG2, #0FFH
	XOR	@RTARG2+1, #0FFH
	RET

	END
