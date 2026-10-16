; Application: boot cluster 0's vector table (0x0000) and option bytes.
;
; Skeleton. Only the reset vector is written. The interrupt vectors follow once the
; device (and so its vector names) is identified; the option bytes are the DEM-3000L's
; (0x0000+0xc0: 6e ff ff 04), the on-chip debug security ID is all zero.

	NAME	vectors

	CSEG	AT 0000H
	DW	_@cstart		; RESET

	CSEG	AT 00c0H
	DB	6EH, 0FFH, 0FFH, 04H	; option bytes
	DB	0, 0, 0, 0, 0, 0, 0, 0, 0, 0	; on-chip debug security ID

	EXTRN	_@cstart
	END
