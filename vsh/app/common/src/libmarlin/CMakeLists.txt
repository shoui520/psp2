# SceLibMarlin, Intertrust's Marlin DRM client, built with GCC in ARM mode, -O2 with the
# vectorizer (its copy loops are vectorized; -O3 would also peel and unroll them, Sony's are not).
# No stack protector: the module imports no __stack_chk_guard.
# One Sony build, shipped as vs0:vsh/common/libmarlin and vs0:sys/external/libmln:
# the target is named from the rel. No debug records or strings name its files: the
# file names are ours, one per address range.
set(MLN_ARM_SOURCES
	buffer.c
	memory.c
	mutex.c
	api.c
	string.c
	platform.c
	time.c
	esb.c
	core.c
)
# The XML reader keeps every store before the loads after it, as -fno-strict-aliasing
# compiles it (its node constructors store the node before reading the object's count).
set(MLN_XML_SOURCES
	xml.c
	xmlnode.c
	xmlbuild.c
)
# The engine too: it re-reads its progress after each store into the step it fills.
set(MLN_ENGINE_SOURCES
	engine.c
	client.c
	message.c
	textbuffer.c
	file.c
	dir.c
	base64.c
	service.c
)
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 PARTIAL SOURCES
	${MLN_ARM_SOURCES}
	${MLN_ENGINE_SOURCES}
	${MLN_XML_SOURCES}
	libmarlin.c
)
set_source_files_properties(${MLN_ARM_SOURCES} PROPERTIES COMPILE_OPTIONS "-marm;-O2;-ftree-vectorize;-finline-functions;-fno-stack-protector")
set_source_files_properties(${MLN_XML_SOURCES} ${MLN_ENGINE_SOURCES} PROPERTIES COMPILE_OPTIONS "-marm;-O2;-ftree-vectorize;-finline-functions;-fno-stack-protector;-fno-strict-aliasing")
