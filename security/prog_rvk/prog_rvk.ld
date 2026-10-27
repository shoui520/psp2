/*
 * prog_rvk.ld: linker script for the PS Vita Program Revocation List
 * (slb2:prog_rvk.srvk), loaded by secure_kernel into SRAM at 0x809000.
 */

OUTPUT_FORMAT("elf32-mep-little")
OUTPUT_ARCH(mep)

SECTIONS
{
	. = 0x809000;
	.srvk_hdr 0x809000 : {
		KEEP(*(.srvk_hdr))
	}
	.srvk_body 0x809020 : {
		KEEP(*(.srvk_body))
	}
	/DISCARD/ : {
		*(.comment)
		*(.note*)
	}
}
