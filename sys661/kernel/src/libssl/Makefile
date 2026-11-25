TARGET := libssl
SRCS   := sce_ssl.c crypto/e_malloc.c crypto/bio_lib.c crypto/x509tc.c ssl/ssl_lib.c
EXTRA_CFLAGS := -O2 -fno-strict-aliasing -I$(CURDIR)

include ../../build.mak

$(shell mkdir -p $(O)/crypto $(O)/ssl)
