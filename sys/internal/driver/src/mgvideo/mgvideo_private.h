/* Private serialized formats; descriptive names where Sony gives no identifier. */
#ifndef MGVIDEO_PRIVATE_H
#define MGVIDEO_PRIVATE_H

#include <stdint.h>
#include <magicgate/magicgate.h>
#include <rtc.h>

struct MgVideoContext;
int loadVideoKeyring(struct MgVideoContext *context);
int lockVideoDriver(void);
int unlockVideoDriver(void);
int createVideoContext(int type, int flags, int options);
int releaseVideoContext(int handle);
int waitVideoRecord(int handle, int index, int flags);
int signalVideoRecord(int handle, int index);
int initializeVideoKeyring(int handle);
int closeVideoRecordStream(int handle);
int saveVideoKeyring(int handle);
int resetVideoKeyring(int handle);
int selectVideoFileGroup(int handle, const MagicGateFileIdentity *input);
int closeVideoCipherChannel(int handle);
int isVideoCipherKeySelected(int handle, unsigned int selection);
int decryptVideoCipherBuffer(unsigned int handle, void *output,
			     const void *input, unsigned int size);
int decryptVideoCipherStream(unsigned int handle, void *output,
			     const void *input, unsigned int size,
			     unsigned int totalSize, unsigned int *processed);
int encryptVideoCipherBuffer(unsigned int handle, void *output,
			     const void *input, unsigned int size,
			     uint32_t blockIndex);
int encryptVideoCipherStream(unsigned int handle, void *output,
			     const void *input, unsigned int size,
			     unsigned int totalSize, unsigned int *processed,
			     uint32_t blockIndex);

typedef struct MgVideoCipherBlock {
	uint32_t words[4];
} MgVideoCipherBlock;

_Static_assert(sizeof(MgVideoCipherBlock) == 16, "cipher block size");

typedef union MgVideoGeneratedKey {
	uint64_t halves[2];
	MgVideoCipherBlock block;
} MgVideoGeneratedKey;

_Static_assert(sizeof(MgVideoGeneratedKey) == 16, "generated key size");


typedef struct MgVideoUserBuffer {
	const void *data;
	unsigned int size;
} MgVideoUserBuffer;

typedef struct MgVideoStreamOpenArguments {
	const MgVideoUserBuffer *keyring;
	const MgVideoUserBuffer *recordInput;
	const MgVideoUserBuffer *input;
	uint32_t uninterpretedMetadata;
} MgVideoStreamOpenArguments;

_Static_assert(sizeof(MgVideoStreamOpenArguments) == 16,
	       "stream open arguments size");
_Static_assert(offsetof(MgVideoStreamOpenArguments, keyring) == 0,
	       "stream open keyring offset");
_Static_assert(offsetof(MgVideoStreamOpenArguments, recordInput) == 4,
	       "stream open record offset");
_Static_assert(offsetof(MgVideoStreamOpenArguments, input) == 8,
	       "stream open input offset");
_Static_assert(offsetof(MgVideoStreamOpenArguments, uninterpretedMetadata) == 12,
	       "stream open metadata offset");

_Static_assert(sizeof(MgVideoUserBuffer) == 8, "user buffer size");

typedef struct MgVideoCipherOpenArguments {
	unsigned int copySize;
	uint32_t uninterpretedMetadata;
} MgVideoCipherOpenArguments;

_Static_assert(sizeof(MgVideoCipherOpenArguments) == 8,
	       "cipher open arguments size");
_Static_assert(offsetof(MgVideoCipherOpenArguments, copySize) == 0,
	       "cipher open copy size offset");
_Static_assert(offsetof(MgVideoCipherOpenArguments, uninterpretedMetadata) == 4,
	       "cipher open metadata offset");

typedef struct MgVideoCipherStreamArguments {
	unsigned int size;
	unsigned int totalSize;
	unsigned int *processed;
	uint32_t uninterpretedMetadata;
} MgVideoCipherStreamArguments;

_Static_assert(sizeof(MgVideoCipherStreamArguments) == 16,
	       "cipher stream arguments size");
_Static_assert(offsetof(MgVideoCipherStreamArguments, size) == 0,
	       "cipher stream size offset");
_Static_assert(offsetof(MgVideoCipherStreamArguments, totalSize) == 4,
	       "cipher stream total size offset");
_Static_assert(offsetof(MgVideoCipherStreamArguments, processed) == 8,
	       "cipher stream processed offset");
_Static_assert(offsetof(MgVideoCipherStreamArguments, uninterpretedMetadata) == 12,
	       "cipher stream metadata offset");

typedef struct MgVideoCipherBufferArguments {
	unsigned int size;
	uint32_t blockIndex;
	uint8_t uninterpretedMetadata[8];
} MgVideoCipherBufferArguments;

_Static_assert(sizeof(MgVideoCipherBufferArguments) == 16,
	       "cipher buffer arguments size");
_Static_assert(offsetof(MgVideoCipherBufferArguments, size) == 0,
	       "cipher buffer size offset");
_Static_assert(offsetof(MgVideoCipherBufferArguments, blockIndex) == 4,
	       "cipher buffer block offset");
_Static_assert(offsetof(MgVideoCipherBufferArguments, uninterpretedMetadata) == 8,
	       "cipher buffer metadata offset");

typedef struct MgVideoCipherEncryptArguments {
	unsigned int size;
	unsigned int totalSize;
	unsigned int *processed;
	uint32_t blockIndex;
	uint8_t uninterpretedMetadata[8];
} MgVideoCipherEncryptArguments;

_Static_assert(sizeof(MgVideoCipherEncryptArguments) == 24,
	       "cipher encrypt arguments size");
_Static_assert(offsetof(MgVideoCipherEncryptArguments, size) == 0,
	       "cipher encrypt size offset");
_Static_assert(offsetof(MgVideoCipherEncryptArguments, totalSize) == 4,
	       "cipher encrypt total size offset");
_Static_assert(offsetof(MgVideoCipherEncryptArguments, processed) == 8,
	       "cipher encrypt processed offset");
_Static_assert(offsetof(MgVideoCipherEncryptArguments, blockIndex) == 12,
	       "cipher encrypt block offset");
_Static_assert(offsetof(MgVideoCipherEncryptArguments, uninterpretedMetadata) == 16,
	       "cipher encrypt metadata offset");

typedef struct MgVideoBlockRemoveArguments {
	const int16_t *blockNumbers;
	int blockCount;
	const MgVideoUserBuffer *input;
	unsigned int copySize;
	uint8_t uninterpretedMetadata[8];
} MgVideoBlockRemoveArguments;

_Static_assert(sizeof(MgVideoBlockRemoveArguments) == 24,
	       "block remove arguments size");
_Static_assert(offsetof(MgVideoBlockRemoveArguments, blockNumbers) == 0,
	       "block remove numbers offset");
_Static_assert(offsetof(MgVideoBlockRemoveArguments, blockCount) == 4,
	       "block remove count offset");
_Static_assert(offsetof(MgVideoBlockRemoveArguments, input) == 8,
	       "block remove input offset");
_Static_assert(offsetof(MgVideoBlockRemoveArguments, copySize) == 12,
	       "block remove copy size offset");
_Static_assert(offsetof(MgVideoBlockRemoveArguments, uninterpretedMetadata) == 16,
	       "block remove metadata offset");

typedef struct MgVideoPlaybackQueryArguments {
	const MgVideoUserBuffer *input;
	int mode;
	unsigned int copySize;
	uint32_t uninterpretedMetadata;
} MgVideoPlaybackQueryArguments;

_Static_assert(sizeof(MgVideoPlaybackQueryArguments) == 16,
	       "playback query arguments size");
_Static_assert(offsetof(MgVideoPlaybackQueryArguments, input) == 0,
	       "playback query input offset");
_Static_assert(offsetof(MgVideoPlaybackQueryArguments, mode) == 4,
	       "playback query mode offset");
_Static_assert(offsetof(MgVideoPlaybackQueryArguments, copySize) == 8,
	       "playback query copy size offset");
_Static_assert(offsetof(MgVideoPlaybackQueryArguments, uninterpretedMetadata) == 12,
	       "playback query metadata offset");

typedef struct MgVideoKeyringReplaceArguments {
	const MgVideoUserBuffer *keyring;
	const MgVideoUserBuffer *input;
	const MgVideoUserBuffer *recordInput;
	const MgVideoUserBuffer *replacementInput;
	uint8_t uninterpretedMetadata[8];
} MgVideoKeyringReplaceArguments;

_Static_assert(sizeof(MgVideoKeyringReplaceArguments) == 24,
	       "keyring replace arguments size");
_Static_assert(offsetof(MgVideoKeyringReplaceArguments, keyring) == 0,
	       "keyring replace keyring offset");
_Static_assert(offsetof(MgVideoKeyringReplaceArguments, input) == 4,
	       "keyring replace input offset");
_Static_assert(offsetof(MgVideoKeyringReplaceArguments, recordInput) == 8,
	       "keyring replace record offset");
_Static_assert(offsetof(MgVideoKeyringReplaceArguments, replacementInput) == 12,
	       "keyring replace replacement offset");
_Static_assert(offsetof(MgVideoKeyringReplaceArguments, uninterpretedMetadata) == 16,
	       "keyring replace metadata offset");

typedef struct MgVideoPlaybackUpdateArguments {
	const MgVideoUserBuffer *keyring;
	const MgVideoUserBuffer *input;
	const MgVideoUserBuffer *recordInput;
	const MgVideoUserBuffer *replacementInput;
	const MgVideoUserBuffer *optionInput;
	const void *extensionInput;
	uint8_t uninterpretedMetadata[8];
} MgVideoPlaybackUpdateArguments;

_Static_assert(sizeof(MgVideoPlaybackUpdateArguments) == 32,
	       "playback update arguments size");
_Static_assert(offsetof(MgVideoPlaybackUpdateArguments, keyring) == 0,
	       "playback update keyring offset");
_Static_assert(offsetof(MgVideoPlaybackUpdateArguments, input) == 4,
	       "playback update input offset");
_Static_assert(offsetof(MgVideoPlaybackUpdateArguments, recordInput) == 8,
	       "playback update record offset");
_Static_assert(offsetof(MgVideoPlaybackUpdateArguments, replacementInput) == 12,
	       "playback update replacement offset");
_Static_assert(offsetof(MgVideoPlaybackUpdateArguments, optionInput) == 16,
	       "playback update option offset");
_Static_assert(offsetof(MgVideoPlaybackUpdateArguments, extensionInput) == 20,
	       "playback update extension offset");
_Static_assert(offsetof(MgVideoPlaybackUpdateArguments, uninterpretedMetadata) == 24,
	       "playback update metadata offset");

typedef union MgVideoRecordSequence {
	uint8_t bytes[4];
	uint32_t encoded;
} MgVideoRecordSequence;

_Static_assert(sizeof(MgVideoRecordSequence) == 4, "record sequence size");

typedef struct MgVideoKeyringRecord {
	uint8_t index;
	uint8_t flags;
	uint8_t payloadSize[2];
	MgVideoRecordSequence sequence;
	uint64_t encodedKeyWords[127];
} MgVideoKeyringRecord;

_Static_assert(sizeof(MgVideoKeyringRecord) == 1024, "keyring record size");
_Static_assert(offsetof(MgVideoKeyringRecord, encodedKeyWords) == 8,
	       "keyring words offset");

int readVideoKeyringRecord(struct MgVideoContext *context, int index,
			   MgVideoKeyringRecord *record);
int writeVideoKeyringRecord(struct MgVideoContext *context, unsigned int index,
			    MgVideoKeyringRecord *record);

extern MgVideoKeyringRecord keyringRecord;
extern const uint64_t emptyKeyringEntry;

typedef struct MgVideoRecordCache {
	uint32_t contentKeys[32][4];
	uint64_t recordTags[32];
	uint64_t recordHeaderTags[32];
} MgVideoRecordCache;

typedef struct MgVideoContextCallbacks {
	int (*installRecord)(int slot, const struct MagicGateRecordHeader *record,
			     unsigned int size);
	int (*unwrapKey)(int slot, const void *input, int sourceSlot);
	int (*unwrapDriverKey)(int slot, const void *input, int sourceSlot);
	int (*getWrappedKey)(void *output, int slot, int sourceSlot);
	int (*getFileInfo)(int handle, struct MagicGateFileInfo *info);
	int (*getEkb2Record)(void *output, unsigned int capacity);
	int (*initialize)(struct MgVideoContext *context, uint64_t *challenge);
	void (*notifyFinalized)(void);
	int (*terminate)(struct MgVideoContext *context);
	int (*readRecord)(struct MgVideoContext *context, int index, MgVideoKeyringRecord *record);
	int (*writeRecord)(struct MgVideoContext *context, unsigned int index, MgVideoKeyringRecord *record);
	int (*removeRecord)(struct MgVideoContext *context, int index, void *record);
	int (*resetRecord)(struct MgVideoContext *context, int index, void *record);
} MgVideoContextCallbacks;

typedef struct MgVideoContext {
	/* Unaccessed by this module's supported driver and its imported callbacks. */
	uint8_t inactiveDriverStorage[1984];
	MgVideoRecordCache records;
	MagicGateDeviceIdentity deviceIdentity;
	uint32_t reservedDeviceState;
	int fileHandle;
	unsigned int state;
	int eventFlag;
	unsigned int recordErrorFlags;
	unsigned int validRecordMask;
	unsigned int modeFlags;
	int parserDialect;
	int references;
	int driverType;
	uint32_t reservedDriverState[2];
	int contentKeySlot;
	MgVideoContextCallbacks callbacks;
	uint32_t reservedCallbacks[6];
} MgVideoContext;

_Static_assert(sizeof(MgVideoContext) == 3152, "video context size");
_Static_assert(offsetof(MgVideoContext, records) == 1984, "record cache offset");
_Static_assert(offsetof(MgVideoContext, deviceIdentity) == 3008,
	       "device identity offset");
_Static_assert(offsetof(MgVideoContext, fileHandle) == 3028,
	       "file handle offset");
_Static_assert(offsetof(MgVideoContext, modeFlags) == 3048, "mode flags offset");
_Static_assert(offsetof(MgVideoContext, contentKeySlot) == 3072,
	       "content key slot offset");
_Static_assert(offsetof(MgVideoContext, callbacks) == 3076, "callbacks offset");
_Static_assert(offsetof(MgVideoContext, reservedCallbacks) == 3128,
	       "inactive callback storage offset");

_Static_assert(sizeof(MgVideoRecordCache) == 1024, "record cache size");
_Static_assert(offsetof(MgVideoRecordCache, recordTags) == 512,
	       "record tags offset");
_Static_assert(offsetof(MgVideoRecordCache, recordHeaderTags) == 768,
	       "record header tags offset");
_Static_assert(sizeof(MgVideoContextCallbacks) == 52, "context callback size");
_Static_assert(offsetof(MgVideoContextCallbacks, getEkb2Record) == 20,
	       "EKB2 callback offset");
_Static_assert(offsetof(MgVideoContextCallbacks, readRecord) == 36,
	       "read callback offset");
_Static_assert(offsetof(MgVideoContextCallbacks, resetRecord) == 48,
	       "reset record callback offset");

typedef struct MgVideoRecordUpdateRequest {
	MgVideoContext *context;
	int index;
	MgVideoCipherBlock *userBlocks;
	const int16_t *blockNumbers;
	int blockCount;
	const MgVideoCipherBlock *replacement;
	MgVideoCipherBlock *recordKey;
	uint64_t *recordTag;
	int slot;
} MgVideoRecordUpdateRequest;

_Static_assert(sizeof(MgVideoRecordUpdateRequest) == 36, "record update size");
_Static_assert(offsetof(MgVideoRecordUpdateRequest, recordTag) == 28,
	       "record update tag offset");

extern const MgVideoCipherBlock zeroVideoBlock;

typedef struct MgVideoKeySystemId {
	uint32_t words[6];
} MgVideoKeySystemId;

_Static_assert(sizeof(MgVideoKeySystemId) == 24, "key system ID size");

typedef struct MgVideoKeyGenerationInput {
	MgVideoKeySystemId keySystemId;
	uint32_t zeroWord;
	uint32_t selector;
} MgVideoKeyGenerationInput;

int generateVideoRecordKey(uint64_t output[2], const MgVideoKeySystemId *keySystemId,
			   unsigned int selector);

_Static_assert(sizeof(MgVideoKeyGenerationInput) == 32,
	       "key generation input size");

typedef union MgVideoStreamKey {
	struct {
		uint64_t counterPrefix;
		uint8_t firstBlock[4];
		uint8_t endBlock[4];
	};
	uint64_t generatedWords[2];
} MgVideoStreamKey;

typedef struct MgVideoAuthenticatedHeader {
	MagicGateFileInfo fileInfo;
	MgVideoRecordSequence sequence;
	uint8_t encodedRecordMetadata[12];
} MgVideoAuthenticatedHeader;

typedef struct MgVideoWrappedRecordHeader {
	uint64_t wrappedKey[2];
	MgVideoAuthenticatedHeader authenticatedData;
} MgVideoWrappedRecordHeader;

_Static_assert(sizeof(MgVideoAuthenticatedHeader) == 32,
	       "authenticated header size");

_Static_assert(sizeof(MgVideoWrappedRecordHeader) == 48,
	       "wrapped record header size");

typedef struct MgVideoKeyringHeader {
	char label[12];
	uint8_t descriptorSize[2];
	uint8_t descriptorCount[2];
	uint8_t keyDataSize[2];
	uint8_t format[2];
	uint8_t keyCount[2];
	uint8_t encodedDescriptorMetadata[10];
	MgVideoCipherBlock wrappedDriverKey;
	MgVideoStreamKey keys[2];
} MgVideoKeyringHeader;

typedef struct MgVideoKeyringEnvelope {
	MgVideoCipherBlock tag;
	MgVideoKeyringHeader header;
} MgVideoKeyringEnvelope;

typedef struct MgVideoAuthenticatedRecord {
	MgVideoWrappedRecordHeader header;
	MgVideoCipherBlock blocks[999];
} MgVideoAuthenticatedRecord;

typedef union MgVideoRecordMagic {
	char text[4];
	uint32_t encoded;
} MgVideoRecordMagic;

typedef struct MgVideoRecordPrefix {
	MgVideoRecordMagic magic;
	uint16_t encodedRecordMetadata;
	uint16_t stateMarker;
	uint8_t encodedPrefixMetadata[8];
} MgVideoRecordPrefix;

typedef struct MgVideoUserRecordHeader {
	MgVideoRecordPrefix prefix;
	MgVideoWrappedRecordHeader record;
} MgVideoUserRecordHeader;

typedef struct MgVideoUserRecord {
	MgVideoRecordPrefix encodedPrefix;
	MgVideoAuthenticatedRecord record;
} MgVideoUserRecord;

_Static_assert(sizeof(MgVideoRecordPrefix) == 16, "record prefix size");
_Static_assert(sizeof(MgVideoUserRecordHeader) == 64, "user record header size");

extern const char videoRecordMagic[5];
extern const uint16_t videoRecordStateMarker;
extern uint64_t videoRandomChallenge;

typedef struct MgVideoAuthenticationRequest {
	MgVideoContext *context;
	MgVideoCipherBlock *recordKey;
	uint64_t *recordTag;
	uint64_t *headerTag;
	const MgVideoAuthenticatedRecord *record;
	int sourceSlot;
} MgVideoAuthenticationRequest;

_Static_assert(sizeof(MgVideoAuthenticatedRecord) == 16032,
	       "authenticated record size");
_Static_assert(sizeof(MgVideoAuthenticationRequest) == 24,
	       "authentication request size");

typedef struct MgVideoKeyringRequest {
	MgVideoContext *context;
	MgVideoKeyringHeader *header;
	MgVideoKeySystemId *keySystemId;
	int index;
	int blockNumber;
	const MgVideoUserBuffer *keyring;
	const MgVideoUserRecord *record;
	const MgVideoUserBuffer *input;
} MgVideoKeyringRequest;

_Static_assert(sizeof(MgVideoKeyringHeader) == 80, "keyring header size");
_Static_assert(offsetof(MgVideoKeyringHeader, wrappedDriverKey) == 32,
	       "keyring driver key offset");
_Static_assert(offsetof(MgVideoKeyringHeader, keys) == 48,
	       "keyring stream keys offset");
_Static_assert(sizeof(MgVideoKeyringEnvelope) == 96, "keyring envelope size");
_Static_assert(sizeof(MgVideoUserRecord) == 16048, "user record size");
_Static_assert(sizeof(MgVideoKeyringRequest) == 32, "keyring request size");

extern const char keyringLabel[13];
int parseVideoProtectionInfo(int dialect, MgVideoKeySystemId *keySystemId,
			     const void *input, unsigned int size);
int installUserVideoRecord(MgVideoContext *context, const MgVideoUserBuffer *input);
int translateParserError(int result);

typedef struct MgVideoCipherChannel {
	MgVideoCipherBlock currentIv;
	MgVideoCipherBlock initialIv;
	MgVideoStreamKey *counterKey;
	uint32_t firstBlock;
	uint32_t endBlock;
	uint32_t reservedRange;
	uint32_t currentBlock;
	MgVideoStreamKey *key;
	uint32_t keySelection;
	uint32_t reservedChannel;
} MgVideoCipherChannel;

typedef struct MgVideoRecordStream {
	MgVideoContext *context;
	int16_t active;
	uint16_t keyCount;
	MgVideoKeySystemId keySystemId;
	int contentKeySlot;
	int processId;
	uint32_t reservedStream[2];
	MgVideoStreamKey keys[2];
	MgVideoCipherChannel channels[4];
} MgVideoRecordStream;

typedef struct MgVideoProcessReference {
	int processId;
	int contextHandle;
	int references;
} MgVideoProcessReference;

_Static_assert(sizeof(MgVideoStreamKey) == 16, "stream key size");
_Static_assert(sizeof(MgVideoCipherChannel) == 64, "cipher channel size");
_Static_assert(offsetof(MgVideoCipherChannel, key) == 52, "channel key offset");
_Static_assert(sizeof(MgVideoRecordStream) == 336, "record stream size");
_Static_assert(offsetof(MgVideoRecordStream, keys) == 48, "stream keys offset");
_Static_assert(offsetof(MgVideoRecordStream, channels) == 80,
	       "stream channels offset");
_Static_assert(sizeof(MgVideoProcessReference) == 12, "process reference size");

extern int videoReferences;
extern MgVideoProcessReference processReferences[8];
extern int driverMutex;
extern int processEventHandler;
extern int contextHeaps[2];
extern MgVideoContext *videoContexts[2];
extern MgVideoRecordStream recordStreams[2];

struct ProcEventParam;
int onVideoProcessKill(int processId, struct ProcEventParam *event,
		       void *common);
int onVideoProcessExit(int processId, struct ProcEventParam *event,
		       void *common);

typedef struct MgVideoFileGroupInfo {
	MagicGateFileIdentity identity;
	uint64_t creationTick;
	uint64_t modificationTick;
	uint32_t zeroTrailer[4];
} MgVideoFileGroupInfo;

_Static_assert(sizeof(MgVideoFileGroupInfo) == 48, "video file group info size");

/* Serialized integers are big endian; byte fields keep unaligned boxes aligned. */
typedef struct MgVideoBoxHeader {
	uint8_t size[4];
	char type[4];
} MgVideoBoxHeader;

typedef struct MgVideoSchemeTypeBox {
	MgVideoBoxHeader header;
	uint8_t version;
	uint8_t flags[3];
	char schemeType[4];
	uint8_t schemeVersion[2];
} MgVideoSchemeTypeBox;

typedef struct MgVideoKeySystemIdBox {
	MgVideoBoxHeader header;
	uint8_t version;
	uint8_t flags[3];
	MgVideoKeySystemId keySystemId;
} __attribute__((packed)) MgVideoKeySystemIdBox;

typedef struct MgVideoKeyIdBox {
	MgVideoBoxHeader header;
	uint8_t version;
	uint8_t flags[3];
	uint8_t keyId[2];
} MgVideoKeyIdBox;

typedef struct MgVideoEncryptionAlgorithmBox {
	MgVideoBoxHeader header;
	uint8_t version;
	uint8_t flags[3];
	uint8_t algorithm[4];
	MgVideoCipherBlock initializationVector;
} __attribute__((packed)) MgVideoEncryptionAlgorithmBox;

typedef struct MgVideoSchemeInfoBox {
	MgVideoBoxHeader header;
	MgVideoKeySystemIdBox keySystem;
	MgVideoKeyIdBox key;
	MgVideoEncryptionAlgorithmBox encryption;
} MgVideoSchemeInfoBox;

typedef struct MgVideoProtectionInfoBox {
	MgVideoBoxHeader header;
	MgVideoSchemeTypeBox scheme;
	MgVideoSchemeInfoBox information;
} MgVideoProtectionInfoBox;

_Static_assert(sizeof(MgVideoBoxHeader) == 8, "box header size");
_Static_assert(sizeof(MgVideoSchemeTypeBox) == 18, "scheme type box size");
_Static_assert(sizeof(MgVideoKeySystemIdBox) == 36, "key system ID box size");
_Static_assert(sizeof(MgVideoKeyIdBox) == 14, "key ID box size");
_Static_assert(sizeof(MgVideoEncryptionAlgorithmBox) == 32, "algorithm box size");
_Static_assert(sizeof(MgVideoSchemeInfoBox) == 90, "scheme info box size");
_Static_assert(sizeof(MgVideoProtectionInfoBox) == 116, "protection info box size");

extern const MgVideoProtectionInfoBox protectionInfoTemplate;

typedef union MgVideoAvOut {
	uint8_t bytes[8];
	struct {
		uint8_t encodedPrefix[5];
		uint8_t controlFlags;
		uint8_t encodedSuffix[2];
	} fields;
} MgVideoAvOut;

_Static_assert(sizeof(MgVideoAvOut) == 8, "AV output policy size");

typedef struct MgVideoPlaybackPeriodInfo {
	SceRtcTick currentTick;
	SceRtcTick startTick;
	SceRtcTick endTick;
	MgVideoAvOut encodedAvOut;
	SceRtcTick periodStartTick;
	SceRtcTick periodEndTick;
	uint32_t periodSeconds;
} __attribute__((packed, aligned(4))) MgVideoPlaybackPeriodInfo;

typedef struct MgVideoPlaybackQueryResult {
	int32_t resultMarker;
	uint32_t zeroWords[2];
	/* The original parser leaves these bytes unwritten; its caller copies them. */
	uint8_t indeterminateCopyBytes[8];
} MgVideoPlaybackQueryResult;

typedef union MgVideoPlaybackQueryWorkspace {
	MgVideoPlaybackPeriodInfo period;
	MgVideoPlaybackQueryResult query;
} MgVideoPlaybackQueryWorkspace;

_Static_assert(sizeof(MgVideoPlaybackPeriodInfo) == 52, "playback period size");
_Static_assert(offsetof(MgVideoPlaybackPeriodInfo, periodSeconds) == 48,
	       "playback period seconds offset");
_Static_assert(sizeof(MgVideoPlaybackQueryResult) == 20, "playback query size");
_Static_assert(sizeof(MgVideoPlaybackQueryWorkspace) == 52,
	       "playback query workspace size");

int parseVideoPlaybackInfo(int dialect, int mode, MgVideoPlaybackQueryWorkspace *output,
			   const void *input, unsigned int size);

typedef struct MgVideoBlockRange {
	uint32_t encodedFirstBlock;
	uint32_t encodedEndBlock;
} MgVideoBlockRange;

typedef struct MgVideoKeyGenerationParameters {
	MgVideoKeySystemId keySystemId;
	MgVideoBlockRange ranges[3];
} MgVideoKeyGenerationParameters;

typedef struct MgVideoGeneratedProtectionInfo {
	MgVideoCipherBlock authenticationKey;
	MgVideoKeySystemId keySystemId;
	MgVideoProtectionInfoBox protection[3];
	uint8_t zeroTrailer[12];
} MgVideoGeneratedProtectionInfo;

typedef struct MgVideoKeyringPrefix {
	uint32_t magic;
	uint16_t capacity;
	uint16_t size;
	uint8_t encodedMetadata[8];
} MgVideoKeyringPrefix;

typedef struct MgVideoKeyringBuffer {
	MgVideoKeyringPrefix prefix;
	MgVideoKeyringHeader header;
	uint8_t serializedPayload[20960];
} MgVideoKeyringBuffer;

_Static_assert(sizeof(MgVideoKeyGenerationParameters) == 48,
	       "key generation parameters size");
_Static_assert(sizeof(MgVideoGeneratedProtectionInfo) == 400,
	       "generated protection info size");
_Static_assert(offsetof(MgVideoGeneratedProtectionInfo, protection) == 40,
	       "generated protection boxes offset");
_Static_assert(sizeof(MgVideoKeyringPrefix) == 16, "keyring prefix size");
_Static_assert(sizeof(MgVideoKeyringBuffer) == 21056, "keyring buffer size");

typedef struct MgVideoKeyringCreateArguments {
	const MgVideoAvOut *avOut;
	const MgVideoKeyGenerationParameters *parameters;
	int mode;
	uint32_t uninterpretedMetadata;
} MgVideoKeyringCreateArguments;

_Static_assert(sizeof(MgVideoKeyringCreateArguments) == 16,
	       "keyring create arguments size");
_Static_assert(offsetof(MgVideoKeyringCreateArguments, avOut) == 0,
	       "keyring create avout offset");
_Static_assert(offsetof(MgVideoKeyringCreateArguments, parameters) == 4,
	       "keyring create parameters offset");
_Static_assert(offsetof(MgVideoKeyringCreateArguments, mode) == 8,
	       "keyring create mode offset");
_Static_assert(offsetof(MgVideoKeyringCreateArguments, uninterpretedMetadata) == 12,
	       "keyring create metadata offset");

int initializeVideoProtectionInfo(MgVideoProtectionInfoBox *output,
				 const MgVideoKeySystemId *keySystemId, int index,
				 unsigned int algorithm,
				 const MgVideoKeySystemId *derivationId);
int serializeVideoPlaybackPolicy(void *output, unsigned int capacity,
				 const MgVideoKeySystemId *keySystemId,
				 const MgVideoAvOut *avOut, int policyMode,
				 int bindOption, const char *comment);

typedef struct MgVideoTaggedRecord {
	char label[12];
	uint8_t descriptorSize[2];
	uint8_t descriptorCount[2];
	uint8_t payload[];
} MgVideoTaggedRecord;

_Static_assert(sizeof(MgVideoTaggedRecord) == 16, "tagged record header size");

typedef struct MgVideoEncodedTime {
	uint8_t yearHigh;
	uint8_t yearLow;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint8_t hundredth;
} MgVideoEncodedTime;

typedef union MgVideoPlaybackValue {
	MgVideoEncodedTime time;
	MgVideoAvOut avOut;
	struct {
		uint8_t encodedPrefix[6];
		uint8_t hours[2];
	} period;
	uint8_t encodedValue[8];
} MgVideoPlaybackValue;

typedef struct MgVideoPlaybackParameter {
	char label[8];
	MgVideoPlaybackValue value;
} MgVideoPlaybackParameter;

_Static_assert(sizeof(MgVideoEncodedTime) == 8, "encoded time size");
_Static_assert(sizeof(MgVideoPlaybackParameter) == 16, "playback parameter size");

extern const MgVideoEncodedTime earliestVideoTime;
extern const MgVideoEncodedTime latestVideoTime;
extern const MgVideoPlaybackValue emptyVideoPlaybackValue;

typedef struct MgVideoPolicyItem {
	char label[15];
	uint8_t instruction;
} MgVideoPolicyItem;

typedef struct MgVideoPolicyList {
	const char *const *labels;
	int count;
} MgVideoPolicyList;

_Static_assert(sizeof(MgVideoPolicyItem) == 16, "policy item size");
_Static_assert(sizeof(MgVideoPolicyList) == 8, "policy list size");

typedef struct MgVideoReferenceId {
	uint8_t format;
	uint8_t encodedReferenceMetadata;
	uint8_t bindingMode;
	uint8_t encodedUsageMetadata;
	uint8_t encodedMetadata[12];
} MgVideoReferenceId;

_Static_assert(sizeof(MgVideoReferenceId) == 16, "reference ID size");

extern const int videoPolicyRecordRequirement[4];
extern const char *videoPlaybackPolicyLabels[9];
extern const char *videoPlaybackClockPolicyLabels[10];
extern const MgVideoPolicyList videoPlaybackPolicies;
extern const MgVideoPolicyList videoPlaybackClockPolicies;
extern const char *videoBindingRecordLabels[2];
extern const char *videoProtectionRecordLabels[6];
extern const char *videoPlaybackRecordLabels[4];
extern const char *videoReferenceBindingRecordLabels[2];
extern const MgVideoAvOut defaultVideoAvOut;
extern const char *videoPlaybackPeriodRecordLabels[4];

typedef struct MgVideoKeySystemPayload {
	MgVideoKeySystemId keySystemId;
	uint8_t zeroTrailer[8];
} MgVideoKeySystemPayload;

_Static_assert(sizeof(MgVideoKeySystemPayload) == 32, "key system payload size");

int enumerateVideoFileGroups(int handle, MgVideoFileGroupInfo *output,
			    unsigned int size);
int openVideoCipherChannel(int handle, const MgVideoBoxHeader *input, int size);
int removeUserVideoRecordBlocks(int handle, unsigned int index,
			       const MgVideoUserBuffer *recordInput,
			       const int16_t *blockNumbers, int blockCount,
			       const MgVideoUserBuffer *input);
int getUserVideoPlaybackInfo(int handle, void *output, unsigned int outputSize,
			     const MgVideoUserBuffer *input, int mode);
int copyVideoEkb2RecordToUser(int handle, const MgVideoUserBuffer *output);
int generateUserVideoKeyring(int handle, const MgVideoUserBuffer *output,
			     MgVideoGeneratedProtectionInfo *protection,
			     const MgVideoAvOut *avOut,
			     const MgVideoKeyGenerationParameters *parameters,
			     int mode);
int replaceUserVideoKeyring(int handle, unsigned int index, int blockNumber,
			    const MgVideoUserBuffer *keyring,
			    const MgVideoUserBuffer *input,
			    const MgVideoUserBuffer *recordInput,
			    const MgVideoUserBuffer *replacementInput);
int updateUserVideoPlaybackPeriod(int handle, unsigned int index, int blockNumber,
				 const MgVideoUserBuffer *keyring,
				 const MgVideoUserBuffer *input,
				 const MgVideoUserBuffer *recordInput,
				 const MgVideoUserBuffer *replacementInput,
				 const MgVideoUserBuffer *optionInput,
				 const void *extensionInput);
int authorizeUserVideoRecord(int handle, unsigned int index,
			     const MgVideoUserBuffer *recordInput,
			     const MgVideoUserBuffer *input);
int openVideoRecordStream(int handle, unsigned int index, int blockNumber,
			  const MgVideoUserBuffer *keyring,
			  const MgVideoUserBuffer *recordInput,
			  const MgVideoUserBuffer *input);

#endif
