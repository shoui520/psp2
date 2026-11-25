#include <common_imp.h>

#include <avcodec_audiocodec.h>
#include <usersystemlib_kernel.h>

#include "libatrac3plus.h"

SCE_MODULE_INFO("sceATRAC3plus_Library", SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_EXCLUSIVE_START, 1, 5);
SCE_MODULE_BOOTSTART("sceAtracStartEntry");
SCE_MODULE_STOP("sceAtracEndEntry");
SCE_SDK_VERSION(0x06060010);
SCE_MODULE_START_THREAD_PARAMETER(3, 0x20, 0x0400, 0);
SCE_MODULE_STOP_THREAD_PARAMETER(3, 0x20, 0x0400, 0);

typedef struct
{
    u16 frameSize;
    u8 channels;
    u8 modeId;
} SceAtracModeEntry;

typedef struct
{
    u16 frameSize;
    u16 channels;
    u8 configLo;
    u8 configHi;
    u16 pad;
} SceAtracPlusModeEntry;

SceAtracModeEntry g_at3ModeTable[] = {
    { 0x00C0, 1, 0xE },
    { 0x0098, 1, 0xF },
    { 0x0180, 2, 0x4 },
    { 0x0130, 2, 0x6 },
    { 0x00C0, 2, 0xB }
};

SceAtracPlusModeEntry g_at3PlusModeTable[] = {
    { 0x00C0, 1, 0x24, 0x17, 0 },
    { 0x0118, 1, 0x24, 0x22, 0 },
    { 0x0178, 1, 0x24, 0x2E, 0 },
    { 0x0230, 1, 0x24, 0x45, 0 },
    { 0x02E8, 1, 0x24, 0x5C, 0 },
    { 0x0118, 2, 0x28, 0x22, 0 },
    { 0x0178, 2, 0x28, 0x2E, 0 },
    { 0x0230, 2, 0x28, 0x45, 0 },
    { 0x02E8, 2, 0x28, 0x5C, 0 },
    { 0x03A8, 2, 0x28, 0x74, 0 },
    { 0x0460, 2, 0x28, 0x8B, 0 },
    { 0x05D0, 2, 0x28, 0xB9, 0 },
    { 0x0748, 2, 0x28, 0xE8, 0 },
    { 0x0800, 2, 0x28, 0xFF, 0 }
};

// 3EF8
u8 g_at3PlusGUID[] = { 0xBF, 0xAA, 0x23, 0xE9, 0x58, 0xCB, 0x71, 0x44, 
                       0xA1, 0x19, 0xFF, 0xFA, 0x01, 0xE4, 0xCE, 0x62 };

// 3F08
SceAtracModeEntry g_at3WaveModeTable[] = {
    { 0x0180, 0x4, 0 },
    { 0x0130, 0x6, 0 },
    { 0x00C0, 0xB, 1 },
    { 0x00C0, 0xE, 0 },
    { 0x0098, 0xF, 0 }
};

// 3F80
int g_edramAddr = -1;

// 3FC0
SceAtracId g_atracIds[6] __attribute__((aligned(64)));

// 45C0
int g_needMemAT3;

// 45C4
int g_needMemAT3plus;

int sceAtracReinit(int numAT3Id, int numAT3plusId)
{
    int oldIntr = sceKernelCpuSuspendIntr();
    SceAtracId *curId = &g_atracIds[0];
    // 003C
    int i;
    for (i = 0; i < 6; i++)
    {
        if (curId->info.state > 0)
        {
            // 0188
            sceKernelCpuResumeIntr(oldIntr);
            return SCE_ERROR_BUSY;
        }
        curId++;
    }
    curId = &g_atracIds[0];
    // 0064
    for (i = 0; i < 6; i++) {
        curId->info.state = 0;
        curId++;
    }
    sceKernelCpuResumeIntr(oldIntr);
    if (numAT3Id != 0 || numAT3plusId != 0 || g_edramAddr == -1)
    {
        // 00BC
        if (g_edramAddr == -1)
        {
            // 0170
            int ret = allocEdram();
            if (ret < 0)
                return ret;
        }
        // 00CC
        int count = g_edramAddr;
        // 0110
        curId = &g_atracIds[0];
        int i;
        for (i = 0; i < numAT3Id + numAT3plusId; i++)
        {
            curId->codec.edramAddr = (void *)count;
            curId->codec.edramFlag = 1;
            if (i >= numAT3plusId)
            {
                // 0164
                curId->info.codec = 0x1001;
                count += g_needMemAT3;
            }
            else
            {
                curId->codec.codecConfig.byte[0] = 40;
                curId->codec.codecConfig.byte[1] = 92;
                curId->info.codec = 0x1000;
                count += g_needMemAT3plus;
            }
            // 0130
            if ((u32)(g_edramAddr + 0x19000) < (u32)count)
                return SCE_ERROR_OUT_OF_MEMORY;
            curId->info.state = -1;
            curId++;
        }
        // 015C
        return 0;
    }
    g_edramAddr = -1;
    return sceAudiocodecReleaseEDRAM(&g_atracIds[0].codec);
}

int sceAtracGetAtracID(u32 codecType)
{
    int ret = 0x80630003;
    if (codecType < 0x1000 || codecType > 0x1001)
        return 0x80630004;
    int oldIntr = sceKernelCpuSuspendIntr();
    SceAtracId *curId = &g_atracIds[0];
    // 01EC
    int i;
    for (i = 0; i < 6; i++)
    {
        if (curId->info.codec == codecType && curId->info.state == -1) // 0228
        {
            ret = i;
            curId->info.state = 1;
            break;
        }
        // 01FC
        curId++;
    }
    // 0208
    sceKernelCpuResumeIntr(oldIntr);
    return ret;
}

int sceAtracSetHalfwayBuffer(int atracID, u8 *buffer, u32 readByte, u32 bufferByte)
{
    SceAtracFile info;
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0) {
        // 0320
        return 0x80630005;
    }
    if (bufferByte < readByte)
        return 0x80630013;
    int codec = loadWaveFile(readByte, &info, buffer);
    if (codec < 0)
        return codec;
    if (g_atracIds[atracID].info.codec != codec)
        return 0x80630007;
    // 02F4
    return setHalfwayBuffer(&g_atracIds[atracID], buffer, readByte, bufferByte, &info);
}

int sceAtracSetHalfwayBufferAndGetID(u8 *buffer, u32 readByte, u32 bufferByte)
{
    SceAtracFile info;
    if (bufferByte < readByte)
        return 0x80630013;
    int codec = loadWaveFile(readByte, &info, buffer);
    if (codec < 0)
        return codec;
    int id = sceAtracGetAtracID(codec);
    // 0388
    if (id < 0)
        return id;
    int setRet = setHalfwayBuffer(&g_atracIds[id], buffer, readByte, bufferByte, &info);
    if (setRet < 0)
    {
        if (id >= 0 && id < 6)
        {
            int oldIntr = sceKernelCpuSuspendIntr();
            if (g_atracIds[id].info.state > 0 && g_atracIds[id].info.state != 16)
                g_atracIds[id].info.state = -1;
            sceKernelCpuResumeIntr(oldIntr);
        }
        return setRet;
    }
    return id;
}

int sceAtracSetMOutHalfwayBuffer(int atracID, u8 *buffer, u32 readByte, u32 bufferByte)
{
    SceAtracFile info;
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0) {
        // 0510
        return 0x80630005;
    }
    if (bufferByte < readByte)
        return 0x80630013;
    int codec = loadWaveFile(readByte, &info, buffer);
    if (codec < 0)
        return codec;
    if (g_atracIds[atracID].info.codec != codec)
        return 0x80630007;
    // 04E4
    return setMOutHalfwayBuffer(&g_atracIds[atracID], buffer, readByte, bufferByte, &info);
}

int sceAtracSetMOutHalfwayBufferAndGetID(u8 *buffer, u32 readByte, u32 bufferByte)
{
    SceAtracFile info;
    if (bufferByte < readByte)
        return 0x80630013;
    int codec = loadWaveFile(readByte, &info, buffer);
    if (codec < 0)
        return codec;
    int id = sceAtracGetAtracID(codec);
    // 0578
    if (id < 0)
        return id;
    int ret = setMOutHalfwayBuffer(&g_atracIds[id], buffer, readByte, bufferByte, &info);
    if (ret < 0)
    {
        // 05D0
        if (id >= 0 && id < 6)
        {
            // 05E4
            int oldIntr = sceKernelCpuSuspendIntr();
            if (g_atracIds[id].info.state > 0 && g_atracIds[id].info.state != 16)
                g_atracIds[id].info.state = -1;
            // 0610
            sceKernelCpuResumeIntr(oldIntr);
        }
        return ret;
    }
    return id;
}

int sceAtracSetAA3HalfwayBufferAndGetID(u8 *buffer, u32 readByte, u32 bufferByte, u32 fileSize, int unusedFlag __attribute__((unused)))
{
    SceAtracFile info;
    if (bufferByte < readByte)
        return 0x80630013;
    int id = openAA3AndGetID(buffer, readByte, fileSize, &info);
    if (id < 0)
        return id;
    int ret = setHalfwayBuffer(&g_atracIds[id], buffer, readByte, bufferByte, &info);
    if (ret < 0)
    {
        // 06CC
        if (id >= 0 && id < 6)
        {
            // 06DC
            int oldIntr = sceKernelCpuSuspendIntr();
            if (g_atracIds[id].info.state > 0 && g_atracIds[id].info.state != 16)
                g_atracIds[id].info.state = -1;
            // 0708
            sceKernelCpuResumeIntr(oldIntr);
        }
        return ret;
    }
    return id;
}

int sceAtracDecodeData(int atracID, short *outAddr, u32 *samples, u32 *finishFlag, int *remainFrame)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0) {
        // 0830
        return 0x80630005;
    }
    int ret = isValidState(g_atracIds[atracID].info.state);
    if (ret < 0)
        return ret;
    if (((int)outAddr & 1) != 0)
        return 0x80630014;
    if (g_atracIds[atracID].info.state == 6 && g_atracIds[atracID].info.secondBufferByte == 0) // 081C
        return 0x80630012;
    // 07A4
    // 07BC
    int i;
    for (i = 0; i <= g_atracIds[atracID].info.numFrame; i++)
    {
        ret = decodeFrame(&g_atracIds[atracID], outAddr, samples, finishFlag);
        if (ret != 0) {
            *samples = 0;
            return ret;
        }
    }
    // 07E0
    *remainFrame = getRemainFrame(&g_atracIds[atracID].info);
    return 0;
}

int sceAtracGetRemainFrame(int atracID, int *remainFrame)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    int ret = isValidState(g_atracIds[atracID].info.state);
    if (ret < 0)
        return ret;
    *remainFrame = getRemainFrame(&g_atracIds[atracID].info);
    return 0;
}

int sceAtracGetStreamDataInfo(int atracID, u8 **writePtr, u32 *writableByte, u32 *readPosition)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    int ret = isValidState(g_atracIds[atracID].info.state);
    if (ret < 0)
        return ret;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    if (info->state == 2)
    {
        // 09A0
        *writableByte = 0;
        *writePtr = info->buffer;
        *readPosition = 0;
    }
    else if (info->state == 3)
    {
        u32 readPos = info->dataOff + info->streamDataByte;
        *writableByte = info->dataEnd - readPos;
        *writePtr = info->buffer + readPos;
        *readPosition = readPos;
    }
    else
    {
        *readPosition = getReadPosition(info);
        *writePtr = getWritePtr(info);
        *writableByte = getWritableByte(info);
    }
    return 0;
}

int sceAtracAddStreamData(int atracID, u32 addByte)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0) {
        // 0A78
        return 0x80630005;
    }
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    int ret = isValidState(info->state);
    if (ret < 0)
        return ret;
    if (info->state == 2)
        return 0x80630009;
    u32 remainingByte;
    if (info->state == 3)
    {
        // 0A58
        remainingByte = info->dataEnd - info->dataOff - info->streamDataByte;
        if (remainingByte == addByte)
            info->state = 2;
    }
    else
        remainingByte = getWritableByte(info);
    // (0A24)
    // 0A28
    if (remainingByte < addByte)
        return 0x80630018;
    info->streamDataByte += addByte;
    return 0;
}

int sceAtracGetSecondBufferInfo(int atracID, u32 *position, u32 *dataByte)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    int ret = isValidState(info->state);
    if (ret < 0)
        return ret;
    if (info->state != 6)
    {
        *position = 0;
        *dataByte = 0;
        return 0x80630022;
    }
    u32 pos = getSecondBufPos(info, info->loopEnd) + 1;
    *position = pos;
    *dataByte = info->dataEnd - pos;
    return 0;
}

int sceAtracSetSecondBuffer(int atracID, u8 *secondBuffer, u32 secondBufferByte)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    int ret = isValidState(info->state);
    if (ret < 0)
        return ret;
    u32 secondBufPos = getSecondBufPos(info, info->loopEnd);
    if (secondBufferByte < info->sampleSize * 3 && secondBufferByte < info->dataEnd - secondBufPos - 1)
        return 0x80630011;
    if (info->state != 6)
        return 0x80630022;
    info->secondBuffer = secondBuffer;
    info->secondBufferByte = secondBufferByte;
    info->secondBufReadOff = 0;
    return 0;
}

int sceAtracGetNextSample(int atracID, u32 *nextSample)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    int ret = isValidState(info->state);
    if (ret < 0)
        return ret;
    *nextSample = getNextSample(info);
    return 0;
}

int sceAtracGetSoundSample(int atracID, int *endSample, int *loopStartSample, int *loopEndSample)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    int ret = isValidState(info->state);
    if (ret < 0)
        return ret;
    int count = info->samplesPerChan;
    *endSample = info->endSample - count;
    if (info->loopEnd != 0)
    {
        // 0D28
        *loopStartSample = info->loopStart - count;
        *loopEndSample = info->loopEnd - count;
    }
    else {
        *loopStartSample = -1;
        *loopEndSample = -1;
    }
    // 0D04
    return 0;
}

int sceAtracGetNextDecodePosition(int atracID, u32 *samplePosition)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    int ret = isValidState(info->state);
    if (ret < 0)
        return ret;
    if (info->dataEnd - info->curOff < info->sampleSize || info->endSample < info->decodePos)
        return 0x80630024;
    *samplePosition = info->decodePos - info->samplesPerChan;
    return 0;
}

int sceAtracGetLoopStatus(int atracID, int *loopNum, u32 *loopStatus)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    int ret = isValidState(info->state);
    if (ret < 0)
        return ret;
    *loopNum = info->loopNum;
    if (info->loopEnd == 0)
        *loopStatus = 0;
    else if (info->loopNum != 0)
        *loopStatus = 1;
    else if (info->loopEnd < info->decodePos)
        *loopStatus = 0;
    else
        *loopStatus = 1;
    return 0;
}

int sceAtracSetLoopNum(int atracID, int loopNum)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    int ret = isValidState(info->state);
    if (ret < 0)
        return ret;
    if (info->loopEnd == 0 || (info->loopNum == 0 && info->loopEnd < info->decodePos))
        return 0x80630021;
    info->loopNum = loopNum;
    return 0;
}

int __attribute__((alias("sceAtracGetBufferInfoForResetting"))) sceAtracGetBufferInfoForReseting(int atracID, u32 sample, SceBufferInfo *bufferInfo);
int sceAtracGetBufferInfoForResetting(int atracID, u32 sample, SceBufferInfo *bufferInfo)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    int ret = isValidState(info->state);
    if (ret < 0)
        return ret;
    if (info->state == 6 && info->secondBufferByte == 0) // 1008
        return 0x80630012;
    // 0FB4
    if (info->endSample < sample + info->samplesPerChan)
        return 0x80630015;
    getBufferInfo(info, sample + info->samplesPerChan, bufferInfo);
    return resetId(&g_atracIds[atracID]);
}

int sceAtracResetPlayPosition(int atracID, u32 sample, u32 writeByteFirstBuf, u32 writeByteSecondBuf)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    int ret = isValidState(info->state);
    if (ret < 0)
        return ret;
    if (info->state == 6 && info->secondBufferByte == 0) // 10F0
        return 0x80630012;
    // 1090
    if (info->endSample < sample + info->samplesPerChan)
        return 0x80630015;
    ret = resetPlayPos(info, sample + info->samplesPerChan, writeByteFirstBuf, writeByteSecondBuf);
    if (ret < 0)
        return ret;
    return resetId(&g_atracIds[atracID]);
}

int sceAtracLowLevelInitDecoder(int atracID, SceAtracLowLevelInitParam *initParam)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    int ret = initDecoder(&g_atracIds[atracID], initParam);
    if (ret != 0)
        return ret;
    if (initParam->numChan == 1 && initParam->numChan == initParam->outputChan)
        return sceAudiocodec_3DD7EE1A(&g_atracIds[atracID].codec, info->codec);
    return sceAudiocodecInit(&g_atracIds[atracID].codec, info->codec);
}

int sceAtracLowLevelDecode(int atracID, void *inBuf, int *readBytes, void *outBuf, int *decodedBytes)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    if (info->state != 8)
        return 0x80630032;
    if (((int)outBuf & 1) != 0)
        return 0x80630014;
    g_atracIds[atracID].codec.inBuf = inBuf;
    g_atracIds[atracID].codec.outBuf = outBuf;
    if (info->codec == 0x1000) {
        g_atracIds[atracID].codec.param[0] = 0;
    }
    if (sceAudiocodecDecode(&g_atracIds[atracID].codec, info->codec) != 0)
        return 0x80630002;
    *readBytes = g_atracIds[atracID].codec.readSample;
    *decodedBytes = g_atracIds[atracID].codec.decodedSample;
    return 0;
}

// 1284
int setHalfwayBuffer(SceAtracId *id, u8 *buffer, u32 readByte, u32 bufferByte, SceAtracFile *info)
{
    int ret = setBuffer(id, buffer, readByte, bufferByte, info);
    if (ret < 0)
        return ret;
    ret = sceAudiocodecInit(&id->codec, id->info.codec);
    if (ret == 0)
        ret = resetId(id);
    // 12D8
    if ((id->info.state & 4) != 0) {
        // 130C
        copyBuffer(&id->info, bufferByte);
    }
    return ret;
}

// 131C
int setMOutHalfwayBuffer(SceAtracId *id, u8 *buffer, u32 readByte, u32 bufferByte, SceAtracFile *info)
{
    int ret = setBuffer(id, buffer, readByte, bufferByte, info);
    if (ret < 0)
        return ret;
    ret = 0x80630019;
    if (info->channels == 1)
    {
        // 13B4
        ret = sceAudiocodec_3DD7EE1A(&id->codec, id->info.codec);
        if (ret == 0)
            ret = resetId(id);
    }
    // 136C
    if ((id->info.state & 4) != 0) {
        // 13A4
        copyBuffer(&id->info, bufferByte);
    }
    return ret;
}

// 13D8
int resetId(SceAtracId *info)
{
    u32 finishFlag;
    // 1404
    while (info->info.numFrame != 0)
    {
        int ret = decodeFrame(info, NULL, NULL, &finishFlag);
        if (ret != 0)
            return ret;
    }
    return 0;
}

int sceAtracStartEntry(SceSize argSize __attribute__((unused)), const void *argBlock __attribute__((unused)))
{
    return (sceAtracReinit(2, 2) < 0);
}

int sceAtracEndEntry(SceSize argSize __attribute__((unused)), const void *argBlock __attribute__((unused)))
{
    if (g_edramAddr != -1)
        sceAudiocodecReleaseEDRAM(&g_atracIds[0].codec);
    return 0;
}

int sceAtracReleaseAtracID(int atracID)
{
    int ret = 0x80630005;
    if (atracID < 0 || atracID >= 6)
        return ret;
    // 14CC
    int oldIntr = sceKernelCpuSuspendIntr();
    if (g_atracIds[atracID].info.state > 0 && g_atracIds[atracID].info.state != 16) {
        g_atracIds[atracID].info.state = -1;
        ret = 0;
    }
    // 1510
    sceKernelCpuResumeIntr(oldIntr);
    return ret;
}

int sceAtracSetData(int atracID, u8 *buffer, u32 bufferByte)
{
    return sceAtracSetHalfwayBuffer(atracID, buffer, bufferByte, bufferByte);
}

int sceAtracSetDataAndGetID(u8 *buffer, u32 bufferByte)
{
    return sceAtracSetHalfwayBufferAndGetID(buffer, bufferByte, bufferByte);
}

int sceAtracSetAA3DataAndGetID(u8 *buffer, u32 bufferByte, u32 fileSize, int unusedFlag)
{
    return sceAtracSetAA3HalfwayBufferAndGetID(buffer, bufferByte, bufferByte, fileSize, unusedFlag);
}

int sceAtracIsSecondBufferNeeded(int atracID)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    int ret = isValidState(g_atracIds[atracID].info.state);
    if (ret < 0)
        return ret;
    return (g_atracIds[atracID].info.state == 6);
}

int sceAtracGetChannel(int atracID, u32 *channel)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    if (g_atracIds[atracID].info.state == 1)
        return 0x80630010;
    *channel = g_atracIds[atracID].info.numChan;
    return 0;
}

int sceAtracGetOutputChannel(int atracID, u32 *outputChan)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    if (g_atracIds[atracID].info.state == 1)
        return 0x80630010;
    *outputChan = getOutputChan(&g_atracIds[atracID].info, &g_atracIds[atracID].codec);
    return 0;
}

int sceAtracGetMaxSample(int atracID, u32 *maxSample)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    if (g_atracIds[atracID].info.state == 1)
        return 0x80630010;
    *maxSample = 0x800u >> (g_atracIds[atracID].info.codec & 1);
    return 0;
}

int sceAtracGetBitrate(int atracID, u32 *outBitrate)
{   
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    if (g_atracIds[atracID].info.state == 1)
        return 0x80630010;
    SceAtracIdInfo *info = &g_atracIds[atracID].info;
    u32 bitrate = (info->sampleSize * 352800) / 1000;
    if (info->codec == 0x1000)
    {
        // 17A0
        bitrate = ((bitrate >> 11) + 8) & 0xFFFFFFF0;
    }
    else
        bitrate = (bitrate + 511) >> 10;
    // 1790
    *outBitrate = bitrate;
    return 0;
}

int sceAtracGetInternalErrorInfo(int atracID, int *result)
{
    if (atracID < 0 || atracID >= 6 || g_atracIds[atracID].info.state <= 0)
        return 0x80630005;
    if (g_atracIds[atracID].info.state == 1)
        return 0x80630010;
    *result = g_atracIds[atracID].codec.err;
    return 0;
}

SceAtracId *_sceAtracGetContextAddress(int atracID)
{
    if (atracID < 0 || atracID >= 6)
        return 0;
    if (g_atracIds[atracID].info.state <= 0)
        return 0;
    return &g_atracIds[atracID];
}

// 182C
int allocEdram(void)
{
    g_atracIds[0].codec.codecConfig.byte[0] = 40;
    g_atracIds[0].codec.codecConfig.byte[1] = 92;
    int ret = sceAudiocodecCheckNeedMem(&g_atracIds[0].codec, 0x1000);
    if (ret < 0)
        return ret;
    g_needMemAT3plus = (g_atracIds[0].codec.neededMem + 0x3F) & 0xFFFFFFC0;
    ret = sceAudiocodecCheckNeedMem(&g_atracIds[0].codec, 0x1001);
    if (ret < 0)
        return ret;
    g_needMemAT3 = (g_atracIds[0].codec.neededMem + 0x3F) & 0xFFFFFFC0;
    g_atracIds[0].codec.neededMem = 0x19000;
    ret = sceAudiocodecGetEDRAM(&g_atracIds[0].codec, 0x1001);
    if (ret < 0)
        return ret;
    g_edramAddr = (int)g_atracIds[0].codec.edramAddr;
    return 0;
}

int openAA3AndGetID(u8 *buffer, u32 readByte, u32 fileSize, SceAtracFile *info)
{
    SceAA3File file;
    int codec = parseAA3(readByte, &file, fileSize, buffer);
    if (codec < 0)
        return codec;
    int ret = setAtracFileInfo(&file, info);
    if (ret < 0)
        return ret;
    return sceAtracGetAtracID(codec);
}

// 1938
int isValidState(int state)
{
    if (state == 8)
        return 0x80630031;
    if (state == 1)
        return 0x80630010;
    if (state == 16)
        return 0x80630040;
    return 0;
}

// 1970
int getOutputChan(SceAtracIdInfo *info, SceAudiocodecCodec *codec)
{
    if (info->codec == 0x1000) {
        // 199C
        return codec->param[6];
    }
    if (codec->codecConfig.word - 0xeU < 2)
        return codec->param[1];
    return 2;
}

int decodeFrame(SceAtracId *id, short *outAddr, u32 *samples, u32 *finishFlag)
{
    char buf[0x2000] __attribute__((aligned(64)));
    int maxFrameSamples = 0x800 >> (id->info.codec & 1);
    int nextSample = getNextSample(&id->info);
    u32 readOff = id->info.curOff;
    if (id->info.dataEnd >= id->info.sampleSize + readOff && id->info.endSample >= id->info.decodePos)
    {
        if (((id->info.state & 4) != 0 && id->info.streamDataByte < id->info.sampleSize)
         || (id->info.state == 3 && id->info.dataOff + id->info.streamDataByte < id->info.sampleSize + id->info.curOff))
        {
            *finishFlag = 0;
            return 0x80630023;
        }
        if ((id->info.state & 4) == 0) {
            id->codec.inBuf = id->info.buffer + readOff;
        } else {
            char useSecondBuf = id->info.secondBufState & 1;
            readOff = useSecondBuf ? id->info.secondBufReadOff : id->info.firstBufReadOff;
            u8 *infoBuf = useSecondBuf ? id->info.secondBuffer : id->info.buffer;
            id->codec.inBuf = infoBuf + readOff;
        }
        id->codec.outBuf = outAddr;
        if (nextSample != maxFrameSamples)
            id->codec.outBuf = buf;
        int ret = sceAudiocodecDecode(&id->codec, id->info.codec);
        if (ret != 0)
        {
            *finishFlag = 0;
            return 0x80630002;
        }
        id->info.curOff += id->info.sampleSize;
        if (id->info.numFrame == 0)
        {
            *samples = nextSample;
            if (id->info.endSample < id->info.decodePos + nextSample)
                *finishFlag = (id->info.loopNum == 0);
            else
                *finishFlag = 0;
            if (nextSample != maxFrameSamples && nextSample != 0 && outAddr != NULL) {
                int numChan = getOutputChan(&id->info, &id->codec);
                sceKernelMemcpy(outAddr, buf + ((id->info.decodePos & (0x7FFu >> (id->info.codec & 1))) << numChan), nextSample << numChan);
            }
            id->info.decodePos += nextSample;
            if (id->info.loopEnd != 0 && (id->info.loopNum != 0 || id->info.loopEnd >= id->info.decodePos) && id->info.loopEnd < id->info.decodePos)
            {
                id->info.curOff = getOffFromSample(&id->info, id->info.loopStart);
                id->info.numFrame = getFrameFromSample(&id->info, id->info.loopStart);
                id->info.decodePos = id->info.loopStart;
                if (id->info.loopNum > 0)
                    id->info.loopNum--;
            }
        }
        else
            id->info.numFrame--;
        if ((id->info.state & 4) != 0) {
            atracConsumeFrame(&id->info);
        }
        return ret;
    }
    *finishFlag = 1;
    return 0x80630024;
}

void atracConsumeFrame(SceAtracIdInfo *info)
{
    info->streamDataByte -= info->sampleSize;
    if (info->secondBufState == 1)
    {
        u32 nextOff = info->secondBufReadOff + info->sampleSize;
        if (info->secondBufferByte >= nextOff + info->sampleSize)
            info->secondBufReadOff = nextOff;
        else
        {
            info->firstBufReadOff = 0;
            info->secondBufState = 2;
            info->secondBufReadOff = 0;
        }
        return;
    }
    u32 nextReadOff = info->firstBufReadOff + info->sampleSize;
    if (info->bufferByte < nextReadOff + info->sampleSize)
        nextReadOff = 0;
    info->firstBufReadOff = nextReadOff;
    if (info->loopEnd != 0)
    {
        if (info->loopNum != 0)
            return;
        if (info->loopEnd >= info->decodePos)
            return;
    }
    // 1CC8
    if (info->secondBufState != 0 || info->state != 6)
        return;
    // 1CFC
    if (getSecondBufPos(info, info->loopEnd) >= info->curOff)
        return;
    info->secondBufState = 1;
    info->streamDataByte = info->secondBufferByte;
    info->secondBufReadOff = 0;
    sceKernelMemcpy(info->buffer, info->secondBuffer + info->secondBufferByte - (info->secondBufferByte % info->sampleSize), info->secondBufferByte % info->sampleSize);
}

#define WAVE_CHUNK_ID_DATA 0x61746164 /* "data" */
#define WAVE_CHUNK_ID_SMPL 0x6C706D73 /* "smpl" */
#define WAVE_CHUNK_ID_FACT 0x74636166 /* "fact" */
#define WAVE_CHUNK_ID_FMT  0x20746D66 /* "fmt " */
#define RIFF_MAGIC 0x46464952 /* "RIFF" */
#define WAVE_MAGIC 0x45564157 /* "WAVE" */

// 1D7C
int loadWaveFile(u32 size, SceAtracFile *info, u8 *in)
{
    u32 curOff = 0;
    info->loopEnd = -1;
    info->samplesPerChan = 0;
    info->loopStart = -1;
    info->factSz = 0;
    info->dataSize = 0;
    int fmt = 0x80630006;
    int multiChan = 0;
    // 1DEC
    for (;;)
    {
        if (size < curOff + 12)
            return 0x80630011;
        if (readWaveData(in, &curOff, 4) != RIFF_MAGIC)
            return 0x80630006;
        int cksize = readWaveData(in, &curOff, 4) - 4;
        int inc = cksize + (cksize & 1);
        if (readWaveData(in, &curOff, 4) == WAVE_MAGIC)
            break;
        if (size < curOff + inc)
            return 0x80630011;
        curOff += inc;
    }
    // 1EAC
    if (curOff + 8 >= size) {
        // 1F58 dup
        return 0x80630011;
    }
    // 1ECC
    do
    {
        int ckid = readWaveData(in, &curOff, 4);
        u32 cksize = readWaveData(in, &curOff, 4);
        cksize = cksize + (cksize & 1);
        if (size < curOff + cksize && ckid != WAVE_CHUNK_ID_DATA)
            return 0x80630011;
        switch (ckid)
        {
            case WAVE_CHUNK_ID_DATA:
                // 22A8
                info->dataSize = cksize;
                info->dataOff = curOff;
                if (info->samplesPerChan == 0)
                {
                    if (fmt != 0x1001)
                        info->samplesPerChan = 2048;
                    else
                        info->samplesPerChan = 1024;
                }
                // 22CC
                if (multiChan && fmt == 0x1000)
                {
                    info->samplesPerChan -= 184;
                    if (info->loopEnd != -1) {
                        info->loopEnd -= 184;
                        info->loopStart -= 184;
                    }
                }
                return fmt;

                // 2160
            case WAVE_CHUNK_ID_SMPL:
                // 2208
                if (info->loopStart < 0)
                {
                    if (cksize < 32)
                        return 0x80630006;
                    curOff += 28;
                    cksize -= 32;
                    if (readWaveData(in, &curOff, 4) != 0)
                    {
                        if (cksize < 20)
                            return 0x80630006;
                        curOff += 12;
                        info->loopStart = readWaveData(in, &curOff, 4);
                        info->loopEnd = readWaveData(in, &curOff, 4);
                        cksize -= 20;
                        if (info->loopStart >= info->loopEnd)
                            return 0x80630008;
                    }
                }
                break;

            case WAVE_CHUNK_ID_FACT:
                if (cksize < 4)
                    return 0x80630006;
                cksize -= 4;
                info->factSz = readWaveData(in, &curOff, 4);
                if (cksize >= 8)
                {
                    // 21D8
                    readWaveData(in, &curOff, 4);
                    multiChan = 1;
                    cksize -= 8;
                    // 21D0 dup
                    info->samplesPerChan = readWaveData(in, &curOff, 4);
                }
                else if (cksize >= 4)
                {
                    // 21BC
                    cksize -= 4;
                    // 21D0 dup
                    info->samplesPerChan = readWaveData(in, &curOff, 4);
                }
                break;

            case WAVE_CHUNK_ID_FMT:
                // 1F64
                if (fmt != (s32)0x80630006)
                    return 0x80630006;
                if (cksize < 32)
                    return 0x80630006;
                short fmtCode = readWaveData(in, &curOff, 2);
                short channels = readWaveData(in, &curOff, 2);
                info->channels = channels;
                if (((channels - 1) & 0xFFFF) >= 2)
                    return 0x80630006;
                if (readWaveData(in, &curOff, 4) != 44100)
                    return 0x80630006;
                curOff += 4; // ignore "data rate"
                short dataBlkSize = readWaveData(in, &curOff, 2);
                info->dataBlkSz = dataBlkSize;
                if ((dataBlkSize & 0xFFFF) == 0)
                    return 0x80630006;
                if (fmtCode == 0x270)
                {
                    // 20C4
                    curOff += 4; // skip bits per sample and cbSize
                    short validBitsPerSample = readWaveData(in, &curOff, 2);
                    if (validBitsPerSample != 1)
                        return 0x80630006;
                    curOff += 4; // skip channel mask
                    short jointStereo = readWaveData(in, &curOff, 2);
                    info->codecParam.half = jointStereo;
                    if ((jointStereo & 0xFFFF) != readWaveData(in, &curOff, 2))
                        return 0x80630006;
                    if (readWaveData(in, &curOff, 4) != validBitsPerSample)
                        return 0x80630006;
                    fmt = 0x1001;
                    cksize -= 32;
                }
                else
                {
                    if (fmtCode != (s16)0xFFFE) {
                        return 0x80630006;
                    }
                    if (cksize < 52)
                        return 0x80630006;
                    u8 *curIn = in + curOff + 10;
                    u8 *curGUID = g_at3PlusGUID;
                    int i;
                    for (i = 16; i != 0; i--)
                    {
                        if (*(curGUID++) != *(curIn++))
                        {
                            if (i == 0)
                                break;
                            return 0x80630006;
                        }
                    }
                    info->codecParam.bytes[0] = in[curOff + 28];
                    info->codecParam.bytes[1] = in[curOff + 29];
                    if (((info->codecParam.bytes[0] >> 2) & 7) != info->channels)
                        return 0x80630006;
                    fmt = 0x1000;
                    cksize -= 52;
                    curOff += 38;
                }
                break;

            default:
                break;
        }
        curOff += cksize;
    } while (size >= curOff && curOff + 8 < size);
    return 0x80630011;
}

int readWaveData(u8 *in, u32 *curOff, int size)
{
    u8 *curStr = in + *curOff + size;
    *curOff = *curOff + size;
    int result = 0;
    do
        result |= *(--curStr) << ((--size) * 8);
    while (size != 0);
    return result;
}

int getOffFromSample(SceAtracIdInfo *info, u32 sample)
{
    u32 max = 368;
    if (info->codec == 0x1001)
        max = 69;
    u32 frameOff = ((sample >> (0x100B - info->codec)) - 1) * info->sampleSize;
    if ((sample & (0x7FFu >> (info->codec & 1))) < max && frameOff != 0)
        frameOff -= info->sampleSize;
    return frameOff + info->dataOff;
}

u32 getSecondBufPos(SceAtracIdInfo *info, u32 sample)
{
    u32 frames = (sample >> (0x100B - info->codec)) + 1;
    return frames * info->sampleSize + info->dataOff - 1;
}

int getFrameFromSample(SceAtracIdInfo *info, u32 sample)
{
    u32 max = 368;
    if (info->codec == 0x1001)
        max = 69;
    if ((sample & (0x7FFu >> (info->codec & 1))) < max)
        return 2;
    return 1;
}

int getNextSample(SceAtracIdInfo *info)
{
    u32 mask = 0x7FFu >> (info->codec & 1);
    u32 nextOff = (info->decodePos & ~mask) + mask;
    u32 excessSamples = nextOff - info->endSample;
    if (info->endSample >= nextOff)
        excessSamples = 0;
    u32 usedSamples = (info->decodePos & mask) + excessSamples;
    u32 frameSamples = 0x800u >> (info->codec & 1);
    if (usedSamples >= frameSamples)
        return 0;
    return frameSamples - usedSamples;
}

int setBuffer(SceAtracId *id, u8 *buffer, u32 readByte, u32 bufferByte, SceAtracFile *info)
{
    id->info.numChan = info->channels;
    id->info.samplesPerChan = info->samplesPerChan + (id->info.codec == 0x1001 ? 69 : 368);
    id->info.sampleSize = info->dataBlkSz;
    setBufferInfo(&id->info, info->factSz, info->dataSize, info->samplesPerChan, info->loopStart, info->loopEnd);
    id->info.streamDataByte = readByte - info->dataOff;
    id->info.buffer = buffer;
    id->info.curOff = info->dataOff;
    id->info.dataOff = info->dataOff;
    id->info.dataEnd = info->dataSize + info->dataOff;
    id->info.secondBufState = 0;
    id->info.bufferByte = bufferByte;
    id->info.firstBufReadOff = info->dataOff;
    if (id->info.endSample >= id->info.loopEnd && (id->info.endSample >> (0x100B - id->info.codec)) * id->info.sampleSize < info->dataSize)
    {
        int ret = setBufferSize(&id->info, readByte, bufferByte);
        if (ret != 0)
            return ret;
        if (id->info.codec == 0x1001)
        {
            SceAtracModeEntry *cur = &g_at3WaveModeTable[4];
            int i;
            for (i = 0; i < 5; i++)
            {
                if (cur->frameSize == id->info.sampleSize)
                {
                    if (cur->modeId == info->codecParam.half) {
                        id->codec.codecConfig.word = cur->channels;
                        return 0;
                    }
                }
                cur--;
            }
            return 0x80630008;
        }
        id->codec.codecConfig.byte[0] = info->codecParam.bytes[0];
        id->codec.param[0] = 0;
        id->codec.codecConfig.byte[1] = info->codecParam.bytes[1];
        return ret;
    }
    return 0x80630008;
}

int initDecoder(SceAtracId *id, SceAtracLowLevelInitParam *initParam)
{
    SceAtracIdInfo *info = &id->info;
    int ret;
    if (info->codec == 0x1001)
        ret = initAT3Decoder(&id->codec, initParam);
    else
        ret = initAT3plusDecoder(&id->codec, initParam);
    if (ret < 0)
        return ret;
    info->state = 8;
    info->buffer = NULL;
    info->numChan = (u8)initParam->numChan;
    info->secondBuffer = NULL;
    info->secondBufferByte = 0;
    info->sampleSize = (u16)initParam->frameSize;
    info->decodePos = 0;
    info->endSample = 0;
    info->loopStart = 0;
    info->loopEnd = 0;
    info->samplesPerChan = 0;
    info->numFrame = 0;
    info->secondBufState = 0;
    info->dataOff = 0;
    info->curOff = 0;
    info->dataEnd = 0;
    info->loopNum = 0;
    info->streamDataByte = 0;
    info->firstBufReadOff = 0;
    info->secondBufReadOff = 0;
    info->bufferByte = 0;
    return ret;
}

int initAT3Decoder(SceAudiocodecCodec *codec, SceAtracLowLevelInitParam *initParam)
{
    int ret = isValidOpt(initParam->numChan, initParam->outputChan);
    if (ret < 0)
        return ret;
    int i;
    for (i = 4; i >= 0; i--)
    {
        if (g_at3ModeTable[i].frameSize == initParam->frameSize && g_at3ModeTable[i].channels == initParam->numChan)
        {
            codec->codecConfig.word = g_at3ModeTable[i].modeId;
            return 0;
        }
    }
    return 0x80630001;
}

int initAT3plusDecoder(SceAudiocodecCodec *codec, SceAtracLowLevelInitParam *initParam)
{
    int ret = isValidOpt(initParam->numChan, initParam->outputChan);
    if (ret < 0)
        return ret;
    int i;
    for (i = 13; i >= 0; i--)
    {
        if (g_at3PlusModeTable[i].frameSize == initParam->frameSize && g_at3PlusModeTable[i].channels == initParam->numChan)
        {
            codec->codecConfig.byte[1] = g_at3PlusModeTable[i].configHi;
            codec->codecConfig.byte[0] = g_at3PlusModeTable[i].configLo;
            codec->param[2] = 0;
            codec->codecConfig.byte[2] = 0;
            codec->codecConfig.byte[3] = 0;
            codec->codecMode.ubyte[0] = 0;
            codec->codecMode.sbyte[1] = 0;
            codec->codecMode.sbyte[2] = 0;
            codec->codecMode.sbyte[3] = 0;
            codec->param[0] = 0;
            return 0;
        }
    }
    return 0x80630001;
}

int isValidOpt(int numChan, int outputChan)
{
    if ((numChan != 2 || outputChan != 2) && (numChan != 1 || outputChan < 1 || outputChan >= 3))
        return 0x80630001;
    return 0;
}

void setBufferInfo(SceAtracIdInfo *info, u32 factSize, u32 dataSize, u32 samplesPerChan, int loopStart, int loopEnd)
{
    int add = 368;
    if (info->codec == 0x1001)
        add = 69;
    u32 shift = 0x100B - info->codec;
    int samples = (int)(samplesPerChan + add);
    if (factSize != 0) {
        factSize += samples;
    }
    else
        factSize = (dataSize / info->sampleSize) << shift;
    info->decodePos = samples;
    info->loopNum = 0;
    info->endSample = factSize - 1;
    info->numFrame = samples >> shift;
    if (loopStart < 0)
    {
        info->loopEnd = 0;
        info->loopStart = 0;
    }
    else {
        info->loopEnd = loopEnd + add;
        info->loopStart = loopStart + add;
    }
}

int setBufferSize(SceAtracIdInfo *info, u32 readByte, u32 bufferByte)
{
    if (bufferByte < info->dataEnd)
    {
        u32 minBufBytes = info->sampleSize * 3;
        if (info->streamDataByte < minBufBytes)
            return 0x80630011;
        if (info->loopEnd == 0) {
            info->state = 4;
            return 0;
        }
        if (info->loopEnd == info->endSample) {
            info->state = 5;
            return 0;
        }
        u32 loopStreamLimit = getSecondBufPos(info, info->loopEnd) - info->dataOff + 1;
        info->state = 6;
        if (loopStreamLimit < info->streamDataByte)
            info->streamDataByte = loopStreamLimit;
        info->secondBufReadOff = 0;
        info->secondBuffer = NULL;
        info->secondBufferByte = 0;
    }
    else if (readByte < info->dataEnd)
        info->state = 3;
    else
        info->state = 2;
    return 0;
}

// 2ABC
int getRemainFrame(SceAtracIdInfo *info)
{
    switch (info->state) // jump table at 0x3F1C
    {
    case 0: case 1:
        // 2B00
        return 0;

    case 2:
        // 2AF0
        return -1;

    case 3: {
        // 2B08
        if (info->curOff >= info->dataOff + info->streamDataByte)
            return 0;
        int ret = (info->dataOff + info->streamDataByte - info->curOff) / info->sampleSize - info->numFrame;
        if (ret <= 0)
            ret = 0;
        return ret;
    }

    case 4:
        // 2B4C
        return atracGetRemainFramesLinear(info);

    case 5:
        return atracGetRemainFramesLoop(info);

    case 6:
        // 2B6C
        if (info->loopEnd < info->decodePos) {
            // 2B4C dup
            return atracGetRemainFramesLinear(info);
        }
        // 2B5C dup
        return atracGetRemainFramesLoop(info);

    default:
        return 0;
    }
}

// 2B88
u8 *getWritePtr(SceAtracIdInfo *info)
{
    int count = 0;
    switch (info->state)
    {
    case 4:
        // 2BD8
        if (info->curOff + info->streamDataByte < info->dataEnd)
            count = atracAdvanceRingOffset(info->firstBufReadOff, info->bufferByte, info->streamDataByte, info->sampleSize);
        break;

    case 5:
        // 2C38/2C3C dup
        count = atracAdvanceRingOffset(info->firstBufReadOff, info->bufferByte, info->streamDataByte, info->sampleSize);
        break;

    case 6:
        // 2C0C
        if (info->loopEnd < info->decodePos && info->secondBufState == 1) {
            // 2C4C
            count = atracAdvanceRingOffset(0, info->bufferByte, atracGetFirstBufStreamBytes(info), info->sampleSize);
        }
        else {
            // 2C38/2C3C dup
            count = atracAdvanceRingOffset(info->firstBufReadOff, info->bufferByte, info->streamDataByte, info->sampleSize);
        }
    
    default:
        break;
    }
    return info->buffer + count;
}

// 2C68
u32 getReadPosition(SceAtracIdInfo *info)
{
    switch (info->state)
    {
    case 4:
        // 2CB8
        return atracAddOffsetBounded(info->dataEnd, info->curOff, info->streamDataByte, info->bufferByte);

    case 6:
        // 2CD4
        if (info->loopEnd < info->decodePos)
        {
            // 2D2C
            int ret = atracGetFirstBufStreamBytes(info);
            return atracAddOffsetBounded(info->dataEnd, info->curOff + info->streamDataByte - ret, ret, info->bufferByte);
        }
        // fall thru
    case 5:
        // 2CF0
        return atracAdvanceLoopOffset(info->curOff, info->streamDataByte, getOffFromSample(info, info->loopStart), getSecondBufPos(info, info->loopEnd), info->bufferByte);
    }
    return 0;
}

u32 getWritableByte(SceAtracIdInfo *info)
{
    u32 contiguous = atracGetContiguousWritable(info->secondBufState != 1 ? info->firstBufReadOff : 0, info->bufferByte, atracGetFirstBufStreamBytes(info), info->sampleSize);
    u32 nextWrite = atracGetNextWriteBytes(info);
    if (nextWrite >= contiguous)
        return contiguous;
    return nextWrite;
}

int atracGetRemainFramesLinear(SceAtracIdInfo *info)
{
    if (info->streamDataByte >= info->dataEnd - info->curOff)
        return -2;
    return pspMax(info->streamDataByte / info->sampleSize - info->numFrame, 0);
}

int atracGetRemainFramesLoop(SceAtracIdInfo *info)
{
    int ret = atracCountLoopFrames(info);
    if (info->loopNum < 0)
        return ret;
    u32 loopEndPos = getSecondBufPos(info, info->loopEnd);
    u32 sum = info->curOff + info->streamDataByte;
    if (sum >= loopEndPos + 1 && (s32)(sum - (loopEndPos + 1)) / (s32)(loopEndPos - getOffFromSample(info, info->loopStart) + 1) >= info->loopNum)
        ret = -3;
    return ret;
}

int atracCountLoopFrames(SceAtracIdInfo *info)
{
    u32 loopEndPos = getSecondBufPos(info, info->loopEnd);
    u32 sum = info->curOff + info->streamDataByte;
    u32 pastLoop = sum - loopEndPos - 1;
    int count;
    if (loopEndPos + 1 < sum)
    {
        u32 skipFrames = getFrameFromSample(info, info->loopStart);
        u32 loopBytes = loopEndPos - getOffFromSample(info, info->loopStart) + 1;
        count = (loopEndPos - info->curOff + 1) / info->sampleSize + (pastLoop / loopBytes) * (loopBytes / info->sampleSize - skipFrames);
        if (skipFrames * info->sampleSize < pastLoop % loopBytes)
            count += (pastLoop % loopBytes) / info->sampleSize - skipFrames;
    }
    else
        count = info->streamDataByte / info->sampleSize;
    return pspMax(count - info->numFrame, 0);
}

int atracAdvanceRingOffset(u32 readOff, u32 bufferByte, u32 streamBytes, u32 frameSize)
{
    u32 endPos = readOff + streamBytes;
    if (readOff < bufferByte)
        bufferByte = (((bufferByte - readOff) / frameSize) * frameSize) + readOff;
    if (endPos < bufferByte)
        return endPos;
    if (bufferByte >= streamBytes)
        return endPos - bufferByte;
    return 0;
}

u32 atracAddOffsetBounded(u32 dataEnd, u32 curOff, u32 streamBytes, u32 bufferByte)
{
    u32 nextOff = curOff + streamBytes;
    if (nextOff < dataEnd && streamBytes < bufferByte)
        return nextOff;
    return 0;
}

u32 atracAdvanceLoopOffset(u32 curOff, u32 streamBytes, u32 loopStartOff, u32 loopEndOff, u32 bufferByte)
{
    if (streamBytes >= bufferByte)
        return 0;
    u32 nextOff = curOff + streamBytes;
    if (nextOff < loopEndOff + 1)
        return nextOff;
    return loopStartOff + (nextOff - loopEndOff - 1) % (loopEndOff - loopStartOff + 1);
}

u32 atracGetNextWriteBytes(SceAtracIdInfo *info)
{
    char state = info->state;
    if (state == 6)
    {
        state = 5;
        if (info->loopEnd < info->decodePos)
            state = 4;
    }
    if (state == 4) {
        return atracGetRemainDataBytes(info);
    }
    if (state == 5) {
        return atracGetLoopRemainBytes(info);
    }
    return 0;
}

u32 atracGetRemainDataBytes(SceAtracIdInfo *info)
{
    if (info->curOff + info->streamDataByte < info->dataEnd)
        return info->dataEnd - (info->curOff + info->streamDataByte);
    return 0;
}

u32 atracGetLoopRemainBytes(SceAtracIdInfo *info)
{
    u32 loopEndPos = getSecondBufPos(info, info->loopEnd);
    u32 sum = info->curOff + info->streamDataByte;
    if (sum >= loopEndPos + 1)
    {
        u32 loopSpan = loopEndPos - getOffFromSample(info, info->loopStart) + 1;
        return loopSpan - (info->curOff + info->streamDataByte - loopEndPos - 1) % loopSpan;
    }
    return loopEndPos - sum + 1;
}

u32 atracGetContiguousWritable(u32 readOff, u32 bufferByte, u32 streamBytes, u32 frameSize)
{
    if (readOff < bufferByte)
        bufferByte = ((bufferByte - readOff) / frameSize) * frameSize + readOff;
    if (readOff + streamBytes < bufferByte)
        return bufferByte - (readOff + streamBytes);
    if (bufferByte >= streamBytes)
        return bufferByte - streamBytes;
    return 0;
}

u32 atracGetFirstBufStreamBytes(SceAtracIdInfo *info)
{
    if (info->loopEnd < info->decodePos && info->secondBufState == 1)
    {
        u32 secondReadOff = info->secondBufReadOff;
        u32 secondEnd = info->secondBufferByte;
        if (secondReadOff < secondEnd)
            secondEnd = ((secondEnd - secondReadOff) / info->sampleSize) * info->sampleSize + secondReadOff;
        if (secondEnd < secondReadOff)
            return 0;
        if (secondEnd - secondReadOff >= info->streamDataByte)
            return 0;
        return info->streamDataByte - (secondEnd - secondReadOff);
    }
    return info->streamDataByte;
}

u32 atracAlignBufferEnd(u32 startOff, u32 frameSize, u32 bufferByte)
{
    if (startOff >= bufferByte)
        return bufferByte;
    return ((bufferByte - startOff) / frameSize) * frameSize + startOff;
}

void copyBuffer(SceAtracIdInfo *info, u32 bufferByte)
{
    u32 bufEnd = info->bufferByte;
    u32 dataStart = info->dataOff;
    if (dataStart < bufEnd)
        bufEnd = ((bufEnd - dataStart) / info->sampleSize) * info->sampleSize + dataStart;
    u32 count = bufferByte - bufEnd;
    if (count <= 0)
        return;
    sceKernelMemcpy(info->buffer, info->buffer + bufEnd, count);
}

void getBufferInfo(SceAtracIdInfo *info, u32 sample, SceBufferInfo *bufferInfo)
{
    switch (info->state)
    {
    case 3: {
        u32 streamPos = info->dataOff + info->streamDataByte;
        u32 samplePos = info->dataOff + ((sample >> (0x100B - info->codec)) + 1) * info->sampleSize;
        bufferInfo->writePositionFirstBuf = info->buffer + streamPos;
        bufferInfo->writableByteFirstBuf = info->dataEnd - streamPos;
        bufferInfo->readPositionFirstBuf = streamPos;
        if (samplePos >= streamPos) {
            bufferInfo->minWriteByteFirstBuf = samplePos - streamPos;
        }
        else
            bufferInfo->minWriteByteFirstBuf = 0;
    }
        break;

    case 4: case 5: {
        u32 maxFirstBuf = atracAlignBufferEnd(0, info->sampleSize, info->bufferByte);
        bufferInfo->writePositionFirstBuf = info->buffer;
        u32 sampleOff = getOffFromSample(info, sample);
        bufferInfo->readPositionFirstBuf = sampleOff;
        u32 remainBytes = info->dataEnd - sampleOff;
        if (remainBytes >= maxFirstBuf)
            remainBytes = maxFirstBuf;
        bufferInfo->writableByteFirstBuf = remainBytes;
        bufferInfo->minWriteByteFirstBuf = (getFrameFromSample(info, sample) + 1) * info->sampleSize;
    }
        break;

    case 6: {
        u32 sampleOff = getOffFromSample(info, sample);
        u32 loopEndOff = getSecondBufPos(info, info->loopEnd);
        u32 maxFirstBuf = atracAlignBufferEnd(0, info->sampleSize, info->bufferByte);
        u32 minFrameBytes = (getFrameFromSample(info, sample) + 1) * info->sampleSize;
        if (sampleOff < loopEndOff)
        {
            u32 diff = loopEndOff - sampleOff + 1;
            if (diff < maxFirstBuf)
                maxFirstBuf = diff;
            if (diff >= minFrameBytes)
                diff = minFrameBytes;
            bufferInfo->writePositionFirstBuf = info->buffer;
            bufferInfo->writableByteFirstBuf = maxFirstBuf;
            bufferInfo->minWriteByteFirstBuf = diff;
            bufferInfo->readPositionFirstBuf = sampleOff;
            break;
        }
        u32 secondEndOff = loopEndOff + atracAlignBufferEnd(0, info->sampleSize, info->secondBufferByte);
        if (sampleOff >= secondEndOff)
        {
            bufferInfo->readPositionFirstBuf = sampleOff;
            bufferInfo->minWriteByteFirstBuf = minFrameBytes;
            secondEndOff = info->dataEnd - sampleOff;
            if (secondEndOff >= maxFirstBuf)
                secondEndOff = maxFirstBuf;
            bufferInfo->writePositionFirstBuf = info->buffer;
            bufferInfo->writableByteFirstBuf = secondEndOff;
            break;
        }
        else if (loopEndOff + info->secondBufferByte + 1 < info->dataEnd)
        {
            secondEndOff++;
            bufferInfo->readPositionFirstBuf = secondEndOff;
            sampleOff += minFrameBytes;
            u32 remainBytes = info->dataEnd - secondEndOff;
            if (remainBytes >= maxFirstBuf)
                remainBytes = maxFirstBuf;
            bufferInfo->writePositionFirstBuf = info->buffer;
            bufferInfo->writableByteFirstBuf = remainBytes;
            if (sampleOff >= secondEndOff) {
                bufferInfo->minWriteByteFirstBuf = sampleOff - secondEndOff;
            }
            else
                bufferInfo->minWriteByteFirstBuf = 0;
            break;
        }
    }
        /* fallthrough */

    default:
        bufferInfo->writableByteFirstBuf = 0;
        bufferInfo->writePositionFirstBuf = info->buffer;
        bufferInfo->minWriteByteFirstBuf = 0;
        bufferInfo->readPositionFirstBuf = 0;
        break;
    }
    bufferInfo->readPositionSecondBuf = 0;
    bufferInfo->writePositionSecondBuf = info->buffer;
    bufferInfo->writableByteSecondBuf = 0;
    bufferInfo->minWriteByteSecondBuf = 0;
}

int resetPlayPos(SceAtracIdInfo *info, u32 sample, u32 writeByteFirstBuf, u32 writeByteSecondBuf)
{
    SceBufferInfo bufferInfo;
    getBufferInfo(info, sample, &bufferInfo);
    if (writeByteFirstBuf < bufferInfo.minWriteByteFirstBuf || bufferInfo.writableByteFirstBuf < writeByteFirstBuf) {
        return 0x80630016;
    }
    if (writeByteSecondBuf < bufferInfo.minWriteByteSecondBuf || bufferInfo.writableByteSecondBuf < writeByteSecondBuf) {
        return 0x80630017;
    }
    info->decodePos = sample;
    info->numFrame = getFrameFromSample(info, sample);
    info->loopNum = 0;
    info->curOff = getOffFromSample(info, sample);
    switch (info->state)
    {
    case 6: {
        u32 sampleOff = getOffFromSample(info, sample);
        u32 loopEndOff = getSecondBufPos(info, info->loopEnd);
        if (sampleOff < loopEndOff)
        {
            info->streamDataByte = writeByteFirstBuf;
            info->secondBufState = 0;
            info->firstBufReadOff = 0;
        }
        else
        {
            u32 secondEndOff = loopEndOff + atracAlignBufferEnd(0, info->sampleSize, info->secondBufferByte);
            if (sampleOff >= secondEndOff)
            {
                info->streamDataByte = writeByteFirstBuf;
                info->secondBufState = 2;
                info->firstBufReadOff = 0;
            }
            else
            {
                info->streamDataByte = secondEndOff - sampleOff + writeByteFirstBuf + 1;
                info->secondBufState = 1;
                info->secondBufReadOff = sampleOff - loopEndOff - 1;
            }
        }
    }
        break;

    case 4: case 5:
        info->streamDataByte = writeByteFirstBuf;
        info->secondBufState = 0;
        info->firstBufReadOff = 0;
        break;

    case 3:
        info->secondBufState = 0;
        info->streamDataByte += writeByteFirstBuf;
        if (info->dataOff + info->streamDataByte >= info->dataEnd)
            info->state = 2;
        break;
    }
    return 0;
}

int parseAA3(u32 readByte, SceAA3File *aa3, u32 fileSize, u8 *buffer)
{
    u32 curOff = 0;
    if (readByte < 3)
        return 0x80631004;
    int magic = aa3ReadU24BE(buffer, &curOff, readByte);
    if (magic != 0x656133 && magic != 0x494433)
    {
        aa3->loopStartSample = 0;
        aa3->dataOff = 0;
    }
    else
    {
        if (readByte - 3 < curOff)
            return 0x80631004;
        if (buffer[curOff] != 3 || buffer[curOff + 1] != 0)
            return 0x80631002;
        curOff = curOff + 3;
        aa3->loopStartSample = aa3ReadSyncSafeU32(buffer, &curOff, readByte);
        if (aa3->loopStartSample == -1)
            return 0x80631003;
        curOff += aa3->loopStartSample;
        if (readByte - 1 < curOff)
            return 0x80631004;
        if (buffer[curOff] == 0)
            curOff += 16;
        aa3->dataOff = curOff;
    }
    if (readByte - 34 < curOff)
        return 0x80631004;
    if (aa3ReadU24BE(buffer, &curOff, readByte) != 0x454133)
        return 0x80631003;
    curOff++;
    aa3->loopEndSample = aa3ReadU16BE(buffer, &curOff, readByte);
    if (aa3->loopEndSample == -1)
        return 0x80631003;
    if (aa3ReadU16BE(buffer, &curOff, readByte) != 0xFFFF)
        return 0x80631003;
    curOff += 24;
    aa3->codingMode = buffer[curOff];
    int codec;
    if (buffer[curOff] == 0)
        codec = 0x1001;
    else if (buffer[curOff] == 1)
        codec = 0x1000;
    else
        return 0x80631004;
    aa3->sampleRate = aa3ReadU32BE(buffer, &curOff, readByte);
    int sum = aa3->dataOff + aa3->loopEndSample;
    aa3->dataSize = fileSize - sum;
    aa3->dataOff = sum;
    return codec;
}

int setAtracFileInfo(SceAA3File *aa3, SceAtracFile *info)
{
    info->loopEnd = -1;
    info->loopStart = -1;
    info->dataSize = aa3->dataSize;
    info->dataOff = aa3->dataOff;
    if (aa3->codingMode != 0)
    {
        if (aa3->codingMode != 1)
            return 0;
        info->samplesPerChan = 2048;
        if ((aa3->sampleRate & 0x1C00) != 0x800)
            return 0x80631005;
        info->channels = 2;
        if ((aa3->sampleRate & 0xE000) != 0x2000)
            return 0x80631005;
        info->factSz = 0;
        info->dataBlkSz = ((aa3->sampleRate & 0x3FF) << 3) + 8;
        info->codecParam.bytes[0] = ((aa3->sampleRate >> 8) & 3) | 0x28;
        info->codecParam.bytes[1] = aa3->sampleRate & 0xFF;
    }
    else
    {
        info->samplesPerChan = 1024;
        info->channels = 2;
        if ((aa3->sampleRate & 0xE000) != 0x2000)
            return 0x80631005;
        info->dataBlkSz = (aa3->sampleRate & 0x3FF) << 3;
        if ((aa3->sampleRate & 0x20000) == 0) {
            info->codecParam.half = 0;
        }
        else
            info->codecParam.half = 1;
        info->factSz = 0;
    }
    return 0;
}

int aa3ReadU32BE(u8 *buffer, u32 *curOff, u32 readByte)
{
    int result = 0;
    if (readByte < *curOff + 4)
    {
        int i;
        for (i = 0; i < 4; i++)
        {
            u8 byteVal = 0;
            if (readByte >= *curOff + i + 1)
                byteVal = buffer[*curOff + i];
            result |= byteVal << (i * 8);
        }
        *curOff = readByte;
    }
    else
    {
        result = (buffer[*curOff + 0] << 24)
               | (buffer[*curOff + 1] << 16)
               | (buffer[*curOff + 2] <<  8)
               | (buffer[*curOff + 3] <<  0);
        *curOff += 4;
    }
    return result;
}

u16 aa3ReadU16BE(u8 *buffer, u32 *curOff, u32 readByte)
{
    u16 result = 0;
    if (readByte < *curOff + 2)
    {
        int i;
        for (i = 0; i < 2; i++)
        {
            u8 byteVal = 0;
            if (readByte >= *curOff + i + 1)
                byteVal = buffer[*curOff + i];
            result |= byteVal << (i * 8);
        }
        *curOff = readByte;
    }
    else {
        result = (buffer[*curOff + 0] << 8) | buffer[*curOff + 1];
        *curOff += 2;
    }
    return result;
}

int aa3ReadU24BE(u8 *buffer, u32 *curOff, u32 readByte)
{
    if (readByte < *curOff + 3)
        return 0xFF000000;
    int result = (buffer[*curOff] << 16) | (buffer[*curOff + 1] << 8) | buffer[*curOff + 2];
    *curOff += 3;
    return result;
}

int aa3ReadSyncSafeU32(u8 *buffer, u32 *curOff, u32 readByte)
{
    if (readByte < *curOff + 4)
        return -1;
    int result = ((buffer[*curOff + 0] & 0x7F) << 21)
               | ((buffer[*curOff + 1] & 0x7F) << 14)
               | ((buffer[*curOff + 2] & 0x7F) <<  7)
               | ((buffer[*curOff + 3] & 0x7F) <<  0);
    *curOff += 4;
    return result;
}
