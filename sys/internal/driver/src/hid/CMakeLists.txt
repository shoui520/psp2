psp2_module(os0_kd_bootimage_hid KIND kernel COMPILER gcc48 SOURCES
	hid_tables.c
	hid_main.c
	hid_memory.c
	hid_bluetooth.c
	hid_device_diagnostics.c
	hid_reports.c
	hid_api.c
	hid_controller.c
	hid_process.c
	hid_workingset.c
	hid_callbacks.c
)
