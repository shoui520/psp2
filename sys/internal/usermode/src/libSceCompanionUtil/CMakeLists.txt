enable_language(CXX)
# SceCompanionUtil (vs0:sys/external/libSceCompanionUtil.suprx), built by Sony with SNC 3.57 in
# C++ (its mutexes' names: "SceCompanionUtil::CTcpSendJobQueueMutex"). No debug record names its
# files; the names are ours, one per class. It links its SHA-1 and its RSA-OAEP encryption (SNC),
# GcAuthMgr's RSA arithmetic (built with GCC, as in the kernel), SNC's weak forwarders, the
# VshBridge stub and the SDK's AES library.
set(cu_marlin ../../../driver/src/marlin_hci)
set(cu_sdk ../../../../sdk/target/src)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	platform.cpp
	callback_lock.cpp
	export.cpp
	companion_util.cpp
	udp.cpp
	tcp.cpp
	tcp_send_queue.cpp
	tcp_recv.cpp
	crypto.cpp
	memory.cpp
	search_list.cpp
	mutex.cpp
	timer.cpp
	sha1.c
	${cu_marlin}/rsa_random.c
	rsa_oaep.c
	rsa_mgf1.c
	${cu_marlin}/rsa_public_operation.c
	${cu_marlin}/rsa_integer_arithmetic.c
	${cu_marlin}/rsa_montgomery.c
	${cu_marlin}/rsa_integer_multiply.c
	${cu_marlin}/rsa_integer_remainder.c
	${cu_marlin}/rsa_integer_square.c
	${cu_marlin}/rsa_integer_tail.c
	${cu_sdk}/libsnc/weak_calls.c
	${cu_sdk}/libSceVshBridge_stub/vshbridge_io_stub.c
	${cu_sdk}/aes/libaes.c
	${cu_sdk}/aes/aes.S
)
# Sony inlined only what these files declare inline (Finalize, RunRecvThread and Flush are called)
set_source_files_properties(companion_util.cpp tcp_send_queue.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=0)
# GcAuthMgr's RSA files, a section per function: the module links only what OAEP needs
set_source_files_properties(
	${cu_marlin}/rsa_random.c
	${cu_marlin}/rsa_public_operation.c
	${cu_marlin}/rsa_integer_arithmetic.c
	${cu_marlin}/rsa_montgomery.c
	${cu_marlin}/rsa_integer_multiply.c
	${cu_marlin}/rsa_integer_remainder.c
	${cu_marlin}/rsa_integer_square.c
	${cu_marlin}/rsa_integer_tail.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS -ffunction-sections)
# SNC's weak forwarders, both of them
set_source_files_properties(${cu_sdk}/libsnc/weak_calls.c
	PROPERTIES COMPILE_DEFINITIONS SNC_WEAK_CALLS_ON)
# the SceVshBridge stub library's wrappers (GCC 4.8, no stack guard): only the PS code call
set_source_files_properties(${cu_sdk}/libSceVshBridge_stub/vshbridge_io_stub.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")
# the SDK's AES library
set_source_files_properties(${cu_sdk}/aes/libaes.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-fno-toplevel-reorder;-ffunction-sections")
set_source_files_properties(${cu_sdk}/aes/aes.S
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
