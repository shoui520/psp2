typedef struct
{
    u16 channels;
    u16 dataBlkSz;
    union {
        u8 bytes[2];
        u16 half;
    } codecParam;
    u32 dataOff;
    u32 factSz;
    u32 dataSize;
    u32 samplesPerChan;
    int loopStart;
    int loopEnd;
} SceAtracFile;

typedef struct
{
    u32 decodePos;
    u32 endSample;
    u32 loopStart;
    u32 loopEnd;
    int samplesPerChan;
    char numFrame;
    char state;
    char secondBufState;
    u8 numChan;
    u16 sampleSize;
    u16 codec;
    u32 dataOff;
    u32 curOff;
    u32 dataEnd;
    int loopNum;
    u32 streamDataByte;
    u32 firstBufReadOff;
    u32 secondBufReadOff;
    u8 *buffer;
    u8 *secondBuffer;
    u32 bufferByte;
    u32 secondBufferByte;
} SceAtracIdInfo;

typedef struct
{
    SceAudiocodecCodec codec __attribute__((aligned(256)));
    SceAtracIdInfo info __attribute__((aligned(128)));
} SceAtracId;

typedef struct
{
    u8 *writePositionFirstBuf;
    u32 writableByteFirstBuf;
    u32 minWriteByteFirstBuf;
    u32 readPositionFirstBuf;
    u8 *writePositionSecondBuf;
    u32 writableByteSecondBuf;
    u32 minWriteByteSecondBuf;
    u32 readPositionSecondBuf;
} SceBufferInfo;

typedef struct
{
    int numChan;
    int outputChan;
    int frameSize;
} SceAtracLowLevelInitParam;

typedef struct
{
    u32 magic;
    u16 frameSize;
    u8 codingMode;
    u8 pad;
    u32 dataOff;
    u32 reservedAfterOff;
    u32 dataSize;
    u32 reservedHeader[3];
    u32 sampleRate;
    u32 reservedBeforeLoop;
    s32 loopStartSample;
    s32 loopEndSample;
} SceAA3File;

int setHalfwayBuffer(SceAtracId *id, u8 *buffer, u32 readByte, u32 bufferByte, SceAtracFile *info);
int setMOutHalfwayBuffer(SceAtracId *id, u8 *buffer, u32 readByte, u32 bufferByte, SceAtracFile *info);
int resetId(SceAtracId *id);
int allocEdram(void);
int openAA3AndGetID(u8 *buffer, u32 readByte, u32 fileSize, SceAtracFile *info);
int isValidState(int state);
int getOutputChan(SceAtracIdInfo *info, SceAudiocodecCodec *codec);
int decodeFrame(SceAtracId *id, short *outAddr, u32 *samples, u32 *finishFlag);
void atracConsumeFrame(SceAtracIdInfo *info);
int loadWaveFile(u32 size, SceAtracFile *info, u8 *in);
int readWaveData(u8 *in, u32 *curOff, int size);
int getOffFromSample(SceAtracIdInfo *info, u32 sample);
u32 getSecondBufPos(SceAtracIdInfo *info, u32 sample);
int getFrameFromSample(SceAtracIdInfo *info, u32 sample);
int getNextSample(SceAtracIdInfo *info);
int setBuffer(SceAtracId *id, u8 *buffer, u32 readByte, u32 bufferByte, SceAtracFile *info);
int initDecoder(SceAtracId *id, SceAtracLowLevelInitParam *initParam);
int initAT3Decoder(SceAudiocodecCodec *codec, SceAtracLowLevelInitParam *initParam);
int initAT3plusDecoder(SceAudiocodecCodec *codec, SceAtracLowLevelInitParam *initParam);
int isValidOpt(int numChan, int outputChan);
void setBufferInfo(SceAtracIdInfo *info, u32 factSize, u32 dataSize, u32 samplesPerChan, int loopStart, int loopEnd);
int setBufferSize(SceAtracIdInfo *info, u32 readByte, u32 bufferByte);
int getRemainFrame(SceAtracIdInfo *info);
u8 *getWritePtr(SceAtracIdInfo *info);
u32 getReadPosition(SceAtracIdInfo *info);
u32 getWritableByte(SceAtracIdInfo *info);
int atracGetRemainFramesLinear(SceAtracIdInfo *info);
int atracGetRemainFramesLoop(SceAtracIdInfo *info);
int atracCountLoopFrames(SceAtracIdInfo *info);
int atracAdvanceRingOffset(u32 readOff, u32 bufferByte, u32 streamBytes, u32 frameSize);
u32 atracAddOffsetBounded(u32 dataEnd, u32 curOff, u32 streamBytes, u32 bufferByte);
u32 atracAdvanceLoopOffset(u32 curOff, u32 streamBytes, u32 loopStartOff, u32 loopEndOff, u32 bufferByte);
u32 atracGetNextWriteBytes(SceAtracIdInfo *info);
u32 atracGetRemainDataBytes(SceAtracIdInfo *info);
u32 atracGetLoopRemainBytes(SceAtracIdInfo *info);
u32 atracGetContiguousWritable(u32 readOff, u32 bufferByte, u32 streamBytes, u32 frameSize);
u32 atracGetFirstBufStreamBytes(SceAtracIdInfo *info);
u32 atracAlignBufferEnd(u32 startOff, u32 frameSize, u32 bufferByte);
void copyBuffer(SceAtracIdInfo *info, u32 bufferByte);
void getBufferInfo(SceAtracIdInfo *info, u32 sample, SceBufferInfo *bufferInfo);
int resetPlayPos(SceAtracIdInfo *info, u32 sample, u32 writeByteFirstBuf, u32 writeByteSecondBuf);
int parseAA3(u32 readByte, SceAA3File *aa3, u32 fileSize, u8 *buffer);
int setAtracFileInfo(SceAA3File *aa3, SceAtracFile *info);
int aa3ReadU32BE(u8 *buffer, u32 *curOff, u32 readByte);
u16 aa3ReadU16BE(u8 *buffer, u32 *curOff, u32 readByte);
int aa3ReadU24BE(u8 *buffer, u32 *curOff, u32 readByte);
int aa3ReadSyncSafeU32(u8 *buffer, u32 *curOff, u32 readByte);
