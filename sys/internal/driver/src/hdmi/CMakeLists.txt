# os0:kd/hdmi - SceHdmi (ADV7533-class HDMI transmitter, CEC).
# Sony built it at -O3 (the InfoFrame checksums are vectorized), but inlined
# only functions declared inline and static functions called once: small
# global accessors stay calls. No IPA-CP clones either: hdmi_device_sm.c
# calls hdmiDeviceSmClearEvent with constants.
set(MODULE_COMPILE_OPTIONS -O3 -fno-inline-functions -fno-inline-small-functions -fno-ipa-cp-clone)
psp2_module(os0_kd_hdmi KIND kernel COMPILER gcc48 SOURCES
	hdmi.c
	hdmi_vic.c
	hdmi_mode.c
	event_handle_manager.c
	infoframe.c
	edid_manager.c
	edid_info.c
	edid_copy.c
	control_hdmi_mode.c
	cec.c
	device/hdmi_device.c
	device/hdmi_device_i2c.c
	device/adi/hdmi_ad80244.c
	device/hdmi_device_ddc.c
	device/hdmi_device_edid.c
	device/hdmi_device_packet.c
	device/hdmi_device_sm.c
	device/hdmi_device_status.c
	device/hdmi_device_cec.c
)
