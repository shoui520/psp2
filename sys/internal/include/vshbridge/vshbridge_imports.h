/*
 * The kernel services SceVshBridge (os0:kd/bootimage/vshbridge.skprx) bridges
 * to the system applications: their exporters' headers.
 */

#pragma once

#include <psp2kern/types.h>
#include <idstorage/idstorage.h>
#include <sdif/sdif.h>
#include <lowio/pervasive.h>
#include <syscon/syscon.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <appmgr/appmgr_rdrfp.h>
#include <power/led.h>
#include <power/power.h>
#include <rtc/rtc.h>
#include <processmgr/processmgr.h>
#include <modulemgr/modulemgr.h>
#include <iofilemgr/iofilemgr.h>
#include <display/display.h>
#include <acmgr/acmgr.h>

#include <sysmem/pmmgr.h>
#include <sysmem/debug.h>
#include <ctrl/ctrl.h>
#include <msif/msif.h>
#include <touch/touch.h>
#include <motion/motion_dev.h>
#include <udcd/udcd.h>
#include <ss_mgr/ss_mgr.h>
#include <npdrm/npdrm.h>
#include <post_ss_mgr/sblpostssmgr.h>
#include <coredump/coredump.h>
#include <regmgr/regmgr.h>
#include <appmgr/appmgr.h>
#include <hdmi/hdmi.h>
#include <gcauthmgr/sblgcauthmgrmlnpsnl.h>
