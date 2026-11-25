#include <common_imp.h>
#include <avcodec_audiocodec.h>
#include <sysmem_utils_kernel.h>

SCE_MODULE_INFO("sceAudiocodec_Driver", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD, 1, 3);
SCE_SDK_VERSION(0x06060110);

void *sceMeMalloc(u32 size);
void sceMeFree(void *ptr);
s32 sceMeAudio_driver_6AD33F60(s32 codec, SceAudiocodecCodec *info);
s32 sceMeAudio_driver_81956A0B(s32 codec, SceAudiocodecCodec *info);
s32 sceMeAudio_driver_9A9E21EE(s32 codec, SceAudiocodecCodec *info);
s32 sceMeAudio_driver_B57F033A(s32 codec, SceAudiocodecCodec *info);
s32 sceMeAudio_driver_C300D466(s32 codec, s32 opt, SceAudiocodecCodec *info);

#define ADDR_IS_RAM(addr) (((0x00220202 >> (((u32)(addr) >> 27) & 0x1F)) & 1) != 0)

s32 g_retValues[] = { 0x807F00FC, 0x807F00FD, 0x80000002, 0x807F00FF, 0x807F0001 };

u8 g_numSamples[][15] = {
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14 },
    { 0x00, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0A, 0x0C,
      0x0E, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x28 }
};

u16 g_freqs[][4] = {
    { 22050, 24000, 16000, 0 },
    { 44100, 48000, 32000, 0 },
    { 11025, 12000, 8000, 0 }
};

static s32 getBufSize(SceAudiocodecCodec *info, s32 codec);
static s32 getOutBufSize(SceAudiocodecCodec *info, s32 codec);
static s32 calcMp3FrameSize(s32 type, s32 layer, s32 bitrateIdx, s32 freqIdx, s32 *sizePtr);

s32 sceAudiocodecCheckNeedMem(SceAudiocodecCodec *info, s32 codec)
{
    if (!ADDR_IS_RAM(info))
        return 0x807F0002;
    if (codec < 0x1000 || codec >= 0x1006)
        return SCE_ERROR_NOT_SUPPORTED;
    s32 oldK1 = pspShiftK1();
    s32 ret = SCE_ERROR_PRIV_REQUIRED;
    if (pspK1StaBufOk(info, 104)) {
        info->version = 0x05100601;
        s32 id = sceMeAudio_driver_81956A0B(codec, info);
        ret = 0;
        if (id != 0) {
            if ((u32)id < (u32)-4)
                id = 0;
            ret = g_retValues[id + 4];
        }
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceAudiocodecInit(SceAudiocodecCodec *info, s32 codec)
{
    if (!ADDR_IS_RAM(info))
        return 0x807F0002;
    if (codec < 0x1000 || codec >= 0x1006)
        return SCE_ERROR_NOT_SUPPORTED;
    s32 oldK1 = pspShiftK1();
    if (!pspK1StaBufOk(info, 104) || !pspK1DynBufOk(info->edramAddr, info->neededMem)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    if (codec == 0x1002)
        info->param[2] = 9999;
    if (ADDR_IS_RAM(info->edramAddr))
        sceKernelDcacheWritebackInvalidateRange(info->edramAddr, info->neededMem);
    info->version = 0x05100601;
    s32 id = sceMeAudio_driver_6AD33F60(codec, info);
    s32 ret = 0;
    if (id != 0) {
        if ((u32)id < (u32)-4)
            id = 0;
        ret = g_retValues[id + 4];
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceAudiocodec_3DD7EE1A(SceAudiocodecCodec *info, s32 codec)
{
    if (!ADDR_IS_RAM(info))
        return 0x807F0002;
    if (codec < 0x1000 || codec >= 0x1006)
        return SCE_ERROR_NOT_SUPPORTED;
    s32 oldK1 = pspShiftK1();
    if (!pspK1StaBufOk(info, 104) || !pspK1DynBufOk(info->edramAddr, info->neededMem)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    if (codec == 0x1002)
        info->param[2] = 9999;
    if (ADDR_IS_RAM(info->edramAddr))
        sceKernelDcacheWritebackInvalidateRange(info->edramAddr, info->neededMem);
    info->version = 0x05100601;
    s32 id = sceMeAudio_driver_B57F033A(codec, info);
    s32 ret = 0;
    if (id != 0) {
        if ((u32)id < (u32)-4)
            id = 0;
        ret = g_retValues[id + 4];
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceAudiocodecDecode(SceAudiocodecCodec *info, s32 codec)
{
    if (!ADDR_IS_RAM(info))
        return 0x807F0002;
    if (codec < 0x1000 || codec >= 0x1006)
        return SCE_ERROR_NOT_SUPPORTED;
    s32 oldK1 = pspShiftK1();
    if (!pspK1StaBufOk(info, 104)
     || !pspK1DynBufOk(info->edramAddr, info->neededMem)
     || !pspK1StaBufOk(info->inBuf, 0x10000)
     || !pspK1StaBufOk(info->outBuf, 0x10000)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    s32 size;
    if (codec == 0x1002 || codec == 0x1004) {
        size = info->param0.word;
    } else {
        size = getBufSize(info, codec);
        if (size < 0) {
            pspSetK1(oldK1);
            return size;
        }
    }
    sceKernelDcacheWritebackRange(info->inBuf, size);
    size = getOutBufSize(info, codec);
    if (size < 0) {
        pspSetK1(oldK1);
        return size;
    }
    sceKernelDcacheWritebackInvalidateRange(info->outBuf, size);
    s32 id = sceMeAudio_driver_9A9E21EE(codec, info);
    s32 ret = 0;
    if (id != 0) {
        if ((u32)id < (u32)-4)
            id = 0;
        ret = g_retValues[id + 4];
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceAudiocodecGetInfo(SceAudiocodecCodec *info, s32 codec)
{
    if (!ADDR_IS_RAM(info))
        return 0x807F0002;
    if (codec < 0x1000 || codec >= 0x1006)
        return SCE_ERROR_NOT_SUPPORTED;
    s32 oldK1 = pspShiftK1();
    if (!pspK1StaBufOk(info, 104) || !pspK1DynBufOk(info->edramAddr, info->neededMem)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    if ((codec == 0x1002 || codec == 0x1004) && !pspK1DynBufOk(info->inBuf, info->param0.word)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    s32 opt;
    switch (codec) {
    case 0x1002:
    case 0x1004:
        opt = 2;
        break;
    case 0x1000:
        opt = 4;
        break;
    default:
        pspSetK1(oldK1);
        return 0;
    }
    info->version = 0x05100601;
    s32 id = sceMeAudio_driver_C300D466(codec, opt, info);
    s32 ret = 0;
    if (id != 0) {
        if ((u32)id < (u32)-4)
            id = 0;
        ret = g_retValues[id + 4];
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceAudiocodecAlcExtendParameter(SceAudiocodecCodec *info, s32 codec, s32 *sizeOut)
{
    if (!ADDR_IS_RAM(info))
        return 0x807F0002;
    if (codec < 0x1000 || codec >= 0x1006)
        return SCE_ERROR_NOT_SUPPORTED;
    s32 oldK1 = pspShiftK1();
    if (!pspK1StaBufOk(info, 104) || !pspK1StaBufOk(sizeOut, 4)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    s32 size = getOutBufSize(info, codec);
    if (size < 0) {
        pspSetK1(oldK1);
        return size;
    }
    *sizeOut = size;
    pspSetK1(oldK1);
    return 0;
}

s32 sceAudiocodecGetEDRAM(SceAudiocodecCodec *info, s32 codec)
{
    if (!ADDR_IS_RAM(info))
        return 0x807F0002;
    if (codec < 0x1000 || codec >= 0x1006)
        return SCE_ERROR_NOT_SUPPORTED;
    s32 oldK1 = pspShiftK1();
    s32 ret = 0;
    if (!pspK1StaBufOk(info, 108)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    void *alloc = sceMeMalloc((info->neededMem + 0x3F) | 0x3F);
    info->allocMem = alloc;
    if (alloc == NULL || ((u32)alloc & 0x1FFFFFFF) > 0x003FFFFF) {
        ret = 0x807F0003;
        sceMeFree(info->allocMem);
        info->allocMem = NULL;
        info->edramAddr = 0;
    } else {
        info->edramAddr = (void *)(((u32)alloc + 0x3F) & 0xFFFFFFC0);
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceAudiocodecReleaseEDRAM(SceAudiocodecCodec *info)
{
    s32 oldK1 = pspShiftK1();
    if (!pspK1StaBufOk(info, 108) || !pspK1DynBufOk(info->edramAddr, info->neededMem)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    if (info->allocMem == NULL || ((u32)info->allocMem & 0x1FFFFFFF) > 0x3FFFFF) {
        pspSetK1(oldK1);
        return 0x807F0004;
    }
    sceMeFree(info->allocMem);
    info->allocMem = NULL;
    info->edramAddr = 0;
    pspSetK1(oldK1);
    return 0;
}

static s32 getBufSize(SceAudiocodecCodec *info, s32 codec)
{
    if (codec < 0x1000 || codec >= 0x1006)
        return SCE_ERROR_NOT_SUPPORTED;
    switch (codec) {
    case 0x1000:
        if (info->param[0] == 0)
            return info->param[4] + 2;
        return 0x100A;
    case 0x1001:
        return info->param[0] * 2 + 1;
    case 0x1002:
        if (info->param[2] == 9999) {
            s32 ret = sceAudiocodecGetInfo(info, 0x1002);
            if (ret < 0)
                return ret;
        }
        return calcMp3FrameSize(info->param[2], info->param[3], info->param[5], info->param[6], &info->param0.word);
    case 0x1003:
        if (info->param1.bytes.b0 == 0)
            return 0x600;
        return 0x609;
    case 0x1004:
        return calcMp3FrameSize(info->param[0], info->param[1], info->param[3], info->param[4], &info->param0.word);
    case 0x1005:
        return 0x420D;
    }
    return SCE_ERROR_NOT_SUPPORTED;
}

static s32 getOutBufSize(SceAudiocodecCodec *info, s32 codec)
{
    if (codec < 0x1000 || codec >= 0x1006)
        return 0x1000;
    switch (codec) {
    case 0x1000:
        if (info->param[2] == 1 && info->param[6] != info->param[2])
            return 0x2000;
        return info->param[6] << 12;
    case 0x1001:
        return 0x1000;
    case 0x1002:
        if (info->param[2] == 9999) {
            s32 ret = sceAudiocodecGetInfo(info, 0x1002);
            if (ret < 0)
                return ret;
        }
        if (info->param[2] == 1)
            return 0x1200;
        return 0x900;
    case 0x1003:
        if ((u8)info->param1.bytes.b1 == 0)
            return 0x1000;
        return 0x2000;
    case 0x1004:
        return 0x1200;
    case 0x1005:
        return 0x2F00;
    }
    return 0x1000;
}

static s32 calcMp3FrameSize(s32 type, s32 layer, s32 bitrateIdx, s32 freqIdx, s32 *sizePtr)
{
    u32 samples = 0;
    if ((u32)type < 3 && bitrateIdx >= 0 && bitrateIdx < 15)
        samples = (u32)g_numSamples[type & 1][bitrateIdx] << 3;
    u32 freq = 0;
    s32 size = 0x7C9;
    if ((u32)type < 3 && freqIdx >= 0 && freqIdx < 4)
        freq = g_freqs[type][freqIdx];
    if (samples != 0 && freq != 0 && (u32)(layer - 1) < 2) {
        size = ((samples * 144) / freq) + 1;
        if (size > 0x7C9)
            size = 0x7C9;
    }
    if (type != 1)
        size >>= 1;
    *sizePtr = size;
    return size;
}
