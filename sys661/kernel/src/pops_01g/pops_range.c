#define POPS_NO_GP_REG
#include "pops_internal.h"

/* Range-coded LZ77 block decompressor for compressed PSISOIMG disc chunks (0x00ce4c).
 * Uses $gp as a range register in Sony's build, so compiled without the global $28 binding. */
static __inline__ u32 rangeRenormAndDecodeBit(u32 *rangePtr, u32 *codePtr,
                                              const u8 **inPtr, u8 *probPtr)
{
    u32 range = *rangePtr;
    u32 code = *codePtr;
    u32 prob = *probPtr;
    u32 bound;
    u32 nextProb;

    if ((range >> 24) == 0U) {
        code = (code << 8) + (*inPtr)[5];
        (*inPtr)++;
        bound = range * prob;
        range <<= 8;
    } else {
        bound = (range >> 8) * prob;
    }

    nextProb = prob - (prob >> 3);
    if (code < bound) {
        *probPtr = (u8)(nextProb + 31U);
        *rangePtr = bound;
        *codePtr = code;
        return 1U;
    }

    *probPtr = (u8)nextProb;
    *rangePtr = range - bound;
    *codePtr = code - bound;
    return 0U;
}

static __inline__ u32 readBe32(const u8 *bytes)
{
    return ((u32)bytes[0] << 24) | ((u32)bytes[1] << 16) |
           ((u32)bytes[2] << 8) | (u32)bytes[3];
}

s32 popsRangeDecodeBlock(s32 dstSize, const u8 *src, u8 *dst)
{
    u8 model[5312];
    u8 *outCur = dst - 1;
    u8 *outEnd = dst + dstSize - 1;
    u8 *stateBase = model + 2656;
    const u8 *inCur = src;
    u32 prevByte = 0U;
    u32 range = 0xFFFFFFFFU;
    u32 code;
    s32 header;
    u32 initProb;
    u32 initWord;
    u32 literalShift;
    u32 idx;
    u32 *modelWords;

    __builtin_allegrex_cache(0x18, dst);

    header = (s8)inCur[0];
    code = readBe32(&inCur[1]);
    if (header < 0) {
        return (s32)0x80000108U;
    }

    initProb = (u8)(0x80U - 16U * (((u32)header >> 3) & 3U));
    initWord = initProb | (initProb << 8);
    initWord |= initWord << 16;
    literalShift = (u32)header & 7U;

    modelWords = (u32 *)(model + 2656);
    for (idx = 0U; idx < 664U; idx++) {
        modelWords[idx] = initWord;
    }

    while (1) {
        u32 isMatch;
        outCur++;
        isMatch = rangeRenormAndDecodeBit(&range, &code, &inCur, stateBase + 2336);

        if (!isMatch) {
            u32 ctx = (((prevByte & 0xFFFFF8FFU) | ((((u32)(uintptr_t)outCur) & 7U) << 8)) >> literalShift) & 7U;
            u8 *litProbs = model + 2656 + (255U * ctx);
            u32 sym = 1U;

            if (stateBase > model + 2656) {
                stateBase--;
            }

            do {
                u32 bit = rangeRenormAndDecodeBit(&range, &code, &inCur, &litProbs[sym - 1U]);
                sym = (sym << 1) | bit;
            } while ((sym >> 8) == 0U);

            prevByte = sym;
            *outCur = (u8)sym;
            if (outCur == outEnd) {
                return (s32)(outCur - dst);
            }
        } else {
            s32 lenClass = -1;
            u32 matchLen;
            u32 distSlot;
            u32 matchDist;
            u8 *lenModel;
            u8 *distBase;
            u8 *copyDstEnd;
            u8 *copySrc;

            while (lenClass < 6) {
                u32 nextBit;
                stateBase += 8;
                nextBit = rangeRenormAndDecodeBit(&range, &code, &inCur, stateBase + 2336);
                if (!nextBit) {
                    break;
                }
                lenClass++;
            }

            if (lenClass < 0) {
                matchLen = 2U;
                lenClass = -1;
            } else {
                s32 extraBits = lenClass - 3;
                u32 accum = 1U;
                u32 lowOffset = ((u32)lenClass * 32U) | ((((u32)(uintptr_t)outCur << lenClass) & 3U) * 8U) | ((u32)(uintptr_t)stateBase & 7U);
                lenModel = model + 2656 + lowOffset;

                if (extraBits >= 0) {
                    u32 bit = rangeRenormAndDecodeBit(&range, &code, &inCur, &lenModel[2424]);
                    accum = 2U + bit;
                    if (extraBits > 0) {
                        u32 bitTwo = rangeRenormAndDecodeBit(&range, &code, &inCur, &lenModel[2424]);
                        accum = (accum << 1) | bitTwo;
                        while (extraBits > 1) {
                            if ((range >> 24) == 0U) {
                                code = (code << 8) + inCur[5];
                                inCur++;
                                range <<= 7;
                            } else {
                                range >>= 1;
                            }
                            accum <<= 1;
                            if (code < range) {
                                accum |= 1U;
                            } else {
                                code -= range;
                            }
                            extraBits--;
                        }
                    }
                }

                matchLen = (accum << 1) | rangeRenormAndDecodeBit(&range, &code, &inCur, &lenModel[2400]);
                if (lenClass > 0) {
                    matchLen = (matchLen << 1) | rangeRenormAndDecodeBit(&range, &code, &inCur, &lenModel[2408]);
                    if (lenClass > 1) {
                        matchLen = (matchLen << 1) | rangeRenormAndDecodeBit(&range, &code, &inCur, &lenModel[2416]);
                    }
                }
            }

            {
                u32 treeLimit = (matchLen == 3U) ? 64U : 128U;
                u8 *treeBase = (matchLen == 3U) ? (model + 2656 + lenClass) : (model + 2712 + lenClass);
                u32 treeNode = 8U;

                while (treeNode < treeLimit) {
                    u32 bit = rangeRenormAndDecodeBit(&range, &code, &inCur, &treeBase[treeNode + 2033]);
                    treeNode = (treeNode << 1) + (bit ? 8U : 0U);
                }

                if (treeNode == treeLimit) {
                    if (outCur == dst) {
                        return (s32)0x80000108U;
                    }
                    matchDist = 0U;
                } else {
                    s32 distClass = (s32)((treeNode - treeLimit) >> 3);
                    s32 distExtra = distClass - 3;
                    u32 distAcc = 1U;
                    distBase = model + 2656 + (treeNode - treeLimit) + 2216;

                    if (distExtra >= 0) {
                        u32 bit = rangeRenormAndDecodeBit(&range, &code, &inCur, &distBase[3]);
                        distAcc = 2U + bit;
                        if (distExtra > 0) {
                            u32 bitTwo = rangeRenormAndDecodeBit(&range, &code, &inCur, &distBase[3]);
                            distAcc = (distAcc << 1) | bitTwo;
                            while (distExtra > 1) {
                                if ((range >> 24) == 0U) {
                                    code = (code << 8) + inCur[5];
                                    inCur++;
                                    range <<= 7;
                                } else {
                                    range >>= 1;
                                }
                                distAcc <<= 1;
                                if (code < range) {
                                    distAcc |= 1U;
                                } else {
                                    code -= range;
                                }
                                distExtra--;
                            }
                        }
                    }

                    distSlot = (distAcc << 1) | rangeRenormAndDecodeBit(&range, &code, &inCur, &distBase[0]);
                    if (distClass > 0) {
                        distSlot = (distSlot << 1) | rangeRenormAndDecodeBit(&range, &code, &inCur, &distBase[1]);
                        if (distClass > 1) {
                            distSlot = (distSlot << 1) | rangeRenormAndDecodeBit(&range, &code, &inCur, &distBase[2]);
                        }
                    }
                    matchDist = distSlot - 1U;
                    if (matchDist >= (u32)(outCur - dst)) {
                        return (s32)0x80000108U;
                    }
                }
            }

            copyDstEnd = outCur + matchLen;
            copySrc = outCur - matchDist;
            if (copyDstEnd > outEnd) {
                return -1;
            }
            if (copyDstEnd <= outEnd - 64) {
                __builtin_allegrex_cache(0x18, outCur + 63);
            }

            stateBase = (u8 *)(((uintptr_t)(model + 2662)) | ((uintptr_t)copyDstEnd & 1U));
            while (1) {
                prevByte = copySrc[-1];
                *outCur = (u8)prevByte;
                if (outCur == copyDstEnd) {
                    break;
                }
                outCur++;
                copySrc++;
            }

            if (outCur == outEnd) {
                return (s32)(outCur - dst);
            }
        }
    }
}
