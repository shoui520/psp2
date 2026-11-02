# SceNet (vs0:sys/external/libnet.suprx), built by Sony with GCC: the SDK's SHA-1 and
# SFMT-19937 libraries and libgcc's division routines are linked in with it.
# Every source is C (the resolver included).
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 SOURCES
	../../../../sdk/target/src/sha1/libsha1.c
	../../../../../toolchains/libgcc/udiv.S
	../../../../../toolchains/libgcc/idiv.S
	../../../../../toolchains/libgcc/dvmd_tls.S
	../../../../../toolchains/libgcc/uldivmod.S
	libnet.c
	ping.c
	dns.c
	emulation.c
	epoll.c
	ether.c
	event.c
	control.c
	interface.c
	socket.c
	memory.c
	stack.c
	show.c
	inet.c
	printf.c
	port.c
	resolver.c
	../../../kernel/src/sysmem/sha1_transform.S
	../../../../sdk/target/src/sfmt19937/sfmt19937.c
	../../../../../toolchains/libgcc/uldivmod64.c
	../../../../../toolchains/libgcc/udiv64.c
)

target_include_directories(${PSP2_TARGET} PRIVATE
	../../../../../toolchains/libgcc
	../../../../../toolchains/libgcc/target
	../../../../../contrib/gcc-4.8-2014.05/gcc
	../../../../../contrib/gcc-4.8-2014.05/gcc/ginclude
	../../../../../contrib/gcc-4.8-2014.05/libgcc
	../../../../../contrib/gcc-4.8-2014.05/include)
include(../../../../../toolchains/libgcc/lib1funcs.cmake)
gcc48_use_lib1funcs(${PSP2_TARGET})
set_source_files_properties(../../../../../toolchains/libgcc/udiv64.c
	PROPERTIES COMPILE_OPTIONS "-fno-stack-protector;-fbuilding-libgcc"
	COMPILE_DEFINITIONS "IN_LIBGCC2")
# Sony built libnet and the SDK's libraries in it without stack guards (it imports no
# __stack_chk_guard)
target_compile_options(${PSP2_TARGET} PRIVATE -fno-stack-protector)
