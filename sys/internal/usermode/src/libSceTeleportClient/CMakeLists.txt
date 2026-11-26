enable_language(CXX)
# SceTeleportClient (vs0:sys/external/libSceTeleportClient.suprx), built by Sony with SNC 3.57
# in C++: Teleport discovery/control client statically linking libSceHmac, libSceSha256,
# libces (UTF-8), libSceXml (DOM), and bXCe.
psp2_module(vs0_sys_external_libSceTeleportClient KIND user COMPILER snc SOURCES
	teleport_api.cpp
	teleport_client.cpp
	teleport_wol.cpp
	teleport_xml.cpp
	cxx_runtime.cpp
	../../../../sdk/target/src/hmac/sha256.c
	../../../../sdk/target/src/hmac/libhmac.c
	../ces/libces/src/UCS/ces_ucs_context.c
	../ces/libces/src/UCS/Str_UTF8_UCS2.c
	../ces/libces/src/UCS/UTF32_UTF8.c
	../ces/libces/src/UCS/UTF8.c
	../ces/libces/src/ces_context.c
	../ces/libces/src/UCS/ces_ucs_callback.c
	../libSceXml/dom_document.cpp
	../libSceXml/dom_document_builder.cpp
	../libSceXml/dom_document_builder_impl.cpp
	../libSceXml/dom_document_impl.cpp
	../libSceXml/initializer.cpp
	../libSceXml/var_alloc_buffer.cpp
	../libSceXml/dom_node_list.cpp
	../libSceXml/dom_node_list_impl.cpp
	../libSceXml/xml_result.cpp
	../libSceXml/types.cpp
	../libSceXml/bxce_ext.cpp
	../bXCe/mem_allocator.cpp
	../bXCe/attribute.cpp
	../bXCe/attribute_list.cpp
	../bXCe/document.cpp
	../bXCe/allocator.cpp
	../bXCe/name_table.cpp
	../bXCe/node_list.cpp
	../bXCe/tokenizer.cpp
	../bXCe/parser.cpp
	../bXCe/serializer.cpp
	../bXCe/clib.cpp
	../bXCe/result.cpp
	../bXCe/stack.cpp
	../../../../sdk/target/src/sha256/libsha256.c
	../../../../sdk/target/src/sha256/sha256.S
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libSceTeleportClient PRIVATE
	../libSceXml
	../bXCe
	"${PSP2_SRC}/sys/sdk/target/src/hmac"
)
target_compile_definitions(vs0_sys_external_libSceTeleportClient PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libSceTeleportClient PRIVATE -Wl,--gc-sections)
# The SDK's libSceHmac and libSceSha256, built by Sony with GCC and without stack guards
set_source_files_properties(
	../../../../sdk/target/src/hmac/sha256.c
	../../../../sdk/target/src/hmac/libhmac.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector")
set_source_files_properties(
	../../../../sdk/target/src/sha256/libsha256.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")
set_source_files_properties(
	../../../../sdk/target/src/sha256/sha256.S
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)
