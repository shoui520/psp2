; The updater's vector table on USS-1001 (PCH-2000, the slim; boot cluster 1, 0x1000), its CALLT
; table (unused) and option bytes, written from the original. The slots are app/vectors_rl78.asm's:
; UART1 in the place of the other models' UART3 (the updater's uart.c), INTP9 in INTP8's
; (power.c), UART4's three. Each slot holds its handler's address in page 0.

	NAME	vectors_rl78

	PUBLIC	_vector_RST, _option_bytes, _security_id
	EXTRN	_cstart, _tm00Interrupt, _uart1SendInterrupt, _uart1ReceiveInterrupt
	EXTRN	_uart1ErrorInterrupt, _rtcInterrupt, _intp8Interrupt, _uart4SendInterrupt
	EXTRN	_uart4ReceiveInterrupt, _uart4ErrorInterrupt

@@VECT	CSEG	BASE
_vector_RST:
	DW	LOWW(_cstart)	; 00H
	DW	0			; 02H
	DW	0			; 04H
	DW	0			; 06H
	DW	0			; 08H
	DW	0			; 0AH
	DW	0			; 0CH
	DW	0			; 0EH
	DW	0			; 10H
	DW	0			; 12H
	DW	0			; 14H
	DW	0			; 16H
	DW	0			; 18H
	DW	0			; 1AH
	DW	0			; 1CH
	DW	0			; 1EH
	DW	LOWW(_tm00Interrupt)	; 20H INTTM00
	DW	0			; 22H
	DW	0			; 24H
	DW	LOWW(_uart1SendInterrupt)	; 26H INTST1
	DW	LOWW(_uart1ReceiveInterrupt)	; 28H INTSR1
	DW	LOWW(_uart1ErrorInterrupt)	; 2AH INTSRE1
	DW	0			; 2CH
	DW	0			; 2EH
	DW	0			; 30H
	DW	0			; 32H
	DW	0			; 34H
	DW	LOWW(_rtcInterrupt)	; 36H INTRTC
	DW	0			; 38H
	DW	0			; 3AH
	DW	0			; 3CH
	DW	0			; 3EH
	DW	0			; 40H
	DW	0			; 42H
	DW	0			; 44H
	DW	0			; 46H
	DW	0			; 48H
	DW	0			; 4AH
	DW	0			; 4CH
	DW	0			; 4EH
	DW	LOWW(_intp8Interrupt)	; 50H INTP9
	DW	0			; 52H
	DW	0			; 54H
	DW	0			; 56H
	DW	0			; 58H
	DW	0			; 5AH
	DW	0			; 5CH
	DW	0			; 5EH
	DW	0			; 60H
	DW	0			; 62H
	DW	0			; 64H
	DW	0			; 66H
	DW	0			; 68H
	DW	0			; 6AH
	DW	0			; 6CH
	DW	LOWW(_uart4SendInterrupt)	; 6EH INTST4
	DW	LOWW(_uart4ReceiveInterrupt)	; 70H INTSR4
	DW	LOWW(_uart4ErrorInterrupt)	; 72H INTSRE4
	DW	0			; 74H
	DW	0			; 76H
	DW	0			; 78H
	DW	0			; 7AH
	DW	0			; 7CH
	DW	0			; 7EH
	DS	40H			; the CALLT table, unused
_option_bytes:
	DB	7FH, 0FFH, 0AAH, 04H	; the watchdog, the LVD, the clock, on-chip debugging
_security_id:
	DB	0, 0, 0, 0, 0, 0, 0, 0, 0, 0	; the on-chip debug security ID

	END
