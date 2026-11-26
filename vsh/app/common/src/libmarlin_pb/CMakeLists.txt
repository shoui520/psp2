# SceLibMarlinPb: playback-only build of SceLibMarlin (vsh/app/common/src/libmarlin),
# compiled with GCC 4.8 in ARM mode (-DMLN_PLAYBACK), omitting the remote-client and
# license-transfer/registration/NEMO SOAP objects.
set(mln_src ../libmarlin)

set(MLN_PB_ARM_SOURCES
	${mln_src}/buffer.c
	${mln_src}/memory.c
	${mln_src}/mutex.c
	${mln_src}/api.c
	${mln_src}/string.c
	${mln_src}/platform.c
	${mln_src}/time.c
	${mln_src}/esb.c
	${mln_src}/core.c
)
set(MLN_PB_XML_SOURCES
	${mln_src}/xml.c
	${mln_src}/xmlnode.c
	${mln_src}/xmlbuild.c
)
set(MLN_PB_ENGINE_SOURCES
	${mln_src}/engine.c
	${mln_src}/client.c
	${mln_src}/message.c
	${mln_src}/textbuffer.c
	${mln_src}/file.c
	${mln_src}/dir.c
	${mln_src}/base64.c
	${mln_src}/service.c
	${mln_src}/xmlwrite.c
	${mln_src}/octopus.c
	${mln_src}/octopusservice.c
	${mln_src}/octopusrequest.c
	${mln_src}/sha1.c
	${mln_src}/config.c
	${mln_src}/asn1.c
	${mln_src}/x509.c
	${mln_src}/object.c
	${mln_src}/store.c
	${mln_src}/session.c
	${mln_src}/handle.c
	${mln_src}/license.c
	${mln_src}/util.c
	${mln_src}/sessionrecord.c
	${mln_src}/cipher.c
	${mln_src}/rsa.c
	${mln_src}/digest.c
	${mln_src}/trust.c
	${mln_src}/crl.c
	${mln_src}/anchor.c
	${mln_src}/envelope.c
	${mln_src}/xmldsig.c
	${mln_src}/xmldsigfree.c
	${mln_src}/psnlkey.c
	${mln_src}/cache.c
	${mln_src}/canonical.c
	${mln_src}/octopuskey.c
	${mln_src}/control.c
	${mln_src}/credentials.c
	${mln_src}/lil.c
	${mln_src}/vm.c
	${mln_src}/hostobject.c
	${mln_src}/keystore.c
	${mln_src}/slotarray.c
	${mln_src}/seashell.c
	${mln_src}/broadcastkey.c
)
set(mln_hci ../../../../../sys/internal/driver/src/marlin_hci)
set(mln_gcauth ../../../../../sys/internal/kernel/src/gcauthmgr)
set(MLN_PB_RSA_SOURCES
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
set(mln_sdk ../../../../../sys/sdk/target/src)
set(mln_libgcc ../../../../../toolchains/libgcc)
set(MLN_PB_SDK_SOURCES
	${mln_sdk}/hmac/sha1.c
	${mln_sdk}/hmac/sha256.c
	${mln_sdk}/hmac/libhmac.c
	${mln_sdk}/sha1/libsha1.c
	${mln_sdk}/sha256/libsha256.c
	${mln_sdk}/mt19937/libmt19937.c
)
set(MLN_PB_LIBGCC_SOURCES
	${mln_libgcc}/udiv.S
	${mln_libgcc}/idiv.S
	${mln_libgcc}/dvmd_tls.S
	${mln_libgcc}/uldivmod.S
)
set(MLN_PB_THUMB_SOURCES
	${mln_src}/hci.c
)
set(MLN_PB_TAIL_SOURCES
	${mln_src}/support.c
	${mln_src}/serviceconfig.c
	${mln_src}/xmlenc.c
	${mln_src}/octopusref.c
	${mln_src}/coreaction.c
)
set(MLN_PB_VM_SOURCES
	${mln_src}/vmstack.c
)
set(MLN_PB_NEMO_SOURCES
	${mln_src}/seashellvalue.c
)
set(MLN_PB_AES_SOURCES
	${mln_sdk}/aes/libaes.c
	${mln_sdk}/aes/aes.S
	${mln_sdk}/sha1/sha1.S
	${mln_sdk}/sha256/sha256.S
	${mln_sdk}/mt19937/mtrand.S
)
set(mln_gcc ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/gcc-4.8-2014.05)
include(${mln_libgcc}/select.cmake)
set(mln_pb_bpabi_source "${CMAKE_CURRENT_BINARY_DIR}/bpabi-uldivmod.c")
gcc48_select_tail("${mln_gcc}/libgcc/config/arm/bpabi.c" "${mln_pb_bpabi_source}"
	"long long\n__gnu_ldivmod_helper (" "unsigned long long\n__gnu_uldivmod_helper (")
set(MLN_PB_LIBGCC_TAIL_SOURCES
	${mln_pb_bpabi_source}
	${mln_libgcc}/udiv64.c
)

psp2_module(vs0_vsh_common_libmarlin_pb KIND user COMPILER gcc48 SOURCES
	${MLN_PB_ARM_SOURCES}
	${MLN_PB_ENGINE_SOURCES}
	${MLN_PB_RSA_SOURCES}
	${MLN_PB_SDK_SOURCES}
	${MLN_PB_LIBGCC_SOURCES}
	${MLN_PB_THUMB_SOURCES}
	${MLN_PB_TAIL_SOURCES}
	${MLN_PB_VM_SOURCES}
	${MLN_PB_NEMO_SOURCES}
	${MLN_PB_AES_SOURCES}
	${MLN_PB_LIBGCC_TAIL_SOURCES}
	${MLN_PB_XML_SOURCES}
	libmarlin_pb.c
)
target_compile_definitions(vs0_vsh_common_libmarlin_pb PRIVATE MLN_PLAYBACK)
set_source_files_properties(${MLN_PB_ARM_SOURCES} ${MLN_PB_TAIL_SOURCES} PROPERTIES COMPILE_OPTIONS "-marm;-O2;-ftree-vectorize;-finline-functions;-fno-stack-protector")
set_source_files_properties(${MLN_PB_XML_SOURCES} ${MLN_PB_ENGINE_SOURCES} ${MLN_PB_VM_SOURCES} ${MLN_PB_NEMO_SOURCES} PROPERTIES COMPILE_OPTIONS "-marm;-O2;-ftree-vectorize;-finline-functions;-fno-stack-protector;-fno-strict-aliasing")
set_source_files_properties(${MLN_PB_SDK_SOURCES} ${MLN_PB_AES_SOURCES} PROPERTIES COMPILE_OPTIONS "-fno-stack-protector")
set_source_files_properties(${MLN_PB_LIBGCC_TAIL_SOURCES} PROPERTIES
	COMPILE_OPTIONS "-fno-stack-protector;-fbuilding-libgcc"
	COMPILE_DEFINITIONS "IN_LIBGCC2"
	INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/${mln_libgcc};${CMAKE_CURRENT_SOURCE_DIR}/${mln_libgcc}/target;${mln_gcc}/gcc;${mln_gcc}/gcc/ginclude;${mln_gcc}/libgcc;${mln_gcc}/include")
include(${mln_libgcc}/lib1funcs.cmake)
gcc48_use_lib1funcs(vs0_vsh_common_libmarlin_pb)
