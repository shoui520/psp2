#ifndef MARLIN_HCI_BUFFER_H
#define MARLIN_HCI_BUFFER_H

/* Reviewed descriptive names: this library carries no debug source records. */
typedef struct MarlinBuffer {
	unsigned int size;
	unsigned char *data;
	unsigned char payload[];
} MarlinBuffer;

void *MarlinFillMemory(void *destination, int value,
		      unsigned int size);
void *MarlinAllocateMemory(void *allocator, unsigned int size);
void MarlinFreeMemory(void *allocator, void *memory);
void *MarlinCopyMemory(void *destination, const void *source, unsigned int size);
int MarlinCompareMemory(const void *first, const void *second, unsigned int size);
int MarlinCompareNullPointers(const void *first, const void *second, int *result);
MarlinBuffer *MarlinBufferInitialize(void *allocator, MarlinBuffer *buffer,
				   unsigned int size);
MarlinBuffer *MarlinBufferCreate(void *allocator, unsigned int size,
				const void *data);
MarlinBuffer *MarlinBufferCreateFilled(void *allocator, unsigned int size,
				      unsigned char value);
void MarlinBufferDestroy(void *allocator, MarlinBuffer *buffer);
int MarlinBufferEraseAndDestroy(MarlinBuffer *buffer);
MarlinBuffer *MarlinBufferConcatenate(void *allocator, const MarlinBuffer *first,
				     const MarlinBuffer *second);
int MarlinBufferCompare(void *allocator, const MarlinBuffer *first,
		       const MarlinBuffer *second);
int MarlinCompareDataRanges(void *allocator, unsigned int firstSize,
			    const void *first, unsigned int secondSize,
			    const void *second);

/* buffer.c */
MarlinBuffer *MarlinBufferInitializeExternal(void *allocator, MarlinBuffer *buffer,
					   unsigned int size, void *data);
MarlinBuffer *MarlinBufferCopyInto(void *allocator, MarlinBuffer *buffer,
				 unsigned int size, const void *source);
void MarlinBufferReleaseData(void *allocator, MarlinBuffer *buffer);

/* buffer_ops.c */
void MarlinEraseMemory(void *data, unsigned int size);
int MarlinBufferObfuscate(MarlinBuffer *buffer);

/* memory.c */
int MarlinMemoryInitialize(void);
void MarlinMemoryTerminate(void);
void *MarlinReallocateMemory(void *allocator, void *memory, unsigned int size);
char *MarlinDuplicateString(void *allocator, const char *string);
void *MarlinMoveMemory(void *destination, const void *source, unsigned int size);

#endif
