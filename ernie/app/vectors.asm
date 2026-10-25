; Application: boot cluster 0's vector table (0x0000), its CALLT table (unused) and option
; bytes. The slots' labels are the database's (IDA's device, whose names for the slots from
; 0x4c on differ from what Sony's handlers serve); each slot holds its handler's address in
; page 0. Written from the original.

	NAME	vectors

	PUBLIC	_vector_RST, _vector_INTP0, _vector_INTP1, _vector_INTP2, _vector_INTP3
	PUBLIC	_vector_INTP4, _vector_INTP5, _vector_INTST3, _vector_INTSR3, _vector_INTSRE3
	PUBLIC	_vector_INTDMA0, _vector_INTST0, _vector_INTSR0, _vector_INTSRE0, _vector_INTST1
	PUBLIC	_vector_INTSRE1, _vector_INTIICA, _vector_INTTM01, _vector_INTTM03
	PUBLIC	_vector_INTAD, _vector_INTRTC, _vector_INTST2, _vector_INTTM04, _vector_INTSR2
	PUBLIC	_vector_INTP7, _vector_INTP8, _vector_INTP10, _vector_INTSRE4, _vector_INTTM10
	PUBLIC	_vector_INTTM11, _vector_INTSRE2, _vector_INTST4, _vector_INTSR4, _option_bytes
	PUBLIC	_security_id
	EXTRN	_cstart, _intp0Interrupt, _intp1Interrupt, _intp2Interrupt, _intp3Interrupt
	EXTRN	_intp4Interrupt, _hostInterrupt, _uart3SendInterrupt, _uart3ReceiveInterrupt
	EXTRN	_uart3ErrorInterrupt, _dma0Interrupt, _csiInterrupt, _csi01Interrupt
	EXTRN	_sre0Interrupt, _csi10Interrupt, _uart4ErrorInterrupt, _iicaInterrupt
	EXTRN	_tm01Interrupt, _tm03Interrupt, _adcInterrupt, _rtcInterrupt
	EXTRN	_uart2SendInterrupt, _tm04Interrupt, _uart2ReceiveInterrupt, _intp7Interrupt
	EXTRN	_intp8Interrupt, _intp10Interrupt, _intp11Interrupt, _tickInterrupt
	EXTRN	_tm11Interrupt, _uart2ErrorInterrupt, _uart4SendInterrupt
	EXTRN	_uart4ReceiveInterrupt

@@VECT	CSEG	BASE
_vector_RST:
	DW	LOWW(_cstart)	; 00H
	DW	0			; 02H
	DW	0			; 04H
	DW	0			; 06H
_vector_INTP0:
	DW	LOWW(_intp0Interrupt)	; 08H
_vector_INTP1:
	DW	LOWW(_intp1Interrupt)	; 0AH
_vector_INTP2:
	DW	LOWW(_intp2Interrupt)	; 0CH
_vector_INTP3:
	DW	LOWW(_intp3Interrupt)	; 0EH
_vector_INTP4:
	DW	LOWW(_intp4Interrupt)	; 10H
_vector_INTP5:
	DW	LOWW(_hostInterrupt)	; 12H
_vector_INTST3:
	DW	LOWW(_uart3SendInterrupt)	; 14H
_vector_INTSR3:
	DW	LOWW(_uart3ReceiveInterrupt)	; 16H
_vector_INTSRE3:
	DW	LOWW(_uart3ErrorInterrupt)	; 18H
_vector_INTDMA0:
	DW	LOWW(_dma0Interrupt)	; 1AH
	DW	0			; 1CH
_vector_INTST0:
	DW	LOWW(_csiInterrupt)	; 1EH
_vector_INTSR0:
	DW	LOWW(_csi01Interrupt)	; 20H
_vector_INTSRE0:
	DW	LOWW(_sre0Interrupt)	; 22H
_vector_INTST1:
	DW	LOWW(_csi10Interrupt)	; 24H
	DW	0			; 26H
_vector_INTSRE1:
	DW	LOWW(_uart4ErrorInterrupt)	; 28H
_vector_INTIICA:
	DW	LOWW(_iicaInterrupt)	; 2AH
	DW	0			; 2CH
_vector_INTTM01:
	DW	LOWW(_tm01Interrupt)	; 2EH
	DW	0			; 30H
_vector_INTTM03:
	DW	LOWW(_tm03Interrupt)	; 32H
_vector_INTAD:
	DW	LOWW(_adcInterrupt)	; 34H
_vector_INTRTC:
	DW	LOWW(_rtcInterrupt)	; 36H
	DW	0			; 38H
	DW	0			; 3AH
_vector_INTST2:
	DW	LOWW(_uart2SendInterrupt)	; 3CH
	DW	0			; 3EH
	DW	0			; 40H
_vector_INTTM04:
	DW	LOWW(_tm04Interrupt)	; 42H
	DW	0			; 44H
	DW	0			; 46H
	DW	0			; 48H
_vector_INTSR2:
	DW	LOWW(_uart2ReceiveInterrupt)	; 4AH
_vector_INTP7:
	DW	LOWW(_intp7Interrupt)	; 4CH
_vector_INTP8:
	DW	LOWW(_intp8Interrupt)	; 4EH
	DW	0			; 50H
_vector_INTP10:
	DW	LOWW(_intp10Interrupt)	; 52H
_vector_INTSRE4:
	DW	LOWW(_intp11Interrupt)	; 54H
_vector_INTTM10:
	DW	LOWW(_tickInterrupt)	; 56H
_vector_INTTM11:
	DW	LOWW(_tm11Interrupt)	; 58H
	DW	0			; 5AH
_vector_INTSRE2:
	DW	LOWW(_uart2ErrorInterrupt)	; 5CH
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
