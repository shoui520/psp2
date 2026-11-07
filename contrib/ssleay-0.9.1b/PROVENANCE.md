# SSLeay 0.9.1b

Pristine upstream source: https://codeload.github.com/openssl/openssl/tar.gz/refs/tags/SSLeay_0_9_1b
(SHA-256 814dfddbf48b71582761926d06f75d557a8b6f81317025961b26258550fead98),
tag `SSLeay_0_9_1b`, revision `162d5f30bead20c01854e892debf58fb3b237d75`
in OpenSSL's historical repository.

Kept, unchanged: `crypto/lhash/lhash.c`, `crypto/lhash/lhash.h`, `crypto/crypto.h`,
`crypto/stack/stack.c`, `crypto/stack/stack.h`, `crypto/ex_data.c`,
`crypto/bio/bio_lib.c`, `crypto/bio/bss_sock.c`, `crypto/bio/b_print.c`,
`crypto/bio/b_dump.c`, `crypto/bio/bss_mem.c`, `crypto/bio/bio.h`,
`crypto/buffer/buffer.c`, `crypto/buffer/buffer.h`, `crypto/cryptlib.c`,
`crypto/evp/encode.c`, `crypto/evp/evp.h`, `crypto/pem/pem_lib.c`,
`crypto/pem/pem.h`, `crypto/asn1/asn1.h`, `crypto/objects/objects.h`,
`crypto/objects/obj_dat.h`, `crypto/mem.c`, `ssl/ssl_stat.c`, `ssl/ssl3.h`,
`ssl/ssl.h`, `ssl/ssl23.h`, `ssl/ssl_lib.c`, `ssl/s3_lib.c`, `ssl/s3_both.c`,
`ssl/ssl_sess.c`, `ssl/ssl_cert.c`, `ssl/ssl_locl.h` and `COPYRIGHT`.
Everything else in the distribution is left out. The copyright notice and
source-file licenses are preserved. This product includes cryptographic
software written by Eric Young (eay@cryptsoft.com).

Used by `libssl.suprx`, whose surrounding middleware identifies itself as RSA
BSAFE SSL-C 2.8.1 with RCOM 2.3.0. SSLeay 0.9.1b was selected as the source
baseline by comparing historical releases against the firmware, particularly
LHASH. Shared code with early OpenSSL prevents identifying RSA's exact source
drop from these components alone.

Downstream adaptations are separate patches in
`sys/internal/usermode/src/libssl/`, including `objects/` for the registry's
SSL-C changes and additions.
