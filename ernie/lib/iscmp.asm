; A runtime helper of CA78K0R's library, one module of it: a program links the ones it
; calls. Written from the original.
;
; AX against @RTARG0, signed: CY set when AX is less

	NAME	iscmp

	PUBLIC	@@iscmp
	EXTRN	@RTARG0

@@RTCODE	CSEG	BASE
@@iscmp:
	CMPW	AX, @RTARG0
	BZ	$?iscmpEnd
	XOR1	CY, A.7
	XOR1	CY, @RTARG0+1.7
	NOT1	CY
?iscmpEnd:
	RET

	END
