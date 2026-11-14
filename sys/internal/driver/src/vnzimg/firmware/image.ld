/*
 * The Venezia image, run at 0x800000 and loaded at 0x40800000:
 *
 *   +0x0      the boot vectors
 *   +0x200    the kernel ("VK0"): its header, the configuration at +0x300,
 *             the vectors at +0x500 and the local RAM code (run at
 *             VNZ_LOCAL_RAM + 0x1000), its code and its state
 *   +0xc000   the drivers ("VK2"), 4 KB apart: unmanaged memory, the
 *             exception hooks, the exception report
 *   +0x10800  the exception hooks' frames (VNZ_KERNEL_MEMORY_EXCEPTION)
 *   +0x11000  the application ("VK1"), with the C library and libgcc
 *
 * Every part's uninitialised data is in the image, as zeros: nothing clears
 * memory before the cores start.
 */
OUTPUT_ARCH(mep)
ENTRY(vnzKernelReset)

SECTIONS
{
	.boot 0x800000 : { KEEP(*(.boot)) }

	.kernel.module 0x800200 : { KEEP(*(.kernel.module)) }
	.kernel.configuration 0x800300 : {
		KEEP(*(.kernel.configuration.header))
		KEEP(*(.kernel.configuration))
	}
	.vectors 0x800500 : { KEEP(*(.vectors)) }
	vnzKernelLocalCodeLoad = .;
	.localcode 0x4e1000 : AT(vnzKernelLocalCodeLoad) {
		*(.localcode)
		*(.localdata)
		. = ALIGN(4);
	}
	vnzKernelLocalCodeSize = SIZEOF(.localcode);
	.kernel vnzKernelLocalCodeLoad + SIZEOF(.localcode) : AT(vnzKernelLocalCodeLoad + SIZEOF(.localcode)) {
		*/kernel/*.o(.text .rodata .rodata.* .data)
		*(.kernel.shared)
		*(.kernel.local)
		*/kernel/*.o(.bss COMMON)
		. = ALIGN(4);
	}
	vnzKernelKernelSize = . - 0x800200;
	ASSERT(. <= 0x80c000, "the kernel runs into the drivers")

	.unmanaged 0x80c000 : AT(0x80c000) {
		KEEP(*(.unmanaged.module))
		. = 0x100;
		*/drivers/unmanaged.o(.text .rodata .data .bss COMMON)
	}
	vnzKernelUnmanagedSize = SIZEOF(.unmanaged);
	ASSERT(. <= 0x80d000, "the unmanaged memory driver runs into the next")

	.exception 0x80d000 : AT(0x80d000) {
		KEEP(*(.exception.module))
		. = 0x100;
		*(.exception.text)
		*/drivers/exception.o(.text .rodata .data .bss COMMON)
	}
	vnzKernelExceptionSize = SIZEOF(.exception);
	ASSERT(. <= 0x80e000, "the exception driver runs into the next")

	.report 0x80e000 : AT(0x80e000) {
		KEEP(*(.report.module))
		. = 0x100;
		*/drivers/report.o(.text .rodata .data .bss COMMON)
	}
	vnzKernelReportSize = SIZEOF(.report);
	ASSERT(. <= 0x810800, "the report driver runs into the exception frames")

	.exception.local 0x810800 : AT(0x810800) {
		*(.exception.local)
		. = 0x800;
	}

	.application 0x811000 : AT(0x811000) {
		KEEP(*(.application.module))
		. = 0x100;
		*(.text .text.*)
		*(.rodata .rodata.*)
		*(.data .data.*)
		*(.bss .bss.* COMMON)
		. = ALIGN(4);
	}
	vnzKernelApplicationSize = SIZEOF(.application);
	ASSERT(. <= 0x957000, "the application runs into the crash record")

	/DISCARD/ : { *(.comment) *(.note.*) }
}
