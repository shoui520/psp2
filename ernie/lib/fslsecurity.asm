; A module of the flash self-programming library (Renesas's, for the 78K0R; fsl.asm has
; the core): a security flag set. A program links the modules it calls. Written from the
; original; the public names are the library's own.

	NAME	fslsecurity

	PUBLIC	_fslSetSecurityFlag
	EXTRN	_fslModifySecurityFlags, _fslSetFlags

@@FSLCOD	CSEG	BASE

; the security flags with bit 0 of their set form cleared, written back (command 0x0a)
_fslSetSecurityFlag:
	PUSH	AX
	MOV	X, #0FEH
	CALL	$!_fslModifySecurityFlags
	BR	$!_fslSetFlags

	END
