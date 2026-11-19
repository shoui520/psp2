; USS-1001's vector table (0x0000), its CALLT table (unused) and option bytes, written from the
; original. Sony's R5F1ZCRK has one more slot at 0x20 than the R5F100's device file (TM00), so
; the slots 0x20-0x2a serve the device file's next interrupt; the labels name what each slot
; serves on the slim. Each slot holds its handler's address in page 0.

	NAME	vectors_rl78

	PUBLIC	_vector_RST, _option_bytes, _security_id
	EXTRN	_cstart, _intp2Interrupt, _intp3Interrupt, _intp0Interrupt, _hostInterrupt
	EXTRN	_uart2SendInterrupt, _uart2ReceiveInterrupt, _uart2ErrorInterrupt, _csiInterrupt
	EXTRN	_csi10Interrupt, _uart1SendInterrupt, _uart1ReceiveInterrupt, _uart1ErrorInterrupt
	EXTRN	_iicaInterrupt, _tm03Interrupt, _adcInterrupt, _rtcInterrupt, _tm04Interrupt
	EXTRN	_sre0Interrupt, _intp9Interrupt, _intp1Interrupt, _tickInterrupt, _iica1Interrupt
	EXTRN	_uart4SendInterrupt, _uart4ReceiveInterrupt, _uart4ErrorInterrupt

@@VECT	CSEG	BASE
_vector_RST:
	DW	LOWW(_cstart)		; 00H
	DW	0			; 02H
	DW	0			; 04H
	DW	0			; 06H
	DW	0			; 08H INTP0
	DW	LOWW(_intp2Interrupt)	; 0AH INTP1
	DW	0			; 0CH INTP2
	DW	LOWW(_intp3Interrupt)	; 0EH INTP3
	DW	LOWW(_intp0Interrupt)	; 10H INTP4
	DW	LOWW(_hostInterrupt)	; 12H INTP5
	DW	LOWW(_uart2SendInterrupt)	; 14H INTST2
	DW	LOWW(_uart2ReceiveInterrupt)	; 16H INTSR2
	DW	LOWW(_uart2ErrorInterrupt)	; 18H INTSRE2
	DW	0			; 1AH
	DW	0			; 1CH
	DW	LOWW(_csiInterrupt)	; 1EH INTCSI00
	DW	0			; 20H INTTM00
	DW	LOWW(_csi10Interrupt)	; 22H INTCSI01
	DW	0			; 24H
	DW	LOWW(_uart1SendInterrupt)	; 26H INTST1
	DW	LOWW(_uart1ReceiveInterrupt)	; 28H INTSR1
	DW	LOWW(_uart1ErrorInterrupt)	; 2AH INTSRE1
	DW	LOWW(_iicaInterrupt)	; 2CH INTIICA0
	DW	0			; 2EH
	DW	0			; 30H
	DW	LOWW(_tm03Interrupt)	; 32H INTTM03
	DW	LOWW(_adcInterrupt)	; 34H INTAD
	DW	LOWW(_rtcInterrupt)	; 36H INTRTC
	DW	0			; 38H
	DW	0			; 3AH
	DW	0			; 3CH
	DW	0			; 3EH
	DW	0			; 40H
	DW	LOWW(_tm04Interrupt)	; 42H INTTM04
	DW	0			; 44H
	DW	0			; 46H
	DW	0			; 48H
	DW	0			; 4AH
	DW	0			; 4CH
	DW	LOWW(_sre0Interrupt)	; 4EH INTP8
	DW	LOWW(_intp9Interrupt)	; 50H INTP9
	DW	LOWW(_intp1Interrupt)	; 52H INTP10
	DW	0			; 54H
	DW	LOWW(_tickInterrupt)	; 56H INTTM10
	DW	0			; 58H
	DW	0			; 5AH
	DW	0			; 5CH
	DW	0			; 5EH
	DW	LOWW(_iica1Interrupt)	; 60H INTIICA1
	DW	0			; 62H
	DW	0			; 64H
	DW	0			; 66H
	DW	0			; 68H
	DW	0			; 6AH
	DW	0			; 6CH
	DW	LOWW(_uart4SendInterrupt)	; 6EH INTST4
	DW	LOWW(_uart4ReceiveInterrupt)	; 70H INTSR4
	DW	LOWW(_uart4ErrorInterrupt)	; 72H INTSRE4
	DS	0CH			; 74H-7FH
	DS	40H			; the CALLT table, unused
_option_bytes:
	DB	7FH, 0FFH, 0AAH, 04H	; the watchdog, the LVD, the clock, on-chip debugging
_security_id:
	DB	0, 0, 0, 0, 0, 0, 0, 0, 0, 0	; the on-chip debug security ID

	END
