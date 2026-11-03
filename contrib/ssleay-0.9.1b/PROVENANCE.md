# SSLeay 0.9.1b

Pinned source baseline for the SSLeay-derived parts of RSA BSAFE SSL-C in
`libssl.suprx`. This product includes cryptographic software written by Eric
Young (eay@cryptsoft.com). See `COPYRIGHT` and the notices in each source file.

Upstream repository: https://github.com/openssl/openssl

Tag: `SSLeay_0_9_1b`, revision
`162d5f30bead20c01854e892debf58fb3b237d75`.

Archive: https://codeload.github.com/openssl/openssl/tar.gz/refs/tags/SSLeay_0_9_1b

Archive SHA-256:
`814dfddbf48b71582761926d06f75d557a8b6f81317025961b26258550fead98`.

Kept unchanged: `crypto/lhash/lhash.c`, `crypto/lhash/lhash.h`, `crypto/crypto.h`,
`crypto/stack/stack.c`, `crypto/stack/stack.h`, `crypto/ex_data.c`,
`crypto/bio/bio_lib.c`, `crypto/bio/bss_sock.c` and `crypto/bio/bio.h`, plus the upstream copyright file.
The libssl LHASH, STACK, ex-data and BIO translation units
read these files. The rest of the distribution is omitted.

The baseline was selected by compiling historical releases with Sony SNC and
checking the original firmware in IDA. Before the SSL-C adaptations, seven of
nine emitted LHASH functions have identical instruction bodies against the
3.65 original; SSLeay 0.8.1b and 0.9.0b each reproduce five, and OpenSSL 0.9.2b
reproduces six. These diagnostic counts mask relocations; they establish source
ancestry, not complete behavioral verification. Explicit original-address
pairs and the project verifier check the adapted implementation separately.

This pins a reconstruction baseline, not the exact name of RSA's historical
source drop. SSLeay and early OpenSSL share files. For example, SSLeay 0.9.1b's
BIO implementation is also present unchanged in OpenSSL 0.9.2b; that file alone
cannot distinguish the releases. The firmware identifies the surrounding
product as RSA BSAFE SSL-C 2.8.1, with RCOM 2.3.0.

The consuming module keeps its changes in `lhash/lhash_sce.patch`: omit the
upstream version banner absent from the binary; grow before splitting a hash
bucket; stop insertion on expansion failure; pass the old allocation size to
the allocator when expanding or contracting. The adapter binds allocation and
string comparison to the module's original interfaces. No upstream file here
contains these changes.

`stack/stack_sce.patch` records the SSL-C stack changes: forty initial slots,
allocator adaptation (including zero old-size on duplicate resize), cleanup on
failed duplication, corrected insertion shift with retained count, retained
search comparator, signed byte-distance indexing, reverse element cleanup and
slot clearing before free. The upstream `sk_delete_ptr` body is omitted because
the original stack translation unit does not contain it. Four additional SSL-C
operations and the binary-search adapter live in the consuming translation unit;
their descriptive names are explicitly ours. The upstream version banner is
absent from the original and removed by the patch.

`ex_data_sce.patch` removes unused private-header dependencies, returns the
registration result rather than the input index on failure, and clamps negative
setter indices to zero. The consuming wrapper binds the original allocator and
error interface and its measured error constants. It disables the optional
upstream memory-debug hooks, which the original functions do not call. Original
callback record layout agrees with the unmodified upstream header.

The BIO adaptation changes the header through `bio/bio_sce.patch` and the
implementation through `bio/bio_lib_sce.patch`. SSL-C uses a separately allocated
eight-byte ex-data object. Its BIO structure is64 bytes: the first60 hold the
adapted upstream fields and its socket method uses an additional descriptor at
offset60. The socket descriptor field's descriptive name is ours. Generic BIO
initialization leaves this field for the method constructor to initialize.
The patch preserves method-creation failure cleanup, SSL-C error values,
write auto-flush, error/return instead of abort on bad references, and backward
link clearing before freeing a chain. Control dispatch captures the method and
reloads it only after a successful pre-callback: a rejecting callback must not
cause a subsequent dereference of a possibly invalidated BIO. The wrapper binds
reference locking to the original source filename and line190. Retry flags are
separate SSL-C helpers, including null handling and its extra retry bit0x40.

The full upstream BIO file is retained. Seven emitted upstream helpers currently
have no established original identity: `BIO_find_type`, `BIO_get_ex_data`,
`BIO_get_ex_new_index`, `BIO_get_retry_BIO`, `BIO_ptr_ctrl`, `BIO_puts` and
`BIO_set_ex_data`. They are not counted as reconstructed original functions;
their presence in upstream does not establish that SSL-C retained those APIs or
used their behavior unchanged. Their native counterparts, if any, remain an
identification task. Do not use an unverified helper as a substitute for an
observed native operation. In particular, SSL-C's socket pointer getter has
different failure behavior from upstream `BIO_ptr_ctrl`.

`bio/bss_sock_sce.patch` adapts the socket method to Sony's network imports and
the additional descriptor field. It preserves SSL-C's unconditional retry on
negative receive results, retry on zero send results, reset/seek success,
one-bit close flags, null-descriptor-pointer rejection after old-socket cleanup,
and a gets method returning-1. The upstream errno retry classifiers are unused
under those measured rules and excluded from this build. Socket descriptor
set/get wrappers retain native callback behavior; the getter returns its output
slot regardless of control success, including the original local-slot behavior.

`bio/bio_dispatch.h` selects the adapted upstream control body through the
`SCE_BIO_CTRL_ONLY`/`SCE_BIO_CTRL_DECL` patch hooks. This lets the socket wrappers
inline the same source as the out-of-line public dispatcher, preserving native
call structure without duplicating or hand-reimplementing the upstream body.
The three additional retry flag helpers are SSL-C's separately compiled forms
of upstream operations, with null handling and the extra retry bit retained.
