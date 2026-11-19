enable_language(CXX)
# ScePromoterUtil (vs0:sys/external/libScePromoterUtil.suprx), built by Sony with SNC in C++. No
# record names its files; the file is named by what it holds (ours).
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	promoter_util.cpp
)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
