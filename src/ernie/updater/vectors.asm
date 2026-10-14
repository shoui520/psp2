; Updater: boot cluster 1's vector table (0x1000) and option bytes.
;
; Skeleton. Only the reset vector is written. The interrupt vectors follow once the
; device (and so its vector names) is identified; the option bytes are the DEM-3000L's
; (0x1000+0xc0: 6e ff ff 04), the on-chip debug security ID is all zero.

	NAME	vectors

	CSEG	AT 1000H
	DW	_@cstart		; RESET

	CSEG	AT 10c0H
	DB	6EH, 0FFH, 0FFH, 04H	; option bytes
	DB	0, 0, 0, 0, 0, 0, 0, 0, 0, 0	; on-chip debug security ID

	EXTRN	_@cstart
	END
