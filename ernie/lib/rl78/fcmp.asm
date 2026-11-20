; A runtime helper of CA78K0R's library (cl0rlf.lib, its RL78 form, module fcmpr): a program
; links the modules it calls. Written from the original.
;
; the float comparison of AX:BC and @RTARG2:@RTARG0

	NAME	fcmpr

	PUBLIC	@@fcmp
	EXTRN	@@fmthrr, @RTARG0, @RTARG2, @RTARG4, @RTARG6

@@LCODE	CSEG	BASE
?fcmp:
	BNZ	$?fcmpr2
	ADDW	AX, @RTARG4
	CMPW	AX, #07F80H
	BNZ	$?fcmpr2
	SET1	@RTARG6.5
	BR	$?fcmpr3
?fcmpr1:
	BNZ	$?fcmpr2
	ADDW	AX, @RTARG0
	CMPW	AX, #07F80H
	BNZ	$?fcmpr2
	BF	@RTARG6.5, $?fcmpr4
	BT	@RTARG6.7, $?fcmpr5
?fcmpr2:
	MOVW	@RTARG6, #0405H
	CALL	!@@fmthrr
	BR	$?fcmpr7
@@fcmp:
	PUSH	DE
	MOVW	DE, AX
	MOVW	AX, @RTARG6
	PUSH	AX
	MOV	A, D
	XOR	A, @RTARG2+1
	AND	A, #080H
	MOV	@RTARG6, A
	MOVW	AX, @RTARG2
	CLR1	A.7
	XCHW	AX, DE
	CLR1	A.7
	CMPW	AX, #07F80H
	BNC	$?fcmp
?fcmpr3:
	XCHW	AX, DE
	CMPW	AX, #07F80H
	BNC	$?fcmpr1
	OR	A, @RTARG0+1
	OR	A, @RTARG0
	OR	A, @RTARG4+1
	OR	A, @RTARG4
	ADDW	AX, DE
	SKC
	BZ	$?fcmpr7
	MOVW	AX, @RTARG2
	CLR1	A.7
?fcmpr4:
	BF	@RTARG6.7, $?fcmpr6
?fcmpr5:
	MOV1	CY, @RTARG2+1.7
	CLRB	A
	INC	A
	BR	$?fcmpr7
?fcmpr6:
	CMPW	AX, DE
	MOVW	AX, @RTARG0
	SKNZ
	CMPW	AX, @RTARG4
	SKZ
	XOR1	CY, @RTARG2+1.7
?fcmpr7:
	POP	AX
	MOVW	@RTARG6, AX
	POP	DE
	RET

	END
