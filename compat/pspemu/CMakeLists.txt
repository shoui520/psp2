# vs0:app/NPXS10028/eboot.bin (ScePspemu): Vita-side PSP & PocketStation emulator
# Built by Sony with GCC 4.8.3 (Thumb-2 + NEON).
include("${PSP2_SRC}/vsh/paf/src/misc/zlib/zlib.cmake")
psp2_paf_zlib(pspemu_zlib)
include("${PSP2_SRC}/toolchains/libgcc/lib1funcs.cmake")

set(pspemu_ces "${PSP2_SRC}/sys/internal/usermode/src/ces/libces/src")
set(pspemu_ces_SOURCES
	${pspemu_ces}/ces_context.c
	${pspemu_ces}/UCS/ces_ucs_context.c
	${pspemu_ces}/UCS/ces_ucs_callback.c
	${pspemu_ces}/UCS/Str_UTF8_UTF16.c
	${pspemu_ces}/UCS/Str_UTF16_UTF8.c
	${pspemu_ces}/UCS/UTF16_UTF8.c
	${pspemu_ces}/UCS/UTF8.c
	${pspemu_ces}/UCS/UTF16.c
	${pspemu_ces}/MBCS/ces_mbcs_context.c
	${pspemu_ces}/MBCS/ces_mbcs_callback.c
	${pspemu_ces}/MBCS/MbcsCtx_Ucs.c
	${pspemu_ces}/MBCS/MbcsStr_ToUTF16.c
	${pspemu_ces}/MBCS/MbcsStr_FromUTF16.c
	${pspemu_ces}/MBCS/Mbcs_ToUTF16.c
	${pspemu_ces}/MBCS/Mbcs_ToUCS4.c
	${pspemu_ces}/MBCS/Mbcs_FromUCS4.c
	${pspemu_ces}/MBCS/Mbcs_FromUTF32.c
	${pspemu_ces}/SBCS/ces_sbcs_profile.c
	${pspemu_ces}/SBCS/Sbc_UCS.c
	${pspemu_ces}/SBCS/ISO_646/ASCII.c
	${pspemu_ces}/DBCS/DBCS.c
	${pspemu_ces}/JIS/JisProfile_SJis1997_CP932.c
	${pspemu_ces}/JIS/SJis_ToUCS.c
	${pspemu_ces}/JIS/SJis_FromUCS.c
	${pspemu_ces}/JIS/SJis_FromUTF16.c
	${pspemu_ces}/JIS/ShiftJIS.c
	${pspemu_ces}/JIS/JisMenKuTen.c
	${pspemu_ces}/JIS/Jis_FromUCS.c
	${pspemu_ces}/JIS/JIS_X_0201_Katakana.c
	${pspemu_ces}/JIS/jis1990_Ext_FromUcs.c
	${pspemu_ces}/JIS/jis1990_Ext_row85_94.c
	${pspemu_ces}/JIS/jis2004_FromUcs.c
	${pspemu_ces}/JIS/jis2004_UcsCJKExB.c
	${pspemu_ces}/JIS/jis2004_row01_08.c
	${pspemu_ces}/JIS/jis2004_row13.c
	${pspemu_ces}/JIS/jis2004_row16_84.c
)

psp2_module(vs0_app_NPXS10028_eboot KIND user COMPILER gcc48 PARTIAL SOURCES
	crt0.c
	lzrc.c
	main.c
	remotefs.c
	remote_msfs.c
	remote_flashfs.c
	remote_audioout.c
	remote_audioin.c
	remote_me.c
	remote_me_avcdec.c
	remote_me_jpegdec.c
	me_edram.c
	remote_dmacplus.c
	remote_sas2.c
	remote_net.c
	remote_usb.c
	remote_peripheral.c
	remote_osk.c
	menu_dlg.c
	sas2_core.c
	compat_list.c
	settings.c
	pocs_dlg.c
	remote_pocs.c
	pocs_gxm.c
	pocs_hw_regs.c
	pocs_rtc_audio.c
	pocs_assets.c
	pops_spu.c
	pops_host.c
	pops_png_enc.c
	sfo_parser.c
	vmc_manager.c
	vmp_crypto.c
	me_wrapper_api.c
	remote_usbpspcm.c
	scejpeg_decmain.c
	scejpeg_csc.c
	zlib_glue.c
	scejpeg_work.c
	remote_low_priority.c
	iocache.c
	osk_filter.c
	net_config.c
	net_wlan.c
	${PSP2_SRC}/sys/internal/usermode/src/libSceMp4Rec/mp4rec_png.c
	${pspemu_ces_SOURCES}
	${PSP2_SRC}/sys/sdk/target/src/sha1/libsha1.c
	${PSP2_SRC}/sys/internal/kernel/src/sysmem/sha1_transform.S
	${PSP2_SRC}/contrib/zlib-1.2.5/deflate.c
	${PSP2_SRC}/contrib/zlib-1.2.5/trees.c
	${PSP2_SRC}/sys/sdk/target/src/md5/libmd5.c
	${PSP2_SRC}/sys/sdk/target/src/md5/md5.S
	${PSP2_SRC}/sys/sdk/target/src/deflt/crc.S
	${PSP2_SRC}/sys/sdk/target/src/deflt/zlibhdr.c
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate.S
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate2.S
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate_common.c
	${PSP2_SRC}/sys/sdk/target/src/libSceSblSsUpdateMgr_stub/update_mgr_stub.c
	${PSP2_SRC}/sys/sdk/target/src/sha256/libsha256.c
	${PSP2_SRC}/sys/sdk/target/src/sha256/sha256.S
	${PSP2_SRC}/sys/sdk/target/src/aes/libaes.c
	${PSP2_SRC}/sys/sdk/target/src/aes/aes.S
	${PSP2_SRC}/toolchains/libgcc/udiv_weak.S
	${PSP2_SRC}/toolchains/libgcc/idiv_weak.S
	${PSP2_SRC}/toolchains/libgcc/ldivmod.S
	${PSP2_SRC}/toolchains/libgcc/uldivmod.S
	${PSP2_SRC}/toolchains/libgcc/udiv64_gnu.c
)

if(TARGET vs0_app_NPXS10028_eboot)
	gcc48_use_lib1funcs(vs0_app_NPXS10028_eboot)
	target_include_directories(vs0_app_NPXS10028_eboot PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${PSP2_SRC}/vsh/app/shell/src/netctl"
		"${PSP2_SRC}/sys/internal/usermode/src/libSceMp4Rec"
		"${pspemu_ces}"
		"${pspemu_ces}/UCS"
		"${pspemu_ces}/MBCS"
		"${pspemu_ces}/SBCS"
		"${pspemu_ces}/DBCS"
		"${pspemu_ces}/JIS"
		"${PSP2_SRC}/sys/sdk/target/src/deflt"
		"${PSP2_SRC}/toolchains/libgcc/target"
		${pspemu_zlib_INCLUDE})
	target_link_options(vs0_app_NPXS10028_eboot PRIVATE -Wl,--gc-sections)
endif()

set_source_files_properties(
	main.c
	remotefs.c
	remote_msfs.c
	remote_flashfs.c
	remote_audioout.c
	remote_audioin.c
	remote_me.c
	remote_me_avcdec.c
	remote_me_jpegdec.c
	me_edram.c
	remote_dmacplus.c
	remote_sas2.c
	remote_net.c
	remote_usb.c
	remote_peripheral.c
	remote_osk.c
	menu_dlg.c
	sas2_core.c
	compat_list.c
	settings.c
	pocs_dlg.c
	remote_pocs.c
	pocs_gxm.c
	pocs_hw_regs.c
	pocs_rtc_audio.c
	pocs_assets.c
	pops_spu.c
	pops_host.c
	pops_png_enc.c
	sfo_parser.c
	vmc_manager.c
	vmp_crypto.c
	me_wrapper_api.c
	remote_usbpspcm.c
	scejpeg_decmain.c
	scejpeg_csc.c
	zlib_glue.c
	scejpeg_work.c
	remote_low_priority.c
	iocache.c
	osk_filter.c
	PROPERTIES COMPILE_OPTIONS "-fno-toplevel-reorder;-fno-section-anchors;-fno-partial-inlining;-ffunction-sections")

set_source_files_properties(
	${pspemu_ces_SOURCES}
	PROPERTIES COMPILE_OPTIONS "-fno-stack-protector;-fno-toplevel-reorder;-ffunction-sections;-Wno-error=implicit-function-declaration")

set_source_files_properties(
	crt0.c
	lzrc.c
	net_config.c
	net_wlan.c
	${PSP2_SRC}/sys/internal/usermode/src/libSceMp4Rec/mp4rec_png.c
	${PSP2_SRC}/sys/sdk/target/src/sha1/libsha1.c
	${PSP2_SRC}/sys/sdk/target/src/md5/libmd5.c
	${PSP2_SRC}/sys/sdk/target/src/sha256/libsha256.c
	${PSP2_SRC}/sys/sdk/target/src/aes/libaes.c
	${PSP2_SRC}/sys/sdk/target/src/libSceSblSsUpdateMgr_stub/update_mgr_stub.c
	PROPERTIES COMPILE_OPTIONS "-fno-stack-protector;-fno-toplevel-reorder;-ffunction-sections")

set_source_files_properties(
	${PSP2_SRC}/contrib/zlib-1.2.5/deflate.c
	${PSP2_SRC}/contrib/zlib-1.2.5/trees.c
	PROPERTIES COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")

set_source_files_properties(
	${PSP2_SRC}/toolchains/libgcc/udiv64_gnu.c
	PROPERTIES
	COMPILE_DEFINITIONS "IN_LIBGCC2"
	COMPILE_OPTIONS "-fno-stack-protector;-fbuilding-libgcc"
	INCLUDE_DIRECTORIES "${PSP2_SRC}/toolchains/libgcc;${PSP2_SRC}/toolchains/libgcc/target;${PSP2_SRC}/contrib/gcc-4.8-2014.05/gcc;${PSP2_SRC}/contrib/gcc-4.8-2014.05/gcc/ginclude;${PSP2_SRC}/contrib/gcc-4.8-2014.05/libgcc;${PSP2_SRC}/contrib/gcc-4.8-2014.05/include")

set_source_files_properties(
	${PSP2_SRC}/sys/sdk/target/src/deflt/zlibhdr.c
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate_common.c
	PROPERTIES COMPILE_OPTIONS "-Os;-fno-stack-protector;-foptimize-sibling-calls;-fdelete-null-pointer-checks;-fsection-anchors;-fshrink-wrap;-funwind-tables;-ffunction-sections;-fdata-sections")

set_source_files_properties(
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate.S
	${PSP2_SRC}/sys/sdk/target/src/deflt/deflate2.S
	PROPERTIES COMPILE_DEFINITIONS DEFLATE_MEMCPY=sceClibMemcpy_safe)
