enable_language(CXX)
# SceNpBasic (vs0:sys/external/np_basic.suprx), built by Sony with SNC 3.57 in C++. Its asserts name
# npbasic/setup.cpp and npbasic/jga_response.cpp; the rest has no name, its files are split where its
# parts are (ours).
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	exports.cpp
)
