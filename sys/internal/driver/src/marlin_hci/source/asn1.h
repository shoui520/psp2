#ifndef MARLIN_HCI_ASN1_H
#define MARLIN_HCI_ASN1_H

#include "buffer.h"

/* Complete original twenty-byte node, shared by its parser and consumers. */
typedef struct MarlinAsn1Node {
	unsigned char tagClass;
	unsigned char constructed;
	unsigned char tag;
	unsigned char flags;
	union {
		struct {
			unsigned int size;
			unsigned char *data;
		} primitive;
		struct {
			unsigned int count;
			struct MarlinAsn1Node **children;
		} sequence;
	} value;
	const unsigned char *encodedData;
	unsigned int encodedSize;
} MarlinAsn1Node;

int MarlinAsn1Parse(void *allocator, const unsigned char *data,
		    unsigned int size, const unsigned char **end,
		    MarlinAsn1Node **output);
int MarlinAsn1Destroy(void *allocator, MarlinAsn1Node *node);

int MarlinAsn1GetChildBuffer(MarlinAsn1Node *parent, unsigned int index,
			   int copy, MarlinBuffer *output);

#endif
