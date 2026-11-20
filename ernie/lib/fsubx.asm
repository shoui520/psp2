; A runtime helper of CA78K0R's library (cl0rlf.lib, module fsubx): a program links the
; modules it calls. Written from the original.
;
; the extended difference: the subtrahend negated, then @@faddx

	NAME	fsubx

	PUBLIC	@@fsubx
	EXTRN	@@faddx, @RTARG2

@@LCODEL	CSEG	BASE
@@fsubx:
	XOR	@RTARG2+1, #080H
	BR	!@@faddx

	END
