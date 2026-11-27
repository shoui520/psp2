TARGET := libssl
SRCS   := \
	crypto/sha2.c \
	crypto/asn1/asn1_tree.c \
	crypto/bn/bn.c \
	crypto/ciphers/ciphers.c \
	crypto/evp/evp.c \
	crypto/asn1/asn1_time_der.c \
	crypto/e_malloc.c \
	crypto/lhash/lhash_obj_buf.c \
	crypto/rsa/rand_rsa_stack.c \
	crypto/asn1/asn1_der_core.c \
	crypto/bio/bio_lib.c \
	crypto/x509tc/x509tc.c \
	crypto/err/err.c \
	ssl/s3_core.c \
	ssl/t1_ciph_cert.c \
	ssl/ssl_lib.c \
	sce_ssl.c
EXTRA_CFLAGS := -O2 -fno-strict-aliasing -I$(CURDIR)

include ../../build.mak

$(shell mkdir -p $(O)/crypto/asn1 $(O)/crypto/bn $(O)/crypto/ciphers $(O)/crypto/evp $(O)/crypto/lhash $(O)/crypto/rsa $(O)/crypto/bio $(O)/crypto/x509tc $(O)/crypto/err $(O)/ssl)
