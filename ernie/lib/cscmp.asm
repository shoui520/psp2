; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; A against @RTARG0, signed: CY set when A is less

	NAME	cscmp

	PUBLIC	@@cscmp
	EXTRN	@RTARG0

@@RTCODE	CSEG	BASE
@@cscmp:
	CMP	A, @RTARG0
	BZ	$?cscmpEnd
	XOR1	CY, A.7
	XOR1	CY, @RTARG0.7
	NOT1	CY
?cscmpEnd:
	RET

	END
