/* The allocator's callable buffer interface, not its private pool allocation. */
#ifndef ROBIN_BUFFER_H
#define ROBIN_BUFFER_H

#include <stddef.h>
#include <stdint.h>

/* Pool control block created by osa_BlockPoolCreate(); layout is the ROM's. */
struct OsaBufferPool {
	uint32_t words[3];
};
typedef struct OsaBuffer OsaBuffer;

struct OsaBuffer {
	/* Supplied by the Mask ROM allocator; preserved without interpretation. */
	uint32_t allocationMetadata;
	uint16_t length;
	union {
		uint16_t ringStart;
		uint16_t transportTag;
	};
	uint16_t dataOffset;
	union {
		uint16_t ringEnd;
		uint16_t poolMetadata;
	};
	uint8_t packetClass;
	uint8_t flags;
	/* Supplied by the allocator; this interface does not interpret the tag. */
	uint8_t allocatorTag;
	uint8_t blockIndex;
	struct OsaBufferPool *pool;
	void *context;
	int (*release)(OsaBuffer *buffer);
	uint8_t data[];
};

_Static_assert(sizeof(OsaBuffer) == 28, "allocator buffer interface");
_Static_assert(offsetof(OsaBuffer, allocationMetadata) == 0, "allocation metadata");
_Static_assert(offsetof(OsaBuffer, length) == 4, "buffer length");
_Static_assert(offsetof(OsaBuffer, ringStart) == 6, "DMA start or transport tag");
_Static_assert(offsetof(OsaBuffer, dataOffset) == 8, "buffer data offset");
_Static_assert(offsetof(OsaBuffer, ringEnd) == 10, "DMA end or pool metadata");
_Static_assert(offsetof(OsaBuffer, packetClass) == 12, "packet class");
_Static_assert(offsetof(OsaBuffer, flags) == 13, "buffer flags");
_Static_assert(offsetof(OsaBuffer, allocatorTag) == 14, "allocator tag");
_Static_assert(offsetof(OsaBuffer, blockIndex) == 15, "pool block index");
_Static_assert(offsetof(OsaBuffer, pool) == 16, "owning pool");
_Static_assert(offsetof(OsaBuffer, context) == 20, "consumer context");
_Static_assert(offsetof(OsaBuffer, release) == 24, "release callback");
_Static_assert(offsetof(OsaBuffer, data) == 28, "variable buffer storage");

#endif /* ROBIN_BUFFER_H */
