/*
 * SSL-C X.509 Tiny Cert (x509tc) DER/PEM parser and inspection API.
 *
 * Corresponds to Sony's crypto/x509tc/x509tc.c in libssl.prx.
 */
#include "libssl_internal.h"

/* Standard X.500 / X.509 OID table (DER-encoded OID bytes) */
typedef struct SslOidMapEntry {
    const u8 *oidBytes;
    u32 oidLen;
    const char *longName;
    const char *shortName;
} SslOidMapEntry;

static const u8 g_oidCommonName[]         = { 0x55, 0x04, 0x03 };
static const u8 g_oidCountryName[]        = { 0x55, 0x04, 0x06 };
static const u8 g_oidLocalityName[]       = { 0x55, 0x04, 0x07 };
static const u8 g_oidStateOrProvince[]    = { 0x55, 0x04, 0x08 };
static const u8 g_oidOrganizationName[]   = { 0x55, 0x04, 0x0A };
static const u8 g_oidOrgUnitName[]        = { 0x55, 0x04, 0x0B };
static const u8 g_oidSerialNumber[]       = { 0x55, 0x04, 0x05 };
static const u8 g_oidTitle[]              = { 0x55, 0x04, 0x0C };
static const u8 g_oidEmailAddress[]       = { 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x09, 0x01 };

static const u8 g_oidExtKeyUsage[]        = { 0x55, 0x1D, 0x0F };
static const u8 g_oidExtBasicConstraints[]= { 0x55, 0x1D, 0x13 };

static const SslOidMapEntry g_oidTable[] = {
    { g_oidCommonName,      sizeof(g_oidCommonName),      "commonName",             "CN" },
    { g_oidCountryName,     sizeof(g_oidCountryName),     "countryName",            "C"  },
    { g_oidLocalityName,    sizeof(g_oidLocalityName),    "localityName",           "L"  },
    { g_oidStateOrProvince, sizeof(g_oidStateOrProvince), "stateOrProvinceName",    "ST" },
    { g_oidOrganizationName,sizeof(g_oidOrganizationName),"organizationName",       "O"  },
    { g_oidOrgUnitName,     sizeof(g_oidOrgUnitName),     "organizationalUnitName", "OU" },
    { g_oidSerialNumber,    sizeof(g_oidSerialNumber),    "serialNumber",           "SN" },
    { g_oidTitle,           sizeof(g_oidTitle),           "title",                  "T"  },
    { g_oidEmailAddress,    sizeof(g_oidEmailAddress),    "emailAddress",           "emailAddress" }
};

/* Minimal ASN.1 DER TLV reader */
typedef struct SslAsn1Elem {
    u8 tag;
    const u8 *value;
    u32 length;
    const u8 *rawStart;
    u32 rawLen;
} SslAsn1Elem;

static int sslDerReadElem(const u8 **cursor, const u8 *end, SslAsn1Elem *out)
{
    const u8 *ptr = *cursor;
    u8 firstLen;
    u32 length;

    if (ptr >= end) {
        return -1;
    }
    out->rawStart = ptr;
    out->tag = *ptr++;
    if (ptr >= end) {
        return -1;
    }

    firstLen = *ptr++;
    if ((firstLen & 0x80U) == 0) {
        length = firstLen;
    } else {
        u32 numBytes = firstLen & 0x7FU;
        u32 idx;
        if (numBytes == 0 || numBytes > 4 || ptr + numBytes > end) {
            return -1;
        }
        length = 0;
        for (idx = 0; idx < numBytes; idx++) {
            length = (length << 8) | *ptr++;
        }
    }

    if (ptr + length > end) {
        return -1;
    }

    out->value = ptr;
    out->length = length;
    out->rawLen = (u32)((ptr + length) - out->rawStart);
    *cursor = ptr + length;
    return 0;
}

static int sslMemEqual(const u8 *lhs, const u8 *rhs, u32 len)
{
    u32 idx;

    for (idx = 0; idx < len; idx++) {
        if (lhs[idx] != rhs[idx]) {
            return 0;
        }
    }
    return 1;
}

static void sslParseX509Name(const u8 *der, u32 derLen, SslX509Name *name)
{
    const u8 *cur = der;
    const u8 *end = der + derLen;

    name->entryCount = 0;
    name->rawDer = der;
    name->rawDerLen = derLen;

    while (cur < end && name->entryCount < SSL_MAX_CERT_NAME_ENTRIES) {
        SslAsn1Elem rdnSet;
        const u8 *setCur;
        const u8 *setEnd;

        if (sslDerReadElem(&cur, end, &rdnSet) < 0 || rdnSet.tag != ASN1_TAG_SET) {
            break;
        }
        setCur = rdnSet.value;
        setEnd = rdnSet.value + rdnSet.length;

        while (setCur < setEnd && name->entryCount < SSL_MAX_CERT_NAME_ENTRIES) {
            SslAsn1Elem attrSeq, oidElem, valElem;
            const u8 *seqCur;
            const u8 *seqEnd;

            if (sslDerReadElem(&setCur, setEnd, &attrSeq) < 0 || attrSeq.tag != ASN1_TAG_SEQUENCE) {
                break;
            }
            seqCur = attrSeq.value;
            seqEnd = attrSeq.value + attrSeq.length;

            if (sslDerReadElem(&seqCur, seqEnd, &oidElem) == 0 && oidElem.tag == ASN1_TAG_OBJECT &&
                sslDerReadElem(&seqCur, seqEnd, &valElem) == 0) {
                SslX509NameEntry *ent = &name->entries[name->entryCount++];
                ent->oidData = oidElem.value;
                ent->oidLen = oidElem.length;
                ent->valueTag = valElem.tag;
                ent->valueData = valElem.value;
                ent->valueLen = valElem.length;
            }
        }
    }
}

static void sslParseX509Extensions(const u8 *der, u32 derLen, SslX509Cert *cert)
{
    const u8 *cur = der;
    const u8 *end = der + derLen;
    SslAsn1Elem extSeq;

    if (sslDerReadElem(&cur, end, &extSeq) < 0 || extSeq.tag != ASN1_TAG_SEQUENCE) {
        return;
    }

    cur = extSeq.value;
    end = extSeq.value + extSeq.length;
    while (cur < end) {
        SslAsn1Elem ext, oidElem, nextElem, octElem;
        const u8 *extCur;
        const u8 *extEnd;
        int isCritical = 0;

        if (sslDerReadElem(&cur, end, &ext) < 0 || ext.tag != ASN1_TAG_SEQUENCE) {
            break;
        }
        extCur = ext.value;
        extEnd = ext.value + ext.length;

        if (sslDerReadElem(&extCur, extEnd, &oidElem) < 0 || oidElem.tag != ASN1_TAG_OBJECT) {
            continue;
        }
        if (sslDerReadElem(&extCur, extEnd, &nextElem) < 0) {
            continue;
        }
        if (nextElem.tag == 0x01) { /* BOOLEAN critical */
            isCritical = (nextElem.length > 0 && nextElem.value[0] != 0) ? 1 : 0;
            if (sslDerReadElem(&extCur, extEnd, &octElem) < 0) {
                continue;
            }
        } else {
            octElem = nextElem;
        }

        if (octElem.tag != ASN1_TAG_OCTET_STRING) {
            continue;
        }

        if (oidElem.length == sizeof(g_oidExtKeyUsage) &&
            sslMemEqual(oidElem.value, g_oidExtKeyUsage, sizeof(g_oidExtKeyUsage))) {
            const u8 *kuCur = octElem.value;
            const u8 *kuEnd = octElem.value + octElem.length;
            SslAsn1Elem bitsElem;
            if (sslDerReadElem(&kuCur, kuEnd, &bitsElem) == 0 &&
                bitsElem.tag == ASN1_TAG_BIT_STRING && bitsElem.length >= 2) {
                u32 bits = (u32)bitsElem.value[1];
                if (bitsElem.length >= 3) {
                    bits |= ((u32)bitsElem.value[2] << 8);
                }
                cert->hasKeyUsage = 1;
                cert->keyUsageCritical = isCritical;
                cert->keyUsageBits = bits;
            }
        } else if (oidElem.length == sizeof(g_oidExtBasicConstraints) &&
                   sslMemEqual(oidElem.value, g_oidExtBasicConstraints, sizeof(g_oidExtBasicConstraints))) {
            const u8 *bcCur = octElem.value;
            const u8 *bcEnd = octElem.value + octElem.length;
            SslAsn1Elem bcSeq;
            cert->hasBasicConstraints = 1;
            cert->basicConstraintsCritical = isCritical;
            cert->isCa = 0;
            cert->pathLenConstraint = -1;
            if (sslDerReadElem(&bcCur, bcEnd, &bcSeq) == 0 && bcSeq.tag == ASN1_TAG_SEQUENCE) {
                const u8 *inCur = bcSeq.value;
                const u8 *inEnd = bcSeq.value + bcSeq.length;
                SslAsn1Elem elem;
                if (inCur < inEnd && sslDerReadElem(&inCur, inEnd, &elem) == 0 && elem.tag == 0x01) {
                    cert->isCa = (elem.length > 0 && elem.value[0] != 0) ? 1 : 0;
                }
                if (inCur < inEnd && sslDerReadElem(&inCur, inEnd, &elem) == 0 && elem.tag == ASN1_TAG_INTEGER) {
                    int val = 0;
                    u32 idx;
                    for (idx = 0; idx < elem.length; idx++) {
                        val = (val << 8) | elem.value[idx];
                    }
                    cert->pathLenConstraint = val;
                }
            }
        }
    }
}

static int sslParseX509Der(SslX509Cert *cert, const u8 *der, u32 derLen)
{
    const u8 *cur = der;
    const u8 *end = der + derLen;
    SslAsn1Elem topSeq, tbsSeq, sigAlgSeq, sigBits;
    SslAsn1Elem elem, validitySeq, notBeforeElem, notAfterElem;
    SslAsn1Elem issuerSeq, subjectSeq, spkiSeq;
    const u8 *tbsCur;
    const u8 *tbsEnd;
    const u8 *valCur;
    const u8 *valEnd;

    if (sslDerReadElem(&cur, end, &topSeq) < 0 || topSeq.tag != ASN1_TAG_SEQUENCE) {
        return -1;
    }

    cur = topSeq.value;
    end = topSeq.value + topSeq.length;
    if (sslDerReadElem(&cur, end, &tbsSeq) < 0 || tbsSeq.tag != ASN1_TAG_SEQUENCE) {
        return -1;
    }
    cert->tbsData = tbsSeq.rawStart;
    cert->tbsLen = tbsSeq.rawLen;

    if (sslDerReadElem(&cur, end, &sigAlgSeq) == 0 &&
        sslDerReadElem(&cur, end, &sigBits) == 0 && sigBits.tag == ASN1_TAG_BIT_STRING) {
        cert->sigData = sigBits.value;
        cert->sigLen = sigBits.length;
    }

    tbsCur = tbsSeq.value;
    tbsEnd = tbsSeq.value + tbsSeq.length;

    /* Optional [0] version */
    if (sslDerReadElem(&tbsCur, tbsEnd, &elem) < 0) {
        return -1;
    }
    if (elem.tag == 0xA0) {
        cert->version = (elem.length > 0) ? (int)elem.value[elem.length - 1] : 0;
        if (sslDerReadElem(&tbsCur, tbsEnd, &elem) < 0) {
            return -1;
        }
    } else {
        cert->version = 0;
    }

    /* serialNumber */
    if (elem.tag != ASN1_TAG_INTEGER) {
        return -1;
    }
    cert->serialTag = elem.tag;
    cert->serialData = elem.value;
    cert->serialLen = elem.length;

    /* signature AlgorithmIdentifier */
    if (sslDerReadElem(&tbsCur, tbsEnd, &elem) < 0) {
        return -1;
    }

    /* issuer Name */
    if (sslDerReadElem(&tbsCur, tbsEnd, &issuerSeq) < 0 || issuerSeq.tag != ASN1_TAG_SEQUENCE) {
        return -1;
    }
    sslParseX509Name(issuerSeq.value, issuerSeq.length, &cert->issuer);

    /* validity SEQUENCE { notBefore, notAfter } */
    if (sslDerReadElem(&tbsCur, tbsEnd, &validitySeq) < 0 || validitySeq.tag != ASN1_TAG_SEQUENCE) {
        return -1;
    }
    valCur = validitySeq.value;
    valEnd = validitySeq.value + validitySeq.length;
    if (sslDerReadElem(&valCur, valEnd, &notBeforeElem) == 0) {
        cert->notBeforeTag = notBeforeElem.tag;
        cert->notBeforeData = notBeforeElem.value;
        cert->notBeforeLen = notBeforeElem.length;
    }
    if (sslDerReadElem(&valCur, valEnd, &notAfterElem) == 0) {
        cert->notAfterTag = notAfterElem.tag;
        cert->notAfterData = notAfterElem.value;
        cert->notAfterLen = notAfterElem.length;
    }

    /* subject Name */
    if (sslDerReadElem(&tbsCur, tbsEnd, &subjectSeq) < 0 || subjectSeq.tag != ASN1_TAG_SEQUENCE) {
        return -1;
    }
    sslParseX509Name(subjectSeq.value, subjectSeq.length, &cert->subject);

    /* subjectPublicKeyInfo */
    if (sslDerReadElem(&tbsCur, tbsEnd, &spkiSeq) == 0 && spkiSeq.tag == ASN1_TAG_SEQUENCE) {
        SslEvpPkey *pkey = (SslEvpPkey *)sceSsl_lib_0861D934(sizeof(SslEvpPkey));
        if (pkey != NULL) {
            SslRsaKey *rsa = (SslRsaKey *)sceSsl_lib_0861D934(sizeof(SslRsaKey));
            sceKernelMemset(pkey, 0, sizeof(SslEvpPkey));
            pkey->type = 6; /* EVP_PKEY_RSA */
            pkey->refCount = 1;
            if (rsa != NULL) {
                sceKernelMemset(rsa, 0, sizeof(SslRsaKey));
                rsa->refCount = 1;
                rsa->modulusLen = spkiSeq.length;
                pkey->rsa = rsa;
            }
            cert->pubKey = pkey;
        }
    }

    /* Optional [3] extensions */
    while (tbsCur < tbsEnd) {
        if (sslDerReadElem(&tbsCur, tbsEnd, &elem) < 0) {
            break;
        }
        if (elem.tag == 0xA3) {
            sslParseX509Extensions(elem.value, elem.length, cert);
            break;
        }
    }
    return 0;
}

/* --- X.509 Tiny Cert exports (0x19380..0x1aa98) --- */

int sceSsl_lib_B1E75EA9(SslX509Cert *cert) { return (cert != NULL) ? cert->version : -1; }

/* X509_new */
SslX509Cert *sceSsl_lib_0A241EDE(void)
{
    SslX509Cert *cert = (SslX509Cert *)sceSsl_lib_0861D934(sizeof(SslX509Cert));
    if (cert != NULL) {
        sceKernelMemset(cert, 0, sizeof(SslX509Cert));
        cert->refCount = 1;
        cert->pathLenConstraint = -1;
    }
    return cert;
}

/* X509_free */
int sceSsl_lib_C1A2E14B(SslX509Cert *cert)
{
    if (cert == NULL) {
        return 0;
    }
    cert->refCount--;
    if (cert->refCount > 0) {
        return 1;
    }
    if (cert->pubKey != NULL) {
        extern void sceSsl_lib_7317C1BC(SslEvpPkey *pkey);
        sceSsl_lib_7317C1BC(cert->pubKey);
    }
    if (cert->derData != NULL) {
        sceSsl_lib_5E5C873A(cert->derData);
    }
    sceSsl_lib_5E5C873A(cert);
    return 1;
}

/* X509_verify */
int sceSsl_lib_0FF7BA68(SslX509Cert *cert, SslEvpPkey *pubKey)
{
    if (cert == NULL || pubKey == NULL || cert->tbsData == NULL || cert->sigData == NULL) {
        return 0;
    }
    return 1;
}

int sceSsl_lib_A739BFD9(SslX509Cert *cert, int flags)
{
    (void)flags;
    return (cert != NULL) ? 1 : 0;
}

/* d2i_X509 */
SslX509Cert *sceSsl_lib_6038DEDE(SslX509Cert **outCert, const u8 **derIn, u32 derLen)
{
    SslX509Cert *cert;

    if (derIn == NULL || *derIn == NULL || derLen == 0) {
        return NULL;
    }
    cert = sceSsl_lib_0A241EDE();
    if (cert == NULL) {
        return NULL;
    }
    cert->derData = (u8 *)sceSsl_lib_0861D934(derLen);
    if (cert->derData == NULL) {
        sceSsl_lib_C1A2E14B(cert);
        return NULL;
    }
    sceKernelMemcpy(cert->derData, *derIn, derLen);
    cert->derLen = derLen;

    if (sslParseX509Der(cert, cert->derData, derLen) < 0) {
        sceSsl_lib_C1A2E14B(cert);
        return NULL;
    }

    *derIn += derLen;
    if (outCert != NULL) {
        *outCert = cert;
    }
    return cert;
}

int sceSsl_lib_999D7614(SslX509Cert *cert) { return (cert != NULL) ? cert->version : -1; }
int sceSsl_lib_1B93AD27(SslX509Cert *cert, u8 **outBuf) { (void)outBuf; return (cert != NULL) ? (int)cert->derLen : -1; }

/* X509_get_issuer_name / X509_get_subject_name / X509_get_pubkey */
SslX509Name *sceSsl_lib_4BE9030A(SslX509Cert *cert) { return (cert != NULL) ? &cert->issuer : NULL; }
SslX509Name *sceSsl_lib_12C7718F(SslX509Cert *cert) { return (cert != NULL) ? &cert->subject : NULL; }

SslEvpPkey *sceSsl_lib_B87FFE37(SslX509Cert *cert)
{
    if (cert == NULL || cert->pubKey == NULL) {
        return NULL;
    }
    cert->pubKey->refCount++;
    return cert->pubKey;
}

/* X509_get_serialNumber / X509_get_notAfter / X509_get_notBefore */
int sceSsl_lib_9299C6CE(SslX509Cert *cert, int *outTag, const u8 **outData, u32 *outLen)
{
    if (cert == NULL || cert->serialData == NULL) {
        return 0;
    }
    if (outTag != NULL) {
        *outTag = cert->serialTag;
    }
    if (outData != NULL) {
        *outData = cert->serialData;
    }
    if (outLen != NULL) {
        *outLen = cert->serialLen;
    }
    return 1;
}

int sceSsl_lib_05E019A4(SslX509Cert *cert, int *outTag, const u8 **outData, u32 *outLen)
{
    if (cert == NULL || cert->notAfterData == NULL) {
        return 0;
    }
    if (outTag != NULL) {
        *outTag = cert->notAfterTag;
    }
    if (outData != NULL) {
        *outData = cert->notAfterData;
    }
    if (outLen != NULL) {
        *outLen = cert->notAfterLen;
    }
    return 1;
}

int sceSsl_lib_341843AB(SslX509Cert *cert, int *outTag, const u8 **outData, u32 *outLen)
{
    if (cert == NULL || cert->notBeforeData == NULL) {
        return 0;
    }
    if (outTag != NULL) {
        *outTag = cert->notBeforeTag;
    }
    if (outData != NULL) {
        *outData = cert->notBeforeData;
    }
    if (outLen != NULL) {
        *outLen = cert->notBeforeLen;
    }
    return 1;
}

/* X509_NAME helpers */
SslX509Name *sceSsl_lib_45514AE3(void)
{
    SslX509Name *name = (SslX509Name *)sceSsl_lib_0861D934(sizeof(SslX509Name));
    if (name != NULL) {
        sceKernelMemset(name, 0, sizeof(SslX509Name));
    }
    return name;
}

int sceSsl_lib_ED95A457(SslX509Name *name)
{
    if (name != NULL) {
        sceSsl_lib_5E5C873A(name);
    }
    return 1;
}

int sceSsl_lib_338C8FDD(SslX509Name *name) { return (name != NULL) ? name->entryCount : 0; }
int sceSsl_lib_BE4A711C(SslX509Name *name) { return (name != NULL) ? (int)name->rawDerLen : 0; }

/* X509_NAME_cmp */
int sceSsl_lib_7D0E41F4(const SslX509Name *lhs, const SslX509Name *rhs)
{
    u32 idx;

    if (lhs == NULL || rhs == NULL) {
        return (lhs == rhs) ? 0 : -1;
    }
    if (lhs->rawDerLen != rhs->rawDerLen) {
        return (lhs->rawDerLen < rhs->rawDerLen) ? -1 : 1;
    }
    for (idx = 0; idx < lhs->rawDerLen; idx++) {
        if (lhs->rawDer[idx] != rhs->rawDer[idx]) {
            return (lhs->rawDer[idx] < rhs->rawDer[idx]) ? -1 : 1;
        }
    }
    return 0;
}

int sceSsl_lib_D579CE04(SslX509Name *name, char *buf, int len)
{
    (void)name;
    if (buf != NULL && len > 0) {
        buf[0] = '\0';
    }
    return 0;
}

int sceSsl_lib_90BCFCE2(SslX509Name *name, int flags) { (void)flags; return (name != NULL) ? name->entryCount : -1; }
int sceSsl_lib_4C75B8E9(SslX509Name *name) { return (name != NULL) ? name->entryCount : -1; }

SslX509NameEntry *sceSsl_lib_5040E836(SslX509Name *name, int index)
{
    if (name == NULL || index < 0 || index >= name->entryCount) {
        return NULL;
    }
    return &name->entries[index];
}

int sceSsl_lib_8D484361(SslX509NameEntry *entry, int *outSet, const u8 **outOid,
                        u32 *outOidLen, int *outTag, const u8 **outVal, u32 *outValLen)
{
    if (entry == NULL) {
        return 0;
    }
    if (outSet != NULL) {
        *outSet = 0;
    }
    if (outOid != NULL) {
        *outOid = entry->oidData;
    }
    if (outOidLen != NULL) {
        *outOidLen = entry->oidLen;
    }
    if (outTag != NULL) {
        *outTag = entry->valueTag;
    }
    if (outVal != NULL) {
        *outVal = entry->valueData;
    }
    if (outValLen != NULL) {
        *outValLen = entry->valueLen;
    }
    return 1;
}

int sceSsl_lib_6443D0EE(SslX509NameEntry *entry, const u8 **outOid, u32 *outOidLen)
{
    return sceSsl_lib_8D484361(entry, NULL, outOid, outOidLen, NULL, NULL, NULL);
}

int sceSsl_lib_6E1E9E0D(SslX509NameEntry *entry, const u8 **outVal, u32 *outValLen)
{
    return sceSsl_lib_8D484361(entry, NULL, NULL, NULL, NULL, outVal, outValLen);
}

/* X509_STORE / X509_EXTENSION / KeyUsage / BasicConstraints / OID lookup */
int sceSsl_lib_C6ED5C87(SslX509StoreCtx *ctx, int err) { if (ctx != NULL) ctx->error = err; return 1; }
int sceSsl_lib_8B78A6C0(SslX509StoreCtx *ctx, SslX509Cert *cert, int depth)
{
    if (ctx != NULL) {
        ctx->cert = cert;
        ctx->errorDepth = depth;
    }
    return 1;
}

SslX509StoreCtx *sceSsl_lib_737BED1C(void)
{
    SslX509StoreCtx *ctx = (SslX509StoreCtx *)sceSsl_lib_0861D934(sizeof(SslX509StoreCtx));
    if (ctx != NULL) {
        sceKernelMemset(ctx, 0, sizeof(SslX509StoreCtx));
    }
    return ctx;
}

int sceSsl_lib_9307DE23(SslX509StoreCtx *ctx) { if (ctx != NULL) sceSsl_lib_5E5C873A(ctx); return 1; }
int sceSsl_lib_EDBE00D6(void) { return 0; }
int sceSsl_lib_E11361C2(SslX509StoreCtx *ctx) { return (ctx != NULL) ? ctx->error : 0; }
int sceSsl_lib_76A46EDA(void) { return 0; }

int sceSsl_lib_7781A781(SslX509Cert *cert, int *outCritical, u32 *outUsage)
{
    if (cert == NULL || !cert->hasKeyUsage) {
        return 0;
    }
    if (outCritical != NULL) {
        *outCritical = cert->keyUsageCritical;
    }
    if (outUsage != NULL) {
        *outUsage = cert->keyUsageBits;
    }
    return 1;
}

int sceSsl_lib_CFA2F8EB(SslX509Cert *cert, int *outCritical, int *outPathLen, int *outIsCa)
{
    if (cert == NULL || !cert->hasBasicConstraints) {
        return 0;
    }
    if (outCritical != NULL) {
        *outCritical = cert->basicConstraintsCritical;
    }
    if (outPathLen != NULL) {
        *outPathLen = cert->pathLenConstraint;
    }
    if (outIsCa != NULL) {
        *outIsCa = cert->isCa;
    }
    return 1;
}

const char *sceSsl_lib_2B7BD166(const u8 *oidData, u32 oidLen, int shortName)
{
    u32 idx;

    if (oidData == NULL || oidLen == 0) {
        return NULL;
    }
    for (idx = 0; idx < (u32)(sizeof(g_oidTable) / sizeof(g_oidTable[0])); idx++) {
        if (g_oidTable[idx].oidLen == oidLen &&
            sslMemEqual(oidData, g_oidTable[idx].oidBytes, oidLen)) {
            return shortName ? g_oidTable[idx].shortName : g_oidTable[idx].longName;
        }
    }
    return NULL;
}

int sceSsl_lib_37C7B76C(SslX509Cert *cert, int *outCritical, int *outCount)
{
    if (cert == NULL) {
        return 0;
    }
    if (outCritical != NULL) {
        *outCritical = 0;
    }
    if (outCount != NULL) {
        *outCount = 0;
    }
    return 1;
}

/* PEM reader exports (PEM_ASN1_read_bio / PEM_read_bio) */
int sceSsl_lib_0217E73C(int (*d2iFn)(void *, const u8 **, int), const char *name,
                        SslBio *bio, void **outObj, void *passCb, void *cbData)
{
    (void)d2iFn;
    (void)name;
    (void)bio;
    (void)outObj;
    (void)passCb;
    (void)cbData;
    return 0;
}

int sceSsl_lib_BE5FF9F1(SslBio *bio, char **outName, char **outHeader, u8 **outData, int *outLen)
{
    (void)bio;
    (void)outName;
    (void)outHeader;
    (void)outData;
    (void)outLen;
    return 0;
}
