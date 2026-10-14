/* Private driver state; original source-file and internal names are unavailable. */
#ifndef MAGICGATE_PRIVATE_H
#define MAGICGATE_PRIVATE_H

#include <stddef.h>
#include <stdint.h>
#include <psp2/types.h>
#include <sce/magicgate.h>

typedef struct MagicGateDriver {
	SceUID mutex;
	int references;
} MagicGateDriver;

_Static_assert(sizeof(MagicGateDriver) == 8, "MagicGateDriver size");
_Static_assert(offsetof(MagicGateDriver, mutex) == 0, "mutex offset");
_Static_assert(offsetof(MagicGateDriver, references) == 4, "references offset");

typedef struct MagicGateSlot {
	int8_t *index;
	uint8_t type;
	uint8_t flags;
	uint16_t storageOffset;
} MagicGateSlot;

_Static_assert(sizeof(MagicGateSlot) == 8, "MagicGateSlot size");
_Static_assert(offsetof(MagicGateSlot, index) == 0, "index offset");
_Static_assert(offsetof(MagicGateSlot, type) == 4, "type offset");
_Static_assert(offsetof(MagicGateSlot, flags) == 5, "flags offset");
_Static_assert(offsetof(MagicGateSlot, storageOffset) == 6, "storage offset");

typedef struct MagicGateBundledRecord {
	MagicGateRecordHeader header;
	uint8_t packedTree[12];
	MagicGateBlock ciphertext[2];
	uint8_t encodedTrailerMetadata[52];
} MagicGateBundledRecord;

_Static_assert(sizeof(MagicGateBundledRecord) == 140, "bundled record size");

typedef union MagicGateKey {
	uint8_t bytes[16];
	uint32_t words[4];
	uint64_t halves[2];
} MagicGateKey;

_Static_assert(sizeof(MagicGateKey) == 16, "key size");

typedef struct MagicGateKeyState {
	MagicGateKey treeKeys[18];
	uint64_t savedChallenge;
} MagicGateKeyState;

_Static_assert(sizeof(MagicGateKeyState) == 296, "key state size");
_Static_assert(offsetof(MagicGateKeyState, savedChallenge) == 288,
	       "saved challenge offset");

typedef struct MagicGateInstalledRecord {
	uint8_t count[4];
	uint8_t depth[4];
	MagicGateRecordIdentity identity;
	MagicGateBlock blocks[63];
} MagicGateInstalledRecord;

_Static_assert(sizeof(MagicGateInstalledRecord) == 1024, "installed record size");
_Static_assert(offsetof(MagicGateInstalledRecord, blocks) == 16, "record blocks offset");

extern const MagicGateDeviceIdentity defaultDeviceIdentity;

typedef struct MagicGateFileRecord {
	uint8_t index;
	uint8_t reserved[3];
	uint8_t sequence[4];
	uint8_t payload[1016];
	uint64_t zeroTrailer[2];
	MagicGateFileIdentity identity;
} MagicGateFileRecord;

typedef struct MagicGateFileService {
	MagicGateFileRecord record;
	MagicGateFileInfo info;
	MagicGateKey key;
	MagicGateFileIdentity identity;
	MagicGateFileGroup groups[4];
	int selectedGroup;
	SceUID mutex;
	int references;
	unsigned int flags;
} MagicGateFileService;

typedef struct MagicGateFileRequest {
	int handle;
	unsigned int index;
	MagicGateFileRecord *record;
	SceSize size;
} MagicGateFileRequest;

_Static_assert(sizeof(MagicGateFileRequest) == 16, "file request size");

_Static_assert(sizeof(MagicGateFileRecord) == 1056, "file record size");
_Static_assert(sizeof(MagicGateFileService) == 1248, "file service size");
_Static_assert(offsetof(MagicGateFileService, groups) == 1104, "file groups offset");
_Static_assert(offsetof(MagicGateFileService, mutex) == 1236, "file mutex offset");

extern MagicGateFileService magicGateFileService;
extern char magicGateFilePath[32];
int recoverFileGroups(void);
int deleteFileGroup(int group);
int readFileRecord(MagicGateFileRequest *request);
int writeFileRecord(MagicGateFileRequest *request);

const MagicGateDeviceIdentity *getDeviceIdentity(int type);
uint64_t *getSavedChallenge(uint64_t *challenge);
uint64_t *setSavedChallenge(uint64_t *challenge);
int selectRecordBlocks(MagicGateBlock *output, int *count, int *depth,
		       const uint8_t *path, const MagicGateRecordHeader *record,
		       unsigned int size);
int verifyRecordTag(const void *tag, const uint32_t *identifier,
		    const void *data, unsigned int size, int slot);
int installRecordKey(int slot, int type,
		     const MagicGateInstalledRecord *record, const uint64_t *tag);

extern MagicGateKey slotKeys[41];
extern MagicGateInstalledRecord installedRecord;
int checkCachedRecord(int slot, const MagicGateRecordHeader *record);
void notifyRecordInstalled(int slot, const MagicGateRecordIdentity *identity);
extern uint64_t sessionKey;
int cipherBuffer(void *output, const void *input, void *iv, unsigned int size,
		 int slot, const void *key, int mode);
int encryptDesEcb(const void *key, const void *input, void *output, unsigned int size);
int decryptDesEcb(const void *key, const void *input, void *output, unsigned int size);
int encryptDesCbc(const void *key, const void *input, void *output,
		  unsigned int size, const void *iv);
int decryptDesCbc(const void *key, const void *input, void *output,
		  unsigned int size, const void *iv);
int calculateDesMac(const void *key, const void *input, void *output,
		    unsigned int size, const void *iv);
int calculateRetailMac(const MagicGateKey *key, const void *input, void *output,
		       unsigned int size, void *iv);
int calculateRecordMac(const MagicGateKey *key, const void *input, void *output,
		       unsigned int size, const void *iv);
int getTreeKey(int type, MagicGateKey *output, unsigned int depth);
int decryptRecordBlock(const MagicGateKey *key, const void *input, void *output);
int encryptRecordBlock(const MagicGateKey *key, const void *input, void *output);
int decryptRecordEcb(const MagicGateKey *key, const void *input, void *output,
		     unsigned int size);
int encryptRecordEcb(const MagicGateKey *key, const void *input, void *output,
		     unsigned int size);
int getDriverKey(void *output, int index);
int encryptRecordCbc(const void *key, const void *input, void *output,
		     unsigned int size, const void *iv);
int decryptRecordCbc(const MagicGateKey *key, const void *input, void *output,
		     unsigned int size, const void *iv);

extern MagicGateDriver magicGateDriver;
int createDriverMutex(void);
int deleteDriverMutex(void);
void terminateDriver(void);
void terminateFileService(void);
int clearSlotState(void);
void initializeSlots(void);
int loadDriverKeyTable(void);
unsigned int getInitialSlot(int type);
int SceMagicGateForDriver_A49B9D70(void);
int SceMagicGateForDriver_492EE950(void);

extern int8_t typeTwoSlotIndex;
extern int typeOneInitialSlot;
extern int typeZeroInitialSlot;
extern int8_t typeOneSlotIndex;
extern int8_t typeThreeSlotIndices[2];
extern int8_t typeZeroSlotIndices[4];
extern int8_t typeFourSlotIndices[32];
extern int8_t typeFiveSlotIndex;
extern int slotState;
extern MagicGateSlot magicGateSlots[41];

#endif
