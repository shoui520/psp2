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
`crypto/bio/bio_lib.c`, `crypto/bio/bss_sock.c`, `crypto/bio/b_print.c`,
`crypto/bio/b_dump.c`, `crypto/bio/bss_mem.c`, `crypto/bio/bio.h`,
`crypto/buffer/buffer.c`, `crypto/buffer/buffer.h`, `ssl/ssl_stat.c`,
`ssl/ssl3.h`, `ssl/ssl.h`, `ssl/ssl23.h`, `ssl/ssl_lib.c`, `ssl/s3_lib.c`,
`ssl/s3_both.c`, `crypto/cryptlib.c`
and `crypto/mem.c`, plus the upstream copyright file.
The libssl LHASH, STACK, ex-data, BIO, buffer, SSL and runtime translation units
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
The read/write/special query functions also compile the pristine upstream macros,
adding the native null-input return of-1. Function declarations preserve these
macros for other call sites that inline them in the original.

`cryptlib_sce.patch` selects the lock and thread-ID callback operations from
`crypto/cryptlib.c`. SSL-C's setters return1, its lock callback returns a status,
and its thread-ID callback takes an integer argument0 and defaults to ID0.
Reference updates preserve32-bit wrapping, lock-failure returns and the callback
reload after locking. The add-lock callback owns the update when installed.
The upstream lock-name registry and platform entry points are excluded; their
presence in the donor establishes no native identity or implemented coverage.
The consuming runtime wrapper's descriptive bindings are ours.

`mem_ctrl_sce.patch` selects only `CRYPTO_mem_ctrl` from `crypto/mem.c`, retaining
its four state-bit operations with SSL-C's unsigned32-bit return and absence of
locking calls. The remaining allocation and memory-debug machinery in that file
is excluded, without a claim that those operations match the native library.
SSL-C's error queue is implemented separately: its global sixteen-entry array of
thread IDs and packed error codes differs from upstream's allocated per-thread
error state, so the upstream error implementation is not substituted for it.

`bio/b_print_sce.patch` retains the upstream varargs interface, with SSL-C's
null-BIO return0,1024-byte buffer, bounded formatter and final-byte termination
when formatting returns-1. `bio/b_dump_sce.patch` disables trailing-space/NUL
truncation, binds bounded formatting/copy/append operations and returns1 after
successful rows or-1 on the first nonpositive write. Its row formats,16-byte
width and ASCII range follow upstream. The string and formatter adapter names
are ours; they wrap the original Sony LibKernel imports.

`buffer/buffer_sce.patch` preserves SSL-C's null-safe destruction, absence of
allocation-error queue calls, growth by requested length plus16, and old capacity
passed to reallocation. The twelve-byte `BUF_MEM` layout and its field names
agree with upstream. `BUF_strdup` is not selected: its native identity has not
been established. A declaration hook lets the memory BIO use the same adapted
buffer bodies for operations inlined in the original.

`bio/bss_mem_sce.patch` removes the null-write error-queue call, retains one-bit
close flags and makes the original32-bit length wrapping explicit. Its read
path retains the upstream overlapping byte-copy loop, empty-read return setting
and retry behavior. Gets scans for a newline, limits the read, and terminates
only a positive result. The wrapper shares buffer and length implementations
with their out-of-line counterparts. The method table and its seven callbacks
are identified in the original, as is Sony's empty-memory-BIO factory export.

`ssl/ssl_stat_sce.patch` selects all eight identified state, read-state and
alert-string operations. SSL-C adds the TLS no-renegotiation description,
corrects upstream message spellings, and adds server-hello-request and
client-hello-renegotiation states. Long state strings retain SSL23 negotiation
labels while short state strings omit them. The `ssl/ssl3_sce.patch` header
selection supplies the original protocol, state and alert constants.

`ssl/ssl_sce.patch` adapts the complete connection structure to SSL-C's352-byte
layout. It retains upstream member names where identified; additional descriptive
names, the record-crypto and extension-list types, and the callback typedef are
ours, not recovered Sony spellings. Embedded read/write crypto records replace
the donor's context pointers. Callback arguments, extension indexing, message
limits and algorithm/key handles follow native readers and writers. The reserved
word has no independently observed access. Opaque pointees remain incomplete;
this layout does not claim implementation of their allocation or behavior.

`ssl/ssl_lib_sce.patch` selects eighteen original connection operations: BIO and
method getters, textual/numeric version, context getter, shutdown setter/getter,
information callback getter, state getter, pending, read, peek, write, shutdown,
handshake dispatch, error classification and record-crypto cleanup. The textual version includes the
native TLS1.1 label `TLSv11` and excludes SSL2. The consuming wrapper retains
Sony's NID export link names and separately compiles the original rwstate query,
which is a macro in this upstream baseline. The remaining SSL library routines
are excluded and are not counted as reconstructed functions.

The complete88-byte `SSL_METHOD` layout follows the upstream header and the
native protocol tables; selected I/O callbacks have explicit prototypes. Other
callback declarations retain the upstream parameter declarations until their
implementations are reconstructed. Shutdown returns1 for null, dispatches when
initialization is clear or option0x40000 is set, and otherwise returns-1. The
option's descriptive name is ours. Read and peek retain their distinct handling
of received shutdown: only read resets rwstate. Write after sent shutdown records
SSL-C's error20/209/207 and returns-1. Handshake dispatch retains the native
null-handler error20/180/144, renegotiation check, state reload and subsequent
handshake callback. These dispatch bodies do not establish completion of the
protocol callbacks they invoke.

`ssl/ssl3_sce.patch` also adapts the complete580-byte SSL3 state,24-byte record
and24-byte buffer types. SSL-C uses64-byte MAC-secret arrays, buffer capacities,
pointer-based handshake digest contexts, and a temporary state without the
donor's DH and compression pointers. The certificate-request flag remains the
last member. Two additional buffers are only observed being wiped and freed;
their descriptive names and the bound/capacity names are ours, without a claim
about the buffers' original purpose. The consuming header asserts the complete
size and the key record, alert, digest and temporary-state offsets.

`ssl/s3_lib_sce.patch` selects `ssl3_new`, `ssl3_pending`, `ssl3_free` and
`ssl3_clear`, with SSL-C's additional handshake cleanup. Allocation failure
records SSL-C error20/147/33, and pending returns0 while the record body is being
read. Free destroys digest handles and buffers, wipes owned temporary storage,
then wipes and frees the state. Clear retains the I/O buffers, capacities and
digest handles, clears the read buffer, releases temporary state and removes
the buffering BIO. A null pop result does not replace the write BIO. Cleanup
reloads the connection's state after callbacks; temporary algorithm pointers
are left unchanged after destruction until the enclosing state clear. The
additional helper's descriptive name is ours. Remaining operations in that
pristine file, including protocol tables, remain unselected and are not
counted as implemented.

Record-crypto cleanup retains the upstream helper's role but replaces the donor's
EVP contexts with SSL-C's embedded read/write states. Each state releases its
cipher algorithm, invokes the connection's MAC destructor, then releases cipher
and MAC key handles. Pointers are cleared after their callbacks. MAC size is
cleared only when a MAC algorithm was present; block and secret sizes remain.

The CRYPTOCME algorithm/key and MES name-destroy dispatchers are implemented
separately from this donor. No open-source origin has been established for those
dispatchers. Their source folders follow recovered CRYPTOCME and MES component
paths; filenames and descriptive private names are ours. The algorithm object
is56 bytes. Common object and crypto-lifecycle types describe the polymorphic
interface prefixes used by destruction; they do not claim reconstruction of
the complete crypto method table or each concrete certificate object. The name
dispatcher returns the native integer status, including null-object and missing
operation errors, and intentionally has no null-method-table guard.
The key-destroy dispatcher instead succeeds on null and returns10009 for a
missing destructor, also without a null-method-table guard. Algorithm copying
forwards its output pointer without checking it. Algorithm information queries
require the module callback even for the five locally handled selectors;
application data, family, algorithm, mode and flags read their native fields.
Only application data and flags are handled locally by the setter. Other
selectors forward unchanged. Backend information callbacks separately require
a nonnull result/value and return10009 for an absent implementation operation.
The native algorithm object remains56 bytes; its module and implementation
method declarations cover only the shared prefixes used here. Additional
descriptive helper, selector and interface names, and key_api/cry_info filenames,
are ours. No open-source donor has been identified for these backend operations.

The separate SSL-C time-object interface is reconstructed in the consuming
module's `time.c` and `time.h`; those filenames and descriptive private names
are ours. No open-source donor has been identified for this interface. Its native
object is12 bytes, with high/low seconds and milliseconds. The selected operations
cover creation, destruction, current-time/add/compare/encode/decode dispatch,
callback registration and a four-byte big-endian seconds decoder. Creation uses
the existing allocator and free callbacks, invokes the current-time callback
after allocation and frees the original allocation on callback failure; success
returns the callback's pointer. No callback is captured across allocation.
Absent optional callbacks return null/zero. Free still invokes the configured
memory destructor, including for null. The platform clock/codec implementations
and the unsigned-seconds getter are not selected by this batch; these interface
bodies do not establish complete session-expiry implementation.

`ssl/s3_both_sce.patch` selects buffer setup: allocate or enlarge the read buffer,
preserve the current packet's32-bit relative address, retain capacities, and
allocate/zero the write buffer from the clamped fragment size plus101. The
connection's send fragment resets to2048 before write allocation, including on
failure. The read resize helper is an SSL-C addition; its descriptive name is
ours. Other handshake operations in the donor are excluded.

Error classification preserves SSL-C's system-error library mapping, distinct
read/write special-retry handling, extension-wait condition for SSL3 through
TLS1.1 servers, and close-alert test. The additional retry/error names are ours;
they are not assigned newer OpenSSL meanings based only on their numeric values.
