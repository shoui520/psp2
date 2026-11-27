/*
 * PlayStation 1 512 KB System ROM memory map (0xBFC00000..0xBFC7FFFF).
 * Preserves exact PS1 hardware/BIOS contract offsets:
 *   0xBFC00000 : R3000 reset vector
 *   0xBFC00108 : ROM identification strings
 *   0xBFC18000 : Boot shell image (0x16120 bytes; overwritten by PAL patch for PAL titles)
 *   0xBFC64000 : 16x16 ROM font glyphs (Krom2RawAdd)
 *   0xBFC7FF32 : System ROM Version banner
 */
OUTPUT_ARCH(mips)
ENTRY(_start)

SECTIONS
{
    . = 0xBFC00000;

    .text.boot : {
        KEEP(*(.text.boot))
    }

    . = 0xBFC00108;
    .rodata.romid : {
        KEEP(*(.rodata.romid))
    }

    . = ALIGN(16);
    .text : {
        *(.text .text.*)
    }

    .rodata : ALIGN(4) {
        *(.rodata .rodata.*)
        ASSERT(. <= 0xBFC18000, "PS1 BIOS kernel code/rodata overruns 0xBFC18000 shell slot");
    }

    . = 0xBFC18000;
    .psx_shell : {
        KEEP(*(.psx_shell))
        ASSERT(. <= 0xBFC64000, "PS1 BIOS shell overruns 0xBFC64000 font region");
    }

    . = 0xBFC64000;
    .psx_romfont : {
        KEEP(*(.psx_romfont))
    }

    . = 0xBFC7FF32;
    .psx_romver : SUBALIGN(1) {
        KEEP(*(.psx_romver))
    }

    /* Pad full ROM image to exactly 512 KB (0x80000 bytes) */
    . = 0xBFC7FFFC;
    .psx_romend : {
        LONG(0x00000000)
    }

    .bss 0x8000F800 (NOLOAD) : {
        *(.data .data.* .sdata .sdata.*)
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
