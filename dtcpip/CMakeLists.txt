# swagner (vs0:app/NPXS10080/eboot.bin), built by Sony with SNC -O0 in C and C++ (with
# SDK crypto libraries built with GCC 4.8). No debug record names its files; the build-path
# strings name Sony's dtcpip repository (DTCPIP-MW, DTCPIP-SW, SWKeyManager) and the files
# are placed there. The SDK's start code (the 3.65 variant) links first.
enable_language(CXX)
if(FW STREQUAL "3.630.011-DEBUGGER")
	psp2_module(vs0_app_NPXS10080_eboot KIND user COMPILER snc SOURCES
		crt0.c
		main.cpp
		${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr0.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr1.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr2.c
	)
	set_source_files_properties(
		${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr0.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr1.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr2.c
		PROPERTIES COMPILE_OPTIONS "-O2"
	)
else()
	psp2_module(vs0_app_NPXS10080_eboot KIND user COMPILER snc PARTIAL SOURCES
		crt0.c
		main.cpp
		DTCPIP-MW/Src/Mw/SwMwKeyBin.c
		DTCPIP-MW/Src/Activate/dtcpip_activate.c
		DTCPIP-MW/Src/Comm/vita/SwMwCommServer.cpp
		DTCPIP-MW/Src/Mw/dtcpip_download.c
		DTCPIP-MW/Src/Mw/SwMwManager.cpp
		DTCPIP-MW/Src/Mw/SwMwMain.cpp
		DTCPIP-MW/Src/Mw/SwMwManagerDispatch.cpp
		DTCPIP-MW/Src/Mw/SwMwSinkDispatch.cpp
		DTCPIP-MW/Src/Mw/SwMwSink.cpp
		DTCPIP-MW/Src/Mw/SwMwSinkStream.cpp
		DTCPIP-MW/Src/Mw/SwMwMoveSink.cpp
		DTCPIP-MW/Src/Mw/SwMwMoveSinkStream.cpp
		DTCPIP-MW/Src/Mw/SwMwSACDevice.cpp
		DTCPIP-MW/Src/Mw/SwMwMoveSinkDispatch.cpp
		DTCPIP-MW/Src/Mw/SwMwSync.cpp
		DTCPIP-MW/Src/Mw/SwMwTrace.c
		DTCPIP-MW/Src/Mw/SwMwRecordStore.c
		SWKeyManager/KeyManager/libkm/libswkeystore.c
		SWKeyManager/KeyManager/libactmgr/libdtcpip_actmgr.c
		DTCPIP-SW/core/src/dep/sw_dep_vita.c
		DTCPIP-SW/core/src/dep/sw_dep_core.c
		DTCPIP-SW/core/src/dep/sw_dep_procs.c
		DTCPIP-SW/core/src/opt/sw_opt_parser.c
		DTCPIP-SW/core/src/ecc/sw_prng.c
		DTCPIP-SW/core/src/ecc/sw_ecc.c
		${PSP2_SRC}/sys/sdk/target/src/hmac/libhmac.c
		${PSP2_SRC}/sys/sdk/target/src/hmac/sha1.c
		${PSP2_SRC}/sys/sdk/target/src/hmac/sha256.c
		${PSP2_SRC}/sys/sdk/target/src/sha1/libsha1.c
		${PSP2_SRC}/sys/sdk/target/src/sha1/sha1.S
		${PSP2_SRC}/sys/sdk/target/src/sha256/libsha256.c
		${PSP2_SRC}/sys/sdk/target/src/sha256/sha256.S
		${PSP2_SRC}/sys/sdk/target/src/sha512/libsha512.c
		${PSP2_SRC}/sys/sdk/target/src/sha512/sha512.S
		${PSP2_SRC}/sys/sdk/target/src/aes/libaes.c
		${PSP2_SRC}/sys/sdk/target/src/aes/aes.S
		${PSP2_SRC}/sys/sdk/target/src/md5/libmd5.c
		${PSP2_SRC}/sys/sdk/target/src/md5/md5.S
		${PSP2_SRC}/sys/sdk/target/src/libsnc/pure_virt.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/error.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/i64ops.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/i64shr.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/i32ops.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/aeabi_div0.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr0.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr1.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr2.c
	)
	set_source_files_properties(
		${PSP2_SRC}/sys/sdk/target/src/hmac/libhmac.c
		${PSP2_SRC}/sys/sdk/target/src/hmac/sha1.c
		${PSP2_SRC}/sys/sdk/target/src/hmac/sha256.c
		${PSP2_SRC}/sys/sdk/target/src/sha1/libsha1.c
		${PSP2_SRC}/sys/sdk/target/src/sha256/libsha256.c
		${PSP2_SRC}/sys/sdk/target/src/sha512/libsha512.c
		${PSP2_SRC}/sys/sdk/target/src/aes/libaes.c
		${PSP2_SRC}/sys/sdk/target/src/md5/libmd5.c
		PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
		COMPILE_OPTIONS "-O2;-fno-stack-protector;-fno-toplevel-reorder;-ffunction-sections"
	)
	set_source_files_properties(
		${PSP2_SRC}/sys/sdk/target/src/sha1/sha1.S
		${PSP2_SRC}/sys/sdk/target/src/sha256/sha256.S
		${PSP2_SRC}/sys/sdk/target/src/sha512/sha512.S
		${PSP2_SRC}/sys/sdk/target/src/aes/aes.S
		${PSP2_SRC}/sys/sdk/target/src/md5/md5.S
		PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	)
endif()

# The app's own code is SNC's -O0 (every local in its stack slot, every call result stored
# and reloaded, SNC's accumulator register walking the expressions). The SDK's start code, whose
# object ships precompiled, is SNC's -O2 instead.
target_compile_options(vs0_app_NPXS10080_eboot PRIVATE -O0)
set_source_files_properties(crt0.c PROPERTIES COMPILE_OPTIONS "-O2")
if(NOT FW STREQUAL "3.630.011-DEBUGGER")
	set_source_files_properties(
		DTCPIP-SW/core/src/ecc/sw_prng.c
		DTCPIP-SW/core/src/ecc/sw_ecc.c
		PROPERTIES COMPILE_OPTIONS "-O2;-Xautoinlinesize=0"
	)
	set_source_files_properties(
		${PSP2_SRC}/sys/sdk/target/src/libsnc/pure_virt.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/i64shr.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/i32ops.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/aeabi_div0.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr0.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr1.c
		${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr2.c
		PROPERTIES COMPILE_OPTIONS "-O2"
	)
	set_source_files_properties(
		${PSP2_SRC}/sys/sdk/target/src/libsnc/i64ops.c
		PROPERTIES COMPILE_DEFINITIONS LIBSNC_OMIT_SIGNED_DIVISION
		COMPILE_OPTIONS "-O2"
	)
	set_source_files_properties(
		${PSP2_SRC}/sys/sdk/target/src/libsnc/error.c
		PROPERTIES LANGUAGE CXX COMPILE_OPTIONS "-O2;-Tp;-Xblxcall=1"
	)
endif()

