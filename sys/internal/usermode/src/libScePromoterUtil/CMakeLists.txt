enable_language(CXX)
# ScePromoterUtil (vs0:sys/external/libScePromoterUtil.suprx), built by Sony with SNC in C++. No
# record names its files; the file is named by what it holds (ours). Sony's linker kept the
# functions nothing calls (the property id setters, the hashes).
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	promoter_util.cpp
	promoter_util_ipc.cpp
)
