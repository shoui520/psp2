#ifndef MARLIN_HCI_INTEGER_ARITHMETIC_H
#define MARLIN_HCI_INTEGER_ARITHMETIC_H

#include <marlin_hci/rsa_arithmetic.h>

/* integer.c */
int MarlinCreateIntegerWords(const MarlinBuffer *buffer, int count, unsigned int **words);
int MarlinCreateIntegerBuffer(int count, const unsigned int *words,
			     int size, MarlinBuffer **output);

/* integer_encode.c */
int MarlinEncodeIntegerWords(unsigned char *data, int size, const unsigned int *words, int count);

/* integer_size.c */
int MarlinIntegerByteLength(int count, const unsigned int *words);

/* integer_words.c */
int MarlinDecodeIntegerWords(unsigned int *words, int count, const unsigned char *data, int size);

#endif
