SECTIONS
{
	.vitalink.fstubs.SceSysmemForDriver 0x810045e8 :
	{ KEEP(*(.vitalink.fstubs.SceSysmemForDriver)) }
	.vitalink.fstubs.SceSysclibForDriver 0x81004618 :
	{ KEEP(*(.vitalink.fstubs.SceSysclibForDriver)) }
	.vitalink.fstubs.SceProcEventForDriver 0x81004678 :
	{ KEEP(*(.vitalink.fstubs.SceProcEventForDriver)) }
	.vitalink.fstubs.SceSblACMgrForDriver 0x81004688 :
	{ KEEP(*(.vitalink.fstubs.SceSblACMgrForDriver)) }
	.vitalink.fstubs.SceKernelUtilsForDriver 0x81004698 :
	{ KEEP(*(.vitalink.fstubs.SceKernelUtilsForDriver)) }
	.vitalink.fstubs.SceThreadmgrForDriver 0x81004718 :
	{ KEEP(*(.vitalink.fstubs.SceThreadmgrForDriver)) }
	.vitalink.fstubs.SceSblSsMgrForDriver 0x81004758 :
	{ KEEP(*(.vitalink.fstubs.SceSblSsMgrForDriver)) }
	.vitalink.fstubs.SceDebugForDriver 0x81004788 :
	{ KEEP(*(.vitalink.fstubs.SceDebugForDriver)) }
	.vitalink.fstubs.SceRtcForDriver 0x810047b8 :
	{ KEEP(*(.vitalink.fstubs.SceRtcForDriver)) }
	.text.krmTail 0x810047c8 :
	{ *(.krmTailInput) }
}
INSERT AFTER .text;
