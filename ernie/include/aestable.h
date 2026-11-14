/*
 * The AES rounds' tables since 0x0100060B (app/aestable.c), in boot cluster 0.
 */
#ifndef ERNIE_AESTABLE_H
#define ERNIE_AESTABLE_H

extern const unsigned char aesSboxMul2[256];
extern const unsigned char aesSboxMul3[256];
extern const unsigned char aesMul9[256];
extern const unsigned char aesMul11[256];
extern const unsigned char aesMul13[256];
extern const unsigned char aesMul14[256];

#endif
