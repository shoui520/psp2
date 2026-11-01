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
# zlib.h for paf::compress, configured as the kernel modules' zlib 1.2.5 is.
include("${PSP2_SRC}/sys/internal/kernel/src/post_ss_mgr/zlib/zlib.cmake")
psp2_zlib_sce(paf_zlib)
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
	src/url/uri.c
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
	src/graphics/arl/gxm/gxm_scene_stack.cpp
	src/graphics/arl/gxm/gxm_surface.cpp
	src/graphics/arl/gxm/gxm_texture.cpp
	src/toplevel/state.c
	src/graphics/state.c
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
	src/misc/state.c
	src/misc/containment.c
	src/misc/global_vector.c
	src/misc/global_state.c
	../../sys/sdk/target/src/libsnc/i32ops.c
	../../sys/sdk/target/src/libsnc/i64ops.c
	../../sys/sdk/target/src/deflt/zlibhdr.c
	../../sys/sdk/target/src/deflt/deflate.c
	../../sys/sdk/target/src/deflt/deflate_common.c
	libpaf.c
)

target_include_directories(vs0_sys_external_libpaf PRIVATE
	"${CMAKE_SOURCE_DIR}/sys/sdk/target/src/deflt"
	"${paf_zlib_INCLUDE}"
	"${CMAKE_SOURCE_DIR}/contrib/squish-1.11")

# paf::thread's and paf::job's constructors and destructors have full C2/D2 copies where 8 makes
# them call C1/D1, and CallQueue::Enqueue calls string::assign out of line; budgets 29 to 31 do
# both (2026-10-31).
set_source_files_properties(
	src/buffer/buffer.cpp src/cache/cache_base.cpp src/cache/cache_manager.cpp
	src/cache/cache_manager_impl.cpp
	src/dir/dir.cpp src/file/bufferedfile.cpp src/job/job.cpp src/math/matrix.cpp src/math/rect.cpp
	src/memory/heap_allocator.cpp src/memory/range_allocator.cpp src/memory/pool_allocator.cpp
	src/thread/callqueue.cpp src/thread/cond.cpp src/thread/mutex.cpp src/thread/priority.cpp
	src/thread/rmutex.cpp src/thread/rwlock.cpp src/thread/semaphore.cpp src/thread/sleep.cpp
	src/thread/synccall.cpp src/thread/thread.cpp src/thread/thread_id_cache.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# Sony's dlmalloc inlined init_mparams, init_top, segment_holding and internal_mallinfo but not
# init_user_mstate: budgets 21 to 24 do that, 20 or less keeps internal_mallinfo out of line
# (2026-10-31).
set_source_files_properties("${paf_dlmalloc_source}" PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=21)

# CacheImage's destructor stays out of line in its subclass's and its own deleting destructors,
# while IsResourcePath is inlined: budget 16 does both, 12 and 20 do not (2026-10-31).
set_source_files_properties(src/cache/cache_image.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=16)

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

# motionsensor's, pad's, touchscreen's and btrm's Update inline the timer list's erase and Unlock
# (2026-10-31).
set_source_files_properties(src/inputdevice/motionsensor/motionsensor.cpp src/inputdevice/pad/pad.cpp
	src/inputdevice/touchscreen/touchscreen.cpp src/inputdevice/btrm/btrm.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# Module inlines map's operator[] and the interface table's find (2026-10-31).
set_source_files_properties(src/module/module.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# paf::system::SystemParam's constructor inlines ReadParamFlags (2026-10-31).
set_source_files_properties(src/system/system_param.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# paf::Timer's InitTimer inlines ResetTimer, the custom elastic curves their "in"
# curve but not the parabolic ones theirs (2026-10-31).
set_source_files_properties(src/timer/timer.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=29)

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
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)

# PropertyList inlines the value map's operator[] and StringElement::GetString but keeps the
# element map's operator[] out of line: budget 24 does since map's const find is defined out of
# the class (16 did before; 8 and 30 do not) (2026-11-01).
set_source_files_properties(src/common/property_list.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=24)
# FormatString and SetFormattedString inline vector<char>'s destructor from budget 12;
# 30 also inlines string_util::mapping's callees, which Sony calls (2026-11-01).
set_source_files_properties(src/common/string_util.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=12)
