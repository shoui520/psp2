; A runtime helper of CA78K0R's library (cl0rlf.lib, module log): a program links the modules
; it calls. Written from the original.
;
; log(): @@flogx, the result rounded by @@frnd

	NAME	log

	PUBLIC	_log
	EXTRN	@@flogx, @@frnd, @RTARG6

@@LCODEL	CSEG	BASE
_log:
	PUSH	HL
	XCHW	AX, BC
	MOVW	DE, AX
	MOV	@RTARG6+1, #080H
	CALL	!!@@flogx
	MOV	@RTARG6+1, #012H
	BR	!!@@frnd

	END
