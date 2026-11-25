/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

/**
 * mcctrl.c
 *    Reverse engineered mcctrl module of the SCE PSP system.
 * Author: _Felix_
 * Version: 6.60
 * 
 */

/* TODO: Reverse the sceDdrdb_driver library functions in openpsid.prx.*/

#include <common_imp.h>

#include "openpsid_ddrdb.h"

SCE_MODULE_INFO("sceMcctrl", 0x5006, 1, 1);
SCE_SDK_VERSION(SDK_VERSION);

//static functions
static int encryptAndMacBlock(u8 *arg1, u8 *arg2, int size);
static int verifyAndDecryptBlock(u8 *arg1, u8 *arg2, int size);
static int computeMcctrlMac(u8 *buf, int size, u8 *arg2);
static int readConsoleCert(u8 *destBuf, u32 len);
static void mcctrlInitStub(int, int); 
static int fillRandomBytes(u8 *prngBuf, u32 len);
static int ecdsaGenKeyPair(u8 *buf);
static int ecdsaMultiply(u8 buf0[40], u8 buf1[40], u8 buf2[20]);
static int computeSha1(u8 *srcBuf, int size, u8 *hash);
static int verifyConsoleCert(u8 buf[184]);
static int verifyEcdsaSignature(u8 *buf, u8 *srcBuf1, u8 *srcBuf2, int size);
static int signEcdsaSignature(u8 *destBuf, u8 *srcBuf1, u8 *srcBuf2, int size);
static int encryptBuffer(u8 *srcBuf, u8 *encryBuf, u32 len);
static int decryptBuffer(u8 *srcBuf, u8 *decryBuf, u32 size);


//global variables
static int g_initialized;
static u8 g_sessionBuf[288];
static u8 g_workBuf[632];
static u8 g_hashBuf[208];
static u8 g_macBuf[84];

/* Subroutine encryptAndMacBlock - Address 0x00000BC0 */
static int encryptAndMacBlock(u8 *arg1, u8 *arg2, int size) {
    int status;
    
    if (size & 0xF) { //0x00000BC8 & 0x00000BD4 & 0x00000BEC
        return 0x80530002; //0x00000BCC & 0x00000BF0 & 0x00000C28 -- invalid size
    }
    status = encryptBuffer(arg1, arg2, size-16); //0x00000BF4
    if (status < 0) { //0x00000BFC
        return 0x80530003; //0x00000C34 & 0x00000C3C
    }
    status = computeMcctrlMac(arg2, size-16, (arg2+size)-16); //0x00000C0C
    return status;
}

/* Subroutine verifyAndDecryptBlock - Address 0x00000C40 */
//                        srcBuf,    decryBuf
static int verifyAndDecryptBlock(u8 *arg1, u8 *arg2, int size) {
    int status;
    int i;
    u8 buf[16];
    u8 tempVal1;
    u8 tempVal2;
    u8 val = 0;
    
    if (size & 0xF) { //0x00000C48 & 0x00000C70
        return 0x80530002; //0x00000C44 & 0x00000C50
    }
    status = computeMcctrlMac(arg1, size-16, buf); //0x00000C80
    if (status >= 0) { //0x00000C88   
        //0x00000CA0 - 0x00000CC4
        for (i = 0; i < 16; i++) {
             tempVal1 = *(u8 *)((arg1 + size)-16+i); //0x00000CA4
             tempVal2 = buf[i]; //0x00000CA8
             if (tempVal1 != tempVal2) { //0x00000CB8
                 val = tempVal1 - tempVal2; //0x00000D30
                 break; 
             }         
        }
        status = 0x80530006; //0x00000CC8 & 0x00000CD0
        if (val == 0) { //0x00000CCC
            status = decryptBuffer(arg1, arg2, size-16); //0x00000CDC
            status = (status < 0) ? 0x80530004 : status; //0x00000CE8
        }
    }
    fillRandomBytes(buf, 16); //0x00000D00
    return status;
}

/* Subroutine computeMcctrlMac - Address 0x00000D34 */
//Thanks to Artart for RE'ing it.
static int computeMcctrlMac(u8 *buf, int size, u8 *arg2) {
    int i;
    int ret;
    char *data;
    
    if (computeSha1(buf, size, &g_macBuf[64]) < 0) {
        fillRandomBytes(g_macBuf, 84);
        return 0x80530006;
    }
    
    asm(".set noreorder\n \
        lui %0, %%hi(asmData)\n \
        addiu %0, %0, %%lo(asmData)\n \
        b next\n \
        .size asmData, 68\n \
         asmData:\n \
        .word 0x2021CDAA\n \
        .word 0xB64FDA4E\n \
        .word 0x8891FEDA\n \
        .word 0x3BC6A8CD\n \
        .word 0x9EB34F8D\n \
        .word 0x12F769F8\n \
        .word 0x4F108508\n \
        .word 0x5CFB72DF\n \
        .word 0x0C4460EC\n \
        .word 0x3F222AD6\n \
        .word 0x52E27FF3\n \
        .word 0x75D00F40\n \
        .word 0x3B9C492B\n \
        .word 0x30826A0A\n \
        .word 0xCE4D6B44\n \
        .word 0x20D12AC2\n \
        .word 0x00000000\n \
    next:" : "=r" (data));
    
    for (i = 0; i < 64; i++) {
         g_macBuf[i] = data[i];
    }
    
    ret = computeSha1(g_macBuf, sizeof(g_macBuf), g_macBuf);
    if (ret < 0) {
        fillRandomBytes(g_macBuf, sizeof(g_macBuf));
        return 0x80530006;
    }
    
    for (i = 0; i < 16; i++) {
         arg2[i] = g_macBuf[i];
    }
    
    fillRandomBytes(g_macBuf, sizeof(g_macBuf));
    return ret;
}

/** 
 * Subroutine readConsoleCert - Address 0x00000E70
 * 
 * Retrieves the value associated with key 256.
 * 
 * @param destBuf Buffer with enough storage to retrieve the key. 
 * @param len Amount of data to retrieve. Set to 0 - 184.
 * 
 * @return 0 on success, otherwise < 0.
 */
static int readConsoleCert(u8 *destBuf, u32 len) {
    int keyVal;

    if (len >= 184) {
        len = 184;
    }
    keyVal = sceIdStorageLookup(256, 56, destBuf, len);
    return (keyVal > 0) ? 0 : keyVal;
}

/* Subroutine mcctrlInitStub - Address 0x00000EAC */
static void mcctrlInitStub(int arg1, int arg2) {
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

/* Subroutine ecdsaGenKeyPair - Address 0x00000F68 */
static int ecdsaGenKeyPair(u8 *buf) {
    int status;
    
    status = sceDdrdbMul1(buf);
    return status;
}

/* Subroutine ecdsaMultiply - Address 0x00000F84 */
static int ecdsaMultiply(u8 buf0[40], u8 buf1[40], u8 buf2[20]) {
    int status;
    
    status = sceDdrdbMul2(buf2, buf1, buf0);
    return status;
}

/** 
 * Subroutine computeSha1 - Address 0x00000FA8
 * 
 * Fill a buffer with the SHA-1 hash of a source buffer.
 * 
 * @param srcBuf The source buffer. 
 * @param size The length of the srcBuf (the bytes taken into account).
 * @param hash The destination buffer of the SHA-1 hash of the source buffer. Has to be big enough.
 * 
 * @return 0 on success, otherwise < 0.
 */
static int computeSha1(u8 *srcBuf, int size, u8 *hash) {
    int status;
    
    status = sceDdrdbHash(srcBuf, size, hash);
    return status;
}

/** 
 * Subroutine verifyConsoleCert - Address 0x00000FC4
 * 
 * Verifies a certification?
 * 
 * @param buf The source buffer holding the verification? 
 * 
 * @return 0 on success, otherwise < 0.
 */
static int verifyConsoleCert(u8 buf[184]) {
    int status; 
    
    status = sceDdrdbCertvry(buf); //0x00000FCC
    return status;
}

/* Subroutine verifyEcdsaSignature - Address 0x00000FE0 */
static int verifyEcdsaSignature(u8 *buf, u8 *srcBuf1, u8 *srcBuf2, int size) {
    int status;
    u8 hash[20];
    
    status = sceDdrdbHash(srcBuf2, size, hash); //0x00001000
    if (status >= 0) { //0x00001014
        status = sceDdrdbSigvry(srcBuf1, hash, buf); //0x0000101C
    }
    return status;
}

/* Subroutine signEcdsaSignature - Address 0x00001040  */
static int signEcdsaSignature(u8 *destBuf, u8 *srcBuf1, u8 *srcBuf2, int size) {
    int status;
    u8 hash[20];
    
    status = sceDdrdbHash(srcBuf2, size, hash); //0x00001060
    if (status >= 0) { //0x00001074
        status = sceDdrdbSiggen(srcBuf1, hash, destBuf); //0x0000107C
    }
    return status;
}

/** 
 * Subroutine encryptBuffer - Address 0x000010A0
 * 
 * Encrypts a buffer.
 * 
 * @param srcBuf The source buffer to encrypt.
 * @param encryBuf The buffer receiving the encrypted data of the source buffer. 
 * @param len The amount of bytes of the source buffer to encrypt.
 * 
 * @return 0 on success, otherwise < 0.
 */
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

/* 
 * Subroutine sceMcctrl_3EF531DB - Address 0x00000010 
 * Exported in sceMcctrl
 */
int sceMcctrl_3EF531DB() {
    if (g_initialized == 0) { //0x00000028
        mcctrlInitStub(0, 0); //0x00000044
        g_initialized = 1; //0x00000054:
    }
    return 0; //0x00000038
}

/* Subroutine sceMcctrl_877CD3A5 - Address 0x00000058 
 * Exported in sceMcctrl 
 */
int sceMcctrl_877CD3A5() {
    g_initialized = 0;
    return 0; //0x0000005C:
}

/*
 * Subroutine sceMcctrl_1EDFD6BB - Address 0x00000068 
 * Exported in sceMcctrl
 */
int sceMcctrl_1EDFD6BB(u8 *arg1, u8 *arg2) {
    int status;
    int i;
    int retVal;
    
    status = 0x80530001; //0x000000A4 & 0x000000B8
    if (g_initialized != 0) { //0x000000B4
        //0x000000BC - 0x000000D0
        for (i = 0; i < 176; i++) {
             g_workBuf[i] = 0; //0x000000D4
        }
        //0x000000D8 - 0x000000EC 
        for (i = 0; i < sizeof g_sessionBuf; i++) {
             g_sessionBuf[i] = 0; //0x000000F0
        }
        
        // TODO: certificate placed at &g_sessionBuf[76]
        retVal = readConsoleCert(&g_sessionBuf[76], 184); //0x000000F8 - 0x00000100
        status = 0x80530012; //0x00000104 & 0x0000010C
        
        if (retVal == 0) { //0x00000108
            // Verify placed certificate
            retVal = verifyConsoleCert(&g_sessionBuf[76]); //0x00000114
            status = 0x80530013; //0x00000118 & 0x00000120
            
            if (retVal == 0) { //0x0000011C
                // Generate (public, private)-key pair
                retVal = ecdsaGenKeyPair(&g_sessionBuf[16]); //0x00000124
                status = 0x80530010; //0x0000012C & 0x00000140
                
                if (retVal == 0) { //0x0000013C
                    //0x00000130 & 0x00000134 & 0x00000138 & 0x00000144 - 0x00000154
                    for (i = 0; i < 40; i++) {
                        /* Copy public key into g_workBuf[16+i]. */
                         g_workBuf[16+i] = g_sessionBuf[36+i]; //0x00000148 & 0x0000015C
                    }
                    //0x00000160 - 0x00000180
                    for (i = 0; i < 96; i++) {
                        // Copy 96 bytes of certificate into &g_workBuf[56]
                         g_workBuf[56+i] = g_sessionBuf[76+i]; //0x00000170 & 0x00000184
                    }
                    // Copy a 20 byte PRN into first 16 bytes of g_13e4
                    fillRandomBytes(g_workBuf, 16); //0x0000018C
                    retVal = encryptAndMacBlock(g_workBuf, arg2, 176); //0x0000019C
                    status = retVal; //0x000001A8
                    if (retVal == 0) { //0x000001A4
                        // Copy a 20 byte PRN into first 16 bytes of g_12c4
                        fillRandomBytes(g_sessionBuf, 16); //0x000001B0
                        status = encryptAndMacBlock(g_sessionBuf, arg1, 288); //0x000001C0
                    }                    
                }
            }
        }  
    }
    fillRandomBytes(g_sessionBuf, 288); //0x000001D0
    fillRandomBytes(g_workBuf, 632); //0x000001E0
    retVal = status & 0xFFFF0000; //0x000001E8
    if (retVal == 0x80530000) { //0x000001F0
        status |= 0x80530300; //0x000001F4 & 0x0000022C
    }
    return status; //0x000001F8
}

/*
 * Subroutine sceMcctrl_7CAC25B2 - Address 0x00000230
 * Exported in sceMcctrl
 */
int sceMcctrl_7CAC25B2(u8 *arg1, u8 *arg2, u8 *arg3, u8 *arg4, u8 *arg5, u8 *arg6) {
    int status;
    int retVal;
    int i;
    char val = 0;
    
    status = 0x80530001; //0x00000268 & 0x0000026C
    if (g_initialized != 0) { //0x00000278 & 0x00000298
        //0x000002A0 - 0x000002B8
        for (i = 0; i < 304; i++) {
             g_workBuf[328+i] = 0;
        }
        status = verifyAndDecryptBlock(arg2, &g_workBuf[40], 288); //0x000002C0      
        if (status == 0) { //0x000002C8
            status = verifyAndDecryptBlock(arg1, g_sessionBuf, 288); //0x000002D8
            
            if (status == 0) { //0x000002E0
                retVal = verifyConsoleCert(&g_sessionBuf[76]); //0x000002F0 -- &g_sessionBuf[76] == 0x00001310
                status  = 0x80530013; // 0x000002F4 & 0x000002FC
                
                if (retVal == 0) { //0x000002F8
                    //0x00000300 - 0x0000032C
                    for (i = 0; i < 56; i++) {
                         g_hashBuf[i] = g_workBuf[212+i]; //0x0000032C -- &g_workBuf[212] == 0x000014B8
                    }
                    retVal = verifyEcdsaSignature(&g_workBuf[40+228], &g_sessionBuf[172], g_hashBuf, 56); //0x00000348
                    status = 0x80530019; //0x0000034C & 0x00000354
                    
                    if (retVal == 0) { //0x00000350
                        //0x00000358 - 0x00000380
                        for (i = 0; i < 96; i++) {
                             g_hashBuf[i] =  g_workBuf[56+i]; //0x00000380 -- &g_workBuf[56] == 0x0000141C
                        }
                        retVal = verifyEcdsaSignature(&g_workBuf[40+112], &g_workBuf[40+188], g_hashBuf, 96); //0x00000398
                        status = 0x8053001A; //0x0000039C & 0x000003A4
                        
                        if (retVal == 0) { //0x000003A0
                            //0x000003AC - 0x000003DC
                            for (i = 0; i < 40; i++) {
                                 //&g_sessionBuf[36] == 0x000012E8
                                 if (g_workBuf[40+56+i] != g_sessionBuf[36+i]) { //0x000003D0
                                     val = g_workBuf[40+56+i] - g_sessionBuf[36+i]; //0x0000089C
                                     break;
                                 }
                            }
                            status = 0x80530029; //0x000003E0 & 0x000003E8
                            if (val == 0) { //0x000003E4
                                //0x000003EC - 0x00000420
                                for (i = 0; i < 16; i++) {
                                     //&g_sessionBuf[76] == 0x00001310
                                     if (g_workBuf[40+96+i] != g_sessionBuf[76+i]) { //0x00000414
                                         val = g_workBuf[40+96+i] - g_sessionBuf[76+i]; //0x00000894
                                         break;
                                     }
                                }
                                status = 0x80530028; //0x00000424 & 0x0000042C
                                if (val == 0) { //0x00000428
                                    //&g_workBuf[56] == 0x0000141C && &g_sessionBuf[16] == 0x000012D4
                                    retVal = ecdsaMultiply(g_workBuf, &g_workBuf[56], &g_sessionBuf[16]); //0x00000444
                                    status = 0x80530011; //0x00000448 & 0x00000450
                                    if (retVal == 0) { //0x0000044C
                                        //0x00000454 - 0x00000480
                                        for (i = 0; i < 20; i++) {
                                             g_hashBuf[i] = g_workBuf[i]; //0x00000480
                                        }
                                        //0x00000484 - 0x000004AC
                                        for (i = 0; i < 96; i++) {
                                             g_hashBuf[20+i] = g_workBuf[56+i]; //0x000004AC -- //&g_workBuf[56] == 0x0000141C
                                        }
                                        retVal = computeSha1(g_hashBuf, 116, g_hashBuf); //0x000004C0
                                        status = 0x80530020; //0x0000087C & 0x00000884
                                        if (retVal == 0) { //0x000004C8
                                            //0x000004CC - 0x000004FC
                                            for (i = 0; i < 20; i++) {
                                                 if (g_workBuf[40+152+i] != g_hashBuf[i]) { //0x000004F0
                                                     val = g_workBuf[40+152+i] - g_hashBuf[i]; //0x0000088C
                                                     break;
                                                 }
                                            }
                                            status = 0x80530021; //0x00000500 & 0x00000508
                                            if (val == 0) { //0x00000504
                                                //0x0000050C - 0x00000538
                                                for (i = 0; i < 40; i++) {
                                                     g_workBuf[328+16+i] = g_sessionBuf[36+i]; //0x00000538 -- &g_sessionBuf[36] == 0x000012E8
                                                } 
                                                //0x0000053C - 0x00000564
                                                for (i = 0; i < 40; i++) {
                                                     g_workBuf[328+56+i] = g_workBuf[56+i]; //0x00000564 -- &g_workBuf[56] == 0x0000141C
                                                }
                                                //0x00000568 - 0x00000590
                                                for (i = 0; i < 16; i++) {
                                                     g_workBuf[328+96+i] = g_workBuf[212+i]; //0x00000590 -- &g_workBuf[212] = 0x000014B8
                                                }
                                                //0x00000594 - 0x000005B8
                                                for (i = 0; i < 32; i++) {
                                                     g_workBuf[328+172+i] = arg3[i]; //0x000005B8
                                                }
                                                //0x000005BC - 0x000005E0
                                                for (i = 0; i < 64; i++) {
                                                     g_workBuf[328+204+i] = arg4[i]; //0x000005E0
                                                }
                                                //0x000005E4 - 0x0000060C
                                                for (i = 0; i < 16; i++) {
                                                     g_workBuf[328+268+i] = arg5[i]; //0x0000060C
                                                }
                                                //0x00000610 - 0x0000063C
                                                for (i = 0; i < 96; i++) {
                                                     g_hashBuf[i] = g_workBuf[328+16+i]; //0x0000063C
                                                }
                                                //0x00000640 - 0x00000664
                                                for (i = 0; i < 32; i++) {
                                                     g_hashBuf[96+i] = g_workBuf[328+172+i]; //0x00000664
                                                }
                                                //0x00000668 - 0x0000068C
                                                for (i = 0; i < 64; i++) {
                                                     g_hashBuf[96+32+i] = g_workBuf[328+204+i]; //0x0000068C
                                                }
                                                //0x00000690 - 0x000006B4
                                                for (i = 0; i < 16; i++) {
                                                     g_hashBuf[96+32+64+i] = g_workBuf[328+268+i]; //0x000006B4
                                                }
                                                retVal = signEcdsaSignature(&g_workBuf[328+112], &g_sessionBuf[212], g_hashBuf, 208); //0x000006D0
                                                status = 0x80530018; //0x000006D4 & 0x000006DC
                                                if (retVal == 0) { //0x000006D8
                                                    //0x000006E0 - 0x00000708
                                                    for (i = 0; i < 20; i++) {
                                                         g_hashBuf[i] = g_workBuf[i]; //0x00000708
                                                    }
                                                    //0x0000070C - 0x00000730
                                                    for (i = 0; i < 96; i++) {
                                                         g_hashBuf[20+i] = g_workBuf[328+16+i]; //0x00000730
                                                    }
                                                    retVal = computeSha1(g_hashBuf, 116, g_hashBuf); //0x00000740
                                                    status = 0x80530020; //0x0000087C & 0x00000884
                                                    if (retVal == 0) {
                                                        //0x00000748 - 0x0000076C
                                                        for (i = 0; i < 20; i++) {
                                                             g_workBuf[328+152+i] = g_hashBuf[i]; //0x0000076C
                                                        }
                                                        //0x00000770 - 0x00000788
                                                        for (i = 0; i < 288; i++) {
                                                             g_sessionBuf[i] = 0; //0x00000788
                                                        }
                                                        //0x0000078C - 0x000007B4
                                                        for (i = 0; i < 20; i++) {
                                                             g_sessionBuf[16+i] = g_workBuf[i]; //0x000007B4 -- &g_sessionBuf[16] == 0x000012D4
                                                        }
                                                        fillRandomBytes(&g_workBuf[328], 16); //0x000007BC
                                                        retVal = encryptAndMacBlock(&g_workBuf[328], arg6, 304); //0x000007CC
                                                        status = retVal; //0x000007D8
                                                        if (retVal == 0) { //0x000007D4
                                                            fillRandomBytes(g_sessionBuf, 16); //0x000007E0
                                                            retVal = encryptAndMacBlock(g_sessionBuf, arg1, 288); //0x000007F0
                                                            status = retVal; //0x000007F8
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
    fillRandomBytes(g_sessionBuf, 288); //0x00000804
    fillRandomBytes(g_workBuf, 632); //0x00000814
    fillRandomBytes(g_hashBuf, 208); //0x00000824
    
    retVal = status & 0xFFFF0000; //0x00000830
    if (retVal == 0x80530000) { //0x00000838
        status |= 0x80530300; //0x0000083C & 0x00000878
    }
    return status;
}

/*
 * Subroutine sceMcctrl_9618EE57 - Address 0x000008A0
 * Exported in sceMcctrl
 */
int sceMcctrl_9618EE57(u8 *arg1, u8 *arg2, int arg3) {
    int status;
    int i;
    u32 val1;
    u32 val2;
    u16 val3;
    u8 val4;
    
    status = 0x80530001; //0x000008DC & 0x000008EC
    if (g_initialized != 0) { //0x000008E8
        status = verifyAndDecryptBlock(arg1, g_sessionBuf, 288); //0x000008F0 - 0x000008F8
        
        if (status == 0) { //0x000008FC
            //0x00000904 - 0x0000092C
            for (i = 0; i < 4; i++) {
                 g_workBuf[i] = g_sessionBuf[32+i] ^ arg2[i];
            }
            val1 = g_workBuf[0]; //0x00000930
            val2 = g_workBuf[1]; //0x00000934
            val3 = g_workBuf[2]; //0x00000938
            
            val1 = val1 << 24; //0x00000938
            val2 = val2 << 16; //0x00000940
            
            val4 = g_workBuf[3]; //0x00000944
            val1 |= val2; //0x00000948
            val3 = val3 << 8; //0x0000094C
            
            val1 |= val3; //0x00000950
            val1 |= val4; //0x00000954
            
            status = 0x80530004; //0x00000958 & 0x00000964
            if (arg3 >= val1) { //0x00000960
                status = val1 + 15; //0x00000968
                status &= 0xFFFFFFF0; //0x0000096C
            }
        }     
    }
    fillRandomBytes(g_sessionBuf, 288); //0x00000974
    fillRandomBytes(g_workBuf, 632); //0x00000980
    val1 = status & 0xFFFF0000; //0x0000098C
    if (val1 == 0x80530000) {
        status |= 0x80530300; //0x000009CC
    }
    return status;
}

/*
 * Subroutine sceMcctrl_61550814 - Address 0x000009D0
 * Exported in sceMcctrl
 */
int sceMcctrl_61550814(u8 *arg1, u8 *arg2, int size, u8 *arg4, int arg5) {
    int status;
    int i;
    u32 val1;
    u32 val2;
    u16 val3;
    u8 val4;
    u32 val5;
    int val6;
    u32 val7;
    
    status = 0x80530001; //0x00000A0C & 0x00000A10
    if (g_initialized != 0) { //0x00000A1C & 0x00000A2C
        status = verifyAndDecryptBlock(arg1, g_sessionBuf, 288); //0x00000A38
        if (status == 0) { //0x00000A40
            //0x00000A48 - 0x00000A70
            for (i = 0; i < 4; i++) {
                 g_workBuf[i] = arg2[i] ^ g_sessionBuf[32+i];
            }
            val1 = g_workBuf[0]; //0x00000A74
            val2 = g_workBuf[1]; //0x00000A78
            val3 = g_workBuf[2]; //0x00000A7C;
            
            val1 = val1 << 24; //0x00000A80
            val2 = val2 << 16; //0x00000A84
            
            val4 = g_workBuf[3]; //0x00000A88
            
            val1 |= val2; //0x00000A8C
            val3 = val3 << 8; //0x00000A90
            val1 |= val3; //0x00000A94
            val1 |= val4; //0x00000A98
            
            status = 0x80530002; //0x00000AA4 & 0x00000AB0
            val5 = val1 + 15; //0x00000A9C
            val5 &= 0xFFFFFFF0; //0x00000AA0
            if (arg5 >= val5) { //0x00000AAC
                status = verifyAndDecryptBlock(arg2+4, arg4, size-4); //0x00000ABC
                if (status == 0) { //0x00000AC4
                    if (status < val1) { //0x00000AD0
                        val6 = 0;                    
                        do {
                            val7 = val6 >> 31; //0x00000AE4
                            val7 = val7 >> 28; //0x00000AE8
                            val7 += val6; //0x00000AEC
                            val7 &= 0xFFFFFFF0; //0x00000AF0
                            val7 = val6 - val7; //0x00000AF4
                    
                            arg4[val6] = arg4[val6] ^ g_sessionBuf[16+val7]; //0x00000B00 & 0x00000B04 & 0x00000B10 & 0x00000B20
                            if (val7 == 15) { //0x00000B1C
                                status = encryptBuffer(&g_sessionBuf[16], &g_sessionBuf[16], 16); //0x00000B0C & 0x00000B14 & 0x00000B18 & 0x00000BA4
                                if (status < 0) { //0x00000BAC
                                    status = 0x80530004;
                                    break; //0x00000BB4
                                }
                            }
                            val6++; //0x00000B08
                        }                      
                        while (val6 < val1); //0x00000B24 & 0x00000B28
                        if (status >= 0) {
                            status = val1;
                        }
                    } else {
                        status = val1;
                    }
                }
            }
        }
    }
    fillRandomBytes(g_sessionBuf, 288); //0x00000B3C
    fillRandomBytes(g_workBuf, 632); //0x00000B4C
    if ((status & 0xFFFF0000) == 0x80530000) { //0x00000B60
        status |= 0x80530300; //0x00000BA0
    }
    return status;
}

/* 
 * Subroutine module_start - Address 0x00000000
 * Exported in syslib  
 */
int module_start(SceSize args, void *argp) {    
    return 0;
}

/*
 * Subroutine module_stop - Address 0x00000008
 * Exported in syslib
 */
int module_stop(SceSize args, void *argp) {
    return 0;
}
