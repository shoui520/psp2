/*
 * Multiplication by a constant in GF(2^8), modulo x^8 + x^4 + x^3 + x + 1 (AES's field), for
 * aes.c's MixColumns and its inverse. Each is assembly inside a C function: the argument is
 * read from its stack slot (volatile keeps it there), and ROL A,1 both doubles A and moves
 * the top bit into bit 0, so a carry is reduced by XOR 0x1a, not 0x1b.
 */
#include "gf.h"

unsigned char gfMul2(volatile unsigned char x)
{
#asm
	MOVW	AX, [HL]
	XCH	A, X
	ROL	A, 1
	BNC	$?GF2
	XOR	A, #01AH
?GF2:
	XCH	A, X
	MOVW	BC, AX
#endasm
}

/* 2x ^ x */
unsigned char gfMul3(volatile unsigned char x)
{
#asm
	MOVW	AX, [HL]
	MOV	A, X
	ROL	A, 1
	BNC	$?GF3
	XOR	A, #01AH
?GF3:
	XOR	A, X
	XCH	A, X
	MOVW	BC, AX
#endasm
}

/* 8x ^ x */
unsigned char gfMul9(volatile unsigned char x)
{
#asm
	MOVW	AX, [HL]
	MOV	A, X
	ROL	A, 1
	BNC	$?GF9A
	XOR	A, #01AH
?GF9A:
	ROL	A, 1
	BNC	$?GF9B
	XOR	A, #01AH
?GF9B:
	ROL	A, 1
	BNC	$?GF9C
	XOR	A, #01AH
?GF9C:
	XOR	A, X
	XCH	A, X
	MOVW	BC, AX
#endasm
}

/* 8x ^ 2x ^ x */
unsigned char gfMul11(volatile unsigned char x)
{
#asm
	MOVW	AX, [HL]
	MOV	A, X
	ROL	A, 1
	BNC	$?GF11A
	XOR	A, #01AH
?GF11A:
	ROL	A, 1
	BNC	$?GF11B
	XOR	A, #01AH
?GF11B:
	ROL	A, 1
	BNC	$?GF11C
	XOR	A, #01AH
?GF11C:
	MOVW	BC, AX
	XCH	A, X
	ROL	A, 1
	BNC	$?GF11D
	XOR	A, #01AH
?GF11D:
	XOR	A, C
	XOR	A, X
	XCH	A, X
	MOVW	BC, AX
#endasm
}

/* 8x ^ 4x ^ x */
unsigned char gfMul13(volatile unsigned char x)
{
#asm
	MOVW	AX, [HL]
	MOV	A, X
	ROL	A, 1
	BNC	$?GF13A
	XOR	A, #01AH
?GF13A:
	ROL	A, 1
	BNC	$?GF13B
	XOR	A, #01AH
?GF13B:
	ROL	A, 1
	BNC	$?GF13C
	XOR	A, #01AH
?GF13C:
	MOVW	BC, AX
	XCH	A, X
	ROL	A, 1
	BNC	$?GF13D
	XOR	A, #01AH
?GF13D:
	ROL	A, 1
	BNC	$?GF13E
	XOR	A, #01AH
?GF13E:
	XOR	A, C
	XOR	A, X
	XCH	A, X
	MOVW	BC, AX
#endasm
}

/* 8x ^ 4x ^ 2x */
unsigned char gfMul14(volatile unsigned char x)
{
#asm
	MOVW	AX, [HL]
	MOV	A, X
	ROL	A, 1
	BNC	$?GF14A
	XOR	A, #01AH
?GF14A:
	ROL	A, 1
	BNC	$?GF14B
	XOR	A, #01AH
?GF14B:
	ROL	A, 1
	BNC	$?GF14C
	XOR	A, #01AH
?GF14C:
	MOVW	BC, AX
	XCH	A, X
	ROL	A, 1
	BNC	$?GF14D
	XOR	A, #01AH
?GF14D:
	MOVW	DE, AX
	ROL	A, 1
	BNC	$?GF14E
	XOR	A, #01AH
?GF14E:
	XOR	A, D
	XOR	A, X
	XCH	A, X
	MOVW	BC, AX
#endasm
}
