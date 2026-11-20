; A runtime helper of CA78K0R's library (cl0rlf.lib, module fcom6): a program links the
; modules it calls. Written from the original.
;
; the library's _matherr: errno 2 for the exception types below 4, else 1, unless the
; routine is a runtime helper (its name starts with @)

	NAME	fcom6

	PUBLIC	_matherr
	EXTRN	_errno

@@LCODE	CSEG	BASE
_matherr:
	MOVW	DE, AX
	MOV	A, [DE]
	MOV	C, A
	MOV	A, [DE+4]
	MOV	ES, A
	MOVW	AX, [DE+2]
	MOVW	DE, AX
	MOV	A, ES:[DE]
	CMP	A, #040H
	BZ	$?fcom61
	MOV	A, C
	CMP	A, #4
	MOVW	AX, #2
	SKC
	MOVW	AX, #1
	MOVW	!_errno, AX
?fcom61:
	RET

	END
