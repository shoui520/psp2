psp2_module(os0_kd_bootimage_av_config KIND kernel SCE_LAYOUT SOURCES
	av_config.c
	avconf_helpers.c
	avconf_bt_state.c
	avconf_hdmi.c
	avconf_bt_transfer.c
)
target_link_libraries(os0_kd_bootimage_av_config PRIVATE gcc)
