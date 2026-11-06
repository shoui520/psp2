enable_language(CXX)
# ScebXCe (vs0:sys/external/bXCe.suprx), built by Sony with SNC 3.57 in C++: the XML library with
# the binary document form "bX". Being converted: the sources compile, nothing links yet; the
# exports not written yet are still stubs in bXCe.c (2026-11-06)
psp2_module(vs0_sys_external_bXCe KIND user COMPILER snc PARTIAL SOURCES
	module.cpp
	attribute_list.cpp
	document.cpp
	allocator.cpp
	name_table.cpp
	node_list.cpp
	tokenizer.cpp
	parser.cpp
	bXCe.c
)
