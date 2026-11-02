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
`crypto/stack/stack.c`, `crypto/stack/stack.h` and `crypto/ex_data.c`, plus the
upstream copyright file. The libssl LHASH, STACK and ex-data translation units
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
