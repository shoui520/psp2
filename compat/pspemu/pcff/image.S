/*
 * SceSblPcffBin: builds the ePSP Flash0 firmware image (SceCompatBootFile[])
 * loaded at 0x8B000000 by compat.skprx from the 187 Flash0 files.
 *
 * Layout at 0x8B000000:
 *   0x0000..0x08C3  SceCompatBootFile entries[187] ({ name, buffer, size })
 *   0x08C4..0x08CF  SceCompatBootFile terminator   ({ 0, 0, sizeof(image) })
 *   0x08D0..0x197F  Null-terminated Flash0 path strings, padded to 64 bytes
 *   0x1980..end     64-byte-aligned Flash0 file payloads
 */

#define PCFF_LOAD_ADDR	0x8b000000
#define PCFF_STR_(x)	#x
#define PCFF_STR(x)	PCFF_STR_(x)

#define PCFF_FILES(X) \
	X(000, "/codepage/cptbl.dat", codepage/cptbl.dat) \
	X(001, "/data/cert/CA_LIST.cer", data/cert/CA_LIST.cer) \
	X(002, "/font/gb3s1518.bwfon", font/gb3s1518.bwfon) \
	X(003, "/font/imagefont.bin", font/imagefont.bin) \
	X(004, "/font/jpn0.pgf", font/jpn0.pgf) \
	X(005, "/font/kr0.pgf", font/kr0.pgf) \
	X(006, "/font/ltn0.pgf", font/ltn0.pgf) \
	X(007, "/font/ltn1.pgf", font/ltn1.pgf) \
	X(008, "/font/ltn10.pgf", font/ltn10.pgf) \
	X(009, "/font/ltn11.pgf", font/ltn11.pgf) \
	X(010, "/font/ltn12.pgf", font/ltn12.pgf) \
	X(011, "/font/ltn13.pgf", font/ltn13.pgf) \
	X(012, "/font/ltn14.pgf", font/ltn14.pgf) \
	X(013, "/font/ltn15.pgf", font/ltn15.pgf) \
	X(014, "/font/ltn2.pgf", font/ltn2.pgf) \
	X(015, "/font/ltn3.pgf", font/ltn3.pgf) \
	X(016, "/font/ltn4.pgf", font/ltn4.pgf) \
	X(017, "/font/ltn5.pgf", font/ltn5.pgf) \
	X(018, "/font/ltn6.pgf", font/ltn6.pgf) \
	X(019, "/font/ltn7.pgf", font/ltn7.pgf) \
	X(020, "/font/ltn8.pgf", font/ltn8.pgf) \
	X(021, "/font/ltn9.pgf", font/ltn9.pgf) \
	X(022, "/kd/amctrl.prx", kd/amctrl.prx) \
	X(023, "/kd/audiocodec_260.prx", kd/audiocodec_260.prx) \
	X(024, "/kd/avcodec.prx", kd/avcodec.prx) \
	X(025, "/kd/cert_loader.prx", kd/cert_loader.prx) \
	X(026, "/kd/chkreg.prx", kd/chkreg.prx) \
	X(027, "/kd/chnnlsv.prx", kd/chnnlsv.prx) \
	X(028, "/kd/dmacman.prx", kd/dmacman.prx) \
	X(029, "/kd/exceptionman.prx", kd/exceptionman.prx) \
	X(030, "/kd/exitgame.prx", kd/exitgame.prx) \
	X(031, "/kd/g729.prx", kd/g729.prx) \
	X(032, "/kd/ge.prx", kd/ge.prx) \
	X(033, "/kd/ge_2.prx", kd/ge_2.prx) \
	X(034, "/kd/http_storage.prx", kd/http_storage.prx) \
	X(035, "/kd/ifhandle.prx", kd/ifhandle.prx) \
	X(036, "/kd/impose_kermit.prx", kd/impose_kermit.prx) \
	X(037, "/kd/init.prx", kd/init.prx) \
	X(038, "/kd/interruptman.prx", kd/interruptman.prx) \
	X(039, "/kd/iofilemgr.prx", kd/iofilemgr.prx) \
	X(040, "/kd/iofilemgr_dnas.prx", kd/iofilemgr_dnas.prx) \
	X(041, "/kd/isofs.prx", kd/isofs.prx) \
	X(042, "/kd/kermit.prx", kd/kermit.prx) \
	X(043, "/kd/kermit_audio.prx", kd/kermit_audio.prx) \
	X(044, "/kd/kermit_ctrl.prx", kd/kermit_ctrl.prx) \
	X(045, "/kd/kermit_display.prx", kd/kermit_display.prx) \
	X(046, "/kd/kermit_flashfs.prx", kd/kermit_flashfs.prx) \
	X(047, "/kd/kermit_hpremote.prx", kd/kermit_hpremote.prx) \
	X(048, "/kd/kermit_idstorage.prx", kd/kermit_idstorage.prx) \
	X(049, "/kd/kermit_lowio.prx", kd/kermit_lowio.prx) \
	X(050, "/kd/kermit_me_wrapper.prx", kd/kermit_me_wrapper.prx) \
	X(051, "/kd/kermit_me_wrapper_2.prx", kd/kermit_me_wrapper_2.prx) \
	X(052, "/kd/kermit_msemu.prx", kd/kermit_msemu.prx) \
	X(053, "/kd/kermit_msfs.prx", kd/kermit_msfs.prx) \
	X(054, "/kd/kermit_peripheral.prx", kd/kermit_peripheral.prx) \
	X(055, "/kd/kermit_power.prx", kd/kermit_power.prx) \
	X(056, "/kd/kermit_rtc.prx", kd/kermit_rtc.prx) \
	X(057, "/kd/kermit_usb.prx", kd/kermit_usb.prx) \
	X(058, "/kd/kermit_utility.prx", kd/kermit_utility.prx) \
	X(059, "/kd/kermit_wlan.prx", kd/kermit_wlan.prx) \
	X(060, "/kd/libaac.prx", kd/libaac.prx) \
	X(061, "/kd/libasfparser.prx", kd/libasfparser.prx) \
	X(062, "/kd/libatrac3plus.prx", kd/libatrac3plus.prx) \
	X(063, "/kd/libaudiocodec2.prx", kd/libaudiocodec2.prx) \
	X(064, "/kd/libdnas.prx", kd/libdnas.prx) \
	X(065, "/kd/libdnas_core.prx", kd/libdnas_core.prx) \
	X(066, "/kd/libgameupdate.prx", kd/libgameupdate.prx) \
	X(067, "/kd/libhttp.prx", kd/libhttp.prx) \
	X(068, "/kd/libmp3.prx", kd/libmp3.prx) \
	X(069, "/kd/libmp4.prx", kd/libmp4.prx) \
	X(070, "/kd/libparse_http.prx", kd/libparse_http.prx) \
	X(071, "/kd/libparse_uri.prx", kd/libparse_uri.prx) \
	X(072, "/kd/libssl.prx", kd/libssl.prx) \
	X(073, "/kd/libupdown.prx", kd/libupdown.prx) \
	X(074, "/kd/loadcore.prx", kd/loadcore.prx) \
	X(075, "/kd/loadexec_01g.prx", kd/loadexec_01g.prx) \
	X(076, "/kd/mcctrl.prx", kd/mcctrl.prx) \
	X(077, "/kd/mediasync.prx", kd/mediasync.prx) \
	X(078, "/kd/memab.prx", kd/memab.prx) \
	X(079, "/kd/memlmd_01g.prx", kd/memlmd_01g.prx) \
	X(080, "/kd/mesg_led_01g.prx", kd/mesg_led_01g.prx) \
	X(081, "/kd/mgvideo.prx", kd/mgvideo.prx) \
	X(082, "/kd/modulemgr.prx", kd/modulemgr.prx) \
	X(083, "/kd/mp4msv.prx", kd/mp4msv.prx) \
	X(084, "/kd/mpeg.prx", kd/mpeg.prx) \
	X(085, "/kd/mpegbase_260.prx", kd/mpegbase_260.prx) \
	X(086, "/kd/np.prx", kd/np.prx) \
	X(087, "/kd/np_auth.prx", kd/np_auth.prx) \
	X(088, "/kd/np_commerce2.prx", kd/np_commerce2.prx) \
	X(089, "/kd/np_commerce2_regcam.prx", kd/np_commerce2_regcam.prx) \
	X(090, "/kd/np_commerce2_store.prx", kd/np_commerce2_store.prx) \
	X(091, "/kd/np_core.prx", kd/np_core.prx) \
	X(092, "/kd/np_inst.prx", kd/np_inst.prx) \
	X(093, "/kd/np_matching2.prx", kd/np_matching2.prx) \
	X(094, "/kd/np_service.prx", kd/np_service.prx) \
	X(095, "/kd/np9660.prx", kd/np9660.prx) \
	X(096, "/kd/npdrm.prx", kd/npdrm.prx) \
	X(097, "/kd/openpsid.prx", kd/openpsid.prx) \
	X(098, "/kd/pops_01g.prx", kd/pops_01g.prx) \
	X(099, "/kd/popsman.prx", kd/popsman.prx) \
	X(100, "/kd/psheet.prx", kd/psheet.prx) \
	X(101, "pspbtcnf.bin", pspbtcnf.bin) \
	X(102, "/kd/pspnet.prx", kd/pspnet.prx) \
	X(103, "/kd/pspnet_adhoc.prx", kd/pspnet_adhoc.prx) \
	X(104, "/kd/pspnet_adhoc_auth.prx", kd/pspnet_adhoc_auth.prx) \
	X(105, "/kd/pspnet_adhoc_discover.prx", kd/pspnet_adhoc_discover.prx) \
	X(106, "/kd/pspnet_adhoc_download.prx", kd/pspnet_adhoc_download.prx) \
	X(107, "/kd/pspnet_adhoc_matching.prx", kd/pspnet_adhoc_matching.prx) \
	X(108, "/kd/pspnet_adhoc_transfer_int.prx", kd/pspnet_adhoc_transfer_int.prx) \
	X(109, "/kd/pspnet_adhocctl.prx", kd/pspnet_adhocctl.prx) \
	X(110, "/kd/pspnet_apctl.prx", kd/pspnet_apctl.prx) \
	X(111, "/kd/pspnet_inet.prx", kd/pspnet_inet.prx) \
	X(112, "/kd/pspnet_resolver.prx", kd/pspnet_resolver.prx) \
	X(113, "/kd/pspnet_upnp.prx", kd/pspnet_upnp.prx) \
	X(114, "/kd/pspnet_wispr.prx", kd/pspnet_wispr.prx) \
	X(115, "/kd/registry.prx", kd/registry.prx) \
	X(116, "/kd/resource/impose.rsc", kd/resource/impose.rsc) \
	X(117, "/kd/sc_sascore.prx", kd/sc_sascore.prx) \
	X(118, "/kd/sircs.prx", kd/sircs.prx) \
	X(119, "/kd/sysmem.prx", kd/sysmem.prx) \
	X(120, "/kd/systimer.prx", kd/systimer.prx) \
	X(121, "/kd/threadman.prx", kd/threadman.prx) \
	X(122, "/kd/usbacc.prx", kd/usbacc.prx) \
	X(123, "/kd/usbcam.prx", kd/usbcam.prx) \
	X(124, "/kd/usbgps.prx", kd/usbgps.prx) \
	X(125, "/kd/usbmic.prx", kd/usbmic.prx) \
	X(126, "/kd/usbpspcm.prx", kd/usbpspcm.prx) \
	X(127, "/kd/usersystemlib.prx", kd/usersystemlib.prx) \
	X(128, "/kd/vaudio.prx", kd/vaudio.prx) \
	X(129, "/kd/videocodec_260.prx", kd/videocodec_260.prx) \
	X(130, "/vsh/etc/index_01g.dat", vsh/etc/index_01g.dat) \
	X(131, "/vsh/etc/version.txt", vsh/etc/version.txt) \
	X(132, "/vsh/module/auth_plugin.prx", vsh/module/auth_plugin.prx) \
	X(133, "/vsh/module/auto_connect.prx", vsh/module/auto_connect.prx) \
	X(134, "/vsh/module/common_gui.prx", vsh/module/common_gui.prx) \
	X(135, "/vsh/module/common_util.prx", vsh/module/common_util.prx) \
	X(136, "/vsh/module/dd_helper_utility.prx", vsh/module/dd_helper_utility.prx) \
	X(137, "/vsh/module/dialogmain.prx", vsh/module/dialogmain.prx) \
	X(138, "/vsh/module/dnas_plugin.prx", vsh/module/dnas_plugin.prx) \
	X(139, "/vsh/module/file_parser_base.prx", vsh/module/file_parser_base.prx) \
	X(140, "/vsh/module/game_install_plugin.prx", vsh/module/game_install_plugin.prx) \
	X(141, "/vsh/module/htmlviewer_ui.prx", vsh/module/htmlviewer_ui.prx) \
	X(142, "/vsh/module/htmlviewer_utility.prx", vsh/module/htmlviewer_utility.prx) \
	X(143, "/vsh/module/hvauth_r.prx", vsh/module/hvauth_r.prx) \
	X(144, "/vsh/module/kermit_netconf_dialog.prx", vsh/module/kermit_netconf_dialog.prx) \
	X(145, "/vsh/module/kermit_osk_plugin.prx", vsh/module/kermit_osk_plugin.prx) \
	X(146, "/vsh/module/libfont_hv.prx", vsh/module/libfont_hv.prx) \
	X(147, "/vsh/module/libpspvmc.prx", vsh/module/libpspvmc.prx) \
	X(148, "/vsh/module/libslim.prx", vsh/module/libslim.prx) \
	X(149, "/vsh/module/libwww.prx", vsh/module/libwww.prx) \
	X(150, "/vsh/module/msgdialog_plugin.prx", vsh/module/msgdialog_plugin.prx) \
	X(151, "/vsh/module/netconf_plugin.prx", vsh/module/netconf_plugin.prx) \
	X(152, "/vsh/module/netfront.prx", vsh/module/netfront.prx) \
	X(153, "/vsh/module/netplay_server_plus_utility.prx", vsh/module/netplay_server_plus_utility.prx) \
	X(154, "/vsh/module/netplay_server_utility.prx", vsh/module/netplay_server_utility.prx) \
	X(155, "/vsh/module/netplay_server2_utility.prx", vsh/module/netplay_server2_utility.prx) \
	X(156, "/vsh/module/npinstaller_plugin.prx", vsh/module/npinstaller_plugin.prx) \
	X(157, "/vsh/module/npsignin_plugin.prx", vsh/module/npsignin_plugin.prx) \
	X(158, "/vsh/module/npsignup_plugin.prx", vsh/module/npsignup_plugin.prx) \
	X(159, "/vsh/module/pafmini.prx", vsh/module/pafmini.prx) \
	X(160, "/vsh/module/ps3scan_plugin.prx", vsh/module/ps3scan_plugin.prx) \
	X(161, "/vsh/module/psn_utility.prx", vsh/module/psn_utility.prx) \
	X(162, "/vsh/module/savedata_auto_dialog.prx", vsh/module/savedata_auto_dialog.prx) \
	X(163, "/vsh/module/savedata_utility.prx", vsh/module/savedata_utility.prx) \
	X(164, "/vsh/module/screenshot_plugin.prx", vsh/module/screenshot_plugin.prx) \
	X(165, "/vsh/module/store_browser_plugin.prx", vsh/module/store_browser_plugin.prx) \
	X(166, "/vsh/module/store_checkout_plugin.prx", vsh/module/store_checkout_plugin.prx) \
	X(167, "/vsh/module/store_checkout_utility.prx", vsh/module/store_checkout_utility.prx) \
	X(168, "/vsh/resource/auth_plugin.rco", vsh/resource/auth_plugin.rco) \
	X(169, "/vsh/resource/dd_helper.rco", vsh/resource/dd_helper.rco) \
	X(170, "/vsh/resource/dnas_plugin.rco", vsh/resource/dnas_plugin.rco) \
	X(171, "/vsh/resource/game_install_plugin.rco", vsh/resource/game_install_plugin.rco) \
	X(172, "/vsh/resource/htmlviewer.res", vsh/resource/htmlviewer.res) \
	X(173, "/vsh/resource/msgdialog_plugin.rco", vsh/resource/msgdialog_plugin.rco) \
	X(174, "/vsh/resource/netconf_dialog.rco", vsh/resource/netconf_dialog.rco) \
	X(175, "/vsh/resource/netplay_plugin.rco", vsh/resource/netplay_plugin.rco) \
	X(176, "/vsh/resource/npinstaller_plugin.rco", vsh/resource/npinstaller_plugin.rco) \
	X(177, "/vsh/resource/npsignin_plugin.rco", vsh/resource/npsignin_plugin.rco) \
	X(178, "/vsh/resource/npsignup_plugin.rco", vsh/resource/npsignup_plugin.rco) \
	X(179, "/vsh/resource/ps3scan_plugin.rco", vsh/resource/ps3scan_plugin.rco) \
	X(180, "/vsh/resource/psn_plugin.rco", vsh/resource/psn_plugin.rco) \
	X(181, "/vsh/resource/savedata_utility.rco", vsh/resource/savedata_utility.rco) \
	X(182, "/vsh/resource/screenshot_plugin.rco", vsh/resource/screenshot_plugin.rco) \
	X(183, "/vsh/resource/store_browser_plugin.rco", vsh/resource/store_browser_plugin.rco) \
	X(184, "/vsh/resource/store_checkout_plugin.rco", vsh/resource/store_checkout_plugin.rco) \
	X(185, "/vsh/resource/system_plugin.rco", vsh/resource/system_plugin.rco) \
	X(186, "/vsh/resource/system_plugin_fg.rco", vsh/resource/system_plugin_fg.rco)

#define PCFF_EMIT_ENTRY(id, path, rel) \
	.word	PCFF_LOAD_ADDR + (.Lpath_##id - image); \
	.word	PCFF_LOAD_ADDR + (.Ldata_##id - image); \
	.word	.Ldata_end_##id - .Ldata_##id;

#define PCFF_EMIT_PATH(id, path, rel) \
.Lpath_##id: \
	.asciz	path;

#define PCFF_EMIT_DATA(id, path, rel) \
	.balign	64, 0; \
.Ldata_##id: \
	.incbin	PCFF_STR(PCFF_RAW_DIR/rel); \
.Ldata_end_##id:

	.data
	.balign	16

	.global	image
	.type	image, %object
image:
	PCFF_FILES(PCFF_EMIT_ENTRY)
	.word	0
	.word	0
	.word	.Limage_end - image

	PCFF_FILES(PCFF_EMIT_PATH)
	.balign	64, 0

	PCFF_FILES(PCFF_EMIT_DATA)
	.balign	64, 0
.Limage_end:
	.size	image, . - image
