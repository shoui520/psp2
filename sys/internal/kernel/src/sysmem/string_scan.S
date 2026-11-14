/*
 * Sony's hand-written string scans in the order the kernel links them (the
 * file name is ours); each scan is its own file, as the C library links
 * them in another order.
 */
#include "strlen.S"

/* the boot loader (SYSMEM_NSKBL) keeps only the length scans */
#if !SYSMEM_NSKBL
#include "memchr.S"
#include "strchr.S"
#include "strrchr.S"
#endif
