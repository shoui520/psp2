enable_language(CXX)
# generated skeleton (T2): stub bodies, original module/export/import tables
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	mem.cpp
	base64.cpp
	strtox.cpp
	itoa.cpp
	npext/thread/cond.cpp
	npext/thread/event_flag.cpp
	npext/thread/mutex.cpp
	npext/thread/semaphore.cpp
	npext/thread/thread.cpp
	../../../../../../sys/sdk/target/src/libsnc/i64ops.c
	../../../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	np_common_ps4.c
)

# the libsnc runtime objects the module does not call are not linked
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
