/*
 * The code the firmware keeps in SQRAM (0xc0007200 - 0xc000faa8). robin.ld puts every object of
 * sqram/ there, and any function of another folder marked SQRAM_CODE.
 */
#ifndef ROBIN_SQRAM_H
#define ROBIN_SQRAM_H

#include <stdint.h>

#define SQRAM_CODE	__attribute__((section(".sqram1.ro")))

/* The byte helpers of the SQRAM security code (sqram/security.c). */
int sqramBytesEqual(const uint8_t *first, const uint8_t *second, int length);		/* 0xc0007648 */
uint8_t *sqramMemset(uint8_t *destination, uint8_t value, int length);			/* 0xc000766e */
uint8_t *sqramMemmove(uint8_t *destination, const uint8_t *source, int length);	/* 0xc000767a */

#endif /* ROBIN_SQRAM_H */
