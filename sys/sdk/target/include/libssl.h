/*
 * libssl.h: under Sony's names and with Sony's definitions
 * (SDK Release 03.570.011, target/include/libssl.h).
 */
#ifndef _SCE_SSL_LIBSSL_H
#define _SCE_SSL_LIBSSL_H

#include <scetypes.h>
#include <rtc.h>

#define SCE_SSL_ERROR_BEFORE_INIT		(-2143072255)	/* 0x80435001 */
#define SCE_SSL_ERROR_ALREADY_INITED	(-2143072224)	/* 0x80435020 */
#define SCE_SSL_ERROR_OUT_OF_MEMORY		(-2143072222)	/* 0x80435022 */
#define SCE_SSL_ERROR_NOT_FOUND			(-2143072219)	/* 0x80435025 */
#define SCE_SSL_ERROR_INTERNAL			(-2143072218)	/* 0x80435026 */
#define SCE_SSL_ERROR_INVALID_FORMAT	(-2143071992)	/* 0x80435108 */
#define SCE_SSL_ERROR_INVALID_VALUE		(-2143071746)	/* 0x804351FE */

typedef void SceSslCert;
typedef void SceSslCertName;

typedef struct SceSslMemoryPoolStats {
	SceSize poolSize;
	SceSize maxInuseSize;
	SceSize currentInuseSize;
	SceInt32 reserved;
} SceSslMemoryPoolStats;

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif

#define sceSslEnd()	sceSslTerm()

SceInt32 sceSslInit(
	SceSize poolSize
	);

SceInt32 sceSslTerm(
	void
	);

SceInt32 sceSslGetMemoryPoolStats(
	SceSslMemoryPoolStats* currentStat
	);

SceInt32 sceSslGetSerialNumber(
	SceSslCert *sslCert,
	const SceUChar8 **sboData,
	SceSize *sboLen
	);

SceSslCertName* sceSslGetSubjectName(
	SceSslCert *sslCert
	);

SceSslCertName* sceSslGetIssuerName(
	SceSslCert *sslCert
	);

SceInt32 sceSslGetNotBefore(
	SceSslCert *sslCert,
	SceRtcTick *begin
	);

SceInt32 sceSslGetNotAfter(
	SceSslCert *sslCert,
	SceRtcTick *limit
	);

SceInt32 sceSslGetNameEntryCount(
	SceSslCertName *certName
	);

SceInt32 sceSslGetNameEntryInfo(
	SceSslCertName *certName,
	SceInt32 entryNum,
	char *oidname,
	SceSize maxOidnameLen,
	char *value,
	SceSize maxValueLen,
	SceSize *valueLen
	);

SceInt32 sceSslFreeSslCertName(
	SceSslCertName* certName
	);

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif

#endif
