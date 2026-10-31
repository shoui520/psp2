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
	src/file/bufferedfile.c
	src/file/localfile.c
	src/file/localfile_platform.c
	src/file/memfile.c
	src/file/httpfile.c
	src/file/httpfile_date.c
	src/file/httpfile_openarg.c
	src/file/error.c
	src/compress/compress.cpp
	src/datetime/datetime.c
	src/datetime/datetime_abi.S
	src/rtc/rtc.c
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
	src/common/call_list.c
	src/common/main_thread_call_list.c
	src/common/main_thread_call.c
	src/common/char_encoding.cpp
	src/common/string_util.cpp
	src/common/string_fields.c
	src/url/uri.c
	src/common/resource_hooks.c
	src/common/frame_count.c
	src/common/halt_assert.c
	src/common/sequence.c
	src/common/job_call_queue.c
	src/common/job_slots.c
	src/common/path.c
	src/common/property_list.c
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
	src/inputdevice/inputdevice.c
	src/inputdevice/inputlistener.c
	src/inputdevice/gesturelistener.c
	src/inputdevice/motionsensor/motionsensor.c
	src/timer/timer.c
	src/timer/curves.c
	src/std/math.c
	src/common/view.c
	src/plugin.c
	src/cxml/cxml.c
	src/cxml/document.c
	src/cxml/attribute.c
	src/cxml/util.c
	src/cxml/file.c
	src/resource/resource_obj.c
	src/resource/short.c
	src/toplevel/state.c
	src/graphics/state.c
	src/widget/state.c
	src/versatileparam/idparam.c
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
	src/misc/enum_table.c
	src/misc/string_variant.c
	src/misc/lock.c
	src/misc/rect_contains.c
	src/misc/list.c
	src/misc/dispatch.c
	src/misc/vtable_init.c
	src/misc/http.c
	src/misc/io.c
	src/misc/copy_guard.c
	src/misc/search.c
	src/misc/state.c
	src/misc/containment.c
	src/misc/global_vector.c
	src/misc/global_state.c
	src/misc/app_program.c
	src/misc/power2.c
	src/misc/viewport.c
	../../sys/sdk/target/src/libsnc/i32ops.c
	../../sys/sdk/target/src/libsnc/i64ops.c
	../../sys/sdk/target/src/deflt/zlibhdr.c
	../../sys/sdk/target/src/deflt/deflate.c
	../../sys/sdk/target/src/deflt/deflate_common.c
	libpaf.c
)

target_include_directories(vs0_sys_external_libpaf PRIVATE
	"${CMAKE_SOURCE_DIR}/sys/sdk/target/src/deflt"
	"${paf_zlib_INCLUDE}")
set_source_files_properties("../../sys/sdk/target/src/deflt/zlibhdr.c" PROPERTIES
	COMPILE_DEFINITIONS "sceZlibDecompress=ScePafLowlayer_D7DAA151")

# paf::thread's and paf::job's constructors and destructors have full C2/D2 copies where 8 makes
# them call C1/D1, and CallQueue::Enqueue calls string::assign out of line; budgets 29 to 31 do
# both (2026-10-31).
set_source_files_properties(
	src/buffer/buffer.cpp src/cache/cache_base.cpp src/cache/cache_manager.cpp
	src/cache/cache_manager_impl.cpp
	src/job/job.cpp src/math/matrix.cpp src/math/rect.cpp
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
