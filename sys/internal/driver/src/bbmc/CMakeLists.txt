# Partial C implementation of SceBbmc.

# The updater decompresses the modem images with bzip2 1.0.4: its decompressor, with
# Sony's changes as patches (no malloc in the kernel; the compressor left out).
set(bbmc_bzip2 "${PSP2_SRC}/contrib/bzip2-1.0.4")
function(bbmc_prepare_patched upstream patch output)
	configure_file("${upstream}" "${output}" COPYONLY)
	execute_process(COMMAND patch -s "${output}" "${patch}"
		RESULT_VARIABLE result ERROR_VARIABLE error)
	if(NOT result EQUAL 0)
		message(FATAL_ERROR "Bbmc adaptation of ${upstream} failed: ${error}")
	endif()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
		"${upstream}" "${patch}")
endfunction()
# The prepared files sit together, so each includes the prepared bzlib_private.h.
set(bbmc_bzip2_out "${CMAKE_CURRENT_BINARY_DIR}/bzip2")
bbmc_prepare_patched("${bbmc_bzip2}/bzlib_private.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/updater/bzlib_private_sce.patch" "${bbmc_bzip2_out}/bzlib_private.h")
bbmc_prepare_patched("${bbmc_bzip2}/bzlib.c" "${CMAKE_CURRENT_SOURCE_DIR}/updater/bzlib_sce.patch"
	"${bbmc_bzip2_out}/bzlib.c")
configure_file("${bbmc_bzip2}/decompress.c" "${bbmc_bzip2_out}/decompress.c" COPYONLY)
bbmc_prepare_patched("${bbmc_bzip2}/huffman.c" "${CMAKE_CURRENT_SOURCE_DIR}/updater/huffman_sce.patch"
	"${bbmc_bzip2_out}/huffman.c")
set(bbmc_bzip2_sources
	"${bbmc_bzip2_out}/bzlib.c"
	"${bbmc_bzip2_out}/decompress.c"
	"${bbmc_bzip2_out}/huffman.c"
	"${bbmc_bzip2}/crctable.c"
	"${bbmc_bzip2}/randtable.c")
# Sony built them at -O3 (vectorised loops, loops turned into memset calls).
set_source_files_properties(${bbmc_bzip2_sources} PROPERTIES
	COMPILE_OPTIONS "-O3"
	COMPILE_DEFINITIONS BZ_NO_STDIO)

psp2_module(os0_kd_bootimage_bbmc KIND kernel COMPILER gcc48 SOURCES
	message_command.c
	bbmc_gpio.c
	bbmc_memory.c
	bbmc_mailbox.c
	bbmc_timer.c
	bbmc.c
	state_machine/activated.c
	state_machine/com_boot.c
	state_machine/connected.c
	state_machine/initial.c
	state_machine/low_power_mode.c
	state_machine/standby.c
	state_machine/suspend.c
	state_machine/setup.c
	state_machine/test_mode.c
	sequence/proc_swdl.c
	sequence/resume.c
	sequence/start_net_if.c
	sequence/stop_net_if.c
	sequence/suspend.c
	sequence/sw_updater.c
	sequence/update_gps.c
	updater/updater_QMDM6200.c
	updater/updater_seq_QMDM6200.c
	${bbmc_bzip2_sources}
	bbmc_syscon_events.c
	bbmc_operation_mode.c
	bbcmd.c
	netif/netif.c
	bbmc_gps.c
	sw_dload/sw_dload_s_nand.c
	qmi_information.c
	qmi_uim_card_status.c
	qmi_service_parameters.c
	qmi_solution.c
	qmi_modem_status.c
	qmi_custom_messages.c
	qmi_network_operations.c
	qmi_usb.c
	qmi_backend.c
	qmi_state.c
	qmi_instance/qmi_instance.c
	qmi_channel.c
	qmi_signal.c
	qmi_completion.c
	qmi_notify.c
	qmi_service.c
	qmi_packet.c
	qmi_transmit.c
	qmi_receive.c
	qmi_fifo.c
	qmi_prot_main.c
	qmi_uart.c
	uart_framing/lib_qmi_uart.c
	qmi_control.c
	usb_extif.c
	usb_host.c
	usb_device.c
)
target_include_directories(os0_kd_bootimage_bbmc PRIVATE "${bbmc_bzip2}")
