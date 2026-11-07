/* SSLeay base64 context and encode/decode prototypes with SSL-C newline state.
 * Private descriptive helper names are ours, not recovered Sony names.
 */
#ifndef LIBSSL_EVP_H
#define LIBSSL_EVP_H

#include <stddef.h>
#include <scetypes.h>

typedef struct evp_Encode_Ctx_st {
	int num;
	int length;
	unsigned char enc_data[80];
	int line_num;
	int expect_nl;
	int newlineFlags;
} EVP_ENCODE_CTX;

_Static_assert(sizeof(EVP_ENCODE_CTX) == 100, "EVP_ENCODE_CTX");
_Static_assert(offsetof(EVP_ENCODE_CTX, line_num) == 88, "line_num");
_Static_assert(offsetof(EVP_ENCODE_CTX, expect_nl) == 92, "expect_nl");
_Static_assert(offsetof(EVP_ENCODE_CTX, newlineFlags) == 96, "newlineFlags");

EVP_ENCODE_CTX *sslNewEncodeContext(void);
void sslFreeEncodeContext(EVP_ENCODE_CTX *ctx);
void EVP_EncodeInit(EVP_ENCODE_CTX *ctx);
int EVP_EncodeBlock(unsigned char *t, unsigned char *f, int dlen);
void EVP_EncodeUpdate(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
		      unsigned char *in, int inl);
void EVP_EncodeFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl);
void EVP_DecodeInit(EVP_ENCODE_CTX *ctx);
int EVP_DecodeBlock(unsigned char *t, unsigned char *f, int n);
int EVP_DecodeUpdate(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
		     unsigned char *in, int inl);
int EVP_DecodeFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl);
int sslGetEncodeNewlineFlags(EVP_ENCODE_CTX *ctx);
void sslSetEncodeNewlineFlags(EVP_ENCODE_CTX *ctx, int flags);
void sslEncodeUpdateAndFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
			     unsigned char *in, int inl);
int sslDecodeUpdateAndFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
			    unsigned char *in, int inl);

#endif
