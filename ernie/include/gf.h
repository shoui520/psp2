/*
 * Multiplication by a constant in GF(2^8), modulo x^8 + x^4 + x^3 + x + 1 (AES's field): the
 * assembly module at 0x2f0b3, not written yet. Each returns x times the constant.
 */
#ifndef ERNIE_GF_H
#define ERNIE_GF_H

extern unsigned char gfMul2(unsigned char x);
extern unsigned char gfMul3(unsigned char x);
extern unsigned char gfMul9(unsigned char x);
extern unsigned char gfMul11(unsigned char x);
extern unsigned char gfMul13(unsigned char x);
extern unsigned char gfMul14(unsigned char x);

#endif
