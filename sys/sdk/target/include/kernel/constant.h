/*
 * kernel/constant.h: the kernel's constants, under Sony's names (SDK Release 03.570.011,
 * target/include/kernel/constant.h).
 */
#ifndef _SCE_KERNEL_CONSTANT_H
#define _SCE_KERNEL_CONSTANT_H

/* UIDs */
#define SCE_KERNEL_PROCESS_ID_SELF	0
#define SCE_UID_THREAD_ID_PROCESS_ALL	(0x00010027)
#define SCE_UID_NAMELEN			(31)

/* sizes */
#define SCE_KERNEL_0KiB			0x00000000	/* a page of any size */
#define SCE_KERNEL_0MiB			0x00000000
#define SCE_KERNEL_1KiB			0x00000400
#define SCE_KERNEL_2KiB			0x00000800
#define SCE_KERNEL_3KiB			0x00000C00
#define SCE_KERNEL_4KiB			0x00001000	/* small page */
#define SCE_KERNEL_8KiB			0x00002000
#define SCE_KERNEL_16KiB		0x00004000
#define SCE_KERNEL_32KiB		0x00008000
#define SCE_KERNEL_64KiB		0x00010000	/* large page */
#define SCE_KERNEL_128KiB		0x00020000
#define SCE_KERNEL_256KiB		0x00040000
#define SCE_KERNEL_512KiB		0x00080000
#define SCE_KERNEL_1MiB			0x00100000	/* section */
#define SCE_KERNEL_2MiB			0x00200000
#define SCE_KERNEL_4MiB			0x00400000
#define SCE_KERNEL_8MiB			0x00800000
#define SCE_KERNEL_16MiB		0x01000000	/* supersection */
#define SCE_KERNEL_32MiB		0x02000000
#define SCE_KERNEL_64MiB		0x04000000
#define SCE_KERNEL_128MiB		0x08000000
#define SCE_KERNEL_256MiB		0x10000000
#define SCE_KERNEL_512MiB		0x20000000
#define SCE_KERNEL_1GiB			0x40000000
#define SCE_KERNEL_2GiB			0x80000000
#define SCE_KERNEL_3GiB			0xC0000000

#endif /* _SCE_KERNEL_CONSTANT_H */
