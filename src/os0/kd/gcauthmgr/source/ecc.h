/* Module-private serialized KIRK elliptic-curve points. */
#pragma once

typedef union GcAuthAdhocPublicKey160 {
	struct {
		unsigned char x[20];
		unsigned char y[20];
	} point;
	unsigned char serialized[40];
} GcAuthAdhocPublicKey160;

typedef union GcAuthAdhocPublicKey224 {
	struct {
		unsigned char x[28];
		unsigned char y[28];
	} point;
	unsigned char serialized[56];
} GcAuthAdhocPublicKey224;

typedef struct GcAuthAdhocSignature160 {
	unsigned char r[20];
	unsigned char s[20];
} GcAuthAdhocSignature160;

typedef struct GcAuthAdhocSignature224 {
	unsigned char r[28];
	unsigned char s[28];
} GcAuthAdhocSignature224;

typedef struct GcAuthAdhocPeerCertificate160 {
	unsigned char certificatePrefix[16];
	GcAuthAdhocPublicKey160 publicKey;
} GcAuthAdhocPeerCertificate160;

typedef struct GcAuthAdhocPeerCertificate224 {
	unsigned char certificatePrefix[16];
	GcAuthAdhocPublicKey224 publicKey;
} GcAuthAdhocPeerCertificate224;

typedef struct GcAuthAdhocSignedCertificate160 {
	GcAuthAdhocPeerCertificate160 certificate;
	GcAuthAdhocSignature160 signature;
} GcAuthAdhocSignedCertificate160;

typedef struct GcAuthAdhocSignedCertificate224 {
	GcAuthAdhocPeerCertificate224 certificate;
	GcAuthAdhocSignature224 signature;
} GcAuthAdhocSignedCertificate224;
