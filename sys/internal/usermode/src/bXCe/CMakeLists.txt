enable_language(CXX)
# ScebXCe (vs0:sys/external/bXCe.suprx), built by Sony with SNC 3.57 in C++: the XML library with
# the binary document form "bX". Being converted: the sources compile, nothing links yet; the
# exports not written yet are still stubs in bXCe.c (2026-11-06)
# Its paf imports (operator new and delete, __cxa_pure_virtual) take libpaf's link names
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_bXCe KIND user COMPILER snc PARTIAL SOURCES
	module.cpp
	attribute_list.cpp
	document.cpp
	allocator.cpp
	name_table.cpp
	node_list.cpp
	tokenizer.cpp
	parser.cpp
	serializer.cpp
	clib.cpp
	dom_document.cpp
	dom_document_builder.cpp
	dom_document_builder_impl.cpp
	dom_document_impl.cpp
	dom_node_list.cpp
	dom_node_list_impl.cpp
	bXCe.c
)
