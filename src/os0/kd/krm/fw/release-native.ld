SECTIONS
{
	.vitalink.fstubs.SceSysmemForDriver 0x81004504 :
	{ KEEP(*(.vitalink.fstubs.SceSysmemForDriver)) }
	.vitalink.fstubs.SceSysclibForDriver 0x81004534 :
	{ KEEP(*(.vitalink.fstubs.SceSysclibForDriver)) }
	.vitalink.fstubs.SceProcEventForDriver 0x81004594 :
	{ KEEP(*(.vitalink.fstubs.SceProcEventForDriver)) }
	.vitalink.fstubs.SceSblACMgrForDriver 0x810045a4 :
	{ KEEP(*(.vitalink.fstubs.SceSblACMgrForDriver)) }
	.vitalink.fstubs.SceKernelUtilsForDriver 0x810045b4 :
	{ KEEP(*(.vitalink.fstubs.SceKernelUtilsForDriver)) }
	.vitalink.fstubs.SceThreadmgrForDriver 0x81004634 :
	{ KEEP(*(.vitalink.fstubs.SceThreadmgrForDriver)) }
	.vitalink.fstubs.SceSblSsMgrForDriver 0x81004674 :
	{ KEEP(*(.vitalink.fstubs.SceSblSsMgrForDriver)) }
	.vitalink.fstubs.SceRtcForDriver 0x810046a4 :
	{ KEEP(*(.vitalink.fstubs.SceRtcForDriver)) }
	.text.krmTail 0x810046b4 :
	{ *(.krmTailInput) }
}
INSERT AFTER .text;
