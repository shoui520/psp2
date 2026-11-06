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
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/updater_core.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/parallel_copy_file.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/parallel_operation_base.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/partition_manager.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/producer_consumer_channel.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/pup_controller.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/segment_analysis.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/update_utility.cpp
	${PSP2_SRC}/sys/sdk/target/src/libSceSblSsUpdateMgr_stub/update_mgr_stub.c
	${PSP2_SRC}/sys/sdk/target/src/libSceVshBridge_stub/vshbridge_io_stub.c
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include")
# updater_core, a library of its own in Sony's build: budget 24 writes its classes' C2 and D2
# copies in full and inlines ParallelCopyFileBuffer's constructor into CreateElement, as Sony's
# build did (12 and 16 call C1 from C2), 2026-11-06
set_source_files_properties(
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/updater_core.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/parallel_copy_file.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/parallel_operation_base.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/partition_manager.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/producer_consumer_channel.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/pup_controller.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/segment_analysis.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/update_utility.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=24)
# the SceSblSsUpdateMgr and SceVshBridge stub libraries' wrappers, which Sony built with GCC 4.8
# and without a stack guard, as libpaf's (2026-11-06)
set_source_files_properties(
	${PSP2_SRC}/sys/sdk/target/src/libSceSblSsUpdateMgr_stub/update_mgr_stub.c
	${PSP2_SRC}/sys/sdk/target/src/libSceVshBridge_stub/vshbridge_io_stub.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48 COMPILE_OPTIONS -fno-stack-protector)
