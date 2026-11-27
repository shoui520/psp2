/*
 * PlayStation 1 Boot Shell / PAL Overlay linker script.
 * Linked at 0x80030000 and padded to exact 0x16120 bytes (90,400 bytes)
 * to match the POPS shell overlay slot at g_psxBiosRom[0x18000] / g_psxBiosPatch.
 */
OUTPUT_ARCH(mips)
ENTRY(shellStart)

SECTIONS
{
    . = 0x80030000;

    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
    }

    .rodata : ALIGN(4) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4) {
        *(.data .data.* .sdata .sdata.*)
        ASSERT(. <= 0x8004611C, "PS1 shell exceeds 0x16120 byte overlay slot");
    }

    . = 0x8004611C;
    .rodata.shellend : {
        LONG(0x00000000)
    }

    .bss 0x80050000 (NOLOAD) : {
        *(.bss .bss.* .sbss .sbss.* COMMON)
    }

    /DISCARD/ : {
        *(.MIPS.abiflags)
        *(.reginfo)
        *(.pdr)
        *(.comment)
        *(.gnu.attributes)
        *(.eh_frame)
    }
}
