enable_language(CXX)
# generated skeleton (T2): stub bodies, original module/export/import tables
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	mem.cpp
	runtime.cpp
	base64.cpp
	strtox.cpp
	itoa.cpp
	npext/handle/cancelable.cpp
	npext/handle/handle.cpp
	npext/handle/scope.cpp
	npext/http/http_trans.cpp
	npext/http/http_uri.cpp
	npext/json/json_doc.cpp
	npext/json/json_parser.cpp
	npext/json/json_value.cpp
	npext/memory/memory.cpp
	npext/memory/ring_buf.cpp
	npext/np/communication_id.cpp
	npext/np/online_id.cpp
	npext/np/title_id.cpp
	npext/stream/stream_reader.cpp
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
