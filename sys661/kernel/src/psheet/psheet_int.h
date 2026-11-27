/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#ifndef PSHEET_INT_H
#define PSHEET_INT_H

#include <common/types.h>
#include <iofilemgr_kernel.h>
#include <threadman_kernel.h>
#include <sysmem_kdebug.h>
#include <string.h>

#ifndef PSP_O_RDONLY
#define PSP_O_RDONLY 0x0001
#define PSP_O_WRONLY 0x0002
#define PSP_O_RDWR   0x0003
#define PSP_O_CREAT  0x0200
#define PSP_O_TRUNC  0x0400
#endif
#ifndef PSP_SEEK_SET
#define PSP_SEEK_SET 0
#endif
#ifndef PSP_SEEK_CUR
#define PSP_SEEK_CUR 1
#endif
#ifndef PSP_SEEK_END
#define PSP_SEEK_END 2
#endif

#define SCE_PSHEET_ERROR_NOT_INITIALIZED 0x80510108
#define SCE_PSHEET_ERROR_ILLEGAL_ADDRESS 0x80510109
#define SCE_PSHEET_ERROR_NO_CORE_HEADER  0x80510111
#define SCE_PSHEET_ERROR_INVALID_FORMAT  0x80510110
#define SCE_PSHEET_ERROR_CRYPTO_FAILED   0x80510102
#define SCE_PSHEET_ERROR_AMCTRL_BASE     0x80510300
#define SCE_PSHEET_ERROR_MAC_MISMATCH    0x80510101
#define SCE_PSHEET_ERROR_CORE_MAC        0x80510113
#define SCE_PSHEET_ERROR_UNSUPPORTED     0x80510112
#define SCE_PSHEET_ERROR_ABORTED         0x80510120

#ifndef IOASSIGN_RDONLY
#define IOASSIGN_RDONLY PSP_O_RDONLY
#endif

extern void *sceKernelMemset(void *dst, int val, SceSize size);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize size);
extern int sceKernelIsToolMode(void);
extern int sceDrmBBCipherInit(void *ckey, int type, int mode, void *headerKey, void *versionKey, u32 seed);
extern int sceDrmBBCipherUpdate(void *ckey, void *buf, int size);
extern int sceDrmBBCipherFinal(void *ckey);
extern int sceDrmBBMacInit(void *mkey, int type);
extern int sceDrmBBMacUpdate(void *mkey, void *buf, int size);
extern int sceDrmBBMacFinal(void *mkey, void *buf, void *vkey);
extern int sceDrmBBMacFinal2(void *mkey, void *out, void *vkey);
extern int sceUtilsPrepareGetLoadModuleSLength(void *buf, int size, void *out);
extern int sceUtilsPrepareGetLoadModuleRLength(void *buf, int size, void *out);

typedef struct {
    SceUID semaId;              /* 0x00 */
    SceUID auxSemaId;           /* 0x04 */
    SceUInt progressBytes;      /* 0x08 */
    SceUInt totalBytes;         /* 0x0C */
    void *workBuf;              /* 0x10 */
    u32 workBufSize;            /* 0x14 */
    int abortState;             /* 0x18 */
    int pad;                    /* 0x1C */
} ScePsheetState;

typedef struct {
    u8 magic[4];                /* 0x00 */
    u32 version;                /* 0x04 */
    u32 keyIndex;               /* 0x08 */
    u32 headerPad;              /* 0x0C */
    u8 headerKey[0x10];         /* 0x10 */
    u8 pathMac[0x10];           /* 0x20 */
    u8 dataKey[0x10];           /* 0x30 */
    u32 dataOffset;             /* 0x40 */
    u32 dataSize;               /* 0x44 */
    u32 blockSize;              /* 0x48 */
    u32 headerSize;             /* 0x4C */
    u8 tablePad[0x10];          /* 0x50 */
    u8 tableMac[0x10];          /* 0x60 */
    u8 headerMacVkey[0x10];     /* 0x70 */
    u8 headerMacPkgKey[0x10];   /* 0x80 */
} ScePsheetPgdHeader;

typedef struct {
    u32 flags;
    u32 size;
    char name[0x100];
} ScePsheetFileInfo;

typedef struct {
    u8 ioBuf[0x400];            /* 0x000 */
    struct {
        u16 nameOff;            /* 0x400 */
        u16 nameLen;            /* 0x402 */
        u32 dataOff;            /* 0x404 */
        u32 dataSize;           /* 0x408 */
        u32 flags;              /* 0x40C */
    } arcHdr;
    u8 coreHeaderKey[0x10];     /* 0x410 */
    u8 paramKey[0x10];          /* 0x420 */
    u8 dataKey[0x10];           /* 0x430 */
    u32 version;                /* 0x440 */
    u32 targetArch;             /* 0x444 */
    u32 pkgType;                /* 0x448 */
    u32 pad;                    /* 0x44C */
    u32 paramOff;               /* 0x450 */
    u32 paramCount;             /* 0x454 */
    u32 dataOff;                /* 0x458 */
    u32 dataSize;               /* 0x45C */
    u8 coreMac[0x10];           /* 0x460 */
    u8 decryptCtx[0x18];        /* 0x470 */
    u8 encryptCtx[0x18];        /* 0x488 */
    u8 macCtx[0x28];            /* 0x4A0 */
    u32 coreOff;                /* 0x4C8 */
    u32 coreSize;               /* 0x4CC */
} ScePsheetWork;

extern ScePsheetState g_state;
extern u32 g_pkgKey[8];
extern ScePsheetWork g_work;

/* bbox_api_raw.c */
int get_package_info(SceUID pkgFd, void *vkey, void *pkgInfo);
int get_file_info(SceUID pkgFd, void *vkey, int fileIdx, void *fileInfo);
int install_package(SceUID pkgFd, char *path, void *vkey, void *opt);
int getSemaIdStatus(SceUInt *progressOut);
int getSemaIdStatusIncrement(SceUInt delta);
int getSemaIdStatusUsingTimeout(int progress, int total);
int get_state(void);
int set_state(int state);

/* bbox_access.c */
int open_and_mac_check(char *file);
int get_core_header(SceUID pkgFd, void *vkey);
int get_archive_header(SceUID pkgFd, void *vkey, int idx, char *outName, u32 maxLen);
int get_params(SceUID pkgFd, void *vkey, u32 *params, u32 count);
int get_string(SceUID pkgFd, void *vkey, u32 desc, char *outStr, u32 maxLen);
int rename_in_dir(char *path, const char *newName);
int recursive_mkdir(char *path, u32 startOff);
int init_pgd_header(void *pgdHdr);

/* encrypt_wrapper.c */
int data_encrypt(void *buf, int size, void *vkey, void *headerKey);
int create_mac(void *buf, int size, void *vkey, void *macOut);
int encrypt_and_mac(void *buf, int size, void *vkey, void *headerKey, void *macOut);

/* decrypt_wrapper.c */
int drmbb_decrypt(void *buf, int size, int seed, void *vkey, void *headerKey);
int verify_file_mac(SceUID pkgFd, int offset, int size, void *vkey, void *mac);

#endif /* PSHEET_INT_H */
