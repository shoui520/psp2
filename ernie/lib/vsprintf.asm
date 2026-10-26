; A C library function of CA78K0R's for the large model, one module of it: a program links
; the ones it calls. Written from the original.
;
; int vsprintf(char *s, const char *format, va_list ap)
;
; The conversions d i u o x X c s p n, the flags - + space # 0, a width and a precision (a
; number or *), and h or l. The format's characters are read through @SEGAX:[HL+2CH:2AH],
; the arguments through @SEGDE+1:[HL+12H], and s is written through @SEGBC:[HL+24H]. The
; frame (HL):
;	[HL+0]	the number's digits, the last first (B counts them)
;	[HL+0CH]	the characters written
;	[HL+0EH]	the prefix ('-', '+', ' ', '0' or 0x's x), [HL+0FH] its length
;	[HL+10H]	the width, [HL+14H] the precision (bit 15: none), [HL+16H] a string's length
;	[HL+18H]	the padding character
; @RTARG0 holds the conversion's character, @RTARG0+1 its flags: bit 0 -, 1 +, 2 space,
; 3 #, 4 0, 5 l; @RTARG4..6 the number.

	NAME	vsprintf

	PUBLIC	_vsprintf
	EXTRN	@RTARG0, @RTARG2, @RTARG4, @RTARG6, @SEGAX, @SEGBC, @SEGDE

@@LCODE	CSEG	BASE
_vsprintf:
	PUSH	AX
	MOV	A, C
	MOV	@SEGBC, A
	MOVW	AX, SP
	MOVW	DE, AX
	PUSH	HL
	MOVW	AX, @RTARG0
	PUSH	AX
	MOVW	AX, @RTARG2
	PUSH	AX
	MOVW	AX, @RTARG4
	PUSH	AX
	MOVW	AX, @RTARG6
	PUSH	AX
	MOVW	AX, SP
	SUBW	AX, #1AH
	MOVW	SP, AX
	MOVW	HL, AX
	MOVW	AX, #0AH
	ADDW	AX, DE
	MOVW	DE, AX
	MOVW	AX, [DE]
	MOVW	[HL+12H], AX
	MOV	A, [DE+2]
	MOV	@SEGDE+1, A
	CLRW	AX
	MOVW	[HL+0CH], AX
	MOV	A, [HL+2CH]
	MOV	@SEGAX, A
; the format, up to its end: characters copied, % a conversion
?next:
	CALL	$!vsGetc
	CMP0	@RTARG0
	SKNZ
	BR	$!?end
	CMP	@RTARG0, #25H
	BZ	$?spec
	MOV	A, @RTARG0
	CALL	$!vsPutc
	BR	$?next
; a conversion: no flags, no width, no precision
?spec:
	CLRW	AX
	MOVW	[HL+10H], AX
	MOV	@RTARG0+1, A
	DECW	AX
	MOVW	[HL+14H], AX
?flags:
	CALL	$!vsGetc
	CMP0	@RTARG0
	SKNZ
	BR	$!?end
	CMP	@RTARG0, #2DH
	BZ	$?flagMinus
	CMP	@RTARG0, #2BH
	BZ	$?flagPlus
	CMP	@RTARG0, #20H
	BZ	$?flagSpace
	CMP	@RTARG0, #23H
	BZ	$?flagHash
	CMP	@RTARG0, #30H
	BZ	$?flagZero
	BR	$?width
?flagMinus:
	SET1	@RTARG0+1.0
	BR	$?flags
?flagPlus:
	SET1	@RTARG0+1.1
	BR	$?flags
?flagSpace:
	SET1	@RTARG0+1.2
	BR	$?flags
?flagHash:
	SET1	@RTARG0+1.3
	BR	$?flags
?flagZero:
	SET1	@RTARG0+1.4
	BR	$?flags
; the width: * an int argument (negative: -), or digits
?width:
	CMP	@RTARG0, #2AH
	BNZ	$?widthDigits
	CALL	$!vsGetc
	CMP0	@RTARG0
	SKNZ
	BR	$!?end
	CALL	$!vsArgInt
	BF	A.7, $?widthSet
	SET1	@RTARG0+1.0
	CLRW	BC
	XCHW	AX, BC
	SUBW	AX, BC
	BR	$?widthSet
?widthDigits:
	CALL	$!vsNumber
?widthSet:
	MOVW	[HL+10H], AX
	CMP	@RTARG0, #2EH
	BNZ	$?size
	CALL	$!vsGetc
	CMP0	@RTARG0
	SKNZ
	BR	$!?end
	CLRW	AX
	MOVW	[HL+14H], AX
	CMP	@RTARG0, #2AH
	BNZ	$?precDigits
	CALL	$!vsGetc
	CMP0	@RTARG0
	SKNZ
	BR	$!?end
	CALL	$!vsArgInt
	MOVW	[HL+14H], AX
	BR	$?size
?precDigits:
	CALL	$!vsNumber
	MOVW	[HL+14H], AX
; h, or l (a long argument)
?size:
	CMP	@RTARG0, #68H
	BZ	$?sizeNext
	CMP	@RTARG0, #6CH
	BNZ	$?conv
	SET1	@RTARG0+1.5
?sizeNext:
	CALL	$!vsGetc
	CMP0	@RTARG0
	SKNZ
	BR	$!?end
; the conversion; the prefix none
?conv:
	MOV	[HL+0FH], #0
	CLRB	B
	MOV	A, @RTARG0
	CMP	A, #70H
	BZ	$?pointer
	CMP	A, #64H
	BZ	$?signed
	CMP	A, #69H
	BZ	$?signed
	CMP	A, #75H
	BZ	$?convUnsigned
	CMP	A, #6FH
	BZ	$?convUnsigned
	CMP	A, #78H
	BZ	$?convUnsigned
	CMP	A, #58H
	BZ	$?convUnsigned
	CMP	A, #63H
	BZ	$?convChar
	CMP	A, #73H
	BZ	$?convString
	CMP	A, #6EH
	BZ	$?convCount
	BR	$!?other
?convUnsigned:
	BR	$!?unsigned
?convChar:
	BR	$!?char
?convString:
	BR	$!?string
?convCount:
	BR	$!?count
; p: a far pointer, as 6 hex digits
?pointer:
	MOVW	AX, #6
	MOVW	[HL+14H], AX
	CALL	$!vsArgPtr
	MOVW	@RTARG4, AX
	MOV	A, ES
	MOV	@RTARG6, A
	CLRB	@RTARG6+1
	BR	$!?hexLoop
; d, i: an int or a long, its sign the prefix
?signed:
	CALL	$!vsArgInt
	MOVW	@RTARG4, AX
	BF	@RTARG0+1.5, $?signedInt
	CALL	$!vsArgInt
	MOVW	@RTARG6, AX
	BR	$?sign
?signedInt:
	BF	A.7, $?signedHigh
	MOVW	@RTARG6, #0FFFFH
	BR	$?sign
?signedHigh:
	CLRW	AX
	MOVW	@RTARG6, AX
?sign:
	BF	@RTARG6+1.7, $?signPlus
	CLRW	AX
	SUBW	AX, @RTARG4
	MOVW	@RTARG4, AX
	CLRW	AX
	SKNC
	DECW	AX
	SUBW	AX, @RTARG6
	MOVW	@RTARG6, AX
	MOV	[HL+0EH], #2DH
	BR	$?signSet
?signPlus:
	BF	@RTARG0+1.1, $?signSpace
	MOV	[HL+0EH], #2BH
	BR	$?signSet
?signSpace:
	BF	@RTARG0+1.2, $?decimal
	MOV	[HL+0EH], #20H
?signSet:
	MOV	[HL+0FH], #1
	BR	$?decimal
; u, o, x, X: an unsigned or an unsigned long
?unsigned:
	CALL	$!vsArgInt
	MOVW	@RTARG4, AX
	BF	@RTARG0+1.5, $?unsignedInt
	CALL	$!vsArgInt
	MOVW	@RTARG6, AX
	BR	$?base
?unsignedInt:
	CLRW	AX
	MOVW	@RTARG6, AX
?base:
	CMP	@RTARG0, #75H
	BZ	$?decimal
	CMP	@RTARG0, #6FH
	BZ	$?octal
	BR	$!?hex
; the decimal digits: @RTARG4..6 divided by 10 bit by bit, the remainder the digit
?decimal:
	CLRW	AX
	CMPW	AX, @RTARG4
	BNZ	$?decimalDigit
	CMPW	AX, @RTARG6
	SKNZ
	BR	$!?field
?decimalDigit:
	PUSH	HL
	PUSH	DE
	PUSH	BC
	MOVW	DE, @RTARG4
	MOVW	HL, @RTARG6
	CLRB	A
	MOV	C, #20H
?divide:
	XCHW	AX, DE
	ADDW	AX, AX
	XCHW	AX, DE
	XCHW	AX, HL
	ROLWC	AX, 1
	XCHW	AX, HL
	ROLC	A, 1
	CMP	A, #0AH
	BC	$?divideNext
	SUB	A, #0AH
	INC	E
?divideNext:
	DEC	C
	BNZ	$?divide
	ADD	A, #30H
	MOV	C, A
	MOVW	AX, DE
	MOVW	@RTARG4, AX
	MOVW	AX, HL
	MOVW	@RTARG6, AX
	MOV	A, C
	POP	BC
	POP	DE
	POP	HL
	MOV	[HL+B], A
	INC	B
	BR	$?decimal
; the octal digits; # makes the first a 0
?octal:
	CLRW	AX
	CMPW	AX, @RTARG4
	BNZ	$?octalDigit
	CMPW	AX, @RTARG6
	BZ	$?octalDone
?octalDigit:
	MOV	A, @RTARG4
	AND	A, #7
	PUSH	AX
	MOVW	AX, @RTARG4
	SHRW	AX, 3
	MOVW	@RTARG4, AX
	MOV	A, @RTARG6
	SHL	A, 5
	OR	A, @RTARG4+1
	MOV	@RTARG4+1, A
	MOVW	AX, @RTARG6
	SHRW	AX, 3
	MOVW	@RTARG6, AX
	POP	AX
	ADD	A, #30H
	MOV	[HL+B], A
	INC	B
	BR	$?octal
?octalDone:
	BF	@RTARG0+1.3, $?field
	PUSH	AX
	MOV	A, [HL+15H]
	ROLC	A, 1
	POP	AX
	BC	$?octalZero
	PUSH	AX
	MOV	A, [HL+15H]
	CMP0	A
	POP	AX
	BNZ	$?field
	MOV	A, [HL+14H]
	CMP	B, A
	BC	$?field
?octalZero:
	MOV	[HL+15H], #0
	MOV	A, B
	INC	A
	MOV	[HL+14H], A
	BR	$?field
; the hex digits; # prefixes 0x or 0X
?hex:
	BF	@RTARG0+1.3, $?hexLoop
	MOV	[HL+0FH], #2
	MOV	A, @RTARG0
	MOV	[HL+0EH], A
?hexLoop:
	CLRW	AX
	CMPW	AX, @RTARG4
	BNZ	$?hexDigit
	CMPW	AX, @RTARG6
	BZ	$?hexDone
?hexDigit:
	MOV	A, @RTARG4
	AND	A, #0FH
	PUSH	AX
	MOVW	AX, @RTARG4
	SHRW	AX, 4
	MOVW	@RTARG4, AX
	MOV	A, @RTARG6
	SHL	A, 4
	OR	A, @RTARG4+1
	MOV	@RTARG4+1, A
	MOVW	AX, @RTARG6
	SHRW	AX, 4
	MOVW	@RTARG6, AX
	POP	AX
	CMP	A, #0AH
	BNC	$?hexLetter
	ADD	A, #30H
	BR	$?hexStore
?hexLetter:
	CMP	@RTARG0, #78H
	BNZ	$?hexUpper
	ADD	A, #57H
	BR	$?hexStore
?hexUpper:
	ADD	A, #37H
?hexStore:
	MOV	[HL+B], A
	INC	B
	BR	$?hexLoop
?hexDone:
	CMP0	B
	BNZ	$?field
	MOV	[HL+0FH], #0
; the field: the precision's zeros, the prefix, padded to the width
?field:
	PUSH	AX
	MOV	A, [HL+15H]
	ROLC	A, 1
	POP	AX
	BNC	$?fieldPrec
	ONEW	AX
	MOVW	[HL+14H], AX
?fieldPrec:
	PUSH	AX
	MOV	A, [HL+15H]
	CMP0	A
	POP	AX
	BZ	$?fieldWiden
	PUSH	AX
	MOV	A, [HL+15H]
	ROLC	A, 1
	POP	AX
	BNC	$?fieldZero
?fieldWiden:
	MOV	A, [HL+14H]
	CMP	A, B
	BNC	$?fieldZero
	MOV	[HL+15H], #0
	MOV	A, B
	MOV	[HL+14H], A
?fieldZero:
	MOVW	AX, [HL+14H]
	OR	A, X
	BZ	$?fieldWidth
	CMP0	B
	BNZ	$?fieldWidth
	MOV	[HL+0], #30H
	INC	B
?fieldWidth:
	MOV	A, [HL+10H]
	SUB	A, [HL+0FH]
	MOV	X, A
	MOV	A, [HL+11H]
	SUBC	A, #0
	XCH	A, X
	SUB	A, [HL+14H]
	XCH	A, X
	SUBC	A, [HL+15H]
	MOVW	[HL+10H], AX
	BF	@RTARG0+1.0, $?fieldRight
	CALL	$!vsPrefix
	CALL	$!vsDigits
	MOV	[HL+18H], #20H
	MOVW	AX, [HL+10H]
	MOVW	@RTARG2, AX
	CALL	$!vsPad
	BR	$!?next
?fieldRight:
	MOVW	AX, [HL+10H]
	MOVW	@RTARG2, AX
	BF	@RTARG0+1.4, $?fieldSpaces
	CALL	$!vsPrefix
	MOV	[HL+18H], #30H
	CALL	$!vsPad
	BR	$?fieldDigits
?fieldSpaces:
	MOV	[HL+18H], #20H
	CALL	$!vsPad
	CALL	$!vsPrefix
?fieldDigits:
	CALL	$!vsDigits
	BR	$!?next
; c: an int argument's character
?char:
	PUSH	DE
	PUSH	AX
	MOVW	AX, [HL+12H]
	MOVW	DE, AX
	MOV	ES, @SEGDE+1
	MOV	A, ES:[DE]
	INCW	DE
	MOV	C, A
	INCW	DE
	MOVW	AX, DE
	MOVW	[HL+12H], AX
	POP	AX
	POP	DE
	BR	$!?charOut
; s: a far string, up to the precision's characters
?string:
	CALL	$!vsArgPtr
	MOVW	DE, AX
	PUSH	AX
	MOV	A, [HL+15H]
	ROLC	A, 1
	POP	AX
	BNC	$?stringAlign
	PUSH	AX
	MOVW	AX, #7FFFH
	MOVW	[HL+14H], AX
	POP	AX
?stringAlign:
	BF	@RTARG0+1.0, $?stringRight
?stringLeft:
	MOV	A, [HL+14H]
	OR	A, [HL+15H]
	BZ	$?stringLeftPad
	MOV	ES, @SEGDE
	MOV	A, ES:[DE]
	INCW	DE
	CMP0	A
	BZ	$?stringLeftPad
	CALL	$!vsPutc
	DECW	[HL+14H]
	DECW	[HL+10H]
	BR	$?stringLeft
?stringLeftPad:
	MOV	[HL+18H], #20H
	MOVW	AX, [HL+10H]
	MOVW	@RTARG2, AX
	CALL	$!vsPad
	BR	$!?next
?stringRight:
	CLRW	AX
	MOVW	[HL+16H], AX
?stringLength:
	PUSH	DE
	MOVW	AX, [HL+16H]
	ADDW	AX, DE
	MOVW	DE, AX
	MOV	ES, @SEGDE
	MOV	A, ES:[DE]
	MOV	[HL+18H], A
	POP	DE
	CMP0	A
	BZ	$?stringLimit
	INCW	[HL+16H]
	BR	$?stringLength
?stringLimit:
	MOVW	AX, [HL+14H]
	CMPW	AX, [HL+16H]
	SKNC
	MOVW	[HL+16H], AX
	BF	@RTARG0+1.4, $?stringSpaces
	MOV	[HL+18H], #30H
	BR	$?stringPad
?stringSpaces:
	MOV	[HL+18H], #20H
?stringPad:
	MOVW	AX, [HL+10H]
	SUBW	AX, [HL+16H]
	MOVW	@RTARG2, AX
	CALL	$!vsPad
?stringCopy:
	MOV	A, [HL+16H]
	OR	A, [HL+17H]
	SKNZ
	BR	$!?next
	MOV	ES, @SEGDE
	MOV	A, ES:[DE]
	INCW	DE
	CALL	$!vsPutc
	DECW	[HL+16H]
	BR	$?stringCopy
; n: the characters written so far, to an int (l: a long)
?count:
	CALL	$!vsArgPtr
	MOVW	DE, AX
	MOVW	AX, [HL+0CH]
	MOV	ES:[DE+1], A
	MOV	A, X
	MOV	ES:[DE], A
	BF	@RTARG0+1.5, $?countDone
	CLRW	AX
	MOV	ES:[DE+3], A
	MOV	ES:[DE+2], A
?countDone:
	BR	$!?next
; any other character, as it is
?other:
	MOV	C, @RTARG0
?charOut:
	MOV	[HL+18H], #20H
	BF	@RTARG0+1.0, $?charRight
	MOV	A, C
	CALL	$!vsPutc
	BR	$?charPad
?charRight:
	BF	@RTARG0+1.4, $?charPad
	MOV	[HL+18H], #30H
?charPad:
	MOVW	AX, [HL+10H]
	DECW	AX
	MOVW	@RTARG2, AX
	CALL	$!vsPad
	BT	@RTARG0+1.0, $?charDone
	MOV	A, C
	CALL	$!vsPutc
?charDone:
	BR	$!?next
; the end: s terminated, the characters written returned
?end:
	PUSH	DE
	MOVW	AX, [HL+24H]
	MOVW	DE, AX
	MOV	ES, @SEGBC
	MOV	ES:[DE+0], #0
	POP	DE
	MOVW	AX, [HL+0CH]
	MOVW	BC, AX
	ADDW	SP, #1AH
	POP	AX
	MOVW	@RTARG6, AX
	POP	AX
	MOVW	@RTARG4, AX
	POP	AX
	MOVW	@RTARG2, AX
	POP	AX
	MOVW	@RTARG0, AX
	POP	HL
	POP	AX
	RET
; the format's next character to @RTARG0, the format pointer advanced
vsGetc:
	PUSH	DE
	PUSH	AX
	MOVW	AX, [HL+2AH]
	MOVW	DE, AX
	MOV	ES, @SEGAX
	MOV	A, ES:[DE]
	MOV	@RTARG0, A
	INCW	[HL+2AH]
	POP	AX
	POP	DE
	RET
; A to s, the count advanced
vsPutc:
	PUSH	DE
	PUSH	AX
	MOVW	AX, [HL+24H]
	MOVW	DE, AX
	INCW	[HL+24H]
	MOV	ES, @SEGBC
	POP	AX
	MOV	ES:[DE], A
	INCW	[HL+0CH]
	POP	DE
	RET
; the next int argument to AX
vsArgInt:
	PUSH	DE
	MOVW	AX, [HL+12H]
	MOVW	DE, AX
	INCW	AX
	INCW	AX
	MOVW	[HL+12H], AX
	MOV	ES, @SEGDE+1
	MOVW	AX, ES:[DE]
	POP	DE
	RET
; the next far pointer argument to AX and ES (@SEGDE)
vsArgPtr:
	PUSH	DE
	MOVW	AX, [HL+12H]
	MOVW	DE, AX
	ADDW	AX, #4
	MOVW	[HL+12H], AX
	MOV	ES, @SEGDE+1
	MOVW	AX, ES:[DE]
	PUSH	AX
	MOV	A, ES:[DE+2]
	MOV	ES, A
	MOV	@SEGDE, A
	POP	AX
	POP	DE
	RET
; the prefix: 0x's 0 and x, or the sign
vsPrefix:
	PUSH	AX
	MOV	A, #2
	CMP	A, [HL+0FH]
	BNZ	$?prefixOne
	MOV	A, #30H
	CALL	$!vsPutc
	BR	$?prefixChar
?prefixOne:
	ONEB	A
	CMP	A, [HL+0FH]
	BNZ	$?prefixEnd
?prefixChar:
	MOV	A, [HL+0EH]
	CALL	$!vsPutc
?prefixEnd:
	POP	AX
	RET
; the precision's zeros, then the digits, the last one first
vsDigits:
	PUSH	AX
	MOVW	AX, [HL+14H]
	XCH	A, X
	SUB	A, B
	XCH	A, X
	SUBC	A, #0
	MOVW	@RTARG2, AX
	MOV	[HL+18H], #30H
	CALL	$!vsPad
?digitsLoop:
	CMP0	B
	BZ	$?digitsEnd
	DEC	B
	MOV	A, [HL+B]
	CALL	$!vsPutc
	BR	$?digitsLoop
?digitsEnd:
	POP	AX
	RET
; @RTARG2 padding characters, none when it is negative
vsPad:
	PUSH	AX
?padLoop:
	BT	@RTARG2+1.7, $?padEnd
	CLRW	AX
	CMPW	AX, @RTARG2
	BZ	$?padEnd
	MOV	A, [HL+18H]
	CALL	$!vsPutc
	DECW	@RTARG2
	BR	$?padLoop
?padEnd:
	POP	AX
	RET
; a decimal number from the format to AX; @RTARG0 the first other character
vsNumber:
	CLRW	AX
?numberLoop:
	CMP	@RTARG0, #30H
	BC	$?numberEnd
	CMP	@RTARG0, #3AH
	BNC	$?numberEnd
	SUB	@RTARG0, #30H
	PUSH	BC
	MOVW	BC, AX
	SHLW	BC, 3
	ADDW	AX, AX
	ADDW	AX, BC
	XCH	A, X
	ADD	A, @RTARG0
	XCH	A, X
	ADDC	A, #0
	POP	BC
	CALL	$!vsGetc
	BR	$?numberLoop
?numberEnd:
	RET

	END
