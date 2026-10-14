# os0:kd/hdmi - SceHdmi (ADV7533-class HDMI transmitter, CEC).
# Sony built it at -O3 (the InfoFrame checksums are vectorized), but inlined
# only functions declared inline and static functions called once: small
# global accessors stay calls. No IPA-CP clones either: hdmi_device_sm.c
# calls hdmiDeviceSmClearEvent with constants.
set(MODULE_COMPILE_OPTIONS -O3 -fno-inline-functions -fno-inline-small-functions -fno-ipa-cp-clone)
psp2_module(os0_kd_hdmi KIND kernel SOURCES
	source/hdmi.c
	source/hdmi_vic.c
	source/hdmi_mode.c
	source/event_handle_manager.c
	source/infoframe.c
	source/edid_manager.c
	source/edid_info.c
	source/edid_copy.c
	source/control_hdmi_mode.c
	source/cec.c
	source/device/hdmi_device.c
	source/device/hdmi_device_i2c.c
	source/device/adi/hdmi_ad80244.c
	source/device/hdmi_device_ddc.c
	source/device/hdmi_device_edid.c
	source/device/hdmi_device_packet.c
	source/device/hdmi_device_sm.c
	source/device/hdmi_device_status.c
	source/device/hdmi_device_cec.c
)
