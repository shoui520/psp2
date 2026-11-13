; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; the long AX:@RTARG4 (high, low) against @RTARG2:@RTARG0, signed: CY set when it is less

	NAME	lscmp

	PUBLIC	@@lscmp
	EXTRN	@RTARG0, @RTARG2, @RTARG4

@@RTCODE	CSEG	BASE
@@lscmp:
	CMPW	AX, @RTARG2
	BNZ	$?lscmpSign
	PUSH	AX
	MOVW	AX, @RTARG4
	CMPW	AX, @RTARG0
	POP	AX
	BZ	$?lscmpEnd
?lscmpSign:
	XOR1	CY, @RTARG2+1.7
	XOR1	CY, A.7
	NOT1	CY
?lscmpEnd:
	RET

	END
