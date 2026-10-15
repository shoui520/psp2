/*
 * The USB controllers, one 0x1000-byte page each. Each host page holds an
 * EHCI register block at its start and an OHCI one at +0x200 (SceUsbd maps
 * the page twice, as SceUsbdEhci and SceUsbdOhci); the device controllers
 * are SceUdcd's. Bus numbers as SceUsbd and SceUdcd count them; addresses
 * from their memblocks and the henkaku wiki's Physical Memory page.
 */
#pragma once

#define USB_HOST0_REG_BASE		0xe40e0000
#define USB_HOST1_REG_BASE		0xe40b0000
#define USB_HOST2_REG_BASE		0xe4020000
#define USB_DEVICE1_REG_BASE		0xe40c0000	/* SceUdcd1 */
#define USB_DEVICE2_REG_BASE		0xe40d0000	/* SceUdcd2 */
#define USB_REG_SIZE			0x1000
