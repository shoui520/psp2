#ifndef MARLIN_HCI_STORAGE_H
#define MARLIN_HCI_STORAGE_H

#include "buffer.h"

/* Parsers normalize the file's big-endian sizes into these fields. */
typedef struct MarlinStoredKeyPairHeader {
	unsigned char formatTag;
	unsigned char reservedPrefix[3];
	unsigned short leadingDataSize;
	unsigned short firstKeySize;
	unsigned short secondKeySize;
	unsigned short reservedSuffix;
} MarlinStoredKeyPairHeader;

typedef struct MarlinStoredBasicPair {
	unsigned short firstOffset;
	unsigned short firstSize;
	unsigned short secondSize;
	unsigned short reserved;
} MarlinStoredBasicPair;

typedef struct MarlinStoredExtendedPair {
	unsigned short firstOffset;
	unsigned short firstSize;
	unsigned short gapSize;
	unsigned short secondSize;
	unsigned short trailingDataSizes[3];
	unsigned short reserved;
} MarlinStoredExtendedPair;

typedef struct MarlinStoredInfoPair {
	unsigned short firstOffset;
	unsigned short firstSize;
	unsigned short intermediateDataSizes[2];
	unsigned short secondSize;
	unsigned short reserved;
} MarlinStoredInfoPair;

typedef struct MarlinStoredSectionsHeader {
	MarlinStoredKeyPairHeader keyPair;
	MarlinStoredBasicPair basicPair;
	MarlinStoredExtendedPair extendedPair;
	MarlinStoredInfoPair infoPair;
} MarlinStoredSectionsHeader;

unsigned int MarlinLoadStoredData(int kind, MarlinBuffer **data);
int MarlinUpdateStoredData(int selector, unsigned int size, const void *data);
int MarlinCheckStoredData(int selector);
int MarlinWriteStoredData(int selector, const MarlinBuffer *data);
int MarlinExtractStoredSections(const MarlinBuffer *data, int selector,
			       MarlinBuffer **first, MarlinBuffer **second);
int MarlinGetStoredSections(int selector, MarlinBuffer **first,
			    MarlinBuffer **second);
int MarlinExtractStoredKeyPair(const MarlinBuffer *data, MarlinBuffer **first,
			      MarlinBuffer **second);

#endif
