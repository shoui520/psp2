enable_language(CXX)
# pup:psp2swu.self (ScePsp2Swu), built by Sony with SNC (statically linking 2014 PAF,
# updater_core, common_gui_dialog, msg_dialog_util, page_wizard, dbgfont, libpvf, libSceFt2,
# libsas, libces, deflt, aes, sha256, sha1, libsnc, libgcc).
set(PAF "${PSP2_SRC}/vsh/paf")

# PAF's heap is dlmalloc 2.8.4 (pristine upstream plus vsh/paf's 0.996 SCE patch).
set(swu_dlmalloc_source "${CMAKE_CURRENT_BINARY_DIR}/dlmalloc.c")
configure_file("${PSP2_SRC}/contrib/dlmalloc-2.8.4/malloc.c" "${swu_dlmalloc_source}" COPYONLY)
execute_process(COMMAND patch -s "${swu_dlmalloc_source}"
	"${PAF}/src/memory/dlmalloc_sce_0996.patch"
	RESULT_VARIABLE swu_patch_result ERROR_VARIABLE swu_patch_error)
if(NOT swu_patch_result EQUAL 0)
	message(FATAL_ERROR "psp2swu dlmalloc patch failed: ${swu_patch_error}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	"${PSP2_SRC}/contrib/dlmalloc-2.8.4/malloc.c"
	"${PAF}/src/memory/dlmalloc_sce_0996.patch")

# FreeType 2.4.9 with Sony's patches (same configuration as sys/internal/usermode/src/libSceFt2).
set(FT2 "${PSP2_SRC}/contrib/freetype-2.4.9")
set(FT2_DIR "${PSP2_SRC}/sys/internal/usermode/src/libSceFt2")
set(FT2_SCE "${CMAKE_CURRENT_BINARY_DIR}/freetype-sce")
file(MD5 "${FT2_DIR}/freetype-sce.patch" ft2_patch_md5)
file(MD5 "${FT2_DIR}/freetype-inline.patch" ft2_inline_md5)
string(MD5 ft2_patch_md5 "${ft2_patch_md5} ${ft2_inline_md5} ${FT2}")
set(ft2_stamp "")
if(EXISTS "${FT2_SCE}/.patched")
	file(READ "${FT2_SCE}/.patched" ft2_stamp)
endif()
if(NOT ft2_stamp STREQUAL ft2_patch_md5)
	file(REMOVE_RECURSE "${FT2_SCE}")
	foreach(f src/base/ftsystem.c src/base/ftinit.c include/freetype/internal/ftobjs.h src/psaux src/truetype)
		get_filename_component(d ${f} DIRECTORY)
		file(COPY "${FT2}/${f}" DESTINATION "${FT2_SCE}/${d}")
	endforeach()
	foreach(patch freetype-sce.patch freetype-inline.patch)
		execute_process(COMMAND patch -s -p1 -d "${FT2_SCE}" -i "${FT2_DIR}/${patch}"
			RESULT_VARIABLE ft2_patch_result ERROR_VARIABLE ft2_patch_error)
		if(NOT ft2_patch_result EQUAL 0)
			message(FATAL_ERROR "psp2swu FreeType patch ${patch} failed: ${ft2_patch_error}")
		endif()
	endforeach()
	file(WRITE "${FT2_SCE}/.patched" "${ft2_patch_md5}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	"${FT2_DIR}/freetype-sce.patch"
	"${FT2_DIR}/freetype-inline.patch")

set(MODULE_COMPILE_OPTIONS $<$<COMPILE_LANGUAGE:C,CXX>:-Xautoinlinesize=8>)

# Third-party image & compression libraries statically linked into 2014 PAF.
include("${PAF}/src/misc/zlib/zlib.cmake")
psp2_paf_zlib(swu_zlib)
include("${PAF}/src/misc/image/libjpeg/libjpeg.cmake")
psp2_libjpeg_sce(swu_jpeg)
include("${PAF}/src/misc/image/libpng12/libpng12.cmake")
psp2_libpng12_sce(swu_png12)
include("${PAF}/src/misc/image/giflib/giflib.cmake")
psp2_giflib_sce(swu_gif)
include("${PAF}/src/misc/image/libtiff/libtiff.cmake")
psp2_libtiff_sce(swu_tiff)
include("${PAF}/src/misc/image/squish/squish.cmake")
psp2_paf_squish(swu_squish)

set(swu_ces "${PSP2_SRC}/sys/internal/usermode/src/ces/libces/src")
set(swu_ces_SOURCES
	${swu_ces}/UCS/ces_ucs_context.c
	${swu_ces}/UCS/Str_UTF16_UTF8.c
	${swu_ces}/UCS/Str_UTF8_UTF16.c
	${swu_ces}/UCS/UTF16_UTF8.c
	${swu_ces}/UCS/UTF16BE_UTF8.c
	${swu_ces}/UCS/UTF8.c
	${swu_ces}/MBCS/MbcsCtx_Ucs.c
	${swu_ces}/JIS/JisProfile_SJis1997_CP932.c
	${swu_ces}/JIS/SJis_FromUCS.c
	${swu_ces}/JIS/SJis_FromUTF16.c
	${swu_ces}/JIS/SJis_ToUCS.c
	${swu_ces}/MBCS/ces_mbcs_context.c
	${swu_ces}/MBCS/Mbcs_FromUTF32.c
	${swu_ces}/MBCS/MbcsStr_FromUTF16.c
	${swu_ces}/MBCS/MbcsStr_ToUTF16.c
	${swu_ces}/GB/ces_gb_profile.c
	${swu_ces}/GB/GB_FromUTF16.c
	${swu_ces}/GB/GBProfile_GB18030_2000.c
	${swu_ces}/ces_context.c
	${swu_ces}/UCS/ces_ucs_callback.c
	${swu_ces}/UCS/UTF16.c
	${swu_ces}/UCS/UTF16BE.c
	${swu_ces}/UCS/UTF16LE.c
	${swu_ces}/SBCS/Sbc_UCS.c
	${swu_ces}/SBCS/ISO_646/ASCII.c
	${swu_ces}/JIS/jis1990_Ext_FromUcs.c
	${swu_ces}/JIS/jis1990_Ext_row85_94.c
	${swu_ces}/JIS/jis2004_row01_08.c
	${swu_ces}/JIS/jis2004_row13.c
	${swu_ces}/JIS/jis2004_row16_84.c
	${swu_ces}/JIS/jis2004_UcsCJKExB.c
	${swu_ces}/JIS/Jis_FromUCS.c
	${swu_ces}/JIS/JIS_X_0201_Katakana.c
	${swu_ces}/JIS/JisMenKuTen.c
	${swu_ces}/JIS/ShiftJIS.c
	${swu_ces}/MBCS/ces_mbcs_callback.c
	${swu_ces}/MBCS/Mbcs_FromUCS4.c
	${swu_ces}/MBCS/Mbcs_ToUTF16.c
	${swu_ces}/GB/gb18030_FromUcs.c
	${swu_ces}/GB/gb2byte1_Ucs.c
	${swu_ces}/GB/gb2byte2_Ucs.c
	${swu_ces}/GB/gb2byte3_Ucs.c
	${swu_ces}/GB/gb2byte4_Ucs.c
	${swu_ces}/GB/gb2byte5_Ucs.c
	${swu_ces}/GB/gb4byte_UcsBMP.c
	${swu_ces}/GB/GB_FromUCS.c
	${swu_ces}/GB/GB_ToUCS.c
	${swu_ces}/GB/gbc_FromUcsPrivate.c
	${swu_ces}/GB/gbk_FromUcs.c
	${swu_ces}/SBCS/ces_sbcs_profile.c
	${swu_ces}/JIS/jis2004_FromUcs.c
	${swu_ces}/MBCS/Mbcs_ToUCS4.c
	${swu_ces}/DBCS/DBCS.c
	${swu_ces}/GB/GB.c
	${swu_ces}/GB/gb2byte1_FromUcs.c
	${swu_ces}/GB/gb2byte2_FromUcs.c
	${swu_ces}/GB/gb2byte5_FromUcs.c
)
set_source_files_properties(${swu_ces_SOURCES} PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=40)

set(swu_sas "${PSP2_SRC}/sys/internal/usermode/src/libsas")
set(swu_sas_SOURCES
	${swu_sas}/as/as.c
	${swu_sas}/as/as_buss.c
	${swu_sas}/as/as_core.c
	${swu_sas}/as/as_rack.c
	${swu_sas}/as/as_util.c
	${swu_sas}/as/as_math.c
	${swu_sas}/as/as_task.c
	${swu_sas}/as/as_master.c
	${swu_sas}/as/as_mixer.c
	${swu_sas}/as/as_reverb.c
	${swu_sas}/as/as_synth.c
	${swu_sas}/as/as_synth_voice.c
	${swu_sas}/sas.c
	${swu_sas}/sas_internal.c
)
set_source_files_properties(${swu_sas_SOURCES} PROPERTIES
	COMPILE_DEFINITIONS "PSP2_COMPILER_GCC48;SCE_SAS_STATIC_RUNTIME"
	COMPILE_OPTIONS "-O3;-fdelete-null-pointer-checks;-mfloat-abi=hard;-fsigned-char")

set(swu_libgcc "${PSP2_SRC}/toolchains/libgcc")
set(swu_libgcc_SOURCES
	${swu_libgcc}/addsubsf3.S
	${swu_libgcc}/addsubdf3.S
	${swu_libgcc}/ldivmod.S
	${swu_libgcc}/uldivmod.S
	${swu_libgcc}/idiv_weak.S
	${swu_libgcc}/udiv_weak.S
	${swu_libgcc}/div64_gnu.c
	${swu_libgcc}/udiv64_gnu.c
	${swu_libgcc}/d2lz.c
	${swu_libgcc}/d2ulz.c
	${swu_libgcc}/bpabi_gnu.c
	${swu_sas}/as/as_ssp.S
)
set_source_files_properties(
	${swu_libgcc}/div64_gnu.c ${swu_libgcc}/udiv64_gnu.c
	${swu_libgcc}/d2lz.c ${swu_libgcc}/d2ulz.c ${swu_libgcc}/bpabi_gnu.c PROPERTIES
	COMPILE_DEFINITIONS "PSP2_COMPILER_GCC48;IN_LIBGCC2"
	COMPILE_OPTIONS "-fno-stack-protector;-fbuilding-libgcc"
	INCLUDE_DIRECTORIES "${swu_libgcc};${swu_libgcc}/target;${PSP2_SRC}/contrib/gcc-4.8-2014.05/gcc;${PSP2_SRC}/contrib/gcc-4.8-2014.05/gcc/ginclude;${PSP2_SRC}/contrib/gcc-4.8-2014.05/libgcc;${PSP2_SRC}/contrib/gcc-4.8-2014.05/include")

set(swu_ft2_SOURCES
	${FT2}/src/base/ftbase.c
	${FT2}/src/base/ftbitmap.c
	${FT2}/src/base/ftglyph.c
	${FT2_SCE}/src/base/ftinit.c
	${FT2}/src/base/ftdebug.c
	${FT2}/src/autofit/autofit.c
	${FT2_SCE}/src/truetype/truetype.c
	${FT2}/src/type1/type1.c
	${FT2}/src/cff/cff.c
	${FT2}/src/otvalid/otvalid.c
	${FT2}/src/base/ftotval.c
	${FT2_SCE}/src/base/ftsystem.c
	${FT2_SCE}/src/psaux/psaux.c
	${FT2}/src/psnames/psnames.c
	${FT2}/src/pshinter/pshinter.c
	${FT2}/src/raster/raster.c
	${FT2}/src/sfnt/sfnt.c
	${FT2}/src/smooth/smooth.c
)
set_source_files_properties(${swu_ft2_SOURCES} PROPERTIES
	COMPILE_DEFINITIONS "PSP2_COMPILER_GCC48;FT2_BUILD_LIBRARY"
	COMPILE_OPTIONS "-O3;-fstack-protector;-fhosted;-fbuiltin;-fdelete-null-pointer-checks;-ffunction-sections;-fdata-sections;-fsigned-char"
	INCLUDE_DIRECTORIES "${FT2_DIR}/include;${FT2_SCE}/include;${FT2}/include;${FT2}/src;${FT2}/src/base")
set_source_files_properties(${FT2_DIR}/libc.c PROPERTIES
	COMPILE_DEFINITIONS "PSP2_COMPILER_GCC48;FT2_BUILD_LIBRARY"
	COMPILE_OPTIONS "-ffreestanding;-fno-builtin;-fsigned-char")
set_source_files_properties(${FT2_DIR}/setjmp.S PROPERTIES
	COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)

set(swu_pvf "${PSP2_SRC}/sys/internal/usermode/src/libpvf")
set(swu_pvf_SOURCES
	${swu_pvf}/libpvf.c
	${swu_pvf}/pvf_gsub.c
	${swu_pvf}/pvf_rasterizer.c
)
set_source_files_properties(${swu_pvf_SOURCES} PROPERTIES
	COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-mfloat-abi=hard"
	INCLUDE_DIRECTORIES "${FT2}/include")

set(swu_updater_SOURCES
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/updater_core.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/parallel_copy_file.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/parallel_operation_base.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/partition_manager.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/producer_consumer_channel.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/pup_controller.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/segment_analysis.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/spkg_group_manager.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/update_utility.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/updater_array.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/spkg_group.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/spkg_group_diff.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/spkg_group_full.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/tar_archive.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/memory_fat_fs.cpp
)

psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	# psp2swu application and embedded RCO/font assets
	update_view_base.cpp
	update_cui_view.cpp
	${PSP2_SRC}/sys/internal/usermode/src/cui_setupper/cui_view.c
	updater_page.cpp
	update_gui_view.cpp
	updater_plugin.cpp
	psp2swu_app.cpp
	index_dat.cpp
	embedded_resources.S
	${PSP2_SRC}/sys/sdk/target/src/crt0/crt0_main.c

	# PageWizard, common_gui_dialog, msg_dialog_util, dbgfont
	${PSP2_SRC}/vsh/app/common/src/page_wizard/page_wizard.cpp
	${PSP2_SRC}/sys/internal/usermode/src/common_gui_dialog/common_gui_dialog.cpp
	${PSP2_SRC}/sys/internal/usermode/src/common_gui_dialog/dialog.cpp
	${PSP2_SRC}/sys/internal/usermode/src/common_gui_dialog/layouts.cpp
	${PSP2_SRC}/vsh/cdlg/src/msg_dialog/msg_dialog_util.cpp
	${PSP2_SRC}/sys/internal/usermode/src/dbgfont/libdbgfont.c

	# updater_core + fsutil
	${swu_updater_SOURCES}
	${PSP2_SRC}/sys/internal/usermode/src/fsutil/fs_util.c

	# 2014 PAF static library
	${PAF}/src/avout/sound.cpp
	${PAF}/src/avout/wave.c
	${PAF}/src/buffer/buffer.cpp
	${PAF}/src/file/bufferedfile.cpp
	${PAF}/src/file/localfile.cpp
	${PAF}/src/file/localfile_platform.cpp
	${PAF}/src/file/memfile.cpp
	${PAF}/src/file/openurl.cpp
	${PAF}/src/http/http.cpp
	${PAF}/src/misc/image/image/image.cpp
	${PAF}/src/misc/image/image/image_filter.cpp
	${PAF}/src/misc/image/image/image_array.cpp
	${PAF}/src/misc/image/image/image_attribute.cpp
	${PAF}/src/misc/image/image/image_blend.cpp
	${PAF}/src/misc/image/image/image_blur.cpp
	${PAF}/src/misc/image/image/image_bmp.cpp
	${PAF}/src/misc/image/image/image_convert.cpp
	${PAF}/src/misc/image/image/image_copy.cpp
	${PAF}/src/misc/image/image/image_copywithedge.cpp
	${PAF}/src/misc/image/image/image_create.cpp
	${PAF}/src/misc/image/image/image_crop.cpp
	${PAF}/src/misc/image/image/image_dds.cpp
	${PAF}/src/misc/image/image/image_fade.cpp
	${PAF}/src/misc/image/image/image_codec.cpp
	${PAF}/src/misc/image/image/image_flip.cpp
	${PAF}/src/misc/image/image/image_gif.cpp
	${PAF}/src/misc/image/image/image_gim.cpp
	${PAF}/src/misc/image/image/image_jpg.cpp
	${PAF}/src/misc/image/image/image_operation.cpp
	${PAF}/src/misc/image/image/image_paste.cpp
	${PAF}/src/misc/image/image/image_png.cpp
	${PAF}/src/misc/image/image/image_resize.cpp
	${PAF}/src/misc/image/image/image_rotate.cpp
	${PAF}/src/misc/image/image/image_roundcorner.cpp
	${PAF}/src/misc/image/image/image_sharpen.cpp
	${PAF}/src/misc/image/image/image_source.cpp
	${PAF}/src/misc/image/image/image_tif.cpp
	${PAF}/src/misc/image/image/image_transparentize.cpp
	${PAF}/src/misc/image/image/image_gxt.cpp
	${swu_jpeg_SOURCES}
	${PAF}/src/misc/image/libjpeg/jdatasrc_paf.cpp
	${PAF}/src/misc/image/libjpeg/jmem_paf.cpp
	${swu_png12_SOURCES}
	${PAF}/src/ime/ime.cpp
	${swu_zlib_SOURCES}
	${swu_ces_SOURCES}
	${swu_zlib_CHECKSUM}
	${swu_zlib_INFFAST}
	${swu_squish_SOURCES}
	${swu_tiff_CODECS}
	${PAF}/src/inputdevice/hidkeyboard/hidkeyboard.cpp
	${PAF}/src/inputdevice/inputdevice.cpp
	${PAF}/src/file/httpfile.cpp
	${PAF}/src/file/httpfile_reader.cpp
	${PAF}/src/file/httpfile_range_reader.cpp
	${PAF}/src/file/error.c
	${PAF}/src/compress/compress.cpp
	${PAF}/src/datetime/datetime.cpp
	${PAF}/src/dir/dir.cpp
	${PAF}/src/rtc/rtc.cpp
	${PAF}/src/thread/callqueue.cpp
	${PAF}/src/thread/cond.cpp
	${PAF}/src/thread/mutex.cpp
	${PAF}/src/thread/priority.cpp
	${PAF}/src/thread/rmutex.cpp
	${PAF}/src/thread/rwlock.cpp
	${PAF}/src/thread/semaphore.cpp
	${PAF}/src/thread/sleep.cpp
	${PAF}/src/thread/synccall.cpp
	${PAF}/src/thread/thread.cpp
	${PAF}/src/thread/thread_id_cache.cpp
	${PAF}/src/framework.cpp
	${PAF}/src/common/call_list.c
	${PAF}/src/common/main_thread_call_list.c
	${PAF}/src/common/main_thread_call.c
	${PAF}/src/common/char_encoding.cpp
	${PAF}/src/common/string_util.cpp
	${PAF}/src/common/transition.cpp
	${PAF}/src/inputdevice/hidrm/hidrm.cpp
	${PAF}/src/graphics/debug/keyboard.cpp
	${PAF}/src/graphics/debug/mouse.cpp
	${PAF}/src/graphics/gpu_clock.cpp
	${PAF}/src/url/uri.c
	${PAF}/src/template_cache.cpp
	${PAF}/src/graphics/camera_target.cpp
	${PAF}/src/common/resource_hooks.c
	${PAF}/src/common/frame_count.c
	${PAF}/src/common/halt_assert.c
	${PAF}/src/common/error_callback.cpp
	${PAF}/src/common/sequence.c
	${PAF}/src/common/job_call_queue.cpp
	${PAF}/src/common/job_slots.cpp
	${PAF}/src/common/path.cpp
	${PAF}/src/common/property_list.cpp
	${PAF}/src/common/ref_obj.cpp
	${PAF}/src/common/shared_string.cpp
	${PAF}/src/common/shared_ptr.c
	${PAF}/src/job/job.cpp
	${PAF}/src/math/matrix.cpp
	${PAF}/src/math/quaternion.cpp
	${PAF}/src/math/rect.cpp
	${PAF}/src/math/math.cpp
	${PAF}/src/cache/cache_job_runner.cpp
	${swu_gif_SOURCES}
	${swu_tiff_SOURCES}
	${swu_squish_API}
	${PAF}/src/cache/cache_base.cpp
	${PAF}/src/cache/cache_image.cpp
	${PAF}/src/cache/cache_manager.cpp
	${PAF}/src/cache/cache_manager_impl.cpp
	${PAF}/src/common/encoded_string.cpp
	${PAF}/src/inputdevice/gesturelistener.cpp
	${PAF}/src/inputdevice/motionsensor/motionsensor.cpp
	${PAF}/src/inputdevice/pad/pad.cpp
	${PAF}/src/inputdevice/touchscreen/touchscreen.cpp
	${PAF}/src/inputdevice/btrm/btrm.cpp
	${PAF}/src/module/module.cpp
	${PAF}/src/module/module_impl.cpp
	${PAF}/src/system/system_param.cpp
	${PAF}/src/system/capability.cpp
	${PAF}/src/system/model.cpp
	${PAF}/src/timer/timer.cpp
	${PAF}/src/timer/timerlistener.cpp
	${PAF}/src/timer/timerlistenerlist.cpp
	${PAF}/src/timer/timerqueue.cpp
	${PAF}/src/timer/get_time.cpp
	${PAF}/src/url/url.cpp
	${PAF}/src/misc/autotest_tty.cpp
	${PAF}/src/std/math.c
	${PAF}/src/std/basic_string.cpp
	${PAF}/src/common/view.cpp
	${PAF}/src/plugin.cpp
	${PAF}/src/versatileparam/idparam.cpp
	${PAF}/src/versatileparam/resourceparam.cpp
	${PAF}/src/versatileparam/versatileparam.cpp
	${PAF}/src/versatileparam/visualparam.cpp
	${PAF}/src/cxml/cxml.c
	${PAF}/src/cxml/document.c
	${PAF}/src/cxml/attribute.c
	${PAF}/src/cxml/util.c
	${PAF}/src/cxml/file.c
	${PAF}/src/resource/resource_obj.cpp
	${PAF}/src/resource/resource_obj_table.cpp
	${PAF}/src/graphics/arl/gxm/gxm_texture_gen.cpp
	${PAF}/src/graphics/arl/gxm/gxm_texture_load.cpp
	${PAF}/src/graphics/arl/gxm/gxm_arl_frontend.cpp
	${PAF}/src/graphics/arl/gxm/gxm_clear.cpp
	${PAF}/src/graphics/arl/gxm/gxm_shader.cpp
	${PAF}/src/graphics/arl/gxm/gxm_framebuffer_copy.cpp
	${PAF}/src/graphics/arl/gxm/gxm_scene_stack.cpp
	${PAF}/src/graphics/arl/gxm/gxm_surface.cpp
	${PAF}/src/graphics/arl/gxm/gxm_texture.cpp
	${PAF}/src/graphics/arl/gxm/gxm_vertex_array.cpp
	${PAF}/src/graphics/arl/gxm/gxm_vertex_array_spec.cpp
	${PAF}/src/graphics/drawobj/bubble_obj.cpp
	${PAF}/src/graphics/drawobj/draw_obj.cpp
	${PAF}/src/graphics/drawobj/page_turn_obj.cpp
	${PAF}/src/graphics/drawobj/panel_obj.cpp
	${PAF}/src/graphics/drawobj/plane_obj.cpp
	${PAF}/src/graphics/drawobj/text_obj.cpp
	${PAF}/src/graphics/graphmem/graphmem.cpp
	${PAF}/src/graphics/gutil.cpp
	${PAF}/src/graphics/surface/surface.cpp
	${PAF}/src/graphics/surface/surface_base.cpp
	${PAF}/src/graphics/surface/surface_pool.cpp
	${PAF}/src/graphics/surface/surface_pool_allocator.cpp
	${PAF}/src/graphics/surface/surface_display.cpp
	${PAF}/src/graphics/surface/surface_texture.cpp
	${PAF}/src/graphics/surface/surface_screen.cpp
	${PAF}/src/graphics/text/attributed_string.cpp
	${PAF}/src/graphics/text/text_init.cpp
	${PAF}/src/graphics/text/text_graphics.cpp
	${PAF}/src/graphics/text/text_layout.cpp
	${PAF}/src/graphics/text/text_queue.cpp
	${PAF}/src/graphics/text/text_style.cpp
	${PAF}/src/graphics/text/text_surface.cpp
	${PAF}/src/graphics/text/text_surface_cache.cpp
	${PAF}/src/graphics/text/text_thread.cpp
	${PAF}/src/graphics/text/text_util.cpp
	${PAF}/src/graphics/effects/param.cpp
	${PAF}/src/graphics/camera.cpp
	${PAF}/src/graphics/renderer.cpp
	${PAF}/src/graphics/effects/fft_wave.cpp
	${PAF}/src/graphics/effects/fft_wave_surface.cpp
	${PAF}/src/graphics/effects/wave_renderer.cpp
	${PAF}/src/graphics/effects/panel_renderer.cpp
	${PAF}/src/graphics/effects/page_turn_renderer.cpp
	${PAF}/src/graphics/effects/earth_renderer.cpp
	${PAF}/src/graphics/effects/particles_param.cpp
	${PAF}/src/graphics/effects/particles.cpp
	${PAF}/src/graphics/effects/particles_emitter.cpp
	${PAF}/src/graphics/effects/particles_pattern1.cpp
	${PAF}/src/graphics/effects/particles_pattern2.cpp
	${PAF}/src/graphics/effects/env_auto.cpp
	${PAF}/src/graphics/effects/probe_background.cpp
	${PAF}/src/graphics/effects/env_probe.cpp
	${PAF}/src/graphics/effects/effect_unused.cpp
	${PAF}/src/graphics/effects/effect_globals.cpp
	${PAF}/src/graphics/effects/gate_blur.cpp
	${PAF}/src/graphics/effects/wave_switch.cpp
	${PAF}/src/graphics/drawobj/panel_obj_list.cpp
	${PAF}/src/graphics/effects/custom_trail.cpp
	${PAF}/src/graphics/effects/screen_copy.cpp
	${PAF}/src/graphics/drawobj/drawobj_factory.cpp
	${PAF}/src/widget/w_appicon2d.cpp
	${PAF}/src/widget/w_appicon_base.cpp
	${PAF}/src/widget/w_appicon_simple.cpp
	${PAF}/src/widget/w_box.cpp
	${PAF}/src/widget/w_busyindicator.cpp
	${PAF}/src/widget/w_button.cpp
	${PAF}/src/widget/w_button_base.cpp
	${PAF}/src/widget/w_camera.cpp
	${PAF}/src/widget/w_canvas.cpp
	${PAF}/src/widget/w_state_button_base.cpp
	${PAF}/src/widget/w_supplier.cpp
	${PAF}/src/widget/w_text.cpp
	${PAF}/src/widget/w_text_box.cpp
	${PAF}/src/widget/wutil.cpp
	${PAF}/src/widget/w_check_box.cpp
	${PAF}/src/widget/w_composite_drawer.cpp
	${PAF}/src/widget/w_corner_button.cpp
	${PAF}/src/widget/w_dialog.cpp
	${PAF}/src/widget/w_directbar.cpp
	${PAF}/src/widget/w_file_list.cpp
	${PAF}/src/widget/w_group.cpp
	${PAF}/src/widget/w_image_button.cpp
	${PAF}/src/widget/w_image_slidebar.cpp
	${PAF}/src/widget/w_item_spin.cpp
	${PAF}/src/widget/w_list_item.cpp
	${PAF}/src/widget/w_list_view.cpp
	${PAF}/src/widget/w_num_spin.cpp
	${PAF}/src/widget/w_pageturn.cpp
	${PAF}/src/widget/w_pageturn_base.cpp
	${PAF}/src/widget/w_plane.cpp
	${PAF}/src/widget/w_plane_button.cpp
	${PAF}/src/widget/w_progress_bar.cpp
	${PAF}/src/widget/w_progress_bar_touch.cpp
	${PAF}/src/widget/w_radio_box.cpp
	${PAF}/src/widget/w_radio_button.cpp
	${PAF}/src/widget/w_rich_text.cpp
	${PAF}/src/widget/w_scene.cpp
	${PAF}/src/widget/w_scroll_view.cpp
	${PAF}/src/widget/w_scroll_view_base.cpp
	${PAF}/src/widget/w_scrollbar.cpp
	${PAF}/src/widget/w_scrollbar2d.cpp
	${PAF}/src/widget/w_scrollbar_touch.cpp
	${PAF}/src/widget/w_slidebar.cpp
	${PAF}/src/widget/w_sliding_drawer.cpp
	${PAF}/src/widget/w_speech_balloon.cpp
	${PAF}/src/widget/w_spin_base.cpp
	${PAF}/src/graphics/arl/gxm/gxm_default_textures.cpp
	${PAF}/src/graphics/arl/gxm/gxm_framebuffer_object.cpp
	${PAF}/src/graphics/effects/fft.cpp
	${PAF}/src/graphics/drawobj/default_shader.cpp
	${PAF}/src/graphics/surface/texture_format.cpp
	${PAF}/src/graphics/text/font_rasterizer_image.cpp
	${PAF}/src/graphics/text/font_rasterizer.cpp
	${PAF}/src/graphics/text/font_rasterizer_pvf.cpp
	${PAF}/src/graphics/text/glyph_metrics.cpp
	${PAF}/src/graphics/text/fontcache.c
	${PAF}/src/graphics/effects/blur.cpp
	${PAF}/src/graphics/effects/cube_map.cpp
	${PAF}/src/graphics/effects/depth_of_field.cpp
	${PAF}/src/graphics/effects/texture_ops.cpp
	${PAF}/src/graphics/effects/skydome.cpp
	${PAF}/src/widget/canvas_painter.cpp
	${PAF}/src/widget/text_box_edit_menu.cpp
	${PAF}/src/widget/text_auto_scroll.cpp
	${PAF}/src/widget/text_box_ime_listener.cpp
	${PAF}/src/widget/gesture_drag_ratio.cpp
	${PAF}/src/widget/gesture_drag_ratio_touch.cpp
	${PAF}/src/widget/gesture_drag.cpp
	${PAF}/src/graphics/state.c
	${PAF}/src/widget/core/environment.cpp
	${PAF}/src/widget/core/widget.cpp
	${PAF}/src/widget/core/widget_gesture_listener.cpp
	${PAF}/src/widget/core/animlistener.cpp
	${PAF}/src/widget/core/event.cpp
	${PAF}/src/widget/core/uiinputlistener.cpp
	${PAF}/src/widget/core/widget_gesture.cpp
	${PAF}/src/widget/core/focuser.cpp
	${PAF}/src/widget/core/handler.cpp
	${PAF}/src/widget/core/screen.cpp
	${PAF}/src/widget/core/layouter.cpp
	${PAF}/src/widget/state.c
	${PAF}/src/hash/sha1.c
	${PAF}/src/std/static_runtime.c
	${PAF}/src/std/new.c
	${PAF}/src/std/stdc++/atexit.c
	${PAF}/src/std/stdc++/guard.c
	${PAF}/src/std/stdc++/paf_crt_mgr.c
	${PAF}/src/std/ctype.c
	${PAF}/src/std/stdlib.c
	${PAF}/src/std/sort.c
	${PAF}/src/std/setjmp_abi.S
	${PAF}/src/std/fill.c
	${PAF}/src/std/mem_neon.S
	${PAF}/src/std/stdio.c
	${PAF}/src/std/global_heap.c
	${PAF}/src/memory/heap_allocator.cpp
	${PAF}/src/memory/range_allocator.cpp
	${PAF}/src/memory/pool_allocator.cpp
	${swu_dlmalloc_source}
	${PAF}/src/std/string.c
	${PAF}/src/std/memory.c
	${PAF}/src/std/tty.cpp
	${PAF}/src/std/printf.c
	${PAF}/src/std/format.c
	${PAF}/src/std/wprintf.c
	${PAF}/src/std/wformat.c
	${PAF}/cg/programs_2014.S

	# SDK & runtime libraries: libsnc, deflt, aes, sha256, sha1, libsas, libgcc, libpvf, libSceFt2, stubs
	${PSP2_SRC}/sys/sdk/target/src/libsnc/i32ops.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/aeabi_div0.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/i64ops.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/i64mod.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/i64shr.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr0.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr1.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr2.c
	${PSP2_SRC}/sys/sdk/target/src/deflt/crc.S
	${PSP2_SRC}/sys/sdk/target/src/deflt/zlibhdr.c
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate.S
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate_common.c
	${PSP2_SRC}/sys/sdk/target/src/aes/libaes.c
	${PSP2_SRC}/sys/sdk/target/src/aes/aes.S
	${PSP2_SRC}/sys/sdk/target/src/sha256/libsha256.c
	${PSP2_SRC}/sys/sdk/target/src/sha256/sha256.S
	${PSP2_SRC}/sys/sdk/target/src/sha1/libsha1.c
	${PSP2_SRC}/sys/internal/kernel/src/sysmem/sha1_transform.S
	${swu_sas_SOURCES}
	${swu_libgcc_SOURCES}
	${swu_pvf_SOURCES}
	${FT2_DIR}/setjmp.S
	${FT2_DIR}/libc.c
	${swu_ft2_SOURCES}
	${PSP2_SRC}/sys/sdk/target/src/libSceSblSsUpdateMgr_stub/update_mgr_stub.c
	${PSP2_SRC}/sys/sdk/target/src/libSceVshBridge_stub/vshbridge_io_stub.c
	${PSP2_SRC}/sys/sdk/target/src/libSceVshBridge_stub/vshbridge_nvs_stub.c
	${PSP2_SRC}/sys/sdk/target/src/libSceVshBridge_stub/vshbridge_stub.c
)

# 2014 PAF GPU programs compiled from Cg
include("${PAF}/cg/cg.cmake")
psp2_paf_cg(${PSP2_TARGET} "${PAF}/cg" programs_2014.S)
include("${PSP2_SRC}/toolchains/libgcc/lib1funcs.cmake")
gcc48_use_lib1funcs(${PSP2_TARGET})

target_compile_definitions(${PSP2_TARGET} PRIVATE
	PAF_BUILD_2014
	PAF_STATIC_BUILD
	PAF_LIBRARY_BUILD
	UPDATER_CORE_2014
	COMMON_GUI_DIALOG_2014
	MSG_DIALOG_UTIL_2014
	PSP2_STRIP_UNUSED)

target_include_directories(${PSP2_TARGET} BEFORE PRIVATE
	"${PAF}/include"
	"${PAF}"
	"${PAF}/src")
target_include_directories(${PSP2_TARGET} PRIVATE
	"${PSP2_SRC}/vsh/cdlg/src/msg_dialog"
	"${PSP2_SRC}/vsh/app/common/src/page_wizard"
	"${PSP2_SRC}/sys/sdk/target/src/deflt"
	${swu_zlib_INCLUDE}
	${swu_jpeg_INCLUDE}
	${swu_png12_INCLUDE}
	${swu_gif_INCLUDE}
	${swu_tiff_INCLUDE}
	${swu_squish_INCLUDE})

target_link_options(${PSP2_TARGET} PRIVATE
	-Wl,--gc-sections
	-Wl,--no-warn-mismatch
	-Wl,--defsym=sceProcessParam=__sce_process_param
	-Wl,--defsym=__stack_chk_guard=__sce_process_param
	-Wl,--defsym=__stack_chk_fail=__cxa_pure_virtual)

set_source_files_properties(${PSP2_SRC}/sys/sdk/target/src/crt0/crt0_main.c PROPERTIES
	COMPILE_DEFINITIONS SCE_CRT0_SDK_VERSION=0x00996100)

set_source_files_properties(${swu_updater_SOURCES} PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=24)

set_source_files_properties(${PSP2_SRC}/sys/sdk/target/src/libsnc/i64ops.c PROPERTIES
	COMPILE_DEFINITIONS LIBSNC_0996)
set_source_files_properties(
	${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr0.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr1.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr2.c
	PROPERTIES COMPILE_OPTIONS -Xblxcall=1)

set_source_files_properties(${PSP2_SRC}/sys/sdk/target/src/deflt/deflate.S PROPERTIES
	COMPILE_DEFINITIONS DEFLATE_MEMCPY=sceClibMemcpy_safe)
set_source_files_properties(
	${PSP2_SRC}/sys/sdk/target/src/deflt/zlibhdr.c
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate_common.c PROPERTIES
	COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-Os;-funwind-tables;-ffunction-sections;-fdata-sections")

set_source_files_properties(${PSP2_SRC}/sys/sdk/target/src/aes/libaes.c PROPERTIES
	COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-fno-toplevel-reorder;-ffunction-sections")
set_source_files_properties(${PSP2_SRC}/sys/sdk/target/src/aes/aes.S PROPERTIES
	COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)

set_source_files_properties(${PSP2_SRC}/sys/sdk/target/src/sha256/libsha256.c PROPERTIES
	COMPILE_DEFINITIONS "PSP2_COMPILER_GCC48;SCE_PSP2_SDK_VERSION=0x03100010"
	COMPILE_OPTIONS "-fno-stack-protector;-fno-toplevel-reorder;-ffunction-sections")
set_source_files_properties(${PSP2_SRC}/sys/sdk/target/src/sha256/sha256.S PROPERTIES
	COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)

set_source_files_properties(${PSP2_SRC}/sys/sdk/target/src/sha1/libsha1.c PROPERTIES
	COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")
set_source_files_properties(${PSP2_SRC}/sys/internal/kernel/src/sysmem/sha1_transform.S PROPERTIES
	COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)

set_source_files_properties(
	${PSP2_SRC}/sys/sdk/target/src/libSceSblSsUpdateMgr_stub/update_mgr_stub.c
	${PSP2_SRC}/sys/sdk/target/src/libSceVshBridge_stub/vshbridge_io_stub.c
	${PSP2_SRC}/sys/sdk/target/src/libSceVshBridge_stub/vshbridge_nvs_stub.c
	${PSP2_SRC}/sys/sdk/target/src/libSceVshBridge_stub/vshbridge_stub.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48 COMPILE_OPTIONS -fno-stack-protector)

# Third-party and PAF per-file SNC inlining budgets (matching vsh/paf/CMakeLists.txt)
set_source_files_properties(${swu_jpeg_SOURCES} PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=32)
set_source_files_properties(${swu_png12_SOURCES} PROPERTIES
	COMPILE_OPTIONS -Xautoinlinesize=32 COMPILE_DEFINITIONS PNG_USER_CONFIG)
set_source_files_properties(${PAF}/src/misc/image/image/image_png.cpp PROPERTIES
	COMPILE_DEFINITIONS PNG_USER_CONFIG)
set_source_files_properties(${swu_gif_SOURCES} PROPERTIES
	COMPILE_OPTIONS -Xautoinlinesize=32 COMPILE_DEFINITIONS _GBA_NO_FILEIO)
set_source_files_properties(${swu_tiff_SOURCES} ${swu_tiff_CODECS} PROPERTIES
	COMPILE_OPTIONS -Xautoinlinesize=32 COMPILE_DEFINITIONS NDEBUG)
set_source_files_properties(${swu_squish_SOURCES} ${swu_squish_API} PROPERTIES
	COMPILE_OPTIONS -Xautoinlinesize=32 COMPILE_DEFINITIONS SQUISH_USE_NEON=1)
set_source_files_properties(${swu_zlib_SOURCES} ${swu_zlib_INFFAST} ${PSP2_PAF_ZLIB_CHECKSUM}/checksum.c
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=32)
set_source_files_properties(${PSP2_PAF_ZLIB_UPSTREAM}/zutil.c PROPERTIES
	COMPILE_DEFINITIONS MY_ZCALLOC)
set_source_files_properties("${swu_dlmalloc_source}" PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=21)

set_source_files_properties(
	${PAF}/src/buffer/buffer.cpp ${PAF}/src/cache/cache_base.cpp
	${PAF}/src/cache/cache_manager.cpp ${PAF}/src/cache/cache_manager_impl.cpp
	${PAF}/src/cache/cache_job_runner.cpp ${PAF}/src/dir/dir.cpp
	${PAF}/src/file/bufferedfile.cpp ${PAF}/src/job/job.cpp
	${PAF}/src/math/matrix.cpp ${PAF}/src/math/rect.cpp
	${PAF}/src/memory/heap_allocator.cpp ${PAF}/src/memory/range_allocator.cpp
	${PAF}/src/memory/pool_allocator.cpp
	${PAF}/src/thread/callqueue.cpp ${PAF}/src/thread/cond.cpp ${PAF}/src/thread/mutex.cpp
	${PAF}/src/thread/priority.cpp ${PAF}/src/thread/rmutex.cpp ${PAF}/src/thread/rwlock.cpp
	${PAF}/src/thread/semaphore.cpp ${PAF}/src/thread/sleep.cpp ${PAF}/src/thread/synccall.cpp
	${PAF}/src/thread/thread.cpp ${PAF}/src/thread/thread_id_cache.cpp
	${PAF}/src/file/httpfile.cpp ${PAF}/src/file/httpfile_reader.cpp
	${PAF}/src/file/httpfile_range_reader.cpp ${PAF}/src/file/localfile.cpp
	${PAF}/src/file/localfile_platform.cpp ${PAF}/src/file/memfile.cpp
	${PAF}/src/file/openurl.cpp
	${PAF}/src/inputdevice/gesturelistener.cpp
	${PAF}/src/inputdevice/motionsensor/motionsensor.cpp
	${PAF}/src/inputdevice/pad/pad.cpp ${PAF}/src/inputdevice/touchscreen/touchscreen.cpp
	${PAF}/src/inputdevice/btrm/btrm.cpp ${PAF}/src/inputdevice/hidrm/hidrm.cpp
	${PAF}/src/inputdevice/hidkeyboard/hidkeyboard.cpp
	${PAF}/src/module/module.cpp ${PAF}/src/system/system_param.cpp
	${PAF}/src/timer/timerlistener.cpp ${PAF}/src/timer/timerlistenerlist.cpp
	${PAF}/src/timer/get_time.cpp ${PAF}/src/url/url.cpp ${PAF}/src/rtc/rtc.cpp
	${PAF}/src/misc/autotest_tty.cpp ${PAF}/src/framework.cpp ${PAF}/src/plugin.cpp
	${PAF}/src/versatileparam/resourceparam.cpp ${PAF}/src/versatileparam/versatileparam.cpp
	${PAF}/src/versatileparam/visualparam.cpp
	${PAF}/src/resource/resource_obj.cpp ${PAF}/src/resource/resource_obj_table.cpp
	${PAF}/src/graphics/debug/keyboard.cpp ${PAF}/src/graphics/debug/mouse.cpp
	${PAF}/src/graphics/arl/gxm/gxm_scene_stack.cpp ${PAF}/src/graphics/arl/gxm/gxm_surface.cpp
	${PAF}/src/graphics/arl/gxm/gxm_texture.cpp ${PAF}/src/graphics/arl/gxm/gxm_texture_gen.cpp
	${PAF}/src/graphics/arl/gxm/gxm_texture_load.cpp ${PAF}/src/graphics/arl/gxm/gxm_arl_frontend.cpp
	${PAF}/src/graphics/arl/gxm/gxm_clear.cpp ${PAF}/src/graphics/arl/gxm/gxm_framebuffer_copy.cpp
	${PAF}/src/graphics/arl/gxm/gxm_vertex_array.cpp ${PAF}/src/graphics/arl/gxm/gxm_vertex_array_spec.cpp
	${PAF}/src/graphics/arl/gxm/gxm_default_textures.cpp ${PAF}/src/graphics/arl/gxm/gxm_framebuffer_object.cpp
	${PAF}/src/graphics/effects/fft.cpp ${PAF}/src/graphics/effects/texture_ops.cpp
	${PAF}/src/graphics/drawobj/default_shader.cpp ${PAF}/src/graphics/surface/texture_format.cpp
	${PAF}/src/graphics/text/attributed_string.cpp ${PAF}/src/graphics/text/text_init.cpp
	${PAF}/src/graphics/text/font_rasterizer_image.cpp ${PAF}/src/graphics/text/font_rasterizer.cpp
	${PAF}/src/graphics/text/font_rasterizer_pvf.cpp ${PAF}/src/graphics/text/text_surface_cache.cpp
	${PAF}/src/graphics/drawobj/draw_obj.cpp ${PAF}/src/graphics/drawobj/bubble_obj.cpp
	${PAF}/src/graphics/drawobj/page_turn_obj.cpp ${PAF}/src/graphics/drawobj/panel_obj.cpp
	${PAF}/src/graphics/drawobj/plane_obj.cpp ${PAF}/src/graphics/drawobj/text_obj.cpp
	${PAF}/src/graphics/graphmem/graphmem.cpp ${PAF}/src/graphics/gutil.cpp
	${PAF}/src/graphics/surface/surface.cpp ${PAF}/src/graphics/surface/surface_base.cpp
	${PAF}/src/graphics/surface/surface_pool.cpp ${PAF}/src/graphics/surface/surface_pool_allocator.cpp
	${PAF}/src/graphics/surface/surface_display.cpp ${PAF}/src/graphics/surface/surface_texture.cpp
	${PAF}/src/widget/w_box.cpp ${PAF}/src/widget/w_busyindicator.cpp
	${PAF}/src/widget/w_button.cpp ${PAF}/src/widget/w_button_base.cpp
	${PAF}/src/widget/w_camera.cpp ${PAF}/src/widget/w_canvas.cpp
	${PAF}/src/widget/w_check_box.cpp ${PAF}/src/widget/w_composite_drawer.cpp
	${PAF}/src/widget/w_corner_button.cpp ${PAF}/src/widget/w_dialog.cpp
	${PAF}/src/widget/w_directbar.cpp ${PAF}/src/widget/w_file_list.cpp
	${PAF}/src/widget/w_group.cpp ${PAF}/src/widget/w_image_button.cpp
	${PAF}/src/widget/w_image_slidebar.cpp ${PAF}/src/widget/w_item_spin.cpp
	${PAF}/src/widget/w_list_item.cpp ${PAF}/src/widget/w_num_spin.cpp
	${PAF}/src/widget/w_pageturn.cpp ${PAF}/src/widget/w_pageturn_base.cpp
	${PAF}/src/widget/w_plane.cpp ${PAF}/src/widget/w_plane_button.cpp
	${PAF}/src/widget/w_progress_bar.cpp ${PAF}/src/widget/w_progress_bar_touch.cpp
	${PAF}/src/widget/w_radio_box.cpp ${PAF}/src/widget/w_radio_button.cpp
	${PAF}/src/widget/w_rich_text.cpp ${PAF}/src/widget/w_scene.cpp
	${PAF}/src/widget/w_text.cpp ${PAF}/src/widget/w_text_box.cpp
	${PAF}/src/widget/wutil.cpp ${PAF}/src/widget/text_auto_scroll.cpp
	${PAF}/src/widget/w_scroll_view.cpp ${PAF}/src/widget/w_scroll_view_base.cpp
	${PAF}/src/widget/w_scrollbar.cpp ${PAF}/src/widget/w_scrollbar2d.cpp
	${PAF}/src/widget/w_scrollbar_touch.cpp ${PAF}/src/widget/w_slidebar.cpp
	${PAF}/src/widget/w_sliding_drawer.cpp ${PAF}/src/widget/w_speech_balloon.cpp
	${PAF}/src/widget/w_spin_base.cpp ${PAF}/src/widget/core/event.cpp
	${PAF}/src/widget/core/uiinputlistener.cpp ${PAF}/src/widget/core/widget_gesture.cpp
	${PAF}/src/widget/core/focuser.cpp ${PAF}/src/widget/core/handler.cpp
	${PAF}/src/widget/core/screen.cpp ${PAF}/src/widget/w_state_button_base.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

set_source_files_properties(${PAF}/src/widget/text_box_ime_listener.cpp PROPERTIES
	COMPILE_DEFINITIONS SCE_PSP2_SDK_VERSION=0x00996100)
set_source_files_properties(${PAF}/src/widget/w_list_view.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=25)
set_source_files_properties(${PAF}/src/cache/cache_image.cpp ${PAF}/src/timer/timerqueue.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=16)
set_source_files_properties(${PAF}/src/graphics/surface/surface_screen.cpp
	${PAF}/src/common/string_util.cpp ${PAF}/src/graphics/text/fontcache.c
	${PAF}/src/graphics/effects/custom_trail.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=12)
set_source_files_properties(${PAF}/src/graphics/text/text_graphics.cpp
	${PAF}/src/graphics/text/text_queue.cpp ${PAF}/src/http/http.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=20)
set_source_files_properties(${PAF}/src/graphics/text/text_layout.cpp
	${PAF}/src/graphics/arl/gxm/gxm_shader.cpp ${PAF}/src/common/property_list.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=24)
set_source_files_properties(${PAF}/src/timer/timer.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=29)
set_source_files_properties(${PAF}/src/graphics/text/text_style.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=32)
