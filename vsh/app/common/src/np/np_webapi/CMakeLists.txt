enable_language(CXX)
# SceNpWebApi (vs0:sys/external/np_webapi.suprx), built by Sony with SNC 3.57 in C++. Its strings name
# np_webapi/webapi_lang.cpp, and the np_push and np_ipc2 client code it carries; the rest has no name,
# its files are split where its parts are (ours).
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	globals.cpp
	exports.cpp
	libctx.cpp
	heap.cpp
	util.cpp
	userctx.cpp
	request.cpp
	connpool.cpp
	push.cpp
	userctx_create.cpp
	request_abort.cpp
	http.cpp
	request_create.cpp
	userctx_destruct.cpp
	multipart.cpp
	userctx_handle.cpp
	push_notify.cpp
	userctx_setup.cpp
	handle.cpp
	request_io.cpp
	request_header.cpp
	pushfilter.cpp
	extd_registry.cpp
	callback.cpp
	callback_user.cpp
	memstats.cpp
	np_push_ipc_tlv.cpp
	np_push_ipc_data.cpp
	np_push_event.cpp
)
