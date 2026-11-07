enable_language(CXX)
# ScePhotoReviewDialogPlugin (vs0:sys/external/libcdlg_photoreview.suprx), built by Sony with SNC
# 3.57 in C++: the photo review dialog's plugin. Its paf imports take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_photoreview KIND user COMPILER snc PARTIAL SOURCES
	src/photoreview_dialog_plugin.cpp
	src/event_dispatcher.cpp
	src/input_listener.cpp
	src/dialog_page.cpp
	src/plugin_app.cpp
	src/exif_reader.cpp
	src/gui_dialog.cpp
	src/gesture_handler.cpp
	src/photo_app_launch_job.cpp
	../../../app/common/src/photo_makernote/photo_makernote.cpp
)
target_include_directories(vs0_sys_external_libcdlg_photoreview PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_photoreview PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_photoreview PRIVATE -Wl,--gc-sections)
