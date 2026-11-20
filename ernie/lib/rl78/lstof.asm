; A runtime helper of CA78K0R's library (cl0rlf.lib, its RL78 form, module lstofr): a program
; links the modules it calls. Written from the original.
;
; a signed long to a float

	NAME	lstofr

	PUBLIC	@@lstof
	EXTRN	@@lutofe, @RTARG0, @RTARG2

@@LCODE	CSEG	BASE
@@lstof:
	PUSH	DE
	PUSH	BC
	PUSH	AX
	MOVW	AX, @RTARG2
	MOVW	BC, @RTARG0
	BF	A.7, $?lstofr1
	MOVW	DE, AX
	CLRW	AX
	SUBW	AX, BC
	MOVW	BC, AX
	CLRW	AX
	SKNC
	DECW	AX
	SUBW	AX, DE
?lstofr1:
	BR	!@@lutofe

	END
