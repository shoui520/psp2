/* Package file headers; multibyte members retain big-endian order. */
#pragma once

#include <stddef.h>
#include <stdint.h>

struct NpDrmPackageDigest {
	uint8_t cmacHash[16];
	uint8_t signature[40];
	uint8_t sha1Tail[8];
};

struct NpDrmPackageMainHeader {
	uint32_t magic;
	uint16_t revision;
	uint16_t type;
	uint32_t metadataOffset;
	uint32_t metadataCount;
	uint32_t metadataSize;
	uint32_t itemCount;
	uint64_t totalSize;
	uint64_t dataOffset;
	uint64_t dataSize;
	char contentId[48];
	uint8_t digest[16];
	uint8_t initialVector[16];
	struct NpDrmPackageDigest headerDigest;
};

_Static_assert(sizeof(struct NpDrmPackageDigest) == 64,
	"package digest ABI");
_Static_assert(sizeof(struct NpDrmPackageMainHeader) == 192,
	"package main header ABI");
_Static_assert(offsetof(struct NpDrmPackageMainHeader, revision) == 4,
	"package revision ABI");
_Static_assert(offsetof(struct NpDrmPackageMainHeader, initialVector) == 112,
	"package initial vector ABI");
