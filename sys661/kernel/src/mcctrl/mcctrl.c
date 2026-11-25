/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

/**
 * mcctrl.c
 *    Reverse engineered mcctrl module of the SCE PSP system.
 * Author: _Felix_
 * Version: 6.60
 */

#include <common_imp.h>

#include "openpsid_ddrdb.h"

SCE_MODULE_INFO("sceMcctrl", 0x5006, 1, 1);
SCE_SDK_VERSION(SDK_VERSION);

static int encryptAndMacBlock(u8 *plainBuf, u8 *cipherBuf, int size);
static int verifyAndDecryptBlock(u8 *cipherBuf, u8 *plainBuf, int size);
static int computeMcctrlMac(u8 *buf, int size, u8 *macOut);
static int readConsoleCert(u8 *destBuf, u32 len);
static void mcctrlInitStub(int unusedA, int unusedB);
static int fillRandomBytes(u8 *prngBuf, u32 len);
static int ecdsaGenKeyPair(u8 *keyPairBuf);
static int ecdsaMultiply(u8 sharedSecret[40], u8 peerPubKey[40], u8 privKey[20]);
static int computeSha1(u8 *srcBuf, int size, u8 *hash);
static int verifyConsoleCert(u8 certBuf[184]);
static int verifyEcdsaSignature(u8 *sigBuf, u8 *pubKeyBuf, u8 *msgBuf, int size);
static int signEcdsaSignature(u8 *sigOut, u8 *privKeyBuf, u8 *msgBuf, int size);
static int encryptBuffer(u8 *srcBuf, u8 *encryBuf, u32 len);
static int decryptBuffer(u8 *srcBuf, u8 *decryBuf, u32 size);

static int g_initialized;
static u8 g_sessionBuf[288];
static u8 g_workBuf[632];
static u8 g_hashBuf[208];
static u8 g_macBuf[84];

static const u8 g_mcctrlMacKey[64] = {
    0xAA, 0xCD, 0x21, 0x20, 0x4E, 0xDA, 0x4F, 0xB6,
    0xDA, 0xFE, 0x91, 0x88, 0xCD, 0xA8, 0xC6, 0x3B,
    0x8D, 0x4F, 0xB3, 0x9E, 0xF8, 0x69, 0xF7, 0x12,
    0x08, 0x85, 0x10, 0x4F, 0xDF, 0x72, 0xFB, 0x5C,
    0xEC, 0x60, 0x44, 0x0C, 0xD6, 0x2A, 0x22, 0x3F,
    0xF3, 0x7F, 0xE2, 0x52, 0x40, 0x0F, 0xD0, 0x75,
    0x2B, 0x49, 0x9C, 0x3B, 0x0A, 0x6A, 0x82, 0x30,
    0x44, 0x6B, 0x4D, 0xCE, 0xC2, 0x2A, 0xD1, 0x20,
};

static int encryptAndMacBlock(u8 *plainBuf, u8 *cipherBuf, int size) {
    int status;

    if (size & 0xF) {
        return 0x80530002;
    }
    status = encryptBuffer(plainBuf, cipherBuf, size - 16);
    if (status < 0) {
        return 0x80530003;
    }
    status = computeMcctrlMac(cipherBuf, size - 16, (cipherBuf + size) - 16);
    return status;
}

static int verifyAndDecryptBlock(u8 *cipherBuf, u8 *plainBuf, int size) {
    int status;
    int i;
    u8 expectedMac[16];
    u8 macByte;
    u8 calcByte;
    u8 diff = 0;

    if (size & 0xF) {
        return 0x80530002;
    }
    status = computeMcctrlMac(cipherBuf, size - 16, expectedMac);
    if (status >= 0) {
        for (i = 0; i < 16; i++) {
            macByte = cipherBuf[size - 16 + i];
            calcByte = expectedMac[i];
            if (macByte != calcByte) {
                diff = macByte - calcByte;
                break;
            }
        }
        status = 0x80530006;
        if (diff == 0) {
            status = decryptBuffer(cipherBuf, plainBuf, size - 16);
            status = (status < 0) ? 0x80530004 : status;
        }
    }
    fillRandomBytes(expectedMac, 16);
    return status;
}

static int computeMcctrlMac(u8 *buf, int size, u8 *macOut) {
    int i;
    int ret;

    if (computeSha1(buf, size, &g_macBuf[64]) < 0) {
        fillRandomBytes(g_macBuf, sizeof(g_macBuf));
        return 0x80530006;
    }

    for (i = 0; i < 64; i++) {
        g_macBuf[i] = g_mcctrlMacKey[i];
    }

    ret = computeSha1(g_macBuf, sizeof(g_macBuf), g_macBuf);
    if (ret < 0) {
        fillRandomBytes(g_macBuf, sizeof(g_macBuf));
        return 0x80530006;
    }

    for (i = 0; i < 16; i++) {
        macOut[i] = g_macBuf[i];
    }

    fillRandomBytes(g_macBuf, sizeof(g_macBuf));
    return ret;
}

static int readConsoleCert(u8 *destBuf, u32 len) {
    int keyVal;

    if (len >= 184) {
        len = 184;
    }
    keyVal = sceIdStorageLookup(256, 56, destBuf, len);
    return (keyVal > 0) ? 0 : keyVal;
}

static void mcctrlInitStub(int unusedA, int unusedB) {
    (void)unusedA;
    (void)unusedB;
    return;
}

static int fillRandomBytes(u8 *prngBuf, u32 len) {
    u32 i;
    u8 buf[20];
    int status = 0;

    for (i = 0; i < len; i++) {
        u32 idx = i % 20;
        if (idx == 0) {
            status = sceDdrdbPrngen(buf);
            if (status < 0) {
                return status;
            }
        }
        prngBuf[i] = buf[idx];
    }
    return status;
}

static int ecdsaGenKeyPair(u8 *keyPairBuf) {
    int status;

    status = sceDdrdbMul1(keyPairBuf);
    return status;
}

static int ecdsaMultiply(u8 sharedSecret[40], u8 peerPubKey[40], u8 privKey[20]) {
    int status;

    status = sceDdrdbMul2(privKey, peerPubKey, sharedSecret);
    return status;
}

static int computeSha1(u8 *srcBuf, int size, u8 *hash) {
    int status;

    status = sceDdrdbHash(srcBuf, size, hash);
    return status;
}

static int verifyConsoleCert(u8 certBuf[184]) {
    int status;

    status = sceDdrdbCertvry(certBuf);
    return status;
}

static int verifyEcdsaSignature(u8 *sigBuf, u8 *pubKeyBuf, u8 *msgBuf, int size) {
    int status;
    u8 hash[20];

    status = sceDdrdbHash(msgBuf, size, hash);
    if (status >= 0) {
        status = sceDdrdbSigvry(pubKeyBuf, hash, sigBuf);
    }
    return status;
}

static int signEcdsaSignature(u8 *sigOut, u8 *privKeyBuf, u8 *msgBuf, int size) {
    int status;
    u8 hash[20];

    status = sceDdrdbHash(msgBuf, size, hash);
    if (status >= 0) {
        status = sceDdrdbSiggen(privKeyBuf, hash, sigOut);
    }
    return status;
}

static int encryptBuffer(u8 *srcBuf, u8 *encryBuf, u32 len) {
    u32 i;
    for (i = 0; i < len; i++) {
        encryBuf[i] = srcBuf[i];
    }
    return sceDdrdbEncrypt(encryBuf, len);
}

static int decryptBuffer(u8 *srcBuf, u8 *decryBuf, u32 size) {
    u32 i;
    for (i = 0; i < size; i++) {
        decryBuf[i] = srcBuf[i];
    }
    return sceDdrdbDecrypt(decryBuf, size);
}

int sceMcctrl_3EF531DB(void) {
    if (g_initialized == 0) {
        mcctrlInitStub(0, 0);
        g_initialized = 1;
    }
    return 0;
}

int sceMcctrl_877CD3A5(void) {
    g_initialized = 0;
    return 0;
}

int sceMcctrl_1EDFD6BB(u8 *sessionOut, u8 *challengeOut) {
    int status;
    int i;
    int retVal;

    status = 0x80530001;
    if (g_initialized != 0) {
        for (i = 0; i < 176; i++) {
            g_workBuf[i] = 0;
        }
        for (i = 0; i < (int)sizeof(g_sessionBuf); i++) {
            g_sessionBuf[i] = 0;
        }

        retVal = readConsoleCert(&g_sessionBuf[76], 184);
        status = 0x80530012;

        if (retVal == 0) {
            retVal = verifyConsoleCert(&g_sessionBuf[76]);
            status = 0x80530013;

            if (retVal == 0) {
                retVal = ecdsaGenKeyPair(&g_sessionBuf[16]);
                status = 0x80530010;

                if (retVal == 0) {
                    for (i = 0; i < 40; i++) {
                        g_workBuf[16 + i] = g_sessionBuf[36 + i];
                    }
                    for (i = 0; i < 96; i++) {
                        g_workBuf[56 + i] = g_sessionBuf[76 + i];
                    }
                    fillRandomBytes(g_workBuf, 16);
                    retVal = encryptAndMacBlock(g_workBuf, challengeOut, 176);
                    status = retVal;
                    if (retVal == 0) {
                        fillRandomBytes(g_sessionBuf, 16);
                        status = encryptAndMacBlock(g_sessionBuf, sessionOut, 288);
                    }
                }
            }
        }
    }
    fillRandomBytes(g_sessionBuf, 288);
    fillRandomBytes(g_workBuf, 632);
    retVal = status & 0xFFFF0000;
    if (retVal == 0x80530000) {
        status |= 0x80530300;
    }
    return status;
}

int sceMcctrl_7CAC25B2(u8 *sessionBuf, u8 *peerPacket, u8 *clientId, u8 *clientPayload, u8 *clientNonce, u8 *responseOut) {
    int status;
    int retVal;
    int i;
    char diff = 0;

    status = 0x80530001;
    if (g_initialized != 0) {
        for (i = 0; i < 304; i++) {
            g_workBuf[328 + i] = 0;
        }
        status = verifyAndDecryptBlock(peerPacket, &g_workBuf[40], 288);
        if (status == 0) {
            status = verifyAndDecryptBlock(sessionBuf, g_sessionBuf, 288);

            if (status == 0) {
                retVal = verifyConsoleCert(&g_sessionBuf[76]);
                status = 0x80530013;

                if (retVal == 0) {
                    for (i = 0; i < 56; i++) {
                        g_hashBuf[i] = g_workBuf[212 + i];
                    }
                    retVal = verifyEcdsaSignature(&g_workBuf[40 + 228], &g_sessionBuf[172], g_hashBuf, 56);
                    status = 0x80530019;

                    if (retVal == 0) {
                        for (i = 0; i < 96; i++) {
                            g_hashBuf[i] = g_workBuf[56 + i];
                        }
                        retVal = verifyEcdsaSignature(&g_workBuf[40 + 112], &g_workBuf[40 + 188], g_hashBuf, 96);
                        status = 0x8053001A;

                        if (retVal == 0) {
                            for (i = 0; i < 40; i++) {
                                if (g_workBuf[40 + 56 + i] != g_sessionBuf[36 + i]) {
                                    diff = g_workBuf[40 + 56 + i] - g_sessionBuf[36 + i];
                                    break;
                                }
                            }
                            status = 0x80530029;
                            if (diff == 0) {
                                for (i = 0; i < 16; i++) {
                                    if (g_workBuf[40 + 96 + i] != g_sessionBuf[76 + i]) {
                                        diff = g_workBuf[40 + 96 + i] - g_sessionBuf[76 + i];
                                        break;
                                    }
                                }
                                status = 0x80530028;
                                if (diff == 0) {
                                    retVal = ecdsaMultiply(g_workBuf, &g_workBuf[56], &g_sessionBuf[16]);
                                    status = 0x80530011;
                                    if (retVal == 0) {
                                        for (i = 0; i < 20; i++) {
                                            g_hashBuf[i] = g_workBuf[i];
                                        }
                                        for (i = 0; i < 96; i++) {
                                            g_hashBuf[20 + i] = g_workBuf[56 + i];
                                        }
                                        retVal = computeSha1(g_hashBuf, 116, g_hashBuf);
                                        status = 0x80530020;
                                        if (retVal == 0) {
                                            for (i = 0; i < 20; i++) {
                                                if (g_workBuf[40 + 152 + i] != g_hashBuf[i]) {
                                                    diff = g_workBuf[40 + 152 + i] - g_hashBuf[i];
                                                    break;
                                                }
                                            }
                                            status = 0x80530021;
                                            if (diff == 0) {
                                                for (i = 0; i < 40; i++) {
                                                    g_workBuf[328 + 16 + i] = g_sessionBuf[36 + i];
                                                }
                                                for (i = 0; i < 40; i++) {
                                                    g_workBuf[328 + 56 + i] = g_workBuf[56 + i];
                                                }
                                                for (i = 0; i < 16; i++) {
                                                    g_workBuf[328 + 96 + i] = g_workBuf[212 + i];
                                                }
                                                for (i = 0; i < 32; i++) {
                                                    g_workBuf[328 + 172 + i] = clientId[i];
                                                }
                                                for (i = 0; i < 64; i++) {
                                                    g_workBuf[328 + 204 + i] = clientPayload[i];
                                                }
                                                for (i = 0; i < 16; i++) {
                                                    g_workBuf[328 + 268 + i] = clientNonce[i];
                                                }
                                                for (i = 0; i < 96; i++) {
                                                    g_hashBuf[i] = g_workBuf[328 + 16 + i];
                                                }
                                                for (i = 0; i < 32; i++) {
                                                    g_hashBuf[96 + i] = g_workBuf[328 + 172 + i];
                                                }
                                                for (i = 0; i < 64; i++) {
                                                    g_hashBuf[96 + 32 + i] = g_workBuf[328 + 204 + i];
                                                }
                                                for (i = 0; i < 16; i++) {
                                                    g_hashBuf[96 + 32 + 64 + i] = g_workBuf[328 + 268 + i];
                                                }
                                                retVal = signEcdsaSignature(&g_workBuf[328 + 112], &g_sessionBuf[212], g_hashBuf, 208);
                                                status = 0x80530018;
                                                if (retVal == 0) {
                                                    for (i = 0; i < 20; i++) {
                                                        g_hashBuf[i] = g_workBuf[i];
                                                    }
                                                    for (i = 0; i < 96; i++) {
                                                        g_hashBuf[20 + i] = g_workBuf[328 + 16 + i];
                                                    }
                                                    retVal = computeSha1(g_hashBuf, 116, g_hashBuf);
                                                    status = 0x80530020;
                                                    if (retVal == 0) {
                                                        for (i = 0; i < 20; i++) {
                                                            g_workBuf[328 + 152 + i] = g_hashBuf[i];
                                                        }
                                                        for (i = 0; i < 288; i++) {
                                                            g_sessionBuf[i] = 0;
                                                        }
                                                        for (i = 0; i < 20; i++) {
                                                            g_sessionBuf[16 + i] = g_workBuf[i];
                                                        }
                                                        fillRandomBytes(&g_workBuf[328], 16);
                                                        retVal = encryptAndMacBlock(&g_workBuf[328], responseOut, 304);
                                                        status = retVal;
                                                        if (retVal == 0) {
                                                            fillRandomBytes(g_sessionBuf, 16);
                                                            retVal = encryptAndMacBlock(g_sessionBuf, sessionBuf, 288);
                                                            status = retVal;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    fillRandomBytes(g_sessionBuf, 288);
    fillRandomBytes(g_workBuf, 632);
    fillRandomBytes(g_hashBuf, 208);

    retVal = status & 0xFFFF0000;
    if (retVal == 0x80530000) {
        status |= 0x80530300;
    }
    return status;
}

int sceMcctrl_9618EE57(u8 *sessionBuf, u8 *headerBuf, int maxSize) {
    int status;
    int i;
    u32 decodedLen;
    u32 byteHighMid;
    u16 byteLowMid;
    u8 byteLow;

    status = 0x80530001;
    if (g_initialized != 0) {
        status = verifyAndDecryptBlock(sessionBuf, g_sessionBuf, 288);

        if (status == 0) {
            for (i = 0; i < 4; i++) {
                g_workBuf[i] = g_sessionBuf[32 + i] ^ headerBuf[i];
            }
            decodedLen = g_workBuf[0];
            byteHighMid = g_workBuf[1];
            byteLowMid = g_workBuf[2];

            decodedLen = decodedLen << 24;
            byteHighMid = byteHighMid << 16;

            byteLow = g_workBuf[3];
            decodedLen |= byteHighMid;
            byteLowMid = byteLowMid << 8;

            decodedLen |= byteLowMid;
            decodedLen |= byteLow;

            status = 0x80530004;
            if ((u32)maxSize >= decodedLen) {
                status = decodedLen + 15;
                status &= 0xFFFFFFF0;
            }
        }
    }
    fillRandomBytes(g_sessionBuf, 288);
    fillRandomBytes(g_workBuf, 632);
    decodedLen = status & 0xFFFF0000;
    if (decodedLen == 0x80530000) {
        status |= 0x80530300;
    }
    return status;
}

int sceMcctrl_61550814(u8 *sessionBuf, u8 *packetBuf, int size, u8 *outBuf, int outBufSize) {
    int status;
    int i;
    u32 decodedLen;
    u32 byteHighMid;
    u16 byteLowMid;
    u8 byteLow;
    u32 alignedLen;
    int offset;
    u32 keyIdx;

    status = 0x80530001;
    if (g_initialized != 0) {
        status = verifyAndDecryptBlock(sessionBuf, g_sessionBuf, 288);
        if (status == 0) {
            for (i = 0; i < 4; i++) {
                g_workBuf[i] = packetBuf[i] ^ g_sessionBuf[32 + i];
            }
            decodedLen = g_workBuf[0];
            byteHighMid = g_workBuf[1];
            byteLowMid = g_workBuf[2];

            decodedLen = decodedLen << 24;
            byteHighMid = byteHighMid << 16;

            byteLow = g_workBuf[3];

            decodedLen |= byteHighMid;
            byteLowMid = byteLowMid << 8;
            decodedLen |= byteLowMid;
            decodedLen |= byteLow;

            status = 0x80530002;
            alignedLen = decodedLen + 15;
            alignedLen &= 0xFFFFFFF0;
            if ((u32)outBufSize >= alignedLen) {
                status = verifyAndDecryptBlock(packetBuf + 4, outBuf, size - 4);
                if (status == 0) {
                    if ((u32)status < decodedLen) {
                        offset = 0;
                        do {
                            keyIdx = offset >> 31;
                            keyIdx = keyIdx >> 28;
                            keyIdx += offset;
                            keyIdx &= 0xFFFFFFF0;
                            keyIdx = offset - keyIdx;

                            outBuf[offset] = outBuf[offset] ^ g_sessionBuf[16 + keyIdx];
                            if (keyIdx == 15) {
                                status = encryptBuffer(&g_sessionBuf[16], &g_sessionBuf[16], 16);
                                if (status < 0) {
                                    status = 0x80530004;
                                    break;
                                }
                            }
                            offset++;
                        } while ((u32)offset < decodedLen);
                        if (status >= 0) {
                            status = decodedLen;
                        }
                    } else {
                        status = decodedLen;
                    }
                }
            }
        }
    }
    fillRandomBytes(g_sessionBuf, 288);
    fillRandomBytes(g_workBuf, 632);
    if ((status & 0xFFFF0000) == 0x80530000) {
        status |= 0x80530300;
    }
    return status;
}

int module_start(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    return 0;
}

int module_stop(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    return 0;
}
