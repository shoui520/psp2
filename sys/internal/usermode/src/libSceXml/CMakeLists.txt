enable_language(CXX)
# SceLibXml (vs0:sys/external/libSceXml.suprx), built by Sony with SNC 3.57 in C++:
# the SDK's sce::Xml DOM/SAX C++ wrapper over the shared bXCe core library, plus
# bXCe's R/W DOM mutation extensions (bxce_ext.cpp).
psp2_module(vs0_sys_external_libSceXml KIND user COMPILER snc SOURCES
	module.cpp
	../bXCe/mem_allocator.cpp
	attr.cpp
	attr_impl.cpp
	attr_list.cpp
	attr_list_impl.cpp
	document_debug.cpp
	dom_document.cpp
	dom_document_builder.cpp
	dom_document_builder_impl.cpp
	dom_document_impl.cpp
	initializer.cpp
	var_alloc_buffer.cpp
	dom_node.cpp
	dom_node_list.cpp
	dom_node_list_impl.cpp
	sax_parser.cpp
	sax_parser_impl.cpp
	xml_result.cpp
	types.cpp
	cxx_runtime.cpp
	bxce_ext.cpp
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
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
)
target_compile_definitions(vs0_sys_external_libSceXml PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libSceXml PRIVATE -Wl,--gc-sections)
