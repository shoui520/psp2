; The updater's vector table (boot cluster 1, 0x1000, which the chip uses once the boot
; clusters are swapped), its CALLT table (unused) and option bytes. The slots' labels are the
; database's. Written from the original.

	NAME	vectors

	PUBLIC	_vector_RST, _vector_INTST3, _vector_INTSR3, _vector_INTSRE3, _vector_INTSRE1
	PUBLIC	_vector_INTTM00, _vector_INTRTC, _vector_INTST4, _vector_INTSR4
	PUBLIC	_option_bytes, _security_id
	EXTRN	_cstart, _uart3SendInterrupt, _uart3ReceiveInterrupt, _uart3ErrorInterrupt, _uart4ErrorInterrupt
	EXTRN	_tm00Interrupt, _rtcInterrupt, _uart4SendInterrupt, _uart4ReceiveInterrupt

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
_vector_INTST3:
	DW	LOWW(_uart3SendInterrupt)	; 14H
_vector_INTSR3:
	DW	LOWW(_uart3ReceiveInterrupt)	; 16H
_vector_INTSRE3:
	DW	LOWW(_uart3ErrorInterrupt)	; 18H
	DW	0			; 1AH
	DW	0			; 1CH
	DW	0			; 1EH
	DW	0			; 20H
	DW	0			; 22H
	DW	0			; 24H
	DW	0			; 26H
_vector_INTSRE1:
	DW	LOWW(_uart4ErrorInterrupt)	; 28H
	DW	0			; 2AH
_vector_INTTM00:
	DW	LOWW(_tm00Interrupt)	; 2CH
	DW	0			; 2EH
	DW	0			; 30H
	DW	0			; 32H
	DW	0			; 34H
_vector_INTRTC:
	DW	LOWW(_rtcInterrupt)	; 36H
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
	DW	0			; 50H
	DW	0			; 52H
	DW	0			; 54H
	DW	0			; 56H
	DW	0			; 58H
	DW	0			; 5AH
	DW	0			; 5CH
	DW	0			; 5EH
_vector_INTST4:
	DW	LOWW(_uart4SendInterrupt)	; 60H
_vector_INTSR4:
	DW	LOWW(_uart4ReceiveInterrupt)	; 62H
	DW	0			; 64H
	DW	0			; 66H
	DW	0			; 68H
	DW	0			; 6AH
	DW	0			; 6CH
	DW	0			; 6EH
	DW	0			; 70H
	DW	0			; 72H
	DW	0			; 74H
	DW	0			; 76H
	DW	0			; 78H
	DW	0			; 7AH
	DW	0			; 7CH
	DW	0			; 7EH
	DS	40H			; the CALLT table, unused
_option_bytes:
	DB	6EH, 0FFH, 0FFH, 04H	; the watchdog, the LVI, the clock, on-chip debugging
_security_id:
	DB	0, 0, 0, 0, 0, 0, 0, 0, 0, 0	; the on-chip debug security ID

	END
