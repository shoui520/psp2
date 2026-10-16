#ifndef UPDATE_MGR_PUP_H
#define UPDATE_MGR_PUP_H

#include <stdint.h>
#include <stddef.h>

/* Complete authenticated file header; uninterpreted descriptions stay serialized. */
typedef struct PupHeader {
	uint8_t m_magic[8];
	uint8_t formatDescription[8];
	uint64_t m_format_version;
	uint64_t m_segment_num;
	uint64_t m_file_offset;
	uint64_t m_file_size;
	uint8_t signingDescription[8];
	uint8_t m_attribute[4];
	uint32_t platform;
	uint8_t formatPolicy[4];
	uint32_t m_support_list;
	uint32_t m_base_version;
	uint32_t m_base_buildno;
	uint8_t authenticatedExtension[48];
} PupHeader;

typedef struct PupSegment {
	uint64_t segment_id;
	uint64_t offset;
	uint64_t size;
	uint64_t sign_algorithm;
} PupSegment;

/* Whole authentication records are passed to the secure verifier. */
typedef struct PupSegmentSignature {
	uint8_t authentication[64];
} PupSegmentSignature;

typedef struct PupHeaderDigest {
	uint8_t digest[32];
} PupHeaderDigest;

typedef struct PupCachedHeader {
	PupHeader header;
	void *allocation;
	PupSegment *segments;
	PupSegmentSignature *signatures;
	PupHeaderDigest *headerDigest;
} PupCachedHeader;

typedef struct PupVerifyThreadArgs {
	int fd;
	int need_header_verify;
	int load_sm;
	int resident_sm;
	uint64_t segment_index;
	uint64_t reserved;
} PupVerifyThreadArgs;

_Static_assert(sizeof(PupVerifyThreadArgs) == 32 &&
	       offsetof(PupVerifyThreadArgs, segment_index) == 16,
	       "PUP verification thread request layout");

_Static_assert(sizeof(PupHeader) == 128, "PUP file header size");
_Static_assert(offsetof(PupHeader, m_segment_num) == 24 &&
	       offsetof(PupHeader, m_attribute) == 56 &&
	       offsetof(PupHeader, m_base_version) == 72,
	       "PUP file header layout");
_Static_assert(sizeof(PupSegment) == 32, "PUP segment size");
_Static_assert(sizeof(PupSegmentSignature) == 64, "PUP segment signature size");
_Static_assert(sizeof(PupHeaderDigest) == 32, "PUP header digest size");
_Static_assert(sizeof(PupCachedHeader) == 144 &&
	       offsetof(PupCachedHeader, allocation) == 128 &&
	       offsetof(PupCachedHeader, segments) == 132 &&
	       offsetof(PupCachedHeader, signatures) == 136 &&
	       offsetof(PupCachedHeader, headerDigest) == 140,
	       "PUP cached header layout");

#endif
