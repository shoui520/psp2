# SceLibMarlin, Intertrust's Marlin DRM client, built with GCC in ARM mode, -O2 with the
# vectorizer (its copy loops are vectorized; -O3 would also peel and unroll them, Sony's are not),
# without the stack protector.
# One Sony build, shipped as vs0:vsh/common/libmarlin and vs0:sys/external/libmln:
# the target is named from the rel. No debug records or strings name its files: the
# file names are ours, one per address range.
set(MLN_ARM_SOURCES
	buffer.c
	memory.c
	mutex.c
	api.c
	string.c
	platform.c
	time.c
	esb.c
	core.c
)
# The XML reader keeps every store before the loads after it, as -fno-strict-aliasing
# compiles it (its node constructors store the node before reading the object's count).
set(MLN_XML_SOURCES
	xml.c
	xmlnode.c
	xmlbuild.c
)
# The engine too: it re-reads its progress after each store into the step it fills.
set(MLN_ENGINE_SOURCES
	engine.c
	client.c
	message.c
	textbuffer.c
	file.c
	dir.c
	base64.c
	service.c
	xmlwrite.c
	octopus.c
	octopusservice.c
	octopusrequest.c
	sha1.c
	config.c
	wsse.c
	asn1.c
	x509.c
	object.c
	store.c
	session.c
	handle.c
	license.c
	psnl.c
	util.c
	sessionrecord.c
	cipher.c
	rsa.c
	digest.c
	trust.c
	crl.c
	anchor.c
	envelope.c
	xmldsig.c
	xmldsigfree.c
	psnlkey.c
	cache.c
	canonical.c
	octopuskey.c
	control.c
	credentials.c
	registration.c
	registrationservice.c
	licenseservice.c
	exchange.c
	agent.c
	lil.c
	vm.c
	hostobject.c
	keystore.c
	nemo.c
	soapfault.c
	slotarray.c
	seashell.c
	broadcastkey.c
)
# The RSA arithmetic SceMarlinHci and SceSblGcAuthMgr link, in Thumb with the stack protector
# like the rest of Sony's GCC code: the HCI's private and public operations, then GcAuthMgr's
# signature check and DigestInfo encoding.
set(mln_hci ../../../../../sys/internal/driver/src/marlin_hci)
set(mln_gcauth ../../../../../sys/internal/kernel/src/gcauthmgr)
set(MLN_RSA_SOURCES
	${mln_hci}/integer_words.c
	${mln_hci}/rsa_random.c
	${mln_hci}/rsa_crt.c
	${mln_hci}/rsa_private.c
	${mln_hci}/rsa_exponent.c
	${mln_hci}/rsa_public_operation.c
	${mln_hci}/rsa_encrypt.c
	${mln_hci}/rsa_sign.c
	${mln_gcauth}/rsa_verify.c
	${mln_hci}/integer_encode.c
	${mln_hci}/rsa_integer_arithmetic.c
	${mln_hci}/rsa_montgomery.c
	${mln_hci}/rsa_integer_multiply.c
	${mln_hci}/rsa_integer_remainder.c
	${mln_hci}/rsa_integer_square.c
	${mln_hci}/rsa_integer_tail.c
	${mln_gcauth}/rsa_digest.c
)
# The SDK's libSceHmac, libSceSha1, libSceSha256 and libSceMt19937, then libgcc's division
# routines, which Sony built with GCC and without stack guards.
set(mln_sdk ../../../../../sys/sdk/target/src)
set(mln_libgcc ../../../../../toolchains/libgcc)
set(MLN_SDK_SOURCES
	${mln_sdk}/hmac/sha1.c
	${mln_sdk}/hmac/sha256.c
	${mln_sdk}/hmac/libhmac.c
	${mln_sdk}/sha1/libsha1.c
	${mln_sdk}/sha256/libsha256.c
	${mln_sdk}/mt19937/libmt19937.c
)
set(MLN_LIBGCC_SOURCES
	${mln_libgcc}/udiv.S
	${mln_libgcc}/idiv.S
	${mln_libgcc}/dvmd_tls.S
	${mln_libgcc}/uldivmod.S
)
# The library's memset, then its HCI calls, Thumb like the RSA code; the memset vectorized
# (16-byte stores after an alignment peel; Sony's byte loops are also unrolled with pld).
set(MLN_THUMB_SOURCES
	libc.c
	hci.c
)
# The ARM code linked after them.
set(MLN_TAIL_SOURCES
	support.c
	serviceconfig.c
	xmlenc.c
	octopusref.c
	coreaction.c
	licensehelp.c
)
# The VM's stack and system calls, like the engine without strict aliasing.
set(MLN_VM_SOURCES
	vmstack.c
)
# The NEMO message security code after them, with the engine's flags too.
set(MLN_NEMO_SOURCES
	nemosecurity.c
	seashellvalue.c
)
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 PARTIAL SOURCES
	${MLN_ARM_SOURCES}
	${MLN_ENGINE_SOURCES}
	${MLN_RSA_SOURCES}
	${MLN_SDK_SOURCES}
	${MLN_LIBGCC_SOURCES}
	${MLN_THUMB_SOURCES}
	${MLN_TAIL_SOURCES}
	${MLN_VM_SOURCES}
	${MLN_NEMO_SOURCES}
	${MLN_XML_SOURCES}
	libmarlin.c
)
set_source_files_properties(${MLN_ARM_SOURCES} ${MLN_TAIL_SOURCES} PROPERTIES COMPILE_OPTIONS "-marm;-O2;-ftree-vectorize;-finline-functions;-fno-stack-protector")
set_source_files_properties(${MLN_XML_SOURCES} ${MLN_ENGINE_SOURCES} ${MLN_VM_SOURCES} ${MLN_NEMO_SOURCES} PROPERTIES COMPILE_OPTIONS "-marm;-O2;-ftree-vectorize;-finline-functions;-fno-stack-protector;-fno-strict-aliasing")
set_source_files_properties(${MLN_SDK_SOURCES} PROPERTIES COMPILE_OPTIONS "-fno-stack-protector")
set_source_files_properties(libc.c PROPERTIES COMPILE_OPTIONS "-ftree-vectorize")
include(${mln_libgcc}/lib1funcs.cmake)
gcc48_use_lib1funcs(${PSP2_TARGET})
