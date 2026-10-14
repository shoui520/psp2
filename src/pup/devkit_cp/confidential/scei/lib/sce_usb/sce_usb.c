/*
 * sce_usb.ko: USB DECI4p gadget ("PSP2 Devkit").
 * /confidential/scei/lib/sce_usb.ko on the CP (fsimage1), a Linux 2.6.23 kernel module.
 * Its .modinfo:
 *   license=GPL
 *   description=USB DECI4p Gadget Driver
 *   author=SCEI
 *   parmtype=debug_dontmakepair:int
 *   parmtype=textnotify:int
 *   parmtype=buflen:uint
 *   parmtype=devicename:charp
 *   parmtype=verbose:int
 *   parmtype=deci_major_no:int
 *   parm=autoresume:zero, or seconds before remote wakeup
 *   parmtype=autoresume:uint
 *   parm=iSerialNumber:SerialNumber string
 *   parmtype=iSerialNumber:charp
 *   parm=iProduct:USB Product string
 *   parmtype=iProduct:charp
 *   parm=iManufacturer:USB Manufacturer string
 *   parmtype=iManufacturer:charp
 *   parm=bcdDevice:USB Device version (BCD)
 *   parmtype=bcdDevice:ushort
 *   parm=idProduct:USB Product ID
 *   parmtype=idProduct:ushort
 *   parm=idVendor:USB Vendor ID
 *   parmtype=idVendor:ushort
 *   srcversion=41D4F4BE67BEB27A6438601
 *   depends=
 *   vermagic=2.6.23.17-alp_nl-grover preempt mod_unload ARMv6
 *
 * Skeleton. Every body is a placeholder; the functions are the module's text symbols,
 * in address order.
 */

#include "skeleton.h"

STUB(usb_add_function)
STUB(usb_composite_unregister)
STUB(cleanup_module)
STUB(usb_interface_id)
STUB(usb_string_id)
STUB_STATIC(composite_setup_complete)
STUB_STATIC(composite_resume)
STUB(usb_find_endpoint)
STUB(usb_ep_autoconfig_reset)
STUB(deci_mgr_get_cdev_param)
STUB(usb_add_config)
STUB(disable_endpoints)
STUB(node_usb_start)
STUB(node_usb_stop)
STUB(deci_node_get_local_state_impl)
STUB(deci_node_get_remote_state_i)
STUB(deci_config_add)
STUB(deci_node_list_set_node)
STUB_STATIC(deci_ntf_poll)
STUB_STATIC(deci_ntf_release)
STUB_STATIC(cp_bind)
STUB(deci_ntf_debugprint)
STUB_STATIC(deci_config_bind)
STUB(deci_node_cdev_release)
STUB(deci_node_cdev_init_done)
STUB(node_usb_xfer_impl)
STUB(usb_ep_autoconfig)
STUB_STATIC(_complete_cb)
STUB(deci_node_get_last_wr_err)
STUB(usb_composite_register)
STUB(deci_mpool_decr_msg)
STUB(init_module)
STUB_STATIC(deci_function_bind)
STUB(deci_mpool_num_msgs)
STUB_STATIC(composite_bind)
STUB(deci_node_has_msgs)
STUB(usb_copy_descriptors)
STUB(deci_mpool_incr_msg)
STUB(deci_node_put_wr_msg)
STUB(deci_mpool_get_msg)
STUB(deci_node_put_rd_msg)
STUB(deci_node_get_msg)
STUB_STATIC(_test_event_work2)
STUB(deci_mpool_delete)
STUB(node_usb_delete)
STUB(_deci_node_delete_cmpl)
STUB(deci_ntf_new)
STUB(deci_node_list_new)
STUB(deci_mpool_new)
STUB(node_usb_new)
STUB(node_cdev_event_impl)
STUB(deci_node_cdev_open)
STUB(deci_node_ep_enabled_i)
STUB(deci_node_set_remote_state_i)
STUB(deci_node_start)
STUB_STATIC(node_cdev_func_ev_work2)
STUB_STATIC(deci_function_resume)
STUB(node_cdev_delete)
STUB(deci_node_delete)
STUB(deci_ntf_remove_node)
STUB(deci_node_list_delete)
STUB(deci_ntf_delete)
STUB(deci_node_new)
STUB(node_cdev_new)
STUB(deci_ntf_add_node)
STUB(node_cdev_unassign_region)
STUB_STATIC(deci_function_delete)
STUB(node_cdev_assign_region)
STUB(usb_gadget_get_string)
STUB(usb_descriptor_fillbuf)
STUB(usb_gadget_config_buf)
STUB_STATIC(composite_unbind)
STUB_STATIC(_node_cdev_delete_cmpl)
STUB(usb_function_deactivate)
STUB(usb_function_activate)
STUB_STATIC(deci_function_ev_work)
STUB(deci_node_event_usb_cmpl_i)
STUB_STATIC(deci_ntf_event_deci_node)
STUB_STATIC(node_cdev_release)
STUB_STATIC(deci_ntf_open)
STUB_STATIC(node_cdev_open)
STUB_STATIC(deci_ntf_read)
STUB_STATIC(deci_config_setup)
STUB(alloc_ep_req)
STUB(free_ep_req)
STUB_STATIC(composite_disconnect)
STUB_STATIC(composite_suspend)
STUB_STATIC(composite_setup)
STUB_STATIC(deci_function_unbind)
STUB_STATIC(deci_function_set_alt)
STUB_STATIC(deci_function_disable)
STUB_STATIC(deci_function_suspend)
STUB_STATIC(node_cdev_poll)
STUB_STATIC(node_cdev_read)
STUB_STATIC(node_cdev_write)
