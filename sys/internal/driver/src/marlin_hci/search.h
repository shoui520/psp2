#ifndef MARLIN_HCI_SEARCH_H
#define MARLIN_HCI_SEARCH_H

#include "buffer.h"

unsigned int MarlinStringLength(const char *string);
int MarlinFindBytes(const void *data, unsigned int size,
		    const void *pattern, unsigned int patternSize);
int MarlinBufferContains(const MarlinBuffer *buffer, const void *pattern,
			 unsigned int size, int *contains);
int MarlinBufferIsStarfishEncrypted(const MarlinBuffer *buffer, int *encrypted);
int MarlinBufferGetKeyFormat(const MarlinBuffer *buffer, unsigned int *format);

/* string.c */
signed char *MarlinStringFindCharacter(signed char *string, int character);

#endif
