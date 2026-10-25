; A module of the flash self-programming library (Renesas's, for the 78K0R; fsl.asm has
; the core): FLMD0's pull-up (FSL_Open, FSL_Close). A program links the modules it calls.
; Written from the original; the public names are the library's own.

	NAME	fslopen

	PUBLIC	_FSL_Open, _FSL_Close

@@FSLOPN	CSEG	BASE

; FLMD0's pull-up on, for the commands, and off
_FSL_Open:
	SET1	BECTL.7
	RET

_FSL_Close:
	CLR1	BECTL.7
	RET

	END
