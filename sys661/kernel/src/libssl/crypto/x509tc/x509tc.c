/*
 * RSA BSAFE SSL-C X.509 dispatch layer (x509_lib) and TinyCert (x509tc)
 * implementation (0x018D64..0x01A693, 0x01B524..0x01CA1F).
 */
#include "libssl_internal.h"

/* Global X.509 method dispatch table (240 bytes at 0x0317B4..0x0318A3) */
typedef struct SslX509MethodTable {
    int certType;                                                                    /* +0x00 (opt 0) */
    int certFlag;                                                                    /* +0x04 (opt 1) */
    SslX509TcCert *(*certNew)(void);                                                 /* +0x08 (case 1) */
    void (*certFree)(SslX509TcCert *cert);                                           /* +0x0C (case 2) */
    int (*certVerify)(SslX509TcCert *cert, SslEvpPkey *pubKey);                      /* +0x10 (case 3) */
    int (*certDigest)(SslX509TcCert *cert, const SslEvpMd *md, u8 *outBuf, u32 *outLen); /* +0x14 (case 4) */
    int (*i2dCert)(const SslX509TcCert *cert, u8 **outPtr);                          /* +0x18 (case 5) */
    SslX509TcCert *(*d2iCert)(SslX509TcCert **outCert, const u8 **inPtr, u32 len);   /* +0x1C (case 6) */
    int (*certUpRef)(SslX509TcCert *cert);                                           /* +0x20 (case 7) */
    int (*certGetSigType)(SslX509TcCert *cert);                                      /* +0x24 (case 8) */
    int (*certSubjectCmp)(const SslX509TcCert *lhs, const SslX509TcCert *rhs);       /* +0x28 (case 9) */
    SslX509TcName *(*certGetIssuer)(SslX509TcCert *cert);                            /* +0x2C (case 10) */
    SslX509TcName *(*certGetSubject)(SslX509TcCert *cert);                           /* +0x30 (case 11) */
    SslEvpPkey *(*certGetPubkey)(SslX509TcCert *cert);                               /* +0x34 (case 12) */
    int (*certGetSerial)(const SslX509TcCert *cert, SslAsn1Item *outItem);           /* +0x38 (case 13) */
    int (*certGetNotAfter)(const SslX509TcCert *cert, SslAsn1Item *outItem);         /* +0x3C (case 14) */
    int (*certGetNotBefore)(const SslX509TcCert *cert, SslAsn1Item *outItem);        /* +0x40 (case 15) */
    int (*certGetExt)(SslX509TcCert *cert, const u8 *oid, int *outCrit,
                      const u8 **outData, u32 *outLen);                              /* +0x44 (case 57) */
    SslX509TcName *(*nameNew)(void);                                                 /* +0x48 (case 16) */
    void (*nameFree)(SslX509TcName *name);                                           /* +0x4C (case 17) */
    SslX509TcName *(*nameDup)(SslX509TcName *name);                                  /* +0x50 (case 18) */
    u32 (*nameHash)(const SslX509TcName *name);                                      /* +0x54 (case 19) */
    int (*nameCmp)(const SslX509TcName *lhs, const SslX509TcName *rhs);              /* +0x58 (case 21) */
    SslX509TcName *(*d2iName)(SslX509TcName **outName, const u8 **inPtr, int len);   /* +0x5C (case 22) */
    int (*i2dName)(const SslX509TcName *name, u8 **outPtr);                          /* +0x60 (case 31) */
    int (*nameGetItem)(const SslX509TcName *name, SslAsn1Item *outItem);             /* +0x64 (case 23) */
    int (*nameEntryCount)(SslX509TcName *name);                                      /* +0x68 (case 24) */
    SslAsn1Item *(*nameGetEntry)(SslX509TcName *name, int entryIndex);               /* +0x6C (case 25) */
    int (*nameEntryGetData)(const SslAsn1Item *entry, SslAsn1Item *outOid,
                            SslAsn1Item *outValue);                                  /* +0x70 (case 26) */
    int (*storeGetIssuer)(void *storeCtx, void **outCert, void *issuerName);         /* +0x74 (case 27) */
    void *(*storeNew)(void);                                                         /* +0x78 (case 35) */
    void (*storeFree)(void *store);                                                  /* +0x7C (case 36) */
    void *storeLoadLocations;                                                        /* +0x80 (case 37) */
    void *storeSetDefaultPaths;                                                      /* +0x84 (case 38) */
    int (*storeSetVerifyArg)(void *store, void *verifyArg);                          /* +0x88 (case 39) */
    void *(*storeCtxNew)(void);                                                      /* +0x8C (case 40) */
    int (*storeCtxInit)(void *storeCtx, void *store, void *cert, void *chain);       /* +0x90 (case 41) */
    void (*storeCtxFree)(void *storeCtx);                                            /* +0x94 (case 42) */
    void (*storeCtxCleanup)(void *storeCtx);                                         /* +0x98 (case 43) */
    int (*verifyCert)(void *storeCtx);                                               /* +0x9C (case 44) */
    int (*storeCtxGetError)(void *storeCtx);                                         /* +0xA0 (case 45) */
    void *storeCtxSetError;                                                          /* +0xA4 (case 46) */
    void *storeCtxGetErrorDepth;                                                     /* +0xA8 (case 47) */
    void *storeCtxGetCurrentCert;                                                    /* +0xAC (case 48) */
    void *storeCtxSetChain;                                                          /* +0xB0 (case 49) */
    void *storeCtxSetCert;                                                           /* +0xB4 (case 50) */
    void *storeCtxSetVerifyCb;                                                       /* +0xB8 (case 59) */
    void *lookupAddDir;                                                              /* +0xBC (case 28) */
    int (*appVerifyCertChain)(Ssl *ssl, SslStack *chain, SslAppVerifyCb cb,
                              void *arg, int *verifyResult);                         /* +0xC0 (case 29) */
    int (*defaultVerifyCertChain)(Ssl *ssl, SslStack *chain, void *store,
                                  void *cb, void *arg, int *verifyResult);           /* +0xC4 (case 30) */
    int (*checkPrivateKey)(SslX509TcCert *cert, SslEvpPkey *privKey);                /* +0xC8 (case 32) */
    SslX509TcCert *(*pemReadBioCert)(SslBio *bio, SslX509TcCert **outCert, void *passCb); /* +0xCC (case 33) */
    int (*certNoop)(void);                                                           /* +0xD0 (case 34) */
    SslEvpPkey *(*pkeyNew)(void);                                                    /* +0xD4 (case 52) */
    void (*pkeyFree)(SslEvpPkey *pkey);                                              /* +0xD8 (case 53) */
    void *storeCtxSetPurpose;                                                        /* +0xDC (case 51) */
    SslEvpPkey *(*d2iPrivateKey)(int keyType, SslEvpPkey **outKey, const u8 **inPtr, int len); /* +0xE0 (case 54) */
    int (*pkeyUpRef)(SslEvpPkey *pkey);                                              /* +0xE4 (case 55) */
    int (*certGetKeyUsageExt)(SslX509TcCert *cert, int *outCrit, const u8 **outData, u32 *outLen); /* +0xE8 (case 58) */
    int (*certGetVersion)(SslX509TcCert *cert);                                      /* +0xEC (case 56) */
} SslX509MethodTable;

static SslX509MethodTable g_x509Table;
static int g_montCacheEnabled = 1;

/* X509TC ASN.1 navigation scripts */
static const u8 g_navCertTbs[] = { 0xC0, 0x00 };
static const u8 g_navCertSigAlgOid[] = { 0x81, 0xC0, 0x00 };
static const u8 g_navCertSigBits[] = { 0xC2, 0x00 };
static const u8 g_navSeqFirstElem[] = { 0x80, 0xC0, 0x00 };
static const u8 g_navDigestOctets[] = { 0xC1, 0x00 };
static const u8 g_navTbsSubject[] = { 0x80, 0xC4, 0x00 };
static const u8 g_navTbsIssuer[] = { 0x80, 0xC2, 0x00 };
static const u8 g_navTbsNotAfter[] = { 0x80, 0x83, 0xC1, 0x00 };
static const u8 g_navTbsNotBefore[] = { 0x80, 0x83, 0xC0, 0x00 };
static const u8 g_navTbsPubkeyInfo[] = { 0x80, 0xC5, 0x00 };
static const u8 g_navTbsVersion[] = { 0x80, 0x21, 0xA0, 0x21, 0x40, 0x00 };
static const u8 g_navTbsExtensions[] = { 0x80, 0x21, 0xA3, 0x21, 0x40, 0x00 };

static const SslAsn1FieldSpec g_specCertVerify[3] = {
    { 40, 47, g_navCertTbs, 0x10000U },
    { 40, 32, g_navCertSigAlgOid, 0x40U },
    { 40, 32, g_navCertSigBits, 0x08U },
};

static SslAsn1FieldSpec g_specDigestInfo[2] = {
    { 48, 32, g_navSeqFirstElem, 0x40U },
    { 48, 32, g_navDigestOctets, 0x10U },
};

static SslAsn1FieldSpec g_specCertFields[5] = {
    { 40, 45, g_navTbsSubject, 0x10000U },
    { 40, 45, g_navTbsIssuer, 0x10000U },
    { 40, 32, g_navSeqFirstElem, 0x04U },
    { 40, 32, g_navTbsNotAfter, 0x1800000U },
    { 40, 32, g_navTbsNotBefore, 0x1800000U },
};

static SslAsn1FieldSpec g_specPubkeyInfo[1] = {
    { 40, 41, g_navTbsPubkeyInfo, 0x10000U },
};

static SslAsn1FieldSpec g_specSigAlgOid[1] = {
    { 40, 32, g_navCertSigAlgOid, 0x40U },
};

static SslAsn1FieldSpec g_specVersion[1] = {
    { 40, 32, g_navTbsVersion, 0x04U },
};

static SslAsn1FieldSpec g_specExtensions[1] = {
    { 40, 32, g_navTbsExtensions, 0x10000U },
};

/* DER-encoded OID value bytes */
static const u8 g_oidBasicConstraints[] = { 85, 29, 19, 0 };
static const u8 g_oidKeyUsage[] = { 85, 29, 15, 0 };
static const u8 g_oidSha1WithRsa[] = { 42, 134, 72, 134, 247, 13, 1, 1, 5 };
static const u8 g_oidSha1WithRsaOiw[] = { 43, 14, 3, 2, 29 };
static const u8 g_oidDigestSha1[] = { 43, 14, 3, 2, 26, 0 };
static const u8 g_oidSha256WithRsa[] = { 42, 134, 72, 134, 247, 13, 1, 1, 11 };
static const u8 g_oidDigestSha256[] = { 96, 134, 72, 1, 101, 3, 4, 2, 1, 0 };
static const u8 g_oidSha384WithRsa[] = { 42, 134, 72, 134, 247, 13, 1, 1, 12 };
static const u8 g_oidDigestSha384[] = { 96, 134, 72, 1, 101, 3, 4, 2, 2, 0 };
static const u8 g_oidSha512WithRsa[] = { 42, 134, 72, 134, 247, 13, 1, 1, 13 };
static const u8 g_oidDigestSha512[] = { 96, 134, 72, 1, 101, 3, 4, 2, 3, 0 };
static const u8 g_oidSha224WithRsa[] = { 42, 134, 72, 134, 247, 13, 1, 1, 14 };
static const u8 g_oidDigestSha224[] = { 96, 134, 72, 1, 101, 3, 4, 2, 4, 0 };
static const u8 g_oidMd2WithRsa[] = { 42, 134, 72, 134, 247, 13, 1, 1, 2 };
static const u8 g_oidDigestMd2[] = { 42, 134, 72, 134, 247, 13, 2, 2, 0 };
static const u8 g_oidMd5WithRsa[] = { 42, 134, 72, 134, 247, 13, 1, 1, 4 };
static const u8 g_oidMd5WithRsaOiw[] = { 43, 14, 3, 2, 3 };
static const u8 g_oidDigestMd5[] = { 42, 134, 72, 134, 247, 13, 2, 5, 0 };
static const u8 g_oidShaWithRsaOiw[] = { 43, 14, 3, 2, 15 };
static const u8 g_oidRipemd160WithRsa[] = { 43, 36, 3, 3, 1, 2 };
static const u8 g_oidDsaWithShaOiw[] = { 43, 14, 3, 2, 13 };
static const u8 g_oidDsaWithSha1[] = { 42, 134, 72, 206, 56, 4, 3 };
static const u8 g_oidDsaWithSha1Oiw[] = { 43, 14, 3, 2, 27 };

/* Forward declarations for TinyCert internal handlers */
static int LIBSSL_NOINLINE sslX509TcNoop(void);
static void LIBSSL_NOINLINE sslX509TableReset(void);
static int LIBSSL_NOINLINE sslX509TableSetMethod(int methodId, void *fnPtr);
static SslX509TcCert *LIBSSL_NOINLINE sslX509TcPemReadBio(SslBio *bio, SslX509TcCert **outCert, void *passCb);
static int LIBSSL_NOINLINE sslX509TcI2dName(const SslX509TcName *name, u8 **outPtr);
static SslX509TcCert *LIBSSL_NOINLINE sslX509TcNew(void);
static void LIBSSL_NOINLINE sslX509TcFree(SslX509TcCert *cert);
static int LIBSSL_NOINLINE sslX509TcUpRef(SslX509TcCert *cert);
static int LIBSSL_NOINLINE sslX509TcVerify(SslX509TcCert *cert, SslEvpPkey *pubKey);
static SslX509TcCert *LIBSSL_NOINLINE sslX509TcD2i(SslX509TcCert **outCert, const u8 **inPtr, u32 len);
static int LIBSSL_NOINLINE sslX509TcI2d(const SslX509TcCert *cert, u8 **outPtr);
static SslEvpPkey *LIBSSL_NOINLINE sslX509TcGetPubkey(SslX509TcCert *cert);
static int LIBSSL_NOINLINE sslX509TcExtractPubkeyItem(SslX509TcCert *cert, SslAsn1Item *outItem);
static int LIBSSL_NOINLINE sslX509TcGetSigType(SslX509TcCert *cert);
static SslX509TcName *LIBSSL_NOINLINE sslX509TcGetIssuerName(SslX509TcCert *cert);
static SslX509TcName *LIBSSL_NOINLINE sslX509TcGetSubjectName(SslX509TcCert *cert);
static int LIBSSL_NOINLINE sslX509TcGetKeyUsageExt(SslX509TcCert *cert, int *outCrit,
                                                   const u8 **outData, u32 *outLen);
static u32 LIBSSL_NOINLINE sslX509TcNameHash(const SslX509TcName *name);
static int LIBSSL_NOINLINE sslX509TcNameCmp(const SslX509TcName *lhs, const SslX509TcName *rhs);
static int LIBSSL_NOINLINE sslX509TcSubjectNameCmp(const SslX509TcCert *lhs, const SslX509TcCert *rhs);
static int LIBSSL_NOINLINE sslX509TcGetSerialItem(const SslX509TcCert *cert, SslAsn1Item *outItem);
static int LIBSSL_NOINLINE sslX509TcGetNotAfterItem(const SslX509TcCert *cert, SslAsn1Item *outItem);
static int LIBSSL_NOINLINE sslX509TcGetNotBeforeItem(const SslX509TcCert *cert, SslAsn1Item *outItem);
static int LIBSSL_NOINLINE sslX509TcGetVersion(SslX509TcCert *cert);
static int LIBSSL_NOINLINE sslX509TcDigest(SslX509TcCert *cert, const SslEvpMd *md, u8 *outBuf, u32 *outLen);
static int LIBSSL_NOINLINE sslX509TcDecodeNameEntries(SslX509TcName *name, SslAsn1ItemList *outList);
static int LIBSSL_NOINLINE sslX509TcNameEntryCount(SslX509TcName *name);
static SslAsn1Item *LIBSSL_NOINLINE sslX509TcNameGetEntry(SslX509TcName *name, int entryIndex);
static int LIBSSL_NOINLINE sslX509TcNameGetItem(const SslX509TcName *name, SslAsn1Item *outItem);
static int LIBSSL_NOINLINE sslX509TcNameEntryGetData(const SslAsn1Item *entry, SslAsn1Item *outOid,
                                                     SslAsn1Item *outValue);
static SslX509TcName *LIBSSL_NOINLINE sslX509TcNameNew(void);
static void LIBSSL_NOINLINE sslX509TcNameFree(SslX509TcName *name);
static SslX509TcName *LIBSSL_NOINLINE sslX509TcNameDup(SslX509TcName *name);
static SslX509TcName *LIBSSL_NOINLINE sslX509TcD2iName(SslX509TcName **outName, const u8 **inPtr, int len);
static int LIBSSL_NOINLINE sslX509TcCheckPrivateKey(SslX509TcCert *cert, SslEvpPkey *privKey);
static int LIBSSL_NOINLINE sslX509TcGetExtension(SslX509TcCert *cert, const u8 *oid, int *outCrit,
                                                 const u8 **outData, u32 *outLen);

/* 0x018D64 */
static SslX509TcCert *LIBSSL_NOINLINE sslX509TcPemReadBio(SslBio *bio, SslX509TcCert **outCert, void *passCb)
{
    return (SslX509TcCert *)sceSsl_lib_0217E73C((SslPemD2iFn)sslX509TcD2i, "CERTIFICATE", bio, (void **)outCert, passCb);
}

/* 0x018D9C */
static int LIBSSL_NOINLINE sslX509TcNoop(void)
{
    return 0;
}

/* 0x018DA4 */
int LIBSSL_NOINLINE sslX509TcInit(void)
{
    sslX509TableReset();
    sceSsl_lib_1E257243(0, 2);
    sceSsl_lib_1E257243(1, 0);
    sslX509TableSetMethod(1, (void *)sslX509TcNew);
    sslX509TableSetMethod(2, (void *)sslX509TcFree);
    sslX509TableSetMethod(3, (void *)sslX509TcVerify);
    sslX509TableSetMethod(4, (void *)sslX509TcDigest);
    sslX509TableSetMethod(5, (void *)sslX509TcI2d);
    sslX509TableSetMethod(6, (void *)sslX509TcD2i);
    sslX509TableSetMethod(7, (void *)sslX509TcUpRef);
    sslX509TableSetMethod(8, (void *)sslX509TcGetSigType);
    sslX509TableSetMethod(9, (void *)sslX509TcSubjectNameCmp);
    sslX509TableSetMethod(10, (void *)sslX509TcGetIssuerName);
    sslX509TableSetMethod(11, (void *)sslX509TcGetSubjectName);
    sslX509TableSetMethod(12, (void *)sslX509TcGetPubkey);
    sslX509TableSetMethod(13, (void *)sslX509TcGetSerialItem);
    sslX509TableSetMethod(14, (void *)sslX509TcGetNotAfterItem);
    sslX509TableSetMethod(15, (void *)sslX509TcGetNotBeforeItem);
    sslX509TableSetMethod(16, (void *)sslX509TcNameNew);
    sslX509TableSetMethod(17, (void *)sslX509TcNameFree);
    sslX509TableSetMethod(18, (void *)sslX509TcNameDup);
    sslX509TableSetMethod(19, (void *)sslX509TcNameHash);
    sslX509TableSetMethod(21, (void *)sslX509TcNameCmp);
    sslX509TableSetMethod(22, (void *)sslX509TcD2iName);
    sslX509TableSetMethod(31, (void *)sslX509TcI2dName);
    sslX509TableSetMethod(23, (void *)sslX509TcNameGetItem);
    sslX509TableSetMethod(24, (void *)sslX509TcNameEntryCount);
    sslX509TableSetMethod(25, (void *)sslX509TcNameGetEntry);
    sslX509TableSetMethod(26, (void *)sslX509TcNameEntryGetData);
    sslX509TableSetMethod(57, (void *)sslX509TcGetExtension);
    sslX509TableSetMethod(29, (void *)sslBioInvokeCertPrinter);
    sslX509TableSetMethod(30, NULL);
    sslX509TableSetMethod(32, (void *)sslX509TcCheckPrivateKey);
    sslX509TableSetMethod(33, (void *)sslX509TcPemReadBio);
    sslX509TableSetMethod(34, (void *)sslX509TcNoop);
    sslX509TableSetMethod(52, (void *)sslEvpPkeyNew);
    sslX509TableSetMethod(53, (void *)sslEvpPkeyFree);
    sslX509TableSetMethod(54, (void *)sslD2iPublicKey);
    sslX509TableSetMethod(55, (void *)sslEvpPkeyUpRef);
    sslX509TableSetMethod(56, (void *)sslX509TcGetVersion);
    sslX509TableSetMethod(58, (void *)sslX509TcGetKeyUsageExt);
    return 1;
}

/* 0x019038 */
static void LIBSSL_NOINLINE sslX509TableReset(void)
{
    sslMemset(&g_x509Table, 0, sizeof(g_x509Table));
}

/* 0x019060 */
int LIBSSL_NOINLINE sceSsl_lib_1E257243(int optId, int value)
{
    if (optId == 0) {
        g_x509Table.certType = value;
        return 1;
    }
    if (optId == 1) {
        g_x509Table.certFlag = value;
        return 1;
    }
    return 0;
}

/* 0x019090 */
static int LIBSSL_NOINLINE sslX509TableSetMethod(int methodId, void *fnPtr)
{
    switch (methodId) {
    case 1:  g_x509Table.certNew = (SslX509TcCert *(*)(void))fnPtr; break;
    case 2:  g_x509Table.certFree = (void (*)(SslX509TcCert *))fnPtr; break;
    case 3:  g_x509Table.certVerify = (int (*)(SslX509TcCert *, SslEvpPkey *))fnPtr; break;
    case 4:  g_x509Table.certDigest = (int (*)(SslX509TcCert *, const SslEvpMd *, u8 *, u32 *))fnPtr; break;
    case 5:  g_x509Table.i2dCert = (int (*)(const SslX509TcCert *, u8 **))fnPtr; break;
    case 6:  g_x509Table.d2iCert = (SslX509TcCert *(*)(SslX509TcCert **, const u8 **, u32))fnPtr; break;
    case 7:  g_x509Table.certUpRef = (int (*)(SslX509TcCert *))fnPtr; break;
    case 8:  g_x509Table.certGetSigType = (int (*)(SslX509TcCert *))fnPtr; break;
    case 9:  g_x509Table.certSubjectCmp = (int (*)(const SslX509TcCert *, const SslX509TcCert *))fnPtr; break;
    case 10: g_x509Table.certGetIssuer = (SslX509TcName *(*)(SslX509TcCert *))fnPtr; break;
    case 11: g_x509Table.certGetSubject = (SslX509TcName *(*)(SslX509TcCert *))fnPtr; break;
    case 12: g_x509Table.certGetPubkey = (SslEvpPkey *(*)(SslX509TcCert *))fnPtr; break;
    case 13: g_x509Table.certGetSerial = (int (*)(const SslX509TcCert *, SslAsn1Item *))fnPtr; break;
    case 14: g_x509Table.certGetNotAfter = (int (*)(const SslX509TcCert *, SslAsn1Item *))fnPtr; break;
    case 15: g_x509Table.certGetNotBefore = (int (*)(const SslX509TcCert *, SslAsn1Item *))fnPtr; break;
    case 16: g_x509Table.nameNew = (SslX509TcName *(*)(void))fnPtr; break;
    case 17: g_x509Table.nameFree = (void (*)(SslX509TcName *))fnPtr; break;
    case 18: g_x509Table.nameDup = (SslX509TcName *(*)(SslX509TcName *))fnPtr; break;
    case 19: g_x509Table.nameHash = (u32 (*)(const SslX509TcName *))fnPtr; break;
    case 21: g_x509Table.nameCmp = (int (*)(const SslX509TcName *, const SslX509TcName *))fnPtr; break;
    case 22: g_x509Table.d2iName = (SslX509TcName *(*)(SslX509TcName **, const u8 **, int))fnPtr; break;
    case 23: g_x509Table.nameGetItem = (int (*)(const SslX509TcName *, SslAsn1Item *))fnPtr; break;
    case 24: g_x509Table.nameEntryCount = (int (*)(SslX509TcName *))fnPtr; break;
    case 25: g_x509Table.nameGetEntry = (SslAsn1Item *(*)(SslX509TcName *, int))fnPtr; break;
    case 26: g_x509Table.nameEntryGetData = (int (*)(const SslAsn1Item *, SslAsn1Item *, SslAsn1Item *))fnPtr; break;
    case 27: g_x509Table.storeGetIssuer = (int (*)(void *, void **, void *))fnPtr; break;
    case 28: g_x509Table.lookupAddDir = fnPtr; break;
    case 29: g_x509Table.appVerifyCertChain = (int (*)(Ssl *, SslStack *, SslAppVerifyCb, void *, int *))fnPtr; break;
    case 30: g_x509Table.defaultVerifyCertChain = (int (*)(Ssl *, SslStack *, void *, void *, void *, int *))fnPtr; break;
    case 31: g_x509Table.i2dName = (int (*)(const SslX509TcName *, u8 **))fnPtr; break;
    case 32: g_x509Table.checkPrivateKey = (int (*)(SslX509TcCert *, SslEvpPkey *))fnPtr; break;
    case 33: g_x509Table.pemReadBioCert = (SslX509TcCert *(*)(SslBio *, SslX509TcCert **, void *))fnPtr; break;
    case 34: g_x509Table.certNoop = (int (*)(void))fnPtr; break;
    case 35: g_x509Table.storeNew = (void *(*)(void))fnPtr; break;
    case 36: g_x509Table.storeFree = (void (*)(void *))fnPtr; break;
    case 37: g_x509Table.storeLoadLocations = fnPtr; break;
    case 38: g_x509Table.storeSetDefaultPaths = fnPtr; break;
    case 39: g_x509Table.storeSetVerifyArg = (int (*)(void *, void *))fnPtr; break;
    case 40: g_x509Table.storeCtxNew = (void *(*)(void))fnPtr; break;
    case 41: g_x509Table.storeCtxInit = (int (*)(void *, void *, void *, void *))fnPtr; break;
    case 42: g_x509Table.storeCtxFree = (void (*)(void *))fnPtr; break;
    case 43: g_x509Table.storeCtxCleanup = (void (*)(void *))fnPtr; break;
    case 44: g_x509Table.verifyCert = (int (*)(void *))fnPtr; break;
    case 45: g_x509Table.storeCtxGetError = (int (*)(void *))fnPtr; break;
    case 46: g_x509Table.storeCtxSetError = fnPtr; break;
    case 47: g_x509Table.storeCtxGetErrorDepth = fnPtr; break;
    case 48: g_x509Table.storeCtxGetCurrentCert = fnPtr; break;
    case 49: g_x509Table.storeCtxSetChain = fnPtr; break;
    case 50: g_x509Table.storeCtxSetCert = fnPtr; break;
    case 51: g_x509Table.storeCtxSetPurpose = fnPtr; break;
    case 52: g_x509Table.pkeyNew = (SslEvpPkey *(*)(void))fnPtr; break;
    case 53: g_x509Table.pkeyFree = (void (*)(SslEvpPkey *))fnPtr; break;
    case 54: g_x509Table.d2iPrivateKey = (SslEvpPkey *(*)(int, SslEvpPkey **, const u8 **, int))fnPtr; break;
    case 55: g_x509Table.pkeyUpRef = (int (*)(SslEvpPkey *))fnPtr; break;
    case 56: g_x509Table.certGetVersion = (int (*)(SslX509TcCert *))fnPtr; break;
    case 57: g_x509Table.certGetExt = (int (*)(SslX509TcCert *, const u8 *, int *, const u8 **, u32 *))fnPtr; break;
    case 58: g_x509Table.certGetKeyUsageExt = (int (*)(SslX509TcCert *, int *, const u8 **, u32 *))fnPtr; break;
    case 59: g_x509Table.storeCtxSetVerifyCb = fnPtr; break;
    default: return 0;
    }
    return 1;
}

/* 0x019380 */
int LIBSSL_NOINLINE sceSsl_lib_B1E75EA9(int optId)
{
    if (optId == 0) {
        return g_x509Table.certType;
    }
    if (optId == 1) {
        return g_x509Table.certFlag;
    }
    return -1;
}

/* 0x0193B4 */
void *LIBSSL_NOINLINE sceSsl_lib_0A241EDE(void)
{
    if (g_x509Table.certNew != NULL) {
        return g_x509Table.certNew();
    }
    sceSsl_lib_9CDCA0D7(26, 132, 14, NULL, 0);
    return NULL;
}

/* 0x019400 */
void LIBSSL_NOINLINE sceSsl_lib_C1A2E14B(void *cert)
{
    if (g_x509Table.certFree != NULL) {
        g_x509Table.certFree((SslX509TcCert *)cert);
        return;
    }
    sceSsl_lib_9CDCA0D7(26, 112, 14, NULL, 0);
}

/* 0x019448 */
int LIBSSL_NOINLINE sceSsl_lib_0FF7BA68(void *cert, SslEvpPkey *pubKey)
{
    if (g_x509Table.certVerify != NULL) {
        return g_x509Table.certVerify((SslX509TcCert *)cert, pubKey);
    }
    sceSsl_lib_9CDCA0D7(26, 150, 14, NULL, 0);
    return 0;
}

/* 0x019490 */
int LIBSSL_NOINLINE sslX509Digest(void *cert, const SslEvpMd *md, u8 *outBuf, u32 *outLen)
{
    if (g_x509Table.certDigest != NULL) {
        return g_x509Table.certDigest((SslX509TcCert *)cert, md, outBuf, outLen);
    }
    sceSsl_lib_9CDCA0D7(26, 110, 14, NULL, 0);
    return 0;
}

/* 0x0194D8 */
int LIBSSL_NOINLINE sceSsl_lib_A739BFD9(const void *cert, u8 **outPtr)
{
    if (g_x509Table.i2dCert != NULL) {
        return g_x509Table.i2dCert((const SslX509TcCert *)cert, outPtr);
    }
    sceSsl_lib_9CDCA0D7(26, 106, 14, NULL, 0);
    return 0;
}

/* 0x019520 */
void *LIBSSL_NOINLINE sceSsl_lib_6038DEDE(void **outCert, const u8 **inPtr, u32 len)
{
    if (g_x509Table.d2iCert != NULL) {
        return g_x509Table.d2iCert((SslX509TcCert **)outCert, inPtr, len);
    }
    sceSsl_lib_9CDCA0D7(26, 105, 14, NULL, 0);
    return NULL;
}

/* 0x019568 */
void LIBSSL_NOINLINE sceSsl_lib_999D7614(void *cert)
{
    if (g_x509Table.certUpRef != NULL) {
        g_x509Table.certUpRef((SslX509TcCert *)cert);
        return;
    }
    sceSsl_lib_9CDCA0D7(26, 136, 14, NULL, 0);
}

/* 0x0195B0 */
int LIBSSL_NOINLINE sslX509GetSignatureType(void *cert)
{
    if (g_x509Table.certGetSigType != NULL) {
        return g_x509Table.certGetSigType((SslX509TcCert *)cert);
    }
    sceSsl_lib_9CDCA0D7(26, 119, 14, NULL, 0);
    return 0;
}

/* 0x0195F8 */
int LIBSSL_NOINLINE sceSsl_lib_1B93AD27(const void *lhs, const void *rhs)
{
    if (g_x509Table.certSubjectCmp != NULL) {
        return g_x509Table.certSubjectCmp((const SslX509TcCert *)lhs, (const SslX509TcCert *)rhs);
    }
    sceSsl_lib_9CDCA0D7(26, 149, 14, NULL, 0);
    return 0;
}

/* 0x019640 */
void *LIBSSL_NOINLINE sceSsl_lib_4BE9030A(void *cert)
{
    if (g_x509Table.certGetIssuer != NULL) {
        return g_x509Table.certGetIssuer((SslX509TcCert *)cert);
    }
    sceSsl_lib_9CDCA0D7(26, 114, 14, NULL, 0);
    return NULL;
}

/* 0x019688 */
void *LIBSSL_NOINLINE sceSsl_lib_12C7718F(void *cert)
{
    if (g_x509Table.certGetSubject != NULL) {
        return g_x509Table.certGetSubject((SslX509TcCert *)cert);
    }
    sceSsl_lib_9CDCA0D7(26, 120, 14, NULL, 0);
    return NULL;
}

/* 0x0196D0 */
SslEvpPkey *LIBSSL_NOINLINE sceSsl_lib_B87FFE37(void *cert)
{
    if (g_x509Table.certGetPubkey != NULL) {
        return g_x509Table.certGetPubkey((SslX509TcCert *)cert);
    }
    sceSsl_lib_9CDCA0D7(26, 117, 14, NULL, 0);
    return NULL;
}

/* 0x019718 */
int LIBSSL_NOINLINE sceSsl_lib_9299C6CE(void *cert, int *outTag, const u8 **outData, u32 *outLen)
{
    SslAsn1Item serialItem;
    int ret;

    if (g_x509Table.certGetSerial != NULL) {
        ret = g_x509Table.certGetSerial((const SslX509TcCert *)cert, &serialItem);
        if (ret != 0) {
            *outData = (const u8 *)serialItem.data;
            *outLen = serialItem.length;
            *outTag = (int)serialItem.itemType;
        }
        return ret;
    }
    sceSsl_lib_9CDCA0D7(26, 118, 14, NULL, 0);
    return 0;
}

/* 0x0197A8 */
int LIBSSL_NOINLINE sceSsl_lib_05E019A4(void *cert, int *outTag, const u8 **outData, u32 *outLen)
{
    SslAsn1Item timeItem;
    int ret;

    if (g_x509Table.certGetNotAfter != NULL) {
        ret = g_x509Table.certGetNotAfter((const SslX509TcCert *)cert, &timeItem);
        if (ret == 1) {
            *outTag = (int)timeItem.itemType;
            *outData = (const u8 *)timeItem.data;
            *outLen = timeItem.length;
            return 1;
        }
        return ret;
    }
    sceSsl_lib_9CDCA0D7(26, 115, 14, NULL, 0);
    return 0;
}

/* 0x019840 */
int LIBSSL_NOINLINE sceSsl_lib_341843AB(void *cert, int *outTag, const u8 **outData, u32 *outLen)
{
    SslAsn1Item timeItem;
    int ret;

    if (g_x509Table.certGetNotBefore != NULL) {
        ret = g_x509Table.certGetNotBefore((const SslX509TcCert *)cert, &timeItem);
        if (ret == 1) {
            *outTag = (int)timeItem.itemType;
            *outData = (const u8 *)timeItem.data;
            *outLen = timeItem.length;
            return 1;
        }
        return ret;
    }
    sceSsl_lib_9CDCA0D7(26, 116, 14, NULL, 0);
    return 0;
}

/* 0x0198D8 */
void *LIBSSL_NOINLINE sceSsl_lib_45514AE3(void)
{
    if (g_x509Table.nameNew != NULL) {
        return g_x509Table.nameNew();
    }
    sceSsl_lib_9CDCA0D7(26, 130, 14, NULL, 0);
    return NULL;
}

/* 0x019924 */
void LIBSSL_NOINLINE sceSsl_lib_ED95A457(void *name)
{
    if (g_x509Table.nameFree != NULL) {
        g_x509Table.nameFree((SslX509TcName *)name);
        return;
    }
    sceSsl_lib_9CDCA0D7(26, 124, 14, NULL, 0);
}

/* 0x01996C */
void *LIBSSL_NOINLINE sceSsl_lib_338C8FDD(void *name)
{
    if (g_x509Table.nameDup != NULL) {
        return g_x509Table.nameDup((SslX509TcName *)name);
    }
    sceSsl_lib_9CDCA0D7(26, 122, 14, NULL, 0);
    return NULL;
}

/* 0x0199B4 */
u32 LIBSSL_NOINLINE sceSsl_lib_BE4A711C(const void *name)
{
    if (g_x509Table.nameHash != NULL) {
        return g_x509Table.nameHash((const SslX509TcName *)name);
    }
    sceSsl_lib_9CDCA0D7(26, 129, 14, NULL, 0);
    return 0;
}

/* 0x0199FC */
int LIBSSL_NOINLINE sceSsl_lib_7D0E41F4(const void *lhs, const void *rhs)
{
    if (g_x509Table.nameCmp != NULL) {
        return g_x509Table.nameCmp((const SslX509TcName *)lhs, (const SslX509TcName *)rhs);
    }
    sceSsl_lib_9CDCA0D7(26, 121, 14, NULL, 0);
    return 0;
}

/* 0x019A44 */
void *LIBSSL_NOINLINE sceSsl_lib_D579CE04(void **outName, const u8 **inPtr, int len)
{
    if (g_x509Table.d2iName != NULL) {
        return g_x509Table.d2iName((SslX509TcName **)outName, inPtr, len);
    }
    sceSsl_lib_9CDCA0D7(26, 125, 14, NULL, 0);
    return NULL;
}

/* 0x019A8C */
int LIBSSL_NOINLINE sceSsl_lib_90BCFCE2(const void *name, u8 **outPtr)
{
    if (g_x509Table.i2dName != NULL) {
        return g_x509Table.i2dName((const SslX509TcName *)name, outPtr);
    }
    sceSsl_lib_9CDCA0D7(26, 131, 14, NULL, 0);
    return 0;
}

/* 0x019AD4 */
int LIBSSL_NOINLINE sceSsl_lib_4C75B8E9(void *name)
{
    if (g_x509Table.nameEntryCount != NULL) {
        return g_x509Table.nameEntryCount((SslX509TcName *)name);
    }
    sceSsl_lib_9CDCA0D7(26, 127, 14, NULL, 0);
    return 0;
}

/* 0x019B1C */
void *LIBSSL_NOINLINE sceSsl_lib_5040E836(void *name, int entryIndex)
{
    if (g_x509Table.nameGetEntry != NULL) {
        return g_x509Table.nameGetEntry((SslX509TcName *)name, entryIndex);
    }
    sceSsl_lib_9CDCA0D7(26, 126, 14, NULL, 0);
    return NULL;
}

/* 0x019B64 */
int LIBSSL_NOINLINE sceSsl_lib_8D484361(void *entry, int *outOidTag, const u8 **outOid,
                                        u32 *outOidLen, int *outValTag, const u8 **outVal, u32 *outValLen)
{
    SslAsn1Item oidItem;
    SslAsn1Item valItem;
    int ret;

    if (g_x509Table.nameEntryGetData != NULL) {
        ret = g_x509Table.nameEntryGetData((const SslAsn1Item *)entry, &oidItem, &valItem);
        if (ret == 1) {
            if (outOidTag != NULL) {
                *outOidTag = (int)oidItem.itemType;
            }
            if (outOid != NULL) {
                *outOid = (const u8 *)oidItem.data;
            }
            if (outOidLen != NULL) {
                *outOidLen = oidItem.length;
            }
            if (outValTag != NULL) {
                *outValTag = (int)valItem.itemType;
            }
            if (outVal != NULL) {
                *outVal = (const u8 *)valItem.data;
            }
            if (outValLen != NULL) {
                *outValLen = valItem.length;
            }
            return 1;
        }
        return ret;
    }
    sceSsl_lib_9CDCA0D7(26, 123, 14, NULL, 0);
    return 0;
}

/* 0x019C54 */
int LIBSSL_NOINLINE sceSsl_lib_6443D0EE(void *entry, int *outOidTag, const u8 **outOid, u32 *outOidLen)
{
    return sceSsl_lib_8D484361(entry, outOidTag, outOid, outOidLen, NULL, NULL, NULL);
}

/* 0x019C78 */
int LIBSSL_NOINLINE sceSsl_lib_6E1E9E0D(void *entry, int *outValTag, const u8 **outVal, u32 *outValLen)
{
    return sceSsl_lib_8D484361(entry, NULL, NULL, NULL, outValTag, outVal, outValLen);
}

/* 0x019CA8 */
void *LIBSSL_NOINLINE sslX509StoreNew(void)
{
    if (g_x509Table.storeNew != NULL) {
        return g_x509Table.storeNew();
    }
    sceSsl_lib_9CDCA0D7(26, 148, 14, NULL, 0);
    return NULL;
}

/* 0x019CF4 */
void LIBSSL_NOINLINE sslX509StoreFree(void *store)
{
    if (g_x509Table.storeFree != NULL) {
        g_x509Table.storeFree(store);
        return;
    }
    sceSsl_lib_9CDCA0D7(26, 146, 14, NULL, 0);
}

/* 0x019D3C */
int LIBSSL_NOINLINE sslX509StoreSetVerifyArg(void *store, void *verifyArg)
{
    if (g_x509Table.storeSetVerifyArg != NULL) {
        return g_x509Table.storeSetVerifyArg(store, verifyArg);
    }
    return 0;
}

/* 0x019D68 */
int LIBSSL_NOINLINE sslX509StoreGetIssuer(void *storeCtx, void **outCert, void *issuerName)
{
    if (g_x509Table.storeGetIssuer != NULL) {
        return g_x509Table.storeGetIssuer(storeCtx, outCert, issuerName);
    }
    sceSsl_lib_9CDCA0D7(26, 147, 14, NULL, 0);
    return 0;
}

/* 0x019DB0 */
void *LIBSSL_NOINLINE sslX509StoreCtxNew(void)
{
    if (g_x509Table.storeCtxNew != NULL) {
        return g_x509Table.storeCtxNew();
    }
    sceSsl_lib_9CDCA0D7(26, 153, 14, NULL, 0);
    return NULL;
}

/* 0x019DFC */
void LIBSSL_NOINLINE sslX509StoreCtxInit(void *storeCtx, void *store, void *cert, void *chain)
{
    if (g_x509Table.storeCtxInit != NULL) {
        g_x509Table.storeCtxInit(storeCtx, store, cert, chain);
        return;
    }
    sceSsl_lib_9CDCA0D7(26, 143, 14, NULL, 0);
}

/* 0x019E44 */
void LIBSSL_NOINLINE sslX509StoreCtxFree(void *storeCtx)
{
    if (g_x509Table.storeCtxFree != NULL) {
        g_x509Table.storeCtxFree(storeCtx);
        return;
    }
    sceSsl_lib_9CDCA0D7(26, 152, 14, NULL, 0);
}

/* 0x019E8C */
void LIBSSL_NOINLINE sslX509StoreCtxCleanup(void *storeCtx)
{
    if (g_x509Table.storeCtxCleanup != NULL) {
        g_x509Table.storeCtxCleanup(storeCtx);
        return;
    }
    sceSsl_lib_9CDCA0D7(26, 137, 14, NULL, 0);
}

/* 0x019ED4 */
int LIBSSL_NOINLINE sslX509VerifyCert(void *storeCtx)
{
    if (g_x509Table.verifyCert != NULL) {
        return g_x509Table.verifyCert(storeCtx);
    }
    sceSsl_lib_9CDCA0D7(26, 102, 14, NULL, 0);
    return -1;
}

/* 0x019F20 */
int LIBSSL_NOINLINE sslX509StoreCtxGetError(void *storeCtx)
{
    if (g_x509Table.storeCtxGetError != NULL) {
        return g_x509Table.storeCtxGetError(storeCtx);
    }
    sceSsl_lib_9CDCA0D7(26, 138, 14, NULL, 0);
    return 0;
}

/* 0x019F6C */
int LIBSSL_NOINLINE sslX509AppVerifyCertChain(Ssl *ssl, SslStack *chain, SslAppVerifyCb cb,
                                             void *arg, int *verifyResult)
{
    if (g_x509Table.appVerifyCertChain != NULL) {
        return g_x509Table.appVerifyCertChain(ssl, chain, cb, arg, verifyResult);
    }
    sceSsl_lib_9CDCA0D7(26, 111, 14, NULL, 0);
    return 0;
}

/* 0x019FB8 */
int LIBSSL_NOINLINE sslX509DefaultVerifyCertChain(Ssl *ssl, SslStack *chain, void *store,
                                                 void *cb, void *arg, int *verifyResult)
{
    if (g_x509Table.defaultVerifyCertChain != NULL) {
        return g_x509Table.defaultVerifyCertChain(ssl, chain, store, cb, arg, verifyResult);
    }
    sceSsl_lib_9CDCA0D7(26, 100, 14, NULL, 0);
    return 0;
}

/* 0x01A004 */
int LIBSSL_NOINLINE sceSsl_lib_C6ED5C87(void *cert, SslEvpPkey *privKey)
{
    if (g_x509Table.checkPrivateKey != NULL) {
        return g_x509Table.checkPrivateKey((SslX509TcCert *)cert, privKey);
    }
    sceSsl_lib_9CDCA0D7(26, 109, 14, NULL, 0);
    return 0;
}

/* 0x01A04C */
void *LIBSSL_NOINLINE sceSsl_lib_8B78A6C0(SslBio *bio, void **outCert, void *passCb)
{
    if (g_x509Table.pemReadBioCert != NULL) {
        return g_x509Table.pemReadBioCert(bio, (SslX509TcCert **)outCert, passCb);
    }
    sceSsl_lib_9CDCA0D7(26, 133, 14, NULL, 0);
    return NULL;
}

/* 0x01A094 */
SslEvpPkey *LIBSSL_NOINLINE sceSsl_lib_737BED1C(void)
{
    if (g_x509Table.pkeyNew != NULL) {
        return g_x509Table.pkeyNew();
    }
    sceSsl_lib_9CDCA0D7(26, 135, 14, NULL, 0);
    return NULL;
}

/* 0x01A0E0 */
void LIBSSL_NOINLINE sceSsl_lib_9307DE23(SslEvpPkey *pkey)
{
    if (g_x509Table.pkeyFree != NULL) {
        g_x509Table.pkeyFree(pkey);
        return;
    }
    sceSsl_lib_9CDCA0D7(26, 134, 14, NULL, 0);
}

/* 0x01A128 */
SslEvpPkey *LIBSSL_NOINLINE sceSsl_lib_EDBE00D6(int keyType, SslEvpPkey **outKey,
                                                const u8 **inPtr, int len)
{
    if (g_x509Table.d2iPrivateKey != NULL) {
        return g_x509Table.d2iPrivateKey(keyType, outKey, inPtr, len);
    }
    sceSsl_lib_9CDCA0D7(26, 108, 14, NULL, 0);
    return NULL;
}

/* 0x01A170 */
void LIBSSL_NOINLINE sceSsl_lib_E11361C2(SslEvpPkey *pkey)
{
    if (g_x509Table.pkeyUpRef != NULL) {
        g_x509Table.pkeyUpRef(pkey);
        return;
    }
    sceSsl_lib_9CDCA0D7(26, 136, 14, NULL, 0);
}

/* 0x01A1B8 */
int LIBSSL_NOINLINE sceSsl_lib_76A46EDA(void *cert, const u8 *oid, int *outCrit,
                                        const u8 **outData, u32 *outLen)
{
    if (g_x509Table.certGetExt != NULL) {
        return g_x509Table.certGetExt((SslX509TcCert *)cert, oid, outCrit, outData, outLen);
    }
    sceSsl_lib_9CDCA0D7(26, 154, 14, NULL, 0);
    return 0;
}

/* 0x01A204 */
int LIBSSL_NOINLINE sceSsl_lib_7781A781(void *cert, int *outCrit, u32 *outUsage)
{
    const u8 *extData;
    u32 extLen;
    u32 usageBits = 0;

    if (g_x509Table.certGetKeyUsageExt == NULL) {
        sceSsl_lib_9CDCA0D7(26, 155, 14, NULL, 0);
        return 0;
    }
    if (g_x509Table.certGetKeyUsageExt((SslX509TcCert *)cert, outCrit, &extData, &extLen) != 1) {
        return 0;
    }
    if (outUsage != NULL) {
        const u8 *bitBytes = extData + 3;
        int byteIdx;

        if (extLen - 3U >= 5U) {
            return 0;
        }
        if (extLen != 3U) {
            usageBits = (u32)(bitBytes[extLen - 4U] & (0xFFU << extData[2]));
        }
        for (byteIdx = (int)extLen - 5; byteIdx >= 0; byteIdx--) {
            usageBits = (usageBits << 8) | bitBytes[byteIdx];
        }
        *outUsage = usageBits;
    }
    return 1;
}

/* 0x01A2F4 */
int LIBSSL_NOINLINE sceSsl_lib_CFA2F8EB(void *cert, int *outCrit, int *outPathLen, int *outIsCa)
{
    SslAsn1Element elem;
    const u8 *extData;
    u32 extLen;
    int remain;
    const u8 *cursor;

    if (g_x509Table.certGetExt == NULL) {
        sceSsl_lib_9CDCA0D7(26, 136, 14, NULL, 0);
        return 0;
    }
    if (g_x509Table.certGetExt((SslX509TcCert *)cert, g_oidBasicConstraints,
                               outCrit, &extData, &extLen) != 1) {
        return 0;
    }
    if (sceSsl_lib_C85F6FCE(&elem, extData, extLen) != 0) {
        return 0;
    }
    extLen = elem.length;
    extData = elem.data;
    if (outIsCa != NULL) {
        *outIsCa = 0;
    }
    if ((int)extLen <= 0) {
        return 1;
    }
    if (sceSsl_lib_C85F6FCE(&elem, extData, extLen) != 0) {
        return 0;
    }
    if (outIsCa != NULL && elem.tag == 1U) {
        *outIsCa = (int)elem.data[0];
    }
    remain = (int)extLen - (int)(elem.length + elem.headerLength);
    cursor = extData + elem.length + elem.headerLength;
    extLen = (u32)remain;
    extData = cursor;
    if (remain <= 0) {
        return 1;
    }
    if (sceSsl_lib_C85F6FCE(&elem, cursor, (u32)remain) != 0) {
        return 0;
    }
    if (elem.length >= 5U) {
        return 0;
    }
    if (outPathLen != NULL) {
        int pathVal = 0;
        u32 idx;
        for (idx = 0; idx < elem.length; idx++) {
            pathVal = (pathVal << 8) | elem.data[idx];
        }
        *outPathLen = pathVal;
    }
    return 1;
}

/* 0x01A488 */
const char *LIBSSL_NOINLINE sceSsl_lib_2B7BD166(const u8 *oidData, u32 oidLen, int shortName)
{
    if (oidLen != 3U || oidData[0] != 85U || oidData[1] != 4U) {
        return NULL;
    }
    switch (oidData[2]) {
    case 3:  return (shortName != 0) ? "CN"  : "commonName";
    case 4:  return (shortName != 0) ? "S"   : "surname";
    case 5:  return (shortName != 0) ? "SN"  : "serialNumber";
    case 6:  return (shortName != 0) ? "C"   : "countryName";
    case 7:  return (shortName != 0) ? "L"   : "localityName";
    case 8:  return (shortName != 0) ? "ST"  : "stateOrProvinceName";
    case 10: return (shortName != 0) ? "O"   : "organizationName";
    case 11: return (shortName != 0) ? "OU"  : "organizationalUnitName";
    case 12: return (shortName != 0) ? "T"   : "title";
    case 13: return (shortName != 0) ? "D"   : "description";
    case 42: return (shortName != 0) ? "G"   : "givenName";
    case 43: return (shortName != 0) ? "I"   : "initials";
    case 45: return (shortName != 0) ? "UID" : "uniqueIdentifier";
    default: return NULL;
    }
}

/* 0x01A628 */
int LIBSSL_NOINLINE sceSsl_lib_37C7B76C(void *cert, u8 *outBuf, u32 *outLen)
{
    return sslX509Digest(cert, sslEvpMd5(), outBuf, outLen);
}

/* 0x01A678 */
void LIBSSL_NOINLINE sslX509DisableMontCache(void)
{
    sslMontCacheDisable();
}

/* 0x01B524 */
int LIBSSL_NOINLINE sslMontCacheEnabled(void)
{
    return g_montCacheEnabled;
}

/* 0x01B530 */
void LIBSSL_NOINLINE sslMontCacheDisable(void)
{
    g_montCacheEnabled = 0;
}

/* 0x01B53C */
static int LIBSSL_NOINLINE sslX509TcI2dName(const SslX509TcName *name, u8 **outPtr)
{
    if (name == NULL) {
        return 0;
    }
    if (outPtr != NULL && *outPtr != NULL) {
        sslMemcpy(*outPtr, name->rawItem.data, name->rawItem.length);
        *outPtr += name->rawItem.length;
    }
    return (int)name->rawItem.length;
}

/* 0x01B5B0 */
static SslX509TcCert *LIBSSL_NOINLINE sslX509TcNew(void)
{
    SslX509TcCert *cert = (SslX509TcCert *)sceSsl_lib_0861D934(sizeof(SslX509TcCert));
    if (cert == NULL) {
        return NULL;
    }
    sslMemset(cert, 0, sizeof(SslX509TcCert));
    sslAsn1InitItemList(&cert->subject.entries, NULL, 0, 0);
    sslAsn1InitItemList(&cert->issuer.entries, NULL, 0, 0);
    cert->pubKey = NULL;
    cert->parsedFlag = 0;
    cert->refCount = 1;
    return cert;
}

/* 0x01B630 */
static void LIBSSL_NOINLINE sslX509TcFree(SslX509TcCert *cert)
{
    if (cert == NULL) {
        return;
    }
    if (sslCryptoAddLock(&cert->refCount, -1, 3, "crypto/x509tc/x509tc.c", 171) > 0) {
        return;
    }
    sslAsn1FreeItem(&cert->rawCert);
    sslAsn1FreeItem(&cert->issuer.rawItem);
    sslAsn1FreeItem(&cert->subject.rawItem);
    sslAsn1DestroyItemList(&cert->issuer.entries);
    sslAsn1DestroyItemList(&cert->subject.entries);
    if (cert->pubKey != NULL) {
        sslEvpPkeyFree(cert->pubKey);
        cert->pubKey = NULL;
    }
    sceSsl_lib_5E5C873A(cert);
}

/* 0x01B6C4 */
static int LIBSSL_NOINLINE sslX509TcUpRef(SslX509TcCert *cert)
{
    return sslCryptoAddLock(&cert->refCount, 1, 3, "crypto/x509tc/x509tc.c", 189);
}

/* 0x01B6F4 */
static int LIBSSL_NOINLINE sslX509TcVerify(SslX509TcCert *cert, SslEvpPkey *pubKey)
{
    SslAsn1Item *outSlots[3];
    SslAsn1Item parsedItems[3];
    u8 digestBuf[32];
    u32 digestLen;
    const SslEvpMd *md;
    const u8 *expectedDigestOid;
    SslEvpMdCtx *mdCtx;
    SslRsa *rsaKey;
    int verified = 0;

    if (pubKey->type != 6) {
        return 0;
    }
    rsaKey = (SslRsa *)pubKey->pkey;
    outSlots[0] = &parsedItems[0];
    outSlots[1] = &parsedItems[1];
    outSlots[2] = &parsedItems[2];
    sslMemset(parsedItems, 0, sizeof(parsedItems));

    if (sslAsn1ExtractFields(&cert->rawCert, outSlots, g_specCertVerify, 3U, 1U) != 0) {
        return 0;
    }

    if ((parsedItems[1].length == 9U && sslMemcmp(parsedItems[1].data, g_oidSha1WithRsa, 9) == 0) ||
        (parsedItems[1].length == 5U && sslMemcmp(parsedItems[1].data, g_oidSha1WithRsaOiw, 5) == 0)) {
        md = sslEvpSha1();
        expectedDigestOid = g_oidDigestSha1;
    } else if (parsedItems[1].length == 9U && sslMemcmp(parsedItems[1].data, g_oidSha256WithRsa, 9) == 0) {
        md = sslEvpSha256();
        expectedDigestOid = g_oidDigestSha256;
    } else if (parsedItems[1].length == 9U && sslMemcmp(parsedItems[1].data, g_oidSha384WithRsa, 9) == 0) {
        md = sslEvpSha384();
        expectedDigestOid = g_oidDigestSha384;
    } else if (parsedItems[1].length == 9U && sslMemcmp(parsedItems[1].data, g_oidSha512WithRsa, 9) == 0) {
        md = sslEvpSha512();
        expectedDigestOid = g_oidDigestSha512;
    } else if (parsedItems[1].length == 9U && sslMemcmp(parsedItems[1].data, g_oidSha224WithRsa, 9) == 0) {
        md = sslEvpSha224();
        expectedDigestOid = g_oidDigestSha224;
    } else if (parsedItems[1].length == 9U && sslMemcmp(parsedItems[1].data, g_oidMd2WithRsa, 9) == 0) {
        md = sslEvpMd2();
        expectedDigestOid = g_oidDigestMd2;
    } else if ((parsedItems[1].length == 9U && sslMemcmp(parsedItems[1].data, g_oidMd5WithRsa, 9) == 0) ||
               (parsedItems[1].length == 5U && sslMemcmp(parsedItems[1].data, g_oidMd5WithRsaOiw, 5) == 0)) {
        md = sslEvpMd5();
        expectedDigestOid = g_oidDigestMd5;
    } else if (parsedItems[1].length == 8U && sslMemcmp(parsedItems[1].data, g_oidDigestMd5, 8) == 0) {
        md = sslEvpMd5();
        expectedDigestOid = g_oidDigestMd5;
    } else {
        return 0;
    }

    mdCtx = sslEvpMdCtxNew();
    if (mdCtx == NULL) {
        return 0;
    }
    if (sslEvpDigestInit(mdCtx, md) != 0) {
        int sigLen;
        u8 *decryptedBuf;

        sslEvpDigestUpdate(mdCtx, parsedItems[0].data, parsedItems[0].length);
        sslEvpDigestFinal(mdCtx, digestBuf, &digestLen);
        sslEvpMdCtxFree(mdCtx);
        mdCtx = NULL;

        sigLen = (int)parsedItems[2].length - 1;
        if (sigLen != sslRsaSize(rsaKey)) {
            return 0;
        }
        decryptedBuf = (u8 *)sceSsl_lib_0861D934((u32)sigLen);
        if (decryptedBuf != NULL) {
            int decLen = sslRsaPublicDecrypt(sigLen, ((const u8 *)parsedItems[2].data) + 1,
                                             decryptedBuf, rsaKey, 1);
            if (decLen > 0) {
                sslMemset(parsedItems, 0, sizeof(parsedItems));
                if (sslAsn1SetItemData(&parsedItems[2], 48U, decryptedBuf, (u32)decLen, 1U) == 0 &&
                    sslAsn1ExtractFields(&parsedItems[2], outSlots, g_specDigestInfo, 2U, 1U) == 0) {
                    u32 oidLen = (u32)sslStrlen((const char *)expectedDigestOid);
                    if (parsedItems[0].length == oidLen &&
                        sslMemcmp(parsedItems[0].data, expectedDigestOid, (int)oidLen) == 0 &&
                        parsedItems[1].length == digestLen &&
                        sslMemcmp(parsedItems[1].data, digestBuf, (int)digestLen) == 0) {
                        verified = 1;
                    }
                }
            }
            sceSsl_lib_5E5C873A(decryptedBuf);
        }
    }
    if (mdCtx != NULL) {
        sslEvpMdCtxFree(mdCtx);
    }
    return verified;
}

/* 0x01BB38 */
static SslX509TcCert *LIBSSL_NOINLINE sslX509TcD2i(SslX509TcCert **outCert, const u8 **inPtr, u32 len)
{
    SslX509TcCert *cert;
    SslAsn1Item *fieldSlots[5];
    u32 consumed = 0;

    if (outCert != NULL && *outCert != NULL) {
        cert = *outCert;
        sslAsn1FreeItem(&cert->rawCert);
        sslAsn1FreeItem(&cert->issuer.rawItem);
        sslAsn1FreeItem(&cert->subject.rawItem);
        sslMemset(&cert->serialNumber, 0, sizeof(cert->serialNumber));
        sslMemset(&cert->notAfter, 0, sizeof(cert->notAfter));
        sslMemset(&cert->notBefore, 0, sizeof(cert->notBefore));
        cert->cachedSigType = 0;
        cert->cachedVersion = 0;
    } else {
        cert = sslX509TcNew();
    }

    if (cert != NULL) {
        cert->parsedFlag = 0;
        if (sslAsn1DecodeSingleItem(&cert->rawCert, &consumed, 40U, *inPtr, len, 0U) != 0) {
            return NULL;
        }
        *inPtr += consumed;
        fieldSlots[0] = &cert->subject.rawItem;
        fieldSlots[1] = &cert->issuer.rawItem;
        fieldSlots[2] = &cert->serialNumber;
        fieldSlots[3] = &cert->notAfter;
        fieldSlots[4] = &cert->notBefore;
        if (sslAsn1ExtractFields(&cert->rawCert, fieldSlots, g_specCertFields, 5U, 1U) == 0) {
            if (outCert != NULL && *outCert == NULL) {
                *outCert = cert;
            }
            return cert;
        }
    }

    if (outCert == NULL || *outCert == NULL) {
        if (cert != NULL) {
            sslX509TcFree(cert);
        }
    }
    return NULL;
}

/* 0x01BCB8 */
static int LIBSSL_NOINLINE sslX509TcI2d(const SslX509TcCert *cert, u8 **outPtr)
{
    if (cert == NULL) {
        return 0;
    }
    if (outPtr != NULL && *outPtr != NULL) {
        sslMemcpy(*outPtr, cert->rawCert.data, cert->rawCert.length);
        *outPtr += cert->rawCert.length;
    }
    return (int)cert->rawCert.length;
}

/* 0x01BD2C */
static SslEvpPkey *LIBSSL_NOINLINE sslX509TcGetPubkey(SslX509TcCert *cert)
{
    SslAsn1Item pubkeyItem;
    SslEvpPkey *pkey;

    if (cert->pubKey != NULL) {
        return cert->pubKey;
    }
    if (sslX509TcExtractPubkeyItem(cert, &pubkeyItem) == 0) {
        return NULL;
    }
    pkey = sslEvpPkeyNew();
    if (sslRsaDecodePubKeyItem(pkey, &pubkeyItem) != 0) {
        if (pkey != NULL) {
            sslEvpPkeyFree(pkey);
        }
        return NULL;
    }
    cert->pubKey = pkey;
    return pkey;
}

/* 0x01BDB4 */
static int LIBSSL_NOINLINE sslX509TcExtractPubkeyItem(SslX509TcCert *cert, SslAsn1Item *outItem)
{
    SslAsn1Item *slotPtr = outItem;

    sslAsn1ClearItem(outItem);
    return (sslAsn1ExtractFields(&cert->rawCert, &slotPtr, g_specPubkeyInfo, 1U, 1U) == 0) ? 1 : 0;
}

/* 0x01BE00 */
static int LIBSSL_NOINLINE sslX509TcGetSigType(SslX509TcCert *cert)
{
    SslAsn1Item oidItem;
    SslAsn1Item *slotPtr = &oidItem;
    u32 oidLen;
    const u8 *oidData;

    sslAsn1ClearItem(&oidItem);
    if (sslAsn1ExtractFields(&cert->rawCert, &slotPtr, g_specSigAlgOid, 1U, 1U) != 0) {
        return 0;
    }
    oidLen = oidItem.length;
    oidData = (const u8 *)oidItem.data;

    if (oidLen == 9U &&
        (sslMemcmp(oidData, g_oidMd2WithRsa, 9) == 0 ||
         sslMemcmp(oidData, g_oidMd5WithRsa, 9) == 0)) {
        return 6;
    }
    if (oidLen == 5U &&
        (sslMemcmp(oidData, g_oidMd5WithRsaOiw, 5) == 0 ||
         sslMemcmp(oidData, g_oidShaWithRsaOiw, 5) == 0)) {
        return 6;
    }
    if (oidLen == 9U && sslMemcmp(oidData, g_oidSha1WithRsa, 9) == 0) {
        return 6;
    }
    if (oidLen == 5U && sslMemcmp(oidData, g_oidSha1WithRsaOiw, 5) == 0) {
        return 6;
    }
    if (oidLen == 9U &&
        (sslMemcmp(oidData, g_oidSha256WithRsa, 9) == 0 ||
         sslMemcmp(oidData, g_oidSha384WithRsa, 9) == 0 ||
         sslMemcmp(oidData, g_oidSha512WithRsa, 9) == 0 ||
         sslMemcmp(oidData, g_oidSha224WithRsa, 9) == 0)) {
        return 6;
    }
    if (oidLen == 6U && sslMemcmp(oidData, g_oidRipemd160WithRsa, 6) == 0) {
        return 6;
    }
    if (oidLen == 5U && sslMemcmp(oidData, g_oidDsaWithShaOiw, 5) == 0) {
        return 116;
    }
    if (oidLen == 7U && sslMemcmp(oidData, g_oidDsaWithSha1, 7) == 0) {
        return 116;
    }
    if (oidLen == 5U && sslMemcmp(oidData, g_oidDsaWithSha1Oiw, 5) == 0) {
        return 116;
    }
    return 0;
}

/* 0x01C04C */
static SslX509TcName *LIBSSL_NOINLINE sslX509TcGetIssuerName(SslX509TcCert *cert)
{
    return &cert->issuer;
}

/* 0x01C054 */
static SslX509TcName *LIBSSL_NOINLINE sslX509TcGetSubjectName(SslX509TcCert *cert)
{
    return &cert->subject;
}

/* 0x01C05C */
static int LIBSSL_NOINLINE sslX509TcGetKeyUsageExt(SslX509TcCert *cert, int *outCrit,
                                                   const u8 **outData, u32 *outLen)
{
    return sslX509TcGetExtension(cert, g_oidKeyUsage, outCrit, outData, outLen);
}

/* 0x01C090 */
static u32 LIBSSL_NOINLINE sslX509TcNameHash(const SslX509TcName *name)
{
    u8 digest[16];

    sceKernelUtilsMd5Digest((u8 *)name->rawItem.data, name->rawItem.length, digest);
    return (u32)digest[0] | ((u32)digest[1] << 8) | ((u32)digest[2] << 16) | ((u32)digest[3] << 24);
}

/* 0x01C0DC */
static int LIBSSL_NOINLINE sslX509TcNameCmp(const SslX509TcName *lhs, const SslX509TcName *rhs)
{
    int diff = (int)lhs->rawItem.length - (int)rhs->rawItem.length;
    if (diff == 0) {
        return sslMemcmp(lhs->rawItem.data, rhs->rawItem.data, (int)lhs->rawItem.length);
    }
    return diff;
}

/* 0x01C114 */
static int LIBSSL_NOINLINE sslX509TcSubjectNameCmp(const SslX509TcCert *lhs, const SslX509TcCert *rhs)
{
    return sslX509TcNameCmp(&lhs->subject, &rhs->subject);
}

/* 0x01C134 */
static int LIBSSL_NOINLINE sslX509TcGetSerialItem(const SslX509TcCert *cert, SslAsn1Item *outItem)
{
    if (cert == NULL || outItem == NULL) {
        return 0;
    }
    *outItem = cert->serialNumber;
    return 1;
}

/* 0x01C17C */
static int LIBSSL_NOINLINE sslX509TcGetNotAfterItem(const SslX509TcCert *cert, SslAsn1Item *outItem)
{
    if (cert == NULL || outItem == NULL) {
        return 0;
    }
    *outItem = cert->notAfter;
    return 1;
}

/* 0x01C1C4 */
static int LIBSSL_NOINLINE sslX509TcGetNotBeforeItem(const SslX509TcCert *cert, SslAsn1Item *outItem)
{
    if (cert == NULL || outItem == NULL) {
        return 0;
    }
    *outItem = cert->notBefore;
    return 1;
}

/* 0x01C20C */
static int LIBSSL_NOINLINE sslX509TcGetVersion(SslX509TcCert *cert)
{
    SslAsn1Item versionItem;
    SslAsn1Item *slotPtr = &versionItem;
    int versionNum = 0;

    if (cert == NULL) {
        return 0;
    }
    sslMemset(&versionItem, 0, sizeof(versionItem));
    if (sslAsn1ExtractFields(&cert->rawCert, &slotPtr, g_specVersion, 1U, 1U) == 0) {
        if (versionItem.itemType == 32U) {
            return 0;
        }
        {
            u32 idx;
            const u8 *bytes = (const u8 *)versionItem.data;
            for (idx = 0; idx < versionItem.length; idx++) {
                versionNum = (versionNum << 8) + bytes[idx];
            }
            versionNum++;
        }
    }
    return versionNum;
}

/* 0x01C2B4 */
static int LIBSSL_NOINLINE sslX509TcDigest(SslX509TcCert *cert, const SslEvpMd *md,
                                           u8 *outBuf, u32 *outLen)
{
    SslEvpMdCtx *mdCtx = sslEvpMdCtxNew();
    int ret = 0;

    if (mdCtx != NULL) {
        if (sslEvpDigestInit(mdCtx, md) != 0) {
            ret = 1;
            sslEvpDigestUpdate(mdCtx, cert->rawCert.data, cert->rawCert.length);
            sslEvpDigestFinal(mdCtx, outBuf, outLen);
        }
        sslEvpMdCtxFree(mdCtx);
    }
    return ret;
}

/* 0x01C354 */
static int LIBSSL_NOINLINE sslX509TcDecodeNameEntries(SslX509TcName *name, SslAsn1ItemList *outList)
{
    SslAsn1ItemList rdnList;
    int status;

    sslAsn1InitItemList(&rdnList, NULL, 0, 0);
    status = sslAsn1DecodeItemList(&name->rawItem, &rdnList, 4097U);
    if (status == 0) {
        u32 rdnIdx;
        for (rdnIdx = 0; rdnIdx < rdnList.count; rdnIdx++) {
            u32 prevCount = outList->count;
            status = sslAsn1DecodeItemList(&rdnList.items[rdnIdx], outList, 1U);
            if (status != 0) {
                break;
            }
            while (prevCount < outList->count) {
                outList->items[prevCount].itemType = 46U;
                outList->items[prevCount].reservedWord = rdnIdx;
                prevCount++;
            }
        }
    }
    sslAsn1DestroyItemList(&rdnList);
    return status;
}

/* 0x01C46C */
static int LIBSSL_NOINLINE sslX509TcNameEntryCount(SslX509TcName *name)
{
    if (name == NULL) {
        return 0;
    }
    if (name->entries.count != 0) {
        return (int)name->entries.count;
    }
    if (sslX509TcDecodeNameEntries(name, &name->entries) == 0) {
        return (int)name->entries.count;
    }
    return 0;
}

/* 0x01C4B8 */
static SslAsn1Item *LIBSSL_NOINLINE sslX509TcNameGetEntry(SslX509TcName *name, int entryIndex)
{
    if (name == NULL) {
        return NULL;
    }
    if (name->entries.count == 0 && sslX509TcDecodeNameEntries(name, &name->entries) != 0) {
        return NULL;
    }
    if (entryIndex < 0 || (u32)entryIndex >= name->entries.count) {
        return NULL;
    }
    return &name->entries.items[entryIndex];
}

/* 0x01C534 */
static int LIBSSL_NOINLINE sslX509TcNameGetItem(const SslX509TcName *name, SslAsn1Item *outItem)
{
    if (outItem != NULL) {
        *outItem = name->rawItem;
    }
    return 1;
}

/* 0x01C56C */
static int LIBSSL_NOINLINE sslX509TcNameEntryGetData(const SslAsn1Item *entry, SslAsn1Item *outOid,
                                                     SslAsn1Item *outValue)
{
    SslAsn1ItemList pairList;
    SslAsn1Item pairItems[2];

    if (entry->itemType != 46U) {
        return 0;
    }
    sslAsn1InitItemList(&pairList, pairItems, 0, 2);
    if (sslAsn1DecodeItemList(entry, &pairList, 1U) != 0 || pairItems[0].itemType != 6U) {
        return 0;
    }
    if (outOid != NULL) {
        *outOid = pairItems[0];
    }
    if (outValue != NULL) {
        *outValue = pairItems[1];
    }
    return 1;
}

/* 0x01C650 */
static SslX509TcName *LIBSSL_NOINLINE sslX509TcNameNew(void)
{
    SslX509TcName *name = (SslX509TcName *)sceSsl_lib_0861D934(sizeof(SslX509TcName));
    if (name != NULL) {
        sslMemset(name, 0, sizeof(SslX509TcName));
    }
    return name;
}

/* 0x01C698 */
static void LIBSSL_NOINLINE sslX509TcNameFree(SslX509TcName *name)
{
    if (name != NULL) {
        sceSsl_lib_5E5C873A(name);
    }
}

/* 0x01C6B8 */
static SslX509TcName *LIBSSL_NOINLINE sslX509TcNameDup(SslX509TcName *name)
{
    (void)name;
    return NULL;
}

/* 0x01C6C0 */
static SslX509TcName *LIBSSL_NOINLINE sslX509TcD2iName(SslX509TcName **outName,
                                                       const u8 **inPtr, int len)
{
    SslX509TcName *name;

    if (outName != NULL && *outName != NULL) {
        name = *outName;
        sslMemset(&name->rawItem, 0, sizeof(name->rawItem));
        sslMemset(&name->entries, 0, sizeof(name->entries));
    } else {
        name = sslX509TcNameNew();
    }

    if (name == NULL || sslAsn1SetItemData(&name->rawItem, 45U, *inPtr, (u32)len, 1U) != 0) {
        if (outName == NULL || *outName == NULL) {
            if (name != NULL) {
                sslX509TcNameFree(name);
            }
        }
        return NULL;
    }

    if (outName != NULL && *outName == NULL) {
        *outName = name;
    }
    *inPtr += len;
    return name;
}

/* 0x01C7B8 */
static int LIBSSL_NOINLINE sslX509TcCheckPrivateKey(SslX509TcCert *cert, SslEvpPkey *privKey)
{
    SslEvpPkey *pubKey = sslX509TcGetPubkey(cert);
    int keyType = sslEvpPkeyGetType(pubKey);

    if (keyType != sslEvpPkeyGetType(privKey)) {
        return 0;
    }
    if (keyType == 6) {
        SslRsa *pubRsa = (SslRsa *)pubKey->pkey;
        SslRsa *privRsa = (SslRsa *)privKey->pkey;
        if (sslBnCmp(pubRsa->modN, privRsa->modN) == 0 &&
            sslBnCmp(pubRsa->pubE, privRsa->pubE) == 0) {
            return 1;
        }
    }
    return 0;
}

/* 0x01C85C */
static int LIBSSL_NOINLINE sslX509TcGetExtension(SslX509TcCert *cert, const u8 *oid, int *outCrit,
                                                 const u8 **outData, u32 *outLen)
{
    SslAsn1Item extSeq;
    SslAsn1Item *slotPtr = &extSeq;
    SslAsn1Element extElem;
    SslAsn1Element oidElem;
    int remain;
    const u8 *cursor;
    u32 targetOidLen;

    if (outCrit != NULL) {
        *outCrit = 0;
    }
    if (cert == NULL) {
        return 0;
    }
    sslMemset(&extSeq, 0, sizeof(extSeq));
    if (sslAsn1ExtractFields(&cert->rawCert, &slotPtr, g_specExtensions, 1U, 1U) != 0) {
        return 0;
    }

    remain = (int)extSeq.length;
    cursor = (const u8 *)extSeq.data;
    targetOidLen = (u32)sslStrlen((const char *)oid);

    while (remain > 0) {
        int step;
        if (sceSsl_lib_C85F6FCE(&extElem, cursor, (u32)remain) != 0 ||
            sceSsl_lib_C85F6FCE(&oidElem, extElem.data, extElem.length) != 0) {
            return 0;
        }
        if (oidElem.length == targetOidLen &&
            sslMemcmp(oidElem.data, oid, (int)oidElem.length) == 0) {
            int innerRem = (int)extElem.length - (int)(oidElem.headerLength + oidElem.length);
            const u8 *innerPtr = extElem.data + oidElem.headerLength + oidElem.length;
            if (innerRem >= 3 && innerPtr[0] == 1U && innerPtr[1] == 1U) {
                if (outCrit != NULL) {
                    *outCrit = (int)innerPtr[2];
                }
                innerPtr += 3;
                innerRem -= 3;
            }
            if (sceSsl_lib_C85F6FCE(&extElem, innerPtr, (u32)innerRem) == 0) {
                *outData = extElem.data;
                *outLen = extElem.length;
                return 1;
            }
            return 0;
        }
        step = (int)(extElem.headerLength + extElem.length);
        if (remain < step) {
            return 0;
        }
        remain -= step;
        cursor += step;
    }
    return 0;
}
