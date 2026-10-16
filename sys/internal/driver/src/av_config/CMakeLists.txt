psp2_module(os0_kd_bootimage_av_config KIND kernel SCE_LAYOUT SOURCES
	source/av_config.c
	source/avconf_helpers.c
	source/avconf_bt_state.c
	source/avconf_hdmi.c
	source/avconf_bt_transfer.c
)
target_link_libraries(os0_kd_bootimage_av_config PRIVATE gcc)
