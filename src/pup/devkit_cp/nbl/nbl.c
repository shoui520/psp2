/*
 * NBL: the CP's second-stage boot loader (stage 2 of nbl.nui, 0x5532c bytes), NBL 0.9.7.5.
 * Hardware setup, page tables, eMMC/FAT, boot config and boot profiles, the command
 * shell (nblargs, nblconfig, update/exec_nup); loads vmlinux.trf and the fsimages.
 * Built on NetBSD's libsa and zlib's inflate 1.1.4, which are vendored, not reversed.
 *
 * Skeleton. Every body is a placeholder.
 */

#include "skeleton.h"

STUB(main)
