# ScePaf was built with SNC. Unconverted exports remain in libpaf.c.
# PAF's heap is dlmalloc 2.8.4: pristine upstream from contrib plus Sony's changes
# (src/memory/dlmalloc_sce.patch), applied at configure time into the build directory.
set(paf_dlmalloc_source "${CMAKE_CURRENT_BINARY_DIR}/src/memory/dlmalloc.c")
configure_file("${PSP2_SRC}/contrib/dlmalloc-2.8.4/malloc.c" "${paf_dlmalloc_source}" COPYONLY)
execute_process(COMMAND patch -s "${paf_dlmalloc_source}"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/memory/dlmalloc_sce.patch"
	RESULT_VARIABLE paf_patch_result ERROR_VARIABLE paf_patch_error)
if(NOT paf_patch_result EQUAL 0)
	message(FATAL_ERROR "PAF's dlmalloc patch failed: ${paf_patch_error}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	"${PSP2_SRC}/contrib/dlmalloc-2.8.4/malloc.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/memory/dlmalloc_sce.patch")
# Sony's build inlined functions declared inline (class bodies) and only the smallest others:
# any budget from 4 to 12 reproduces TTY's and Output's constructors, Output::WaitSoundStopped
# and common::Utf8ToUtf16, 0 and 13 or more do not (2026-10-31).
set(MODULE_COMPILE_OPTIONS $<$<COMPILE_LANGUAGE:C,CXX>:-Xautoinlinesize=8>)
# zlib 1.2.5 unmodified, with libhttp's check values and paf's allocator (src/misc/zlib).
include("${CMAKE_CURRENT_SOURCE_DIR}/src/misc/zlib/zlib.cmake")
psp2_paf_zlib(paf_zlib)
# libjpeg 8c with Sony's changes (src/misc/image/libjpeg).
include("${CMAKE_CURRENT_SOURCE_DIR}/src/misc/image/libjpeg/libjpeg.cmake")
psp2_libjpeg_sce(paf_jpeg)
# libpng 1.5.10 with Sony's configuration (src/misc/image/libpng).
include("${CMAKE_CURRENT_SOURCE_DIR}/src/misc/image/libpng/libpng.cmake")
psp2_libpng_sce(paf_png)
# giflib 4.1.6 with Sony's changes (src/misc/image/giflib).
include("${CMAKE_CURRENT_SOURCE_DIR}/src/misc/image/giflib/giflib.cmake")
psp2_giflib_sce(paf_gif)
# libtiff 3.9.5 with Sony's changes (src/misc/image/libtiff).
include("${CMAKE_CURRENT_SOURCE_DIR}/src/misc/image/libtiff/libtiff.cmake")
psp2_libtiff_sce(paf_tiff)
# squish 1.11 (src/misc/image/squish).
include("${CMAKE_CURRENT_SOURCE_DIR}/src/misc/image/squish/squish.cmake")
psp2_paf_squish(paf_squish)
enable_language(CXX)
psp2_module(vs0_sys_external_libpaf KIND user COMPILER snc PARTIAL SOURCES
	src/module.c
	src/avout/sound.cpp
	src/avout/wave.c
	src/buffer/buffer.cpp
	src/file/file.c
	src/file/bufferedfile.cpp
	src/file/localfile.cpp
	src/file/localfile_platform.cpp
	src/file/memfile.cpp
	src/file/openurl.cpp
	src/http/http.cpp
	src/misc/image/image/image.cpp
	src/misc/image/image/image_filter.cpp
	src/misc/image/image/image_array.cpp
	src/misc/image/image/image_attribute.cpp
	src/misc/image/image/image_blend.cpp
	src/misc/image/image/image_blur.cpp
	src/misc/image/image/image_bmp.cpp
	src/misc/image/image/image_convert.cpp
	src/misc/image/image/image_copy.cpp
	src/misc/image/image/image_copywithedge.cpp
	src/misc/image/image/image_create.cpp
	src/misc/image/image/image_crop.cpp
	src/misc/image/image/image_dds.cpp
	src/misc/image/image/image_fade.cpp
	src/misc/image/image/image_codec.cpp
	src/misc/image/image/image_flip.cpp
	src/misc/image/image/image_gif.cpp
	src/misc/image/image/image_gim.cpp
	src/misc/image/image/image_jpg.cpp
	src/misc/image/image/image_operation.cpp
	src/misc/image/image/image_paste.cpp
	src/misc/image/image/image_png.cpp
	src/misc/image/image/image_resize.cpp
	src/misc/image/image/image_rotate.cpp
	src/misc/image/image/image_roundcorner.cpp
	src/misc/image/image/image_sharpen.cpp
	src/misc/image/image/image_source.cpp
	src/misc/image/image/image_tif.cpp
	src/misc/image/image/image_transparentize.cpp
	src/misc/image/image/image_gxt.cpp
	${paf_jpeg_SOURCES}
	src/misc/image/libjpeg/jdatasrc_paf.cpp
	src/misc/image/libjpeg/jmem_paf.cpp
	${paf_png_SOURCES}
	src/ime/ime.cpp
	${paf_png_NEON}
	${paf_zlib_SOURCES}
	${paf_zlib_CHECKSUM}
	${paf_zlib_INFFAST}
	${paf_squish_SOURCES}
	${paf_tiff_CODECS}
	src/inputdevice/hidkeyboard/hidkeyboard.cpp
	src/inputdevice/inputdevice.cpp
	src/file/httpfile.cpp
	src/file/httpfile_reader.cpp
	src/file/httpfile_range_reader.cpp
	src/file/error.c
	src/compress/compress.cpp
	src/datetime/datetime.cpp
	src/dir/dir.cpp
	src/rtc/rtc.cpp
	src/thread/callqueue.cpp
	src/thread/cond.cpp
	src/thread/mutex.cpp
	src/thread/priority.cpp
	src/thread/rmutex.cpp
	src/thread/rwlock.cpp
	src/thread/semaphore.cpp
	src/thread/sleep.cpp
	src/thread/synccall.cpp
	src/thread/thread.cpp
	src/thread/thread_id_cache.cpp
	src/framework.cpp
	src/common/call_list.c
	src/common/main_thread_call_list.c
	src/common/main_thread_call.c
	src/common/char_encoding.cpp
	src/common/string_util.cpp
	src/common/transition.cpp
	src/inputdevice/hidrm/hidrm.cpp
	src/graphics/debug/keyboard.cpp
	src/graphics/debug/mouse.cpp
	src/graphics/gpu_clock.cpp
	src/url/uri.c
	src/template_cache.cpp
	src/graphics/camera_target.cpp
	src/common/resource_hooks.c
	src/common/frame_count.c
	src/common/halt_assert.c
	src/common/sequence.c
	src/common/job_call_queue.c
	src/common/job_slots.cpp
	src/common/path.c
	src/common/property_list.cpp
	src/common/ref_obj.cpp
	src/common/shared_string.cpp
	src/common/shared_ptr.c
	src/job/job.cpp
	src/math/matrix.cpp
	src/math/quaternion.cpp
	src/math/rect.cpp
	src/math/math.cpp
	src/cache/cache_job_runner.cpp
	${paf_gif_SOURCES}
	${paf_tiff_SOURCES}
	${paf_squish_API}
	src/cache/cache_base.cpp
	src/cache/cache_image.cpp
	src/cache/cache_manager.cpp
	src/cache/cache_manager_impl.cpp
	src/common/encoded_string.cpp
	src/inputdevice/gesturelistener.cpp
	src/inputdevice/motionsensor/motionsensor.cpp
	src/inputdevice/pad/pad.cpp
	src/inputdevice/touchscreen/touchscreen.cpp
	src/inputdevice/btrm/btrm.cpp
	src/module/module.cpp
	src/module/module_impl.cpp
	src/system/system_param.cpp
	src/system/capability.cpp
	src/system/model.cpp
	src/timer/timer.cpp
	src/timer/timerlistener.cpp
	src/timer/timerlistenerlist.cpp
	src/timer/timerqueue.cpp
	src/timer/get_time.cpp
	src/url/url.cpp
	src/misc/autotest_tty.cpp
	src/std/math.c
	src/common/view.cpp
	src/plugin.cpp
	src/versatileparam/idparam.cpp
	src/versatileparam/resourceparam.cpp
	src/versatileparam/versatileparam.cpp
	src/versatileparam/visualparam.cpp
	src/cxml/cxml.c
	src/cxml/document.c
	src/cxml/attribute.c
	src/cxml/util.c
	src/cxml/file.c
	src/resource/resource_obj.cpp
	src/resource/resource_obj_table.cpp
	src/graphics/arl/gxm/gxm_texture_gen.cpp
	src/graphics/arl/gxm/gxm_texture_load.cpp
	src/graphics/arl/gxm/gxm_arl_frontend.cpp
	src/graphics/arl/gxm/gxm_clear.cpp
	src/graphics/arl/gxm/gxm_shader.cpp
	src/graphics/arl/gxm/gxm_framebuffer_copy.cpp
	src/graphics/arl/gxm/gxm_scene_stack.cpp
	src/graphics/arl/gxm/gxm_surface.cpp
	src/graphics/arl/gxm/gxm_texture.cpp
	src/graphics/arl/gxm/gxm_vertex_array.cpp
	src/graphics/arl/gxm/gxm_vertex_array_spec.cpp
	src/graphics/drawobj/bubble_obj.cpp
	src/graphics/drawobj/draw_obj.cpp
	src/graphics/drawobj/page_turn_obj.cpp
	src/graphics/drawobj/panel_obj.cpp
	src/graphics/drawobj/plane_obj.cpp
	src/graphics/drawobj/text_obj.cpp
	src/graphics/graphmem/graphmem.cpp
	src/graphics/gutil.cpp
	src/graphics/surface/surface.cpp
	src/graphics/surface/surface_base.cpp
	src/graphics/surface/surface_pool.cpp
	src/graphics/surface/surface_pool_allocator.cpp
	src/graphics/surface/surface_display.cpp
	src/graphics/surface/surface_texture.cpp
	src/graphics/surface/surface_screen.cpp
	src/graphics/text/attributed_string.cpp
	src/graphics/text/text_init.cpp
	src/graphics/text/text_graphics.cpp
	src/graphics/text/text_layout.cpp
	src/graphics/text/text_queue.cpp
	src/graphics/text/text_style.cpp
	src/graphics/text/text_surface.cpp
	src/graphics/text/text_surface_cache.cpp
	src/graphics/text/text_thread.cpp
	src/graphics/text/text_util.cpp
	src/graphics/effects/param.cpp
	src/graphics/camera.cpp
	src/graphics/renderer.cpp
	src/graphics/effects/fft_wave.cpp
	src/graphics/effects/fft_wave_surface.cpp
	src/graphics/effects/wave_renderer.cpp
	src/graphics/effects/panel_renderer.cpp
	src/graphics/effects/page_turn_renderer.cpp
	src/graphics/effects/env_auto.cpp
	src/graphics/effects/probe_background.cpp
	src/graphics/effects/env_probe.cpp
	src/graphics/effects/effect_unused.cpp
	src/graphics/effects/effect_globals.cpp
	src/graphics/effects/gate_blur.cpp
	src/graphics/effects/wave_switch.cpp
	src/graphics/drawobj/panel_obj_list.cpp
	src/graphics/effects/custom_trail.cpp
	src/graphics/effects/screen_copy.cpp
	src/graphics/drawobj/drawobj_factory.cpp
	src/graphics/arl/gxm/gxm_default_textures.cpp
	src/graphics/arl/gxm/gxm_framebuffer_object.cpp
	src/graphics/effects/fft.cpp
	src/graphics/drawobj/default_shader.cpp
	src/graphics/surface/texture_format.cpp
	src/graphics/text/font_rasterizer_image.cpp
	src/graphics/text/font_rasterizer.cpp
	src/graphics/text/font_rasterizer_pvf.cpp
	src/graphics/text/glyph_metrics.cpp
	src/graphics/text/fontcache.c
	src/graphics/effects/blur.cpp
	src/graphics/effects/cube_map.cpp
	src/graphics/effects/depth_of_field.cpp
	src/graphics/effects/texture_ops.cpp
	src/graphics/effects/skydome.cpp
	src/graphics/drawobj/focus_shader.cpp
	src/widget/canvas_painter.cpp
	src/widget/text_box_edit_menu.cpp
	src/widget/text_auto_scroll.cpp
	src/widget/text_box_ime_listener.cpp
	src/widget/gesture_drag_ratio.cpp
	src/widget/gesture_drag_ratio_touch.cpp
	src/widget/gesture_drag.cpp
	src/toplevel/state.c
	src/graphics/state.c
	src/widget/core/environment.cpp
	src/widget/core/widget.cpp
	src/widget/state.c
	src/hash/sha1.c
	src/std/new.c
	src/std/stdc++/atexit.c
	src/std/stdc++/guard.c
	src/std/stdc++/paf_crt_mgr.c
	src/std/ctype.c
	src/std/stdlib.c
	src/std/sort.c
	src/std/setjmp_abi.S
	src/std/fill.c
	src/std/stdio.c
	src/std/global_heap.c
	src/memory/heap_allocator.cpp
	src/memory/range_allocator.cpp
	src/memory/pool_allocator.cpp
	${paf_dlmalloc_source}
	src/std/string.c
	src/std/memory.c
	src/std/tty.cpp
	src/std/printf.c
	src/std/format.c
	src/std/format_abi.S
	src/std/wprintf.c
	src/std/wformat.c
	src/std/wformat_abi.S
	src/std/string_vector.c
	src/misc/constant.c
	src/misc/fields.c
	src/misc/nested.c
	src/misc/ops.c
	src/misc/short.c
	src/misc/bounds.c
	src/misc/bitmask.c
	src/misc/string_variant.c
	src/misc/lock.c
	src/misc/rect_contains.c
	src/misc/search.c
	src/misc/containment.c
	src/misc/global_state.c
	../../sys/sdk/target/src/libsnc/i32ops.c
	../../sys/sdk/target/src/libsnc/i64ops.c
	../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../sys/sdk/target/src/libsnc/unwind_pr1.c
	../../sys/sdk/target/src/deflt/crc.S
	../../sys/sdk/target/src/deflt/zlibhdr.c
	../../sys/sdk/target/src/deflt/deflate.c
	../../sys/sdk/target/src/deflt/deflate_common.c
	../../sys/sdk/target/src/libSceVshBridge_stub/vshbridge_stub.c
	libpaf.c
)
# libpaf's own build of its headers (shared_string.h inlines what other modules import)
target_compile_definitions(vs0_sys_external_libpaf PRIVATE PAF_LIBRARY_BUILD)

# the SceVshBridge stub library's wrapper, which Sony built with GCC 4.8 and without a stack
# guard, like the SDK's libraries in libhttp (2026-11-02)
set_source_files_properties(../../sys/sdk/target/src/libSceVshBridge_stub/vshbridge_stub.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48 COMPILE_OPTIONS -fno-stack-protector)

target_include_directories(vs0_sys_external_libpaf PRIVATE
	"${CMAKE_SOURCE_DIR}/sys/sdk/target/src/deflt"
	${paf_zlib_INCLUDE}
	${paf_jpeg_INCLUDE}
	${paf_png_INCLUDE}
	${paf_gif_INCLUDE}
	${paf_tiff_INCLUDE}
	${paf_squish_INCLUDE})

# paf::thread's and paf::job's constructors and destructors have full C2/D2 copies where 8 makes
# them call C1/D1, and CallQueue::Enqueue calls string::assign out of line; budgets 29 to 31 do
# both (2026-10-31).
set_source_files_properties(
	src/buffer/buffer.cpp src/cache/cache_base.cpp src/cache/cache_manager.cpp
	src/cache/cache_manager_impl.cpp src/cache/cache_job_runner.cpp
	src/dir/dir.cpp src/file/bufferedfile.cpp src/job/job.cpp src/math/matrix.cpp src/math/rect.cpp
	src/memory/heap_allocator.cpp src/memory/range_allocator.cpp src/memory/pool_allocator.cpp
	src/thread/callqueue.cpp src/thread/cond.cpp src/thread/mutex.cpp src/thread/priority.cpp
	src/thread/rmutex.cpp src/thread/rwlock.cpp src/thread/semaphore.cpp src/thread/sleep.cpp
	src/thread/synccall.cpp src/thread/thread.cpp src/thread/thread_id_cache.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# libjpeg was built with SNC's default budget, 32 (its manual: the old -Xinllev's equivalent):
# read_markers inlines get_soi, get_dac, get_dri and first_marker, while write_scan_header and
# consume_markers keep their calls; 30 and 20 change these, none and 32 do not (2026-11-06).
set_source_files_properties(${paf_jpeg_SOURCES} PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=32)
# libpng too (SNC's default budget), built with its NEON filters (configure's --enable-arm-neon:
# png_read_filter_row installs arm/filter_neon.S's functions for 3 and 4 bytes a pixel).
set_source_files_properties(${paf_png_SOURCES} PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=32
	COMPILE_DEFINITIONS "PNG_USER_CONFIG;PNG_ARM_NEON")
# giflib too (_GBA_NO_FILEIO: no DGifOpenFileName, DGifOpenFileHandle or DGifSlurp).
set_source_files_properties(${paf_gif_SOURCES} PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=32
	COMPILE_DEFINITIONS _GBA_NO_FILEIO)
# libtiff too, without its asserts.
set_source_files_properties(${paf_tiff_SOURCES} ${paf_tiff_CODECS} PROPERTIES
	COMPILE_OPTIONS -Xautoinlinesize=32 COMPILE_DEFINITIONS NDEBUG)
# squish too.
set_source_files_properties(${paf_squish_SOURCES} ${paf_squish_API} PROPERTIES
	COMPILE_OPTIONS -Xautoinlinesize=32 COMPILE_DEFINITIONS SQUISH_USE_NEON=1)
# zlib too; zutil.c without its allocator (zalloc.c has paf's).
set_source_files_properties(${paf_zlib_SOURCES} ${paf_zlib_INFFAST} ${PSP2_PAF_ZLIB_CHECKSUM}/checksum.c
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=32)
set_source_files_properties(${PSP2_PAF_ZLIB_UPSTREAM}/zutil.c PROPERTIES
	COMPILE_DEFINITIONS MY_ZCALLOC)

# Sony's dlmalloc inlined init_mparams, init_top, segment_holding and internal_mallinfo but not
# init_user_mstate: budgets 21 to 24 do that, 20 or less keeps internal_mallinfo out of line
# (2026-10-31).
set_source_files_properties("${paf_dlmalloc_source}" PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=21)

# CacheImage's destructor stays out of line in its subclass's and its own deleting destructors,
# while IsResourcePath is inlined: budget 16 does both, 12 and 20 do not (2026-10-31).
set_source_files_properties(src/cache/cache_image.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=16)

# The debug keyboard and mouse inline their buttons' and pointers' constructors and updates
# (2026-11-06).
set_source_files_properties(src/graphics/debug/keyboard.cpp src/graphics/debug/mouse.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# ScreenSurface inlines its table lookups and GetRawContext but calls InitDepthSurface,
# CreateRenderTarget and its complete destructor: budgets 12 to 20 do that, 24 and 30 inline
# them (2026-11-02).
set_source_files_properties(src/graphics/surface/surface_screen.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=12)

# AttributedString inlines the list's erase and destructor and its destructor's body into D0 and
# D2, and calls ClearStyleRuns, its attribute templates and the list's insert: budget 30 with
# those three noinline does that; 20 keeps D1 out of D0 and D2, 12 calls erase (2026-11-02).
set_source_files_properties(src/graphics/text/attributed_string.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# InitText inlines both rasterizer factories (the emoji one too): budget 30 does that, 20 calls
# CreateEmojiFontRasterizer, 40 inlines StopTextThreads (2026-11-02).
set_source_files_properties(src/graphics/text/text_init.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)
# FontRasterizerImage: the glyph map's find and operator[] inline, its frame helpers stay calls
set_source_files_properties(src/graphics/text/font_rasterizer_image.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# TextGraphics: budget 16 and up inline the list's erase into UpdateChars and the vector's
# destructor into the destructors; 12 and below call them (2026-11-02).
set_source_files_properties(src/graphics/text/text_graphics.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=20)

# TextLayout inlines the vectors' destructors into its own and the context's: budget 24 does that
# and keeps HitTestChar, SetSelectionPos and MoveCaretUp as Sony's; 20 or less calls the
# destructors, 30 inlines too much into the hit tests (2026-11-02).
set_source_files_properties(src/graphics/text/text_layout.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=24)
set_source_files_properties(src/graphics/text/text_queue.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=20)
set_source_files_properties(src/graphics/text/text_style.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=32)
set_source_files_properties(src/graphics/text/text_surface_cache.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# The file units (HttpFile, LocalFile, MemFile, OpenUrl) were written and matched at the C++
# units' budget, 30 (2026-10-31): LocalFile's constructors inline CreateImpl, its destructor Close and DeleteImpl.
set_source_files_properties(
	src/file/httpfile.cpp src/file/httpfile_reader.cpp src/file/httpfile_range_reader.cpp
	src/file/localfile.cpp src/file/localfile_platform.cpp src/file/memfile.cpp
	src/file/openurl.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# paf::http::Transaction's base destructor calls its complete one, which inlines Destroy: budget 20
# does that, 30 copies the complete one (2026-10-31).
set_source_files_properties(src/http/http.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=20)

# The gesture listeners inline map::find, the erase by key into erase(iterator) and
# GestureListener::Detach: budgets 24 to 31 do (2026-10-31).
set_source_files_properties(src/inputdevice/gesturelistener.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# motionsensor's, pad's, touchscreen's, btrm's, hidrm's and hidkeyboard's Update inline the timer list's erase and Unlock
# (2026-10-31).
set_source_files_properties(src/inputdevice/motionsensor/motionsensor.cpp src/inputdevice/pad/pad.cpp
	src/inputdevice/touchscreen/touchscreen.cpp src/inputdevice/btrm/btrm.cpp src/inputdevice/hidrm/hidrm.cpp
	src/inputdevice/hidkeyboard/hidkeyboard.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# Module inlines map's operator[] and the interface table's find (2026-10-31).
set_source_files_properties(src/module/module.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# paf::system::SystemParam's constructor inlines ReadParamFlags (2026-10-31).
set_source_files_properties(src/system/system_param.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# paf::Timer's InitTimer inlines ResetTimer, the custom elastic curves their "in"
# curve but not the parabolic ones theirs (2026-10-31).
set_source_files_properties(src/timer/timer.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=29)

# TextAutoScroll's destructor inlines vector<IDParam>'s and Update inlines Stop; the
# budgets 12 to 24 inline neither, 40 more than Sony (2026-11-06).
set_source_files_properties(src/widget/text_auto_scroll.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# The timer listeners inline list erase, the destructor into D0 and D2, and the list
# constructor and destructor into Init and Exit (2026-11-01).
set_source_files_properties(src/timer/timerlistener.cpp src/timer/timerlistenerlist.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)
set_source_files_properties(src/timer/timerqueue.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=16)
set_source_files_properties(src/timer/get_time.cpp src/url/url.cpp src/rtc/rtc.cpp src/misc/autotest_tty.cpp
	src/framework.cpp src/plugin.cpp src/versatileparam/resourceparam.cpp
	src/versatileparam/versatileparam.cpp src/versatileparam/visualparam.cpp
	src/resource/resource_obj.cpp src/resource/resource_obj_table.cpp
	src/graphics/arl/gxm/gxm_scene_stack.cpp src/graphics/arl/gxm/gxm_surface.cpp
	src/graphics/arl/gxm/gxm_texture.cpp src/graphics/arl/gxm/gxm_texture_gen.cpp
	src/graphics/arl/gxm/gxm_texture_load.cpp src/graphics/arl/gxm/gxm_arl_frontend.cpp
	src/graphics/arl/gxm/gxm_clear.cpp src/graphics/arl/gxm/gxm_framebuffer_copy.cpp
	src/graphics/arl/gxm/gxm_vertex_array.cpp src/graphics/arl/gxm/gxm_vertex_array_spec.cpp
	src/graphics/arl/gxm/gxm_default_textures.cpp src/graphics/arl/gxm/gxm_framebuffer_object.cpp src/graphics/effects/fft.cpp
	src/graphics/drawobj/default_shader.cpp src/graphics/surface/texture_format.cpp
	src/graphics/text/font_rasterizer.cpp
	src/graphics/text/font_rasterizer_pvf.cpp
	src/graphics/drawobj/draw_obj.cpp
	src/graphics/drawobj/bubble_obj.cpp src/graphics/drawobj/page_turn_obj.cpp
	src/graphics/drawobj/panel_obj.cpp
	src/graphics/drawobj/plane_obj.cpp
	src/graphics/drawobj/text_obj.cpp
	src/graphics/graphmem/graphmem.cpp
	src/graphics/gutil.cpp
	src/graphics/surface/surface.cpp src/graphics/surface/surface_base.cpp src/graphics/surface/surface_pool.cpp
	src/graphics/surface/surface_pool_allocator.cpp src/graphics/surface/surface_display.cpp src/graphics/surface/surface_texture.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# The shader objects keep vector<uint8_t>'s copy constructor out of line in the binary
# loaders, which 30 inlines (2026-11-01).
set_source_files_properties(src/graphics/arl/gxm/gxm_shader.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=24)

# PropertyList inlines the value map's operator[] and StringElement::GetString but keeps the
# element map's operator[] out of line: budget 24 does since map's const find is defined out of
# the class (16 did before; 8 and 30 do not) (2026-11-01).
set_source_files_properties(src/common/property_list.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=24)
# FormatString and SetFormattedString inline vector<char>'s destructor from budget 12;
# 30 also inlines string_util::mapping's callees, which Sony calls (2026-11-01).
set_source_files_properties(src/common/string_util.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=12)

# The PVF font cache inlines initAllCacheSlots into myPvfCache_new and the slot helpers into
# freeAllCacheSlots, which stays a call: budgets 12 to 30 all do that (2026-11-06).
set_source_files_properties(src/graphics/text/fontcache.c PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=12)

# Sony inlined TextureOps::IsFlipped into the operations of its file and kept it out of line
# for its other callers (2026-11-06)
set_source_files_properties(src/graphics/effects/texture_ops.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# The custom trail's SharedPtr deleter inlines the outline's vector destructor at 12, as Sony's does (2026-11-06).
set_source_files_properties(src/graphics/effects/custom_trail.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=12)
