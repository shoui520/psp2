# os0:kd/crashdump.skprx - core file from the crash record after a crash reboot
# The firmware version written into the 0x1000 note is the one the module was built for
# (the CEX build ships the 3.650.011 binary).
set(CRASHDUMP_FW_VERSION_4.00-DEBUGGER          0x04000000)
set(CRASHDUMP_FW_VERSION_3.650.011-TOOL     0x03650011)
set(CRASHDUMP_FW_VERSION_3.65-CEX           0x03650011)
set(CRASHDUMP_FW_VERSION_3.630.011-DEBUGGER 0x03630011)
fw_value(crashdump_fw_version CRASHDUMP_FW_VERSION)

psp2_module(os0_kd_crashdump KIND kernel SOURCES
	crashdump.c
)
target_compile_definitions(os0_kd_crashdump PRIVATE CRASHDUMP_FW_VERSION=${crashdump_fw_version})
