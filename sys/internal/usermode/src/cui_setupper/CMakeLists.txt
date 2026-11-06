enable_language(CXX)
# os0:ue/cui_setupper.self (SceCuiSetUpper), built by Sony with SNC; one build, shipped as
# os0:ue/cui_setupper and in the PUP (pup:cui_setupper): the target is named from the rel.
# Being converted to C++: the sources compile, nothing links yet.
# Sony's build kept every method out of line and wrote both constructor and destructor copies
# (C1/C2, D1/D2) in full: budgets 12 to 16 do both, 0 to 8 call C1 from C2, 24 inlines
# CuiSetUpper::Initialize (2026-11-06)
set(MODULE_COMPILE_OPTIONS $<$<COMPILE_LANGUAGE:C,CXX>:-Xautoinlinesize=12>)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	cui_setupper.cpp
	cui_view.c
	${PSP2_SRC}/sys/sdk/target/src/crt0/crt0_main.c
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include")
