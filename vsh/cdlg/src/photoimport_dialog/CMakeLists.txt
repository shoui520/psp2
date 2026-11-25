enable_language(CXX)
# ScePhotoImportDialogPlugin (vs0:sys/external/libcdlg_photoimport.suprx), built by Sony
# with SNC 3.57 in C++: photo import common dialog plugin.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml vsh/app/common/src/dbutil/exports.yml)
psp2_module(vs0_sys_external_libcdlg_photoimport KIND user COMPILER snc SOURCES
	src/photoimport_dialog_plugin.cpp
	src/photo_db.cpp
	src/photo_list_view.cpp
	../savedata_dialog/filelist_view.cpp
	../companion_dialog/src/furl_stack.cpp
	../friendlist2_dialog/src/popup_list_view.cpp
	../msg_dialog/msg_dialog_util.cpp
	../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_photoimport PRIVATE
	"${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
	"${PSP2_SRC}/contrib/sqlite-3.7.6.3"
	"${PSP2_SRC}/vsh/app/common/src"
)
target_compile_definitions(vs0_sys_external_libcdlg_photoimport PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_photoimport PRIVATE -Wl,--gc-sections)
