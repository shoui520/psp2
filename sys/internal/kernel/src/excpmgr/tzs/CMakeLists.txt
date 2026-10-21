# slb2:kbl/SceExcpmgr - the secure world's SceExcpmgr: os0's exception entry and
# context return assembly built with EXCPMGR_TZS, its own vectors and excpmgr.c.
psp2_module(slb2_kbl_SceExcpmgr KIND kernel COMPILER gcc48
	SOURCES excpmgr_vector.S ../excpmgr_initial.S ../excpmgr_context.S excpmgr.c)
set_source_files_properties(../excpmgr_initial.S ../excpmgr_context.S
	PROPERTIES COMPILE_DEFINITIONS EXCPMGR_TZS)
