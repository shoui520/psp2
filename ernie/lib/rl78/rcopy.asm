; A runtime helper of CA78K0R's RL78 library (cl78l.lib): _rcopy copies code that runs from
; RAM out of the flash. Its table (_rcopyTable: a count, then for each copy its size, its RAM
; address's low word, its flash address's low word, the RAM's and the flash's high bytes) is
; the one fsl.asm keeps, the library's RAM commands. AX: the copy to make (from 1), 0xffff all
; of them; BC returns 0 done, 0xffff no such copy. Written from the original.

	NAME	rcopy

	PUBLIC	__rcopy
	EXTRN	_rcopyTable

@@RCOPY	CSEG	BASE
__rcopy:
	PUSH	DE
	PUSH	HL
	MOVW	DE, AX
	MOV	A, ES
	PUSH	AX
	MOV	ES, #0
	MOVW	BC, ES:!_rcopyTable
	CLRW	AX
	CMPW	AX, BC
	BZ	$?none
	DECW	AX
	CMPW	AX, DE
	BZ	$?all
	MOVW	AX, BC
	CMPW	AX, DE
	BC	$?none
?all:
	MOV	A, #0
	MOVW	HL, #LOWW(_rcopyTable + 2)
	CLRB	X
?entry:
	INC	X
	PUSH	DE
	PUSH	BC
	PUSH	HL
	PUSH	AX
	PUSH	AX
	CLRW	AX
	DECW	AX
	CMPW	AX, DE
	POP	AX
	BZ	$?copy
	PUSH	AX
	CLRB	A
	CMPW	AX, DE
	POP	AX
	BNZ	$?next
?copy:
	MOV	ES, A
	MOVW	AX, ES:[HL]
	MOVW	BC, AX
	MOVW	AX, ES:[HL+2]
	MOVW	DE, AX
	MOVW	AX, ES:[HL+4]
	PUSH	AX
	MOVW	AX, ES:[HL+6]
	MOV	ES, A
	POP	HL
?byte:
	PUSH	AX
	MOV	A, ES:[HL]
	XCH	A, X
	MOV	ES, A
	XCH	A, X
	MOV	ES:[DE], A
	INCW	HL
	CLRW	AX
	CMPW	AX, HL
	POP	AX
	SKNZ
	INC	A
	MOV	ES, A
	PUSH	AX
	INCW	DE
	CLRW	AX
	CMPW	AX, DE
	POP	AX
	SKNZ
	INC	X
	PUSH	AX
	DECW	BC
	CLRW	AX
	CMPW	AX, BC
	POP	AX
	BNZ	$?byte
?next:
	POP	DE
	POP	AX
	ADDW	AX, #8
	MOVW	HL, AX
	SKNC
	INC	D
	POP	BC
	DECW	BC
	CLRW	AX
	CMPW	AX, BC
	MOVW	AX, DE
	POP	DE
	BZ	$?done
	PUSH	AX
	CLRW	AX
	DECW	AX
	CMPW	AX, DE
	POP	AX
	BZ	$?entry
	PUSH	AX
	CLRB	A
	CMPW	AX, DE
	POP	AX
	BNZ	$?entry
	BR	$?done
?none:
	CLRW	BC
	DECW	BC
?done:
	POP	AX
	MOV	ES, A
	POP	HL
	POP	DE
	RET

	END
