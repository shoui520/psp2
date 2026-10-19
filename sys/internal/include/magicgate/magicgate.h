/* File-group records shared by MagicGate and its video consumer. */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

typedef struct MagicGateDeviceIdentity {
	uint8_t prefix[8];
	uint8_t path[8];
} MagicGateDeviceIdentity;

_Static_assert(sizeof(MagicGateDeviceIdentity) == 16, "device identity size");
_Static_assert(offsetof(MagicGateDeviceIdentity, prefix) == 0,
	       "device identity prefix offset");
_Static_assert(offsetof(MagicGateDeviceIdentity, path) == 8,
	       "device identity path offset");

/* Wire-field roles are inferred from the record readers, not Sony type names. */
typedef struct MagicGateRecordIdentity {
	uint32_t identifier;
	uint8_t format;
	uint8_t authenticationMode;
	uint8_t keyMode;
	uint8_t extension;
} MagicGateRecordIdentity;

typedef struct MagicGateRecordHeader {
	MagicGateRecordIdentity identity;
	uint8_t encodedHeaderMetadata[8];
	uint8_t tag[16];
	uint8_t treeSize[4];
	uint8_t ciphertextSize[4];
	uint8_t trailerSize[4];
} MagicGateRecordHeader;

typedef struct MagicGateFileInfo {
	uint8_t format;
	uint8_t size;
	uint8_t group;
	uint8_t identifier[13];
} MagicGateFileInfo;

_Static_assert(sizeof(MagicGateRecordIdentity) == 8, "record identity size");
_Static_assert(offsetof(MagicGateRecordIdentity, identifier) == 0,
	       "record identifier offset");
_Static_assert(offsetof(MagicGateRecordIdentity, format) == 4,
	       "record format offset");
_Static_assert(offsetof(MagicGateRecordIdentity, authenticationMode) == 5,
	       "authentication mode offset");
_Static_assert(offsetof(MagicGateRecordIdentity, keyMode) == 6,
	       "record key mode offset");
_Static_assert(offsetof(MagicGateRecordIdentity, extension) == 7,
	       "record extension offset");
_Static_assert(sizeof(MagicGateRecordHeader) == 44, "record header size");
_Static_assert(offsetof(MagicGateRecordHeader, identity) == 0,
	       "record identity offset");
_Static_assert(offsetof(MagicGateRecordHeader, encodedHeaderMetadata) == 8,
	       "encoded header metadata offset");
_Static_assert(offsetof(MagicGateRecordHeader, tag) == 16, "record tag offset");
_Static_assert(offsetof(MagicGateRecordHeader, treeSize) == 32, "tree size offset");
_Static_assert(offsetof(MagicGateRecordHeader, ciphertextSize) == 36,
	       "ciphertext size offset");
_Static_assert(offsetof(MagicGateRecordHeader, trailerSize) == 40,
	       "trailer size offset");
_Static_assert(sizeof(MagicGateFileInfo) == 16, "file info size");
_Static_assert(offsetof(MagicGateFileInfo, format) == 0, "file format offset");
_Static_assert(offsetof(MagicGateFileInfo, size) == 1, "file size offset");
_Static_assert(offsetof(MagicGateFileInfo, group) == 2, "file group offset");
_Static_assert(offsetof(MagicGateFileInfo, identifier) == 3,
	       "file identifier offset");

typedef union MagicGateFileIdentity {
	uint64_t halves[2];
	uint32_t words[4];
} MagicGateFileIdentity;

typedef struct MagicGateFileGroup {
	MagicGateFileIdentity identity;
	uint64_t creationTick;
	uint64_t modificationTick;
} MagicGateFileGroup;

_Static_assert(sizeof(MagicGateFileIdentity) == 16, "file identity size");
_Static_assert(offsetof(MagicGateFileIdentity, halves) == 0, "identity halves offset");
_Static_assert(offsetof(MagicGateFileIdentity, words) == 0, "identity words offset");
_Static_assert(sizeof(MagicGateFileGroup) == 32, "file group size");
_Static_assert(offsetof(MagicGateFileGroup, identity) == 0, "group identity offset");
_Static_assert(offsetof(MagicGateFileGroup, creationTick) == 16, "creation tick offset");
_Static_assert(offsetof(MagicGateFileGroup, modificationTick) == 24, "modification tick offset");

int SceMagicGateForDriver_E62B8A67(int handle, MagicGateFileGroup *info,
				int group);

int SceMagicGateForDriver_12CE769D(uint64_t *challenge);
int SceMagicGateForDriver_E0888D78(uint64_t *random, uint64_t *challenge);

typedef struct MagicGateBlock {
	uint8_t bytes[16];
} MagicGateBlock;

_Static_assert(sizeof(MagicGateBlock) == 16, "block size");

typedef struct MagicGateBundledEkb2Record {
	char magic[4];
	uint8_t encodedPrefixMetadata[28];
	MagicGateRecordHeader header;
	uint8_t packedTree[20];
	MagicGateBlock ciphertext[7];
	uint8_t encodedTrailerMetadata[52];
} MagicGateBundledEkb2Record;

_Static_assert(sizeof(MagicGateBundledEkb2Record) == 260, "EKB2 record size");
_Static_assert(offsetof(MagicGateBlock, bytes) == 0, "block bytes offset");
_Static_assert(offsetof(MagicGateBundledEkb2Record, magic) == 0, "EKB2 magic offset");
_Static_assert(offsetof(MagicGateBundledEkb2Record, encodedPrefixMetadata) == 4,
	       "EKB2 encoded prefix offset");
_Static_assert(offsetof(MagicGateBundledEkb2Record, header) == 32,
	       "EKB2 header offset");
_Static_assert(offsetof(MagicGateBundledEkb2Record, packedTree) == 76,
	       "EKB2 tree offset");
_Static_assert(offsetof(MagicGateBundledEkb2Record, ciphertext) == 96,
	       "EKB2 ciphertext offset");
_Static_assert(offsetof(MagicGateBundledEkb2Record, encodedTrailerMetadata) == 208,
	       "EKB2 encoded trailer offset");

int SceMagicGateForDriver_01FD0260(int slot, const void *input, int sourceSlot);
int SceMagicGateForDriver_0201E54B(MagicGateDeviceIdentity *output);
int SceMagicGateForDriver_05912BF5(void *output, const void *input, const void *iv,
				   unsigned int size, int slot);
int SceMagicGateForDriver_07BDCE2F(void *output, int slot, int sourceSlot);
int SceMagicGateForDriver_0B01FE3F(const void *tag, const MagicGateRecordHeader *record,
				   unsigned int size, int slot);
int SceMagicGateForDriver_0EBA5958(void *output, unsigned int size);
int SceMagicGateForDriver_144BFA76(int index);
int SceMagicGateForDriver_1BCE34AD(int handle, unsigned int index, void *record, SceSize size);
int SceMagicGateForDriver_2B026FFF(void *output, const void *input, unsigned int size, int slot,
				   const void *iv);
int SceMagicGateForDriver_2D5069DE(int slot, const MagicGateRecordHeader *record,
				   unsigned int size);
int SceMagicGateForDriver_30DF2E82(void);
int SceMagicGateForDriver_3EDD73EB(int handle, int index);
struct MagicGateInstalledRecord;
int SceMagicGateForDriver_42A9D74E(int slot, void *output, const uint64_t *info,
				   const uint64_t *challenge, const uint64_t *response,
				   const struct MagicGateInstalledRecord *record);
int SceMagicGateForDriver_46A28FAA(void);
int SceMagicGateForDriver_4E8B4EB5(int slot, const void *input, int sourceSlot);
int SceMagicGateForDriver_4F4B2DC9(void *output, const void *input, void *iv, unsigned int size,
				   int slot);
int SceMagicGateForDriver_593016E1(void *output, const void *input, void *iv, unsigned int size,
				   int slot);
int SceMagicGateForDriver_6A240950(void *output, const void *input, void *iv, unsigned int size,
				   int slot);
int SceMagicGateForDriver_6D3B42E6(void);
int SceMagicGateForDriver_75B445DA(int handle, unsigned int index, const void *record,
				   SceSize size);
int SceMagicGateForDriver_758D7158(void *output, unsigned int size);
int SceMagicGateForDriver_7B052A10(void);
int SceMagicGateForDriver_8E2E577E(void);
int SceMagicGateForDriver_8E87BA88(int handle, unsigned int group);
int SceMagicGateForDriver_9CEF4965(void *output, const void *input, void *iv, unsigned int size,
				   const void *key);
int SceMagicGateForDriver_A855C045(int slot, const void *input);
int SceMagicGateForDriver_ACA0F529(int slot, const void *input, int sourceSlot);
int SceMagicGateForDriver_B0827B4F(void *output, const void *input, void *iv, unsigned int size,
				   int slot);
int SceMagicGateForDriver_C9658A76(void *output, const void *input, const void *iv,
				   unsigned int size, int slot);
int SceMagicGateForDriver_CA74A13D(void *output, unsigned int capacity,
				   const MagicGateRecordHeader *record, unsigned int size,
				   const MagicGateDeviceIdentity *challenge);
int SceMagicGateForDriver_CFD943E0(void *output, const void *input, void *iv, unsigned int size);
int SceMagicGateForDriver_D819D5C6(int handle, MagicGateFileInfo *info);
int SceMagicGateForDriver_F2502A3B(void *output, const void *input, unsigned int size, int slot,
				   const void *iv);
int SceMagicGateForDriver_FEF11D84(int type);
