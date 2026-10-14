/*
 * sce_sdio.ko: SDIO DECI link to the Vita.
 * /confidential/scei/lib/sce_sdio.ko on the CP (fsimage1), a Linux 2.6.23 kernel module.
 * Its .modinfo:
 *   license=GPL
 *   srcversion=E53EA29DE9D0917909CBA9B
 *   alias=sdio:c*v*d*
 *   depends=
 *   vermagic=2.6.23.17-alp_nl-grover preempt mod_unload ARMv6
 *
 * Skeleton. Every body is a placeholder; the functions are the module's text symbols,
 * in address order.
 */

#include "skeleton.h"

STUB(cleanup_module)
STUB_STATIC(sdio_deci_remove)
STUB_STATIC(sdio_deci_close)
STUB_STATIC(sdio_deci_port_destroy)
STUB_STATIC(sdio_deci_probe)
STUB(init_module)
STUB_STATIC(sdio_deci_open)
STUB_STATIC(sdio_deci_irq)
STUB_STATIC(sdio_deci_read)
STUB_STATIC(sdio_deci_write)
STUB_STATIC(sdio_deci_poll)
