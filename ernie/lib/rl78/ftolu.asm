; A runtime helper of CA78K0R's library (cl0rlf.lib, its RL78 form, module ftolur): a program
; links the modules it calls. Written from the original.
;
; a float to an unsigned long

	NAME	ftolur

	PUBLIC	@@ftolu
	EXTRN	@@ftolse, @RTARG0, @RTARG2

@@LCODE	CSEG	BASE
@@ftolu:
	PUSH	DE
	PUSH	BC
	PUSH	AX
	MOV	A, @RTARG2
	SET1	A.7
	MOV	B, A
	MOV	C, @RTARG0+1
	MOVW	AX, @RTARG2
	ADDW	AX, AX
	CLRB	X
	BC	$?ftolur2
	CMP	A, #09EH
	SKNC
	BR	!@@ftolse
	BNZ	$?ftolur2
	MOV	A, @RTARG0
	MOVW	@RTARG0, AX
	MOVW	AX, BC
	MOVW	@RTARG2, AX
?ftolur1:
	POP	AX
	POP	BC
	POP	DE
	RET
?ftolur2:
	CLRW	AX
	MOVW	@RTARG2, AX
	MOVW	@RTARG0, AX
	BR	$?ftolur1

	END
