enable_language(CXX)
# the IPMI client of Sony's libipmi, linked into the module
set(ipmi_source ../../../../../lowlayer/src/ipmi)
# generated skeleton (T2): stub bodies, original module/export/import tables
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	mem.cpp
	runtime.cpp
	base64.cpp
	strtox.cpp
	itoa.cpp
	npext/handle/cancelable.cpp
	npext/handle/handle.cpp
	npext/handle/scope.cpp
	npext/http/http_context.cpp
	npext/http/http_conn_pool.cpp
	npext/http/http_template.cpp
	npext/http/http_trans.cpp
	npext/http/http_uri.cpp
	npext/json/json_doc.cpp
	npext/json/json_parser.cpp
	npext/json/json_string.cpp
	npext/json/json_value.cpp
	npext/memory/memory.cpp
	npext/memory/ring_buf.cpp
	npext_npm/np_service_base_url/get_service_base_url_trans.cpp
	npext_npm/np_token.cpp
	npext_npm/np_trans.cpp
	npext_npm/np_system_param.cpp
	np_util/server_error/np_server_error.c
	crypto/aes.c
	crypto/hmac.c
	crypto/sha256.c
	npext/json/json_lexer.c
	npext_npm/np_environment.cpp
	npext/np/communication_id.cpp
	npext/np/online_id.cpp
	npext/np/title_id.cpp
	npext/stream/stream_reader.cpp
	npext/thread/callback.cpp
	npext/thread/cond.cpp
	npext/thread/event_flag.cpp
	npext/thread/event_queue.cpp
	npext/thread/mutex.cpp
	npext/thread/semaphore.cpp
	npext/thread/timer.cpp
	npext/time/time.cpp
	npasm/np_asm_wrap.cpp
	npasm/np_asm_transport.cpp
	npasm/np_asm_request.cpp
	npasm/np_asm_registry.cpp
	npasm/np_asm_connection.cpp
	npasm/np_asm_client.cpp
	npext/thread/thread.cpp
	../np_ipc_ps4/np_service.cpp
	../np_ipc_ps4/np_ipc2/client/np_ipc2_service_client.cpp
	../np_ipc_ps4/np_ipc2/client/np_ipc2_client_blocking.cpp
	../np_ipc_ps4/np_ipc2/client/np_ipc2_client_eventflag.cpp
	../np_ipc_ps4/np_ipc2/client/np_ipc2_client_eventqueue.cpp
	../np_ipc_ps4/np_ipc2/client/np_ipc2_ipmi_client.cpp
	${ipmi_source}/client.cpp
	${ipmi_source}/client_impl.cpp
	${ipmi_source}/psp2/event_flag.cpp
	${ipmi_source}/psp2/msg_pipe.cpp
	${ipmi_source}/psp2/mutex.cpp
	${ipmi_source}/psp2/semaphore.cpp
	../../../../../../sys/sdk/target/src/libsnc/i64ops.c
	../../../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
	../../../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	np_common_ps4.c
)

# the libsnc runtime objects the module does not call are not linked
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)

# the SHA-256 and the HMAC over it were built with GCC 4.8, without a stack guard
set_source_files_properties(crypto/hmac.c crypto/sha256.c PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS -fno-stack-protector)

# <ipmi/client.h>: the IPMI library's interface
target_include_directories(${PSP2_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../lowlayer/src)
