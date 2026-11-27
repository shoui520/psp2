# vs0:app/NPXS10028/eboot.bin (ScePspemu): Vita-side PSP & PocketStation emulator
# Built by Sony with GCC 4.8.3 (Thumb-2 + NEON).
include("${PSP2_SRC}/vsh/paf/src/misc/zlib/zlib.cmake")
psp2_paf_zlib(pspemu_zlib)
include("${PSP2_SRC}/toolchains/libgcc/lib1funcs.cmake")

psp2_module(vs0_app_NPXS10028_eboot KIND user COMPILER gcc48 PARTIAL SOURCES
	crt0.c
	main.c
	remotefs.c
	remote_msfs.c
	remote_flashfs.c
	iocache.c
	${PSP2_SRC}/sys/sdk/target/src/sha1/libsha1.c
	${PSP2_SRC}/sys/internal/kernel/src/sysmem/sha1_transform.S
	${PSP2_SRC}/contrib/zlib-1.2.5/adler32.c
	${PSP2_SRC}/contrib/zlib-1.2.5/crc32.c
	${PSP2_SRC}/contrib/zlib-1.2.5/deflate.c
	${PSP2_SRC}/contrib/zlib-1.2.5/trees.c
	${PSP2_SRC}/contrib/zlib-1.2.5/zutil.c
	${PSP2_SRC}/sys/sdk/target/src/md5/libmd5.c
	${PSP2_SRC}/sys/sdk/target/src/md5/md5.S
	${PSP2_SRC}/sys/sdk/target/src/deflt/crc.S
	${PSP2_SRC}/sys/sdk/target/src/deflt/zlibhdr.c
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate.S
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate2.S
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate_common.c
	${PSP2_SRC}/sys/sdk/target/src/libSceSblSsUpdateMgr_stub/update_mgr_stub.c
	${PSP2_SRC}/sys/sdk/target/src/sha256/libsha256.c
	${PSP2_SRC}/sys/sdk/target/src/sha256/sha256.S
	${PSP2_SRC}/sys/sdk/target/src/aes/libaes.c
	${PSP2_SRC}/sys/sdk/target/src/aes/aes.S
	${PSP2_SRC}/toolchains/libgcc/udiv_weak.S
	${PSP2_SRC}/toolchains/libgcc/idiv_weak.S
	${PSP2_SRC}/toolchains/libgcc/ldivmod.S
	${PSP2_SRC}/toolchains/libgcc/uldivmod.S
	${PSP2_SRC}/toolchains/libgcc/udiv64_gnu.c
)

if(TARGET vs0_app_NPXS10028_eboot)
	gcc48_use_lib1funcs(vs0_app_NPXS10028_eboot)
	target_include_directories(vs0_app_NPXS10028_eboot PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${PSP2_SRC}/sys/sdk/target/src/deflt"
		"${PSP2_SRC}/toolchains/libgcc/target"
		${pspemu_zlib_INCLUDE})
	target_link_options(vs0_app_NPXS10028_eboot PRIVATE -Wl,--gc-sections)
endif()

set_source_files_properties(
	main.c
	remotefs.c
	remote_msfs.c
	remote_flashfs.c
	iocache.c
	PROPERTIES COMPILE_OPTIONS "-fno-toplevel-reorder;-fno-section-anchors;-fno-partial-inlining;-ffunction-sections")

set_source_files_properties(
	crt0.c
	${PSP2_SRC}/sys/sdk/target/src/sha1/libsha1.c
	${PSP2_SRC}/sys/sdk/target/src/md5/libmd5.c
	${PSP2_SRC}/sys/sdk/target/src/sha256/libsha256.c
	${PSP2_SRC}/sys/sdk/target/src/aes/libaes.c
	${PSP2_SRC}/sys/sdk/target/src/libSceSblSsUpdateMgr_stub/update_mgr_stub.c
	PROPERTIES COMPILE_OPTIONS "-fno-stack-protector;-fno-toplevel-reorder;-ffunction-sections")

set_source_files_properties(
	${PSP2_SRC}/contrib/zlib-1.2.5/adler32.c
	${PSP2_SRC}/contrib/zlib-1.2.5/crc32.c
	${PSP2_SRC}/contrib/zlib-1.2.5/deflate.c
	${PSP2_SRC}/contrib/zlib-1.2.5/trees.c
	${PSP2_SRC}/contrib/zlib-1.2.5/zutil.c
	PROPERTIES COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")

set_source_files_properties(
	${PSP2_SRC}/toolchains/libgcc/udiv64_gnu.c
	PROPERTIES
	COMPILE_DEFINITIONS "IN_LIBGCC2"
	COMPILE_OPTIONS "-fno-stack-protector;-fbuilding-libgcc"
	INCLUDE_DIRECTORIES "${PSP2_SRC}/toolchains/libgcc;${PSP2_SRC}/toolchains/libgcc/target;${PSP2_SRC}/contrib/gcc-4.8-2014.05/gcc;${PSP2_SRC}/contrib/gcc-4.8-2014.05/gcc/ginclude;${PSP2_SRC}/contrib/gcc-4.8-2014.05/libgcc;${PSP2_SRC}/contrib/gcc-4.8-2014.05/include")

set_source_files_properties(
	${PSP2_SRC}/sys/sdk/target/src/deflt/zlibhdr.c
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate_common.c
	PROPERTIES COMPILE_OPTIONS "-Os;-fno-stack-protector;-foptimize-sibling-calls;-fdelete-null-pointer-checks;-fsection-anchors;-fshrink-wrap;-funwind-tables;-ffunction-sections;-fdata-sections")

set_source_files_properties(
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate.S
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate2.S
	PROPERTIES COMPILE_DEFINITIONS DEFLATE_MEMCPY=sceClibMemcpy_safe)
