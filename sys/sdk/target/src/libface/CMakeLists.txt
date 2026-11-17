# SceFace (the SDK's target/sce_module/libface.suprx), built by Sony with SNC 3.57 in C++: face
# detection, facial parts, attributes, age, identification and shape fitting over boosted
# cascades of pixel-difference features loaded from the dictionaries the application gives.
enable_language(CXX)
psp2_module(app0_sce_module_libface KIND user COMPILER snc PARTIAL SOURCES
	age.cpp
	allparts.cpp
	attribute.cpp
	libface.c
)
