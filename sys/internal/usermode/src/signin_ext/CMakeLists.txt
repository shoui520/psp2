enable_language(CXX)
# SceSigninExt (vs0:sys/external/signin_ext.suprx), built by Sony with
# SNC 3.57 in C++: standalone PAF plugin packaging the SceSigninExt
# flow, custom list_edge/plane_shadow widgets, SceNpCommerce2 async
# requests, HTTP/image clients, and signup_library helpers.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_signin_ext KIND user COMPILER snc SOURCES
	signin_ext_module.cpp
	signin_ext_app.cpp
	list_edge_shader.cpp
	list_edge.cpp
	plane_shadow.cpp
	signin_ext_ui.cpp
	signin_ext_model.cpp
	signin_ext_commerce.cpp
	signin_ext_http.cpp
	signin_ext_reg.cpp
	signin_ext_job_queue.cpp
	signin_ext_image_manager.cpp
	list_edge_obj.cpp
	signin_ext_eula.cpp
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_signin_ext PRIVATE
	"${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
	"${PSP2_SRC}/sys/internal/include"
)
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_signin_ext PRIVATE PSP2_STRIP_UNUSED)
target_compile_options(vs0_sys_external_signin_ext PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Xautoinlinesize=0>)
target_link_options(vs0_sys_external_signin_ext PRIVATE -Wl,--gc-sections)
