/*
 * SceUsbEtherSmsc: the USB Ethernet driver for the SMSC LAN9500 family
 * (smsc_driver.c, smsc_usb.c), a SceNetPs network driver (net_ps/netdrv.h).
 * Function names marked 0.990 come from the prototype's strings; register
 * names are the LAN9500 datasheet's.
 */
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <sysmem/cpu.h>
#include <sysmem/procevent.h>
#include <sysmem/suspend.h>
#include <sysmem/sysmem.h>
#include <sysmem/sblaimgr.h>
#include <usbd/usbd.h>
#include <usbserv/usbserv.h>
#include <idstorage/idstorage.h>
#include <net/ether.h>
#include <stddef.h>
#include <string.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <net_ps/netdrv.h>
#include <sysmem/sysevent_ids.h>
#include <usb_ether_smsc/usb_ether_smsc.h>

/* names unknown */
#define SMSC_ERROR_INVALID_ARGUMENT	((int)0x80418001)
#define SMSC_ERROR_MII			((int)0x80418002)
#define SMSC_ERROR_TIMEOUT		((int)0x80418003)
#define SMSC_ERROR_NOT_READY		((int)0x80418004)
#define SMSC_ERROR_NO_DEVICE		((int)0x80418007)
#define SMSC_ERROR_NOT_SUPPORTED	((int)0x80418008)

/* the supported devices */
#define SMSC_VENDOR			0x0424
#define SMSC_PRODUCT_LAN9500		0x9500
#define SMSC_PRODUCT_LAN9500A		0x9e00
#define SONY_VENDOR			0x054c
#define SONY_PRODUCT_ETHER		0x04c6

/* vendor requests */
#define SMSC_REQUEST_WRITE_REGISTER	0xa0
#define SMSC_REQUEST_READ_REGISTER	0xa1

/* LAN9500 registers */
#define SMSC_INT_STS		0x008
#define   INT_STS_RX_STOP		0x00010000
#define SMSC_RX_CFG		0x00c
#define   RX_CFG_FIFO_FLUSH		0x00000001
#define SMSC_TX_CFG		0x010
#define   TX_CFG_FIFO_FLUSH		0x00000001
#define   TX_CFG_STOP			0x00000002
#define   TX_CFG_ON			0x00000004
#define SMSC_HW_CFG		0x014
#define   HW_CFG_BCE			0x00000002	/* burst cap enable */
#define   HW_CFG_LRST			0x00000008	/* soft lite reset */
#define   HW_CFG_MEF			0x00000020	/* multiple ethernet frames */
#define   HW_CFG_RXDOFF_MASK		0x00000600
#define   HW_CFG_RXDOFF_2		0x00000400	/* frames start 2 bytes in */
#define   HW_CFG_BIR			0x00001000	/* bulk-in empty response */
#define SMSC_PM_CTRL		0x020
#define   PM_CTRL_PHY_RST		0x00000010
#define SMSC_LED_GPIO_CFG	0x024
#define SMSC_GPIO_CFG		0x028
#define SMSC_AFC_CFG		0x02c
#define SMSC_E2P_CMD		0x030
#define   E2P_CMD_LOADED		0x00000200
#define   E2P_CMD_TIMEOUT		0x00000400
#define   E2P_CMD_BUSY			0x80000000
#define SMSC_E2P_DATA		0x034
#define SMSC_BURST_CAP		0x038
#define SMSC_INT_EP_CTL		0x068
#define SMSC_BULK_IN_DLY	0x06c
#define SMSC_MAC_CR		0x100
#define   MAC_CR_RXEN			0x00000004
#define   MAC_CR_TXEN			0x00000008
#define   MAC_CR_HPFILT			0x00002000	/* hash perfect filter */
#define   MAC_CR_PRMS			0x00040000	/* promiscuous */
#define   MAC_CR_MCPAS			0x00080000	/* pass all multicast */
#define   MAC_CR_FDPX			0x00100000	/* full duplex */
#define   MAC_CR_RCVOWN			0x00800000	/* half duplex: no receive own */
#define SMSC_ADDRH		0x104
#define SMSC_ADDRL		0x108
#define SMSC_HASHH		0x10c
#define SMSC_HASHL		0x110
#define SMSC_MII_ADDR		0x114
#define   MII_BUSY			0x00000001
#define   MII_WRITE			0x00000002
#define   MII_PHY_ADDR			(1 << 11)	/* the internal PHY */
#define   MII_REG(n)			((unsigned int)(n) << 6)
#define SMSC_MII_DATA		0x118
#define SMSC_VLAN1		0x120
#define SMSC_COE_CR		0x130

/* PHY registers (MII) */
#define MII_BMCR		0x00
#define   BMCR_FULLDPLX			0x0100
#define   BMCR_ANRESTART		0x0200
#define   BMCR_PDOWN			0x0800
#define   BMCR_ANENABLE			0x1000
#define   BMCR_SPEED100			0x2000
#define   BMCR_RESET			0x8000
#define MII_BMSR		0x01
#define   BMSR_LSTATUS			0x0004
#define MII_ADVERTISE		0x04
#define MII_LPA			0x05
#define   ADVERTISE_10HALF		0x0020
#define   ADVERTISE_10FULL		0x0040
#define   ADVERTISE_100HALF		0x0080
#define   ADVERTISE_100FULL		0x0100
#define   ADVERTISE_ALL			0x01e1		/* all of them, IEEE 802.3 */
#define PHY_INT_SRC		0x1d
#define PHY_INT_MASK		0x1e
#define   PHY_INT_ANEG_COMPLETE		0x0040
#define   PHY_INT_LINK_DOWN		0x0010

/* the frame headers: TX_CMD_A/B before a frame, the status word after one */
#define TX_CMD_A_LAST_SEG	0x1000
#define TX_CMD_A_FIRST_SEG	0x2000
#define TX_PREFIX		8
#define RX_STS_LENGTH(status)	(((status) >> 16) & 0x3fff)
#define RX_STS_MULTICAST	0x0400
#define RX_STS_BROADCAST	0x2000
#define RX_STS_ERROR		0x8000
#define RX_HEADER		6	/* the status word and the RXDOFF bytes */
#define ETHER_MAX_FRAME		0x5ea	/* 1514 */

/* the link modes this PHY reports (NET_DRIVER_LINK_*) */
#define LINK_UP_10M		(NET_DRIVER_LINK_UP | NET_DRIVER_LINK_10M)
#define LINK_UP_100M		(NET_DRIVER_LINK_UP | NET_DRIVER_LINK_100M)
#define LINK_AUTO_10M		(LINK_UP_10M | NET_DRIVER_LINK_AUTO)
#define LINK_AUTO_100M		(LINK_UP_100M | NET_DRIVER_LINK_AUTO)

/* controlEvent bits */
#define CONTROL_FAILED		0x1
#define CONTROL_DONE		0x2

/* workerEvent bits */
#define WORKER_ATTACHED		0x01
#define WORKER_OPEN		0x02
#define WORKER_CLOSE		0x04
#define WORKER_TX		0x08
#define WORKER_RX		0x10
#define WORKER_DETACH		0x20
#define WORKER_EVENTS		0x3f

/* SceUsbEtherSmscContext.flags */
#define SMSC_FLAG_ATTACHED	0x1
#define SMSC_FLAG_REGISTERED	0x2	/* with SceNetPs */
#define SMSC_FLAG_INITIALIZED	0x4
#define SMSC_FLAG_OPEN		0x8

/* SceUsbEtherSmscContext.state */
#define SMSC_STATE_USB_POWER	0x1	/* open/close go through the worker */

#define SMSC_LINK_POLL		3000000	/* microseconds */

/* proc events: a process takes the system cores, or gives them back */
#define PROC_EVENT_EXCLUSIVE	0x10
#define PROC_EVENT_SHARED	0x11

/* ScePowerForDriver_B24263F0 kind 2 */
#define POWER_EVENT_USB		0x10021
#define POWER_USB_OFF		0x10

struct SceUsbEtherSmscContext;
/* the driver context; the name is Sony's (0.990) */
struct SceUsbEtherSmscContext {
	struct NetDriver driver;
	unsigned int flags;
	int controlPipe;
	unsigned int reserved;
	int rxPipe;
	unsigned int rxMaxPacket;
	int txPipe;
	unsigned int txMaxPacket;
	int controlEvent;
	int device;
	int pendingTx;
	int workerEvent;
	unsigned int bufferSize;
	unsigned char *rxBuffer;
	unsigned char *txBuffer;
	unsigned int macControl;
	SceKernelFastMutex mutex;
	int thread;
	unsigned int link;
	int references;
	int active;
	unsigned int rxCount;
	SceKernelProcEventHandler proc;
	int procId;
	unsigned int state;
};
_Static_assert(sizeof(struct SceUsbEtherSmscContext) == 0x164, "adapter size");
_Static_assert(offsetof(struct SceUsbEtherSmscContext, mutex) == 0xec, "mutex offset");
static struct SceUsbEtherSmscContext s_context;
static unsigned int s_suspend, s_suspending, s_probed;
static SceUsbdDriver s_driver;
static unsigned int s_readValue __attribute__((aligned(64)));
static unsigned int s_writeValue __attribute__((aligned(64)));
static SceUsbdDeviceRequest s_readRequest __attribute__((aligned(64)));
static SceUsbdDeviceRequest s_writeRequest __attribute__((aligned(64)));

static void onControlTransferCompletion(int result, int count, void *arg)
{
	struct SceUsbEtherSmscContext *a = arg;

	(void)count;
	sceKernelSetEventFlag(a->controlEvent, result < 0 ? CONTROL_FAILED : CONTROL_DONE);
}

static int readRegister(struct SceUsbEtherSmscContext *a, unsigned short reg, unsigned int *value)
{
	unsigned int bits;
	int ret;

	s_readRequest.bmRequestType = SCE_USBD_REQTYPE_DIR_TO_HOST | SCE_USBD_REQTYPE_TYPE_VENDOR;
	s_readRequest.bRequest = SMSC_REQUEST_READ_REGISTER;
	s_readRequest.wValue = 0;
	s_readRequest.wIndex = reg;
	s_readRequest.wLength = sizeof(s_readValue);
	s_readValue = 0;
	ret = SceUsbdForDriver_2E05660F(a->controlPipe, &s_readRequest, (unsigned char *)&s_readValue, onControlTransferCompletion, a);
	if (ret >= 0) {
		ret = sceKernelWaitEventFlag(a->controlEvent, CONTROL_FAILED | CONTROL_DONE,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
		if (ret >= 0) {
			if (bits & CONTROL_FAILED)
				ret = SMSC_ERROR_NOT_READY;
			else
				*value = s_readValue;
		}
	}
	return ret;
}

static int writeRegister(struct SceUsbEtherSmscContext *a, unsigned short reg, unsigned int value)
{
	unsigned int bits;
	int ret;

	s_writeRequest.bmRequestType = SCE_USBD_REQTYPE_DIR_TO_DEVICE | SCE_USBD_REQTYPE_TYPE_VENDOR;
	s_writeRequest.bRequest = SMSC_REQUEST_WRITE_REGISTER;
	s_writeRequest.wValue = 0;
	s_writeRequest.wIndex = reg;
	s_writeRequest.wLength = sizeof(s_writeValue);
	s_writeValue = value;
	ret = SceUsbdForDriver_2E05660F(a->controlPipe, &s_writeRequest, (unsigned char *)&s_writeValue, onControlTransferCompletion, a);
	if (ret >= 0) {
		ret = sceKernelWaitEventFlag(a->controlEvent, CONTROL_FAILED | CONTROL_DONE,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
		if (ret >= 0 && (bits & CONTROL_FAILED))
			ret = SMSC_ERROR_NOT_READY;
	}
	return ret;
}

static void usbEtherSmscOnTxTransferCompletion(int result, int count, void *arg)
{
	struct SceUsbEtherSmscContext *a = arg;

	(void)count;
	if (result >= 0)
		sceKernelSetEventFlag(a->workerEvent, WORKER_TX);
}
static void usbEtherSmscOnRxTransferCompletion(int result, int count, void *arg)
{
	struct SceUsbEtherSmscContext *a = arg;

	if (result >= 0) {
		a->rxCount = count;
		sceKernelSetEventFlag(a->workerEvent, WORKER_RX);
	}
}
static int sendBuffer(struct SceUsbEtherSmscContext *a, void *data, unsigned int length)
{
	return SceUsbdForDriver_470D5289(a->txPipe, data, length, usbEtherSmscOnTxTransferCompletion, a);
}
static int receiveBuffer(struct SceUsbEtherSmscContext *a, void *data, unsigned int length)
{
	return SceUsbdForDriver_470D5289(a->rxPipe, data, length, usbEtherSmscOnRxTransferCompletion, a);
}

static int connected(struct SceUsbEtherSmscContext *a)
{
	__atomic_fetch_or(&a->flags, SMSC_FLAG_ATTACHED, __ATOMIC_SEQ_CST);
	return sceKernelSetEventFlag(a->workerEvent, WORKER_ATTACHED);
}
static int disconnected(struct SceUsbEtherSmscContext *a)
{
	__atomic_fetch_and(&a->flags, ~SMSC_FLAG_ATTACHED, __ATOMIC_SEQ_CST);
	sceKernelSetEventFlag(a->workerEvent, WORKER_DETACH);
	a->link = 0;
	return 0;
}
static int detachDevice(int device)
{
	(void)device;
	if (sceKernelSetEventFlag(s_context.controlEvent, CONTROL_FAILED) >= 0)
		return disconnected(&s_context) >> 31;
	return -1;
}
static int probeDevice(int device)
{
	SceUsbdDeviceDescriptor *desc;
	SceUsbdInterfaceDescriptor *iface;
	void *scan;

	desc = SceUsbdForDriver_BC3EF82B(device, NULL, 1);
	if (!desc)
		return -1;
	if (desc->idVendor == SMSC_VENDOR) {
		if (desc->idProduct != SMSC_PRODUCT_LAN9500 &&
		    desc->idProduct != SMSC_PRODUCT_LAN9500A)
			return -1;
	} else if (desc->idVendor != SONY_VENDOR || desc->idProduct != SONY_PRODUCT_ETHER) {
		return -1;
	}
	s_probed = 1;
	scan = desc;
	while ((iface = SceUsbdForDriver_BC3EF82B(device, scan, 4))) {
		if (iface->bInterfaceClass == 0xff)
			return 0;
		scan = iface;
	}
	return -1;
}
static int onAttach(int device)
{
	struct SceUsbEtherSmscContext *a = &s_context;
	SceUsbdInterfaceDescriptor *iface;
	SceUsbdEndpointDescriptor *endpoint;
	SceUsbdDeviceRequest request;
	unsigned int bits;
	unsigned int speed = 0xb01;
	void *scan;
	int ret, i;

	a->controlPipe = a->rxPipe = a->txPipe = -1;
	ret = sceKernelClearEventFlag(a->controlEvent, 0);
	if (ret < 0)
		return ret;
	scan = SceUsbdForDriver_BC3EF82B(device, NULL, 1);
	if (!scan)
		return -1;
	while ((iface = SceUsbdForDriver_BC3EF82B(device, scan, 4))) {
		a->controlPipe = SceUsbdForDriver_1CDBFF9F(device, NULL);
		if (a->controlPipe < 0)
			return -1;
		scan = iface;
		endpoint = (void *)iface;
		for (i = 0; i < iface->bNumEndpoints; i++) {
			endpoint = SceUsbdForDriver_BC3EF82B(device, endpoint, 5);
			if ((endpoint->bEndpointAddress & 0x1f) == 1) {
				a->rxPipe = SceUsbdForDriver_1CDBFF9F(device, endpoint);
				if (a->rxPipe < 0)
					return -1;
				a->rxMaxPacket = endpoint->wMaxPacketSize;
			} else if ((endpoint->bEndpointAddress & 0x1f) == 2) {
				a->txPipe = SceUsbdForDriver_1CDBFF9F(device, endpoint);
				if (a->txPipe < 0)
					return -1;
				a->txMaxPacket = endpoint->wMaxPacketSize;
			}
		}
	}
	if (a->controlPipe < 0 || a->rxPipe < 0 || a->txPipe < 0)
		return -1;
	request.bmRequestType = 0;
	request.bRequest = 9;
	request.wValue = 1;
	request.wIndex = 0;
	request.wLength = 0;
	ret = SceUsbdForDriver_2E05660F(a->controlPipe, &request, NULL, onControlTransferCompletion, a);
	if (ret < 0)
		return ret;
	ret = sceKernelWaitEventFlag(a->controlEvent, CONTROL_FAILED | CONTROL_DONE,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	if (ret < 0)
		return ret;
	request.bmRequestType = 1;
	request.bRequest = 0xb;
	request.wValue = 0;
	ret = SceUsbdForDriver_2E05660F(a->controlPipe, &request, NULL, onControlTransferCompletion, a);
	if (ret < 0)
		return ret;
	ret = sceKernelWaitEventFlag(a->controlEvent, CONTROL_FAILED | CONTROL_DONE,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	if (ret < 0)
		return ret;
	ret = SceUsbdForDriver_C3BECF5F(device, &speed);
	if (ret < 0)
		return ret;
	a->bufferSize = speed == 1 ? 0x1ec0 : 0x4a00;
	a->device = device;
	ret = connected(a);
	return ret < 0 ? ret : 0;
}
static int initializeUsb(struct SceUsbEtherSmscContext *a)
{
	unsigned int hardware;
	int ret, n;

	ret = sceKernelCreateEventFlag("SceSmscControlTransfer", 0, 0, NULL);
	if (ret < 0)
		return ret;
	a->controlEvent = ret;
	s_driver.name = "SceUsbEtherSmsc";
	s_driver.probe = probeDevice;
	s_driver.attach = onAttach;
	s_driver.detach = detachDevice;
	if (SceSysconForDriver_9ADD60D2())
		return SMSC_ERROR_NOT_READY;
	if (SceSblAIMgrForDriver_71608CA3()) {
		SceSysconForDriver_3274A925(1);
		SceSysconForDriver_40FF3898(0x20, 1);
	}
	hardware = SceSysconForDriver_CBD6D8BC();
	if ((hardware & 0xff0000) != 0x100000)
		return SceUsbdForDriver_1EC94F18(&s_driver);
	ret = SceUsbServForDriver_7AD36284(2, 0);
	if (ret < 0 && ret != (int)0x80248001)
		return ret;
	s_probed = 0;
	ret = SceUsbdForDriver_1EC94F18(&s_driver);
	if (ret < 0)
		return ret;
	for (n = 0; n < 50; n++) {
		if (s_probed)
			return 0;
		sceKernelDelayThread(10000);
	}
	SceUsbdForDriver_216F108D(&s_driver);
	ret = SceUsbServForDriver_7AD36284(2, 1);
	return ret < 0 ? ret : SMSC_ERROR_NO_DEVICE;
}

static int pollMii(struct SceUsbEtherSmscContext *a, unsigned int *value)
{
	int ret, count = 0;

	*value = 0;
	for (;;) {
		ret = readRegister(a, SMSC_MII_ADDR, value);
		if (ret < 0)
			return SMSC_ERROR_MII;
		if (!(*value & MII_BUSY))
			return 0;
		if (count > 100)
			sceKernelDelayThread(100000);
		count++;
	}
}
static int readPhy(struct SceUsbEtherSmscContext *a, int reg, unsigned short *value)
{
	unsigned int data;
	int ret;

	ret = pollMii(a, &data);
	if (ret < 0)
		return ret;
	ret = writeRegister(a, SMSC_MII_ADDR, MII_PHY_ADDR | MII_REG(reg));
	if (ret < 0)
		return ret;
	ret = pollMii(a, &data);
	if (ret < 0)
		return ret;
	ret = readRegister(a, SMSC_MII_DATA, &data);
	if (ret >= 0)
		*value = data;
	return ret;
}
static int writePhy(struct SceUsbEtherSmscContext *a, int reg, unsigned int value)
{
	unsigned int data;
	int ret;

	ret = pollMii(a, &data);
	if (ret < 0)
		return ret;
	ret = writeRegister(a, SMSC_MII_DATA, value);
	if (ret < 0)
		return ret;
	ret = writeRegister(a, SMSC_MII_ADDR, MII_PHY_ADDR | MII_REG(reg) | MII_WRITE);
	if (ret < 0)
		return ret;
	return pollMii(a, &data) < 0 ? SMSC_ERROR_MII : ret;
}
static int updateLink(struct SceUsbEtherSmscContext *a)
{
	unsigned short partner;
	unsigned int data, advertised, mac;
	int ret;

	lockFastMutex(&a->mutex);
	ret = readPhy(a, MII_BMSR, (unsigned short *)&data);
	if (ret < 0)
		goto out;
	if (a->link & NET_DRIVER_LINK_UP) {
		if (!(data & BMSR_LSTATUS)) {
			a->link = 0;
			a->driver.notify(&a->driver, (void *)a->driver.notify, NET_DRIVER_EVENT_LINK_DOWN, 0, NULL, 0);
		}
		goto out;
	}
	if (!(data & BMSR_LSTATUS))
		goto out;
	ret = readPhy(a, MII_BMCR, (unsigned short *)&data);
	if (ret < 0)
		goto out;
	if (data & BMCR_ANENABLE) {
		ret = readPhy(a, MII_LPA, &partner);
		if (ret < 0)
			goto out;
		ret = readPhy(a, MII_ADVERTISE, (unsigned short *)&advertised);
		if (ret < 0)
			goto out;
		data = partner & (unsigned short)advertised;
		if (data & ADVERTISE_100FULL)
			a->link = LINK_AUTO_100M | NET_DRIVER_LINK_FULL_DUPLEX;
		else if (data & ADVERTISE_100HALF)
			a->link = LINK_AUTO_100M;
		else if (data & ADVERTISE_10FULL)
			a->link = LINK_AUTO_10M | NET_DRIVER_LINK_FULL_DUPLEX;
		else if (data & ADVERTISE_10HALF)
			a->link = LINK_AUTO_10M;
	} else {
		a->link = data & BMCR_SPEED100 ? LINK_UP_100M : LINK_UP_10M;
		if (data & BMCR_FULLDPLX)
			a->link |= NET_DRIVER_LINK_FULL_DUPLEX;
	}
	mac = a->macControl & ~(MAC_CR_FDPX | MAC_CR_RCVOWN);
	mac |= a->link & NET_DRIVER_LINK_FULL_DUPLEX ? MAC_CR_FDPX : MAC_CR_RCVOWN;
	a->macControl = mac;
	ret = writeRegister(a, SMSC_MAC_CR, mac);
	if (ret >= 0)
		a->driver.notify(&a->driver, (void *)a->driver.notify, NET_DRIVER_EVENT_LINK_UP, 0, NULL, 0);
out:
	sceKernelUnlockFastMutex(&a->mutex);
	return ret;
}
static int setLink(struct SceUsbEtherSmscContext *a, unsigned int mode)
{
	unsigned int control, advertisement = 0, data;
	int ret, n;

	lockFastMutex(&a->mutex);
	switch (mode) {
	case 0:
		control = BMCR_PDOWN;
		break;
	case LINK_UP_10M:
		control = 0;
		break;
	case LINK_UP_10M | NET_DRIVER_LINK_FULL_DUPLEX:
		control = BMCR_FULLDPLX;
		break;
	case LINK_AUTO_10M:
		control = BMCR_ANENABLE | BMCR_ANRESTART;
		advertisement = ADVERTISE_10HALF;
		break;
	case LINK_AUTO_10M | NET_DRIVER_LINK_FULL_DUPLEX:
		control = BMCR_ANENABLE | BMCR_ANRESTART | BMCR_FULLDPLX;
		advertisement = ADVERTISE_10FULL;
		break;
	case LINK_UP_100M:
		control = BMCR_SPEED100;
		break;
	case LINK_UP_100M | NET_DRIVER_LINK_FULL_DUPLEX:
		control = BMCR_SPEED100 | BMCR_FULLDPLX;
		break;
	case LINK_AUTO_100M:
		control = BMCR_SPEED100 | BMCR_ANENABLE | BMCR_ANRESTART;
		advertisement = ADVERTISE_100HALF;
		break;
	case LINK_AUTO_100M | NET_DRIVER_LINK_FULL_DUPLEX:
		control = BMCR_SPEED100 | BMCR_ANENABLE | BMCR_ANRESTART | BMCR_FULLDPLX;
		advertisement = ADVERTISE_100FULL;
		break;
	case NET_DRIVER_LINK_ANY:
		control = BMCR_SPEED100 | BMCR_ANENABLE | BMCR_ANRESTART | BMCR_FULLDPLX;
		advertisement = ADVERTISE_ALL;
		break;
	default:
		ret = SMSC_ERROR_NOT_SUPPORTED;
		goto out;
	}
	ret = writePhy(a, MII_BMCR, control);
	if (ret < 0)
		goto out;
	if (mode & NET_DRIVER_LINK_AUTO) {
		ret = writePhy(a, MII_ADVERTISE, advertisement);
		if (ret < 0)
			goto out;
	}
	a->link = 0;
	if (a->flags & SMSC_FLAG_REGISTERED)
		a->driver.notify(&a->driver, (void *)a->driver.notify, NET_DRIVER_EVENT_LINK_DOWN, 0, NULL, 0);
	ret = readPhy(a, MII_BMCR, (unsigned short *)&data);
	if (ret < 0)
		goto out;
	ret = writePhy(a, MII_BMCR, (data & 0x7fff) | BMCR_RESET);
	if (ret < 0)
		goto out;
	for (n = 0; n < 100; n++) {
		ret = readPhy(a, MII_BMCR, (unsigned short *)&data);
		if (ret < 0 || !(data & BMCR_RESET))
			goto out;
		sceKernelDelayThread(10000);
	}
	ret = SMSC_ERROR_TIMEOUT;
out:
	sceKernelUnlockFastMutex(&a->mutex);
	return ret;
}

static int setMulticast(struct NetDriver *driver, const unsigned char *addresses, unsigned int length)
{
	struct SceUsbEtherSmscContext *a = driver->private;
	unsigned int lower = 0, upper = 0, crc, index, i, bit, count;
	int ret;

	if (!a->active)
		return SMSC_ERROR_NOT_READY;
	SceCpuForDriver_5F6A8743(&a->references, 1);
	lockFastMutex(&a->mutex);
	if (!addresses) {
		if (length) {
			ret = SMSC_ERROR_INVALID_ARGUMENT;
			goto out;
		}
		a->macControl &= ~(MAC_CR_MCPAS | MAC_CR_HPFILT);
	} else {
		if (length % sizeof(SceNetEtherAddr)) {
			ret = SMSC_ERROR_INVALID_ARGUMENT;
			goto out;
		}
		a->macControl = (a->macControl & ~MAC_CR_MCPAS) | MAC_CR_HPFILT;
		for (count = length / sizeof(SceNetEtherAddr); count; count--, addresses += sizeof(SceNetEtherAddr)) {
			crc = ~0U;
			for (i = 0; i < 6; i++) {
				for (bit = 0; bit < 8; bit++) {
					unsigned int feedback = (crc ^ (addresses[i] >> bit)) & 1;

					crc >>= 1;
					if (feedback)
						crc ^= 0xedb88320;
				}
			}
			index = 0;
			for (bit = 0; bit < 6; bit++)
				index = (index << 1) | ((crc >> bit) & 1);
			if (index & 32)
				upper |= 1U << (index & 31);
			else
				lower |= 1U << index;
		}
	}
	ret = writeRegister(a, SMSC_MAC_CR, a->macControl);
	if (ret >= 0) {
		ret = writeRegister(a, SMSC_HASHH, upper);
		if (ret >= 0)
			ret = writeRegister(a, SMSC_HASHL, lower);
	}
out:
	sceKernelUnlockFastMutex(&a->mutex);
	SceCpuForDriver_A4884C4E(&a->references, 1);
	return ret;
}
static int kickTx(struct NetDriver *driver, int argument)
{
	struct SceUsbEtherSmscContext *a = driver->private;

	(void)argument;

	if (!a->active)
		return SMSC_ERROR_NOT_READY;
	SceCpuForDriver_5F6A8743(&a->references, 1);
	if (__atomic_fetch_add(&a->pendingTx, 1, __ATOMIC_SEQ_CST) == 0)
		sceKernelSetEventFlag(a->workerEvent, WORKER_TX);
	SceCpuForDriver_A4884C4E(&a->references, 1);
	return 0;
}
static int interfaceStart(struct NetDriver *driver, void *memoryBase)
{
	struct SceUsbEtherSmscContext *a = driver->private;

	(void)memoryBase;
	if (!a->active)
		return SMSC_ERROR_NOT_READY;
	SceCpuForDriver_5F6A8743(&a->references, 1);
	SceCpuForDriver_A4884C4E(&a->references, 1);
	return 0;
}
static int interfaceStop(struct NetDriver *driver, void *memoryBase, int memorySize)
{
	(void)memorySize;
	return interfaceStart(driver, memoryBase);
}
static int interfaceClose(struct NetDriver *driver, int argument)
{
	struct SceUsbEtherSmscContext *a = driver->private;

	(void)argument;
	if (a->state & SMSC_STATE_USB_POWER)
		sceKernelSetEventFlag(a->workerEvent, WORKER_CLOSE);
	else
		driver->notify(driver, (void *)driver->notify, NET_DRIVER_EVENT_STATUS, NET_DRIVER_STATUS_STOPPED, NULL, 0);
	return 0;
}
static int interfaceOpen(struct NetDriver *driver, int argument)
{
	struct SceUsbEtherSmscContext *a = driver->private;
	int error = NET_DRIVER_ERROR_NOT_READY;

	(void)argument;
	if (a->state & SMSC_STATE_USB_POWER)
		sceKernelSetEventFlag(a->workerEvent, WORKER_OPEN);
	else
		driver->notify(driver, &error, NET_DRIVER_EVENT_STATUS, NET_DRIVER_STATUS_ERROR, &error, 4);
	return 0;
}
static int interfaceControl(struct NetDriver *driver, unsigned int command,
			    int *value, unsigned int length)
{
	struct SceUsbEtherSmscContext *a = driver->private;
	int ret;

	if (length != 4) {
		ret = SMSC_ERROR_INVALID_ARGUMENT;
	} else if (command == NET_DRIVER_CTL_GET_LINK) {
		*value = a->link;
		ret = 0;
	} else {
		if (!a->active)
			return SMSC_ERROR_NOT_READY;
		SceCpuForDriver_5F6A8743(&a->references, 1);
		switch (command) {
		case NET_DRIVER_CTL_SET_LINK:
			ret = setLink(a, *value);
			break;
		case NET_DRIVER_CTL_SET_PROMISCUOUS:
			lockFastMutex(&a->mutex);
			if (*value == 1)
				a->macControl |= MAC_CR_PRMS;
			else
				a->macControl &= ~MAC_CR_PRMS;
			ret = writeRegister(a, SMSC_MAC_CR, a->macControl);
			sceKernelUnlockFastMutex(&a->mutex);
			break;
		case NET_DRIVER_CTL_GET_PROMISCUOUS:
			*value = !!(a->macControl & MAC_CR_PRMS);
			ret = 0;
			break;
		default:
			ret = SMSC_ERROR_NOT_SUPPORTED;
			break;
		}
	}
	/* Sony also decrements this on the paths which never incremented it. */
	SceCpuForDriver_A4884C4E(&a->references, 1);
	return ret;
}
static int processEvent(int event, int type, ProcEventCreateParam *param, int context)
{
	struct SceUsbEtherSmscContext *a = (void *)context;

	(void)type;
	(void)param;
	if (event == PROC_EVENT_EXCLUSIVE)
		return sceKernelChangeThreadCpuAffinityMask(a->thread, 0x80000);
	if (event == PROC_EVENT_SHARED)
		return sceKernelChangeThreadCpuAffinityMask(a->thread, 0xf0000);
	return 0;
}
static int systemEvent(int resume, int event, void *param, void *context)
{
	struct SceUsbEtherSmscContext *a = context;

	(void)param;
	if (!resume) {
		if (event == SYSEVENT_SUSPEND_IDLE) {
			if (s_suspend && !s_suspending) {
				if (!(a->flags & SMSC_FLAG_INITIALIZED))
					return -2;
				sceKernelSetEventFlag(a->workerEvent, WORKER_DETACH);
				s_suspending = 1;
				return 0;
			}
		} else if (event == SYSEVENT_SUSPEND_IDLE_QUERY) {
			if (s_suspend)
				return a->flags & SMSC_FLAG_INITIALIZED ? -1 : 0;
		} else if (event == SYSEVENT_SUSPEND_IDLE_ENTER) {
			s_suspending = 0;
			if (a->flags & SMSC_FLAG_ATTACHED)
				s_suspend = 1;
		}
	}
	return 0;
}
static void powerEvent(int event, int value)
{
	if (event != POWER_EVENT_USB)
		return;
	if (!(value & POWER_USB_OFF)) {
		s_context.state |= SMSC_STATE_USB_POWER;
	} else {
		s_context.state &= ~SMSC_STATE_USB_POWER;
		if (s_context.flags & SMSC_FLAG_REGISTERED)
			s_context.driver.notify(&s_context.driver, (void *)s_context.driver.notify, NET_DRIVER_EVENT_RESUMED, 0, NULL, 0);
	}
}

static int initializeAdapter(struct SceUsbEtherSmscContext *a)
{
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned int value = 0;
	unsigned char mac[6];
	unsigned short phy;
	int ret, count, uid;

	lockFastMutex(&a->mutex);
	ret = writeRegister(a, SMSC_HW_CFG, HW_CFG_BIR | HW_CFG_LRST);
	if (ret < 0)
		goto unlock;
	for (;;) {
		ret = readRegister(a, SMSC_HW_CFG, &value);
		if (ret < 0)
			goto unlock;
		if (!(value & HW_CFG_LRST))
			break;
		sceKernelDelayThread(10000);
	}
	ret = writeRegister(a, SMSC_PM_CTRL, PM_CTRL_PHY_RST);
	if (ret < 0)
		goto unlock;
	for (;;) {
		ret = readRegister(a, SMSC_PM_CTRL, &value);
		if (ret < 0)
			goto unlock;
		if (!(value & PM_CTRL_PHY_RST))
			break;
		sceKernelDelayThread(10000);
	}
	ret = readRegister(a, SMSC_HW_CFG, &value);
	if (ret < 0)
		goto unlock;
	value = (value & ~HW_CFG_RXDOFF_MASK) | HW_CFG_BIR | HW_CFG_RXDOFF_2 | HW_CFG_MEF | HW_CFG_BCE;
	ret = writeRegister(a, SMSC_HW_CFG, value);
	if (ret < 0)
		goto unlock;
	value = __aeabi_uidiv(a->bufferSize, a->rxMaxPacket);
	ret = writeRegister(a, SMSC_BURST_CAP, value);
	if (ret < 0)
		goto unlock;
	ret = writeRegister(a, SMSC_BULK_IN_DLY, 0x2000);
	if (ret < 0)
		goto unlock;
	ret = writeRegister(a, SMSC_INT_EP_CTL, 0x48000);
	if (ret < 0)
		goto unlock;
	a->macControl = 0;
	ret = writeRegister(a, SMSC_MAC_CR, 0);
	if (ret < 0)
		goto unlock;
	ret = writeRegister(a, SMSC_VLAN1, 0x8100);	/* the 802.1Q tag */
	if (ret < 0)
		goto unlock;
	ret = writeRegister(a, SMSC_COE_CR, 0x10001);
	if (ret < 0)
		goto unlock;
	ret = writeRegister(a, SMSC_AFC_CFG, 0xf830a1);
	if (ret < 0)
		goto unlock;
	ret = writeRegister(a, SMSC_LED_GPIO_CFG, 0x1110000);
	if (ret < 0)
		goto unlock;
	ret = writeRegister(a, SMSC_GPIO_CFG, 0xfe01018f);
	if (ret < 0)
		goto unlock;
	ret = readPhy(a, PHY_INT_SRC, &phy);
	if (ret < 0)
		goto unlock;
	ret = writePhy(a, PHY_INT_MASK, PHY_INT_ANEG_COMPLETE | PHY_INT_LINK_DOWN);
	sceKernelUnlockFastMutex(&a->mutex);
	if (ret < 0)
		return ret;
	ret = setLink(a, NET_DRIVER_LINK_ANY);
	if (ret < 0)
		return ret;
	if (SceSblAIMgrForDriver_C6E83F34()) {
		ret = sceIdStorageLookup(0x119, 0, mac, 6);
		if (ret < 0)
			return ret;
	} else {
		value = 0;
		for (count = 0; count < 1000; count++) {
			ret = readRegister(a, SMSC_E2P_CMD, &value);
			if (ret < 0)
				return ret;
			if ((value & E2P_CMD_LOADED) && !(value & E2P_CMD_BUSY))
				break;
			sceKernelDelayThread(40);
		}
		if (count == 1000)
			return SMSC_ERROR_TIMEOUT;
		for (count = 1; count < 7; count++) {
			ret = writeRegister(a, SMSC_E2P_CMD, E2P_CMD_BUSY | count);
			if (ret < 0)
				return ret;
			for (;;) {
				value = 0;
				ret = readRegister(a, SMSC_E2P_CMD, &value);
				if (ret < 0)
					return ret;
				if (!(value & E2P_CMD_BUSY) || (value & E2P_CMD_TIMEOUT))
					break;
				sceKernelDelayThread(1000000);
			}
			if (value & (E2P_CMD_BUSY | E2P_CMD_TIMEOUT))
				return SMSC_ERROR_TIMEOUT;
			readRegister(a, SMSC_E2P_DATA, &value);
			mac[count - 1] = value;
		}
	}
	value = mac[0] | (mac[1] << 8) | (mac[2] << 16) | ((unsigned int)mac[3] << 24);
	ret = writeRegister(a, SMSC_ADDRL, value);
	if (ret < 0)
		return ret;
	value = mac[4] | (mac[5] << 8);
	ret = writeRegister(a, SMSC_ADDRH, value);
	if (ret < 0)
		return ret;
	a->driver.mac.data[4] = mac[4];
	a->driver.mac.data[5] = mac[5];
	*(unsigned int *)a->driver.mac.data = *(unsigned int *)mac;
	a->references = 0;
	a->active = 1;
	if (!(a->flags & SMSC_FLAG_REGISTERED)) {
		a->driver.type = NET_DRIVER_TYPE_ETHER;
		a->driver.stop = interfaceStop;
		a->driver.start = interfaceStart;
		a->driver.open = interfaceOpen;
		a->driver.close = interfaceClose;
		a->driver.kickTx = kickTx;
		a->driver.control = interfaceControl;
		a->driver.private = a;
		a->driver.multicast = setMulticast;
		a->driver.mtu = 1500;
		a->driver.capabilities = NET_DRIVER_CAPABILITIES_ETHER;
		a->driver.prefix = TX_PREFIX;
		ret = SceNetPsForDriver_1ABF937D(&a->driver);
		if (ret < 0)
			return ret;
		__atomic_fetch_or(&a->flags, SMSC_FLAG_REGISTERED, __ATOMIC_SEQ_CST);
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT |
			   SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
		opt.alignment = 0x1000;
		uid = SceSysmemForDriver_C94850C9("SceNetDrvEtherTx", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW,
					    (a->bufferSize + 0xfff) & ~0xfffU, &opt);
		ret = uid;
		if (ret < 0)
			return ret;
		ret = SceSysmemForDriver_A841EDDA(uid, (void **)&value);
		if (ret < 0 || !value)
			return ret;
		a->txBuffer = (void *)value;
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT |
			   SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
		opt.alignment = 0x1000;
		uid = SceSysmemForDriver_C94850C9("SceNetDrvEtherRx", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW,
					    (a->bufferSize + 0xfff) & ~0xfffU, &opt);
		ret = uid;
		if (ret < 0)
			return ret;
		ret = SceSysmemForDriver_A841EDDA(uid, (void **)&value);
		if (ret < 0 || !value)
			return ret;
		a->rxBuffer = (void *)value;
	}
	ret = receiveBuffer(a, a->rxBuffer, a->bufferSize);
	if (ret < 0)
		return ret;
	__atomic_fetch_or(&a->flags, SMSC_FLAG_INITIALIZED, __ATOMIC_SEQ_CST);
	a->state |= SMSC_STATE_USB_POWER;
	ScePowerForDriver_B24263F0(2, powerEvent);
	for (count = 0; count < 30; count++) {
		updateLink(a);
		sceKernelDelayThread(100000);
		if (a->link & NET_DRIVER_LINK_UP)
			break;
	}
	return 0;
unlock:
	sceKernelUnlockFastMutex(&a->mutex);
	return ret;
}

static int startInterface(struct SceUsbEtherSmscContext *a, int *retries)
{
	int ret;

	for (int n = 0; n < 10; n++) {
		if (a->flags & SMSC_FLAG_ATTACHED)
			goto attached;
		sceKernelDelayThread(1000000);
	}
	ret = SMSC_ERROR_NO_DEVICE;
	goto error;
attached:
	if ((a->flags & SMSC_FLAG_INITIALIZED) && (a->link & NET_DRIVER_LINK_UP)) {
		lockFastMutex(&a->mutex);
		a->macControl |= MAC_CR_TXEN;
		ret = writeRegister(a, SMSC_MAC_CR, a->macControl);
		if (ret >= 0)
			ret = writeRegister(a, SMSC_TX_CFG, TX_CFG_ON);
		sceKernelUnlockFastMutex(&a->mutex);
		if (ret < 0)
			goto error;
		lockFastMutex(&a->mutex);
		a->macControl |= MAC_CR_RXEN;
		ret = writeRegister(a, SMSC_MAC_CR, a->macControl);
		sceKernelUnlockFastMutex(&a->mutex);
		if (ret < 0)
			goto error;
		a->driver.notify(&a->driver, (void *)a->driver.notify, NET_DRIVER_EVENT_STATUS, NET_DRIVER_STATUS_STARTED, NULL, 0);
		__atomic_fetch_or(&a->flags, SMSC_FLAG_OPEN, __ATOMIC_SEQ_CST);
		*retries = 0;
	} else if (*retries == 10) {
		ret = SMSC_ERROR_TIMEOUT;
		*retries = 0;
		a->driver.notify(&a->driver, (void *)a->driver.notify, NET_DRIVER_EVENT_STATUS, NET_DRIVER_STATUS_ERROR, &ret, 4);
		updateLink(a);
	} else {
		sceKernelSetEventFlag(a->workerEvent, WORKER_OPEN);
		(*retries)++;
		sceKernelDelayThread(1000000);
	}
	return 0;
error:
	*retries = 0;
	a->driver.notify(&a->driver, (void *)a->driver.notify, NET_DRIVER_EVENT_STATUS, NET_DRIVER_STATUS_ERROR, &ret, 4);
	return ret;
}
static int stopInterface(struct SceUsbEtherSmscContext *a)
{
	unsigned int value = 0;
	int ret, count;

	if ((a->flags & (SMSC_FLAG_INITIALIZED | SMSC_FLAG_OPEN)) !=
	    (SMSC_FLAG_INITIALIZED | SMSC_FLAG_OPEN)) {
		a->driver.notify(&a->driver, (void *)a->driver.notify, NET_DRIVER_EVENT_STATUS, NET_DRIVER_STATUS_STOPPED, NULL, 0);
		return 0;
	}
	__atomic_fetch_and(&a->flags, ~SMSC_FLAG_OPEN, __ATOMIC_SEQ_CST);
	lockFastMutex(&a->mutex);
	ret = writeRegister(a, SMSC_TX_CFG, TX_CFG_STOP);
	if (ret < 0)
		goto errorUnlock;
	for (count = 0; count < 100; count++) {
		ret = readRegister(a, SMSC_TX_CFG, &value);
		if (ret < 0 || !(value & (TX_CFG_ON | TX_CFG_STOP)))
			break;
		sceKernelDelayThread(1000);
	}
	if (value & (TX_CFG_ON | TX_CFG_STOP)) {
		ret = SMSC_ERROR_TIMEOUT;
		goto errorUnlock;
	}
	a->macControl &= ~MAC_CR_TXEN;
	ret = writeRegister(a, SMSC_MAC_CR, a->macControl);
	if (ret < 0)
		goto errorUnlock;
	ret = writeRegister(a, SMSC_TX_CFG, TX_CFG_FIFO_FLUSH);
	if (ret < 0)
		goto errorUnlock;
	ret = readRegister(a, SMSC_TX_CFG, &value);
	if (ret < 0)
		goto errorUnlock;
	if (value & TX_CFG_FIFO_FLUSH) {
		ret = SMSC_ERROR_NOT_READY;
		goto errorUnlock;
	}
	sceKernelUnlockFastMutex(&a->mutex);
	lockFastMutex(&a->mutex);
	ret = writeRegister(a, SMSC_INT_STS, INT_STS_RX_STOP);
	if (ret < 0)
		goto errorUnlock;
	ret = readRegister(a, SMSC_MAC_CR, &value);
	if (ret < 0)
		goto errorUnlock;
	a->macControl &= ~MAC_CR_RXEN;
	ret = writeRegister(a, SMSC_MAC_CR, a->macControl);
	if (ret < 0)
		goto errorUnlock;
	for (count = 0; count < 2; count++) {
		ret = readRegister(a, SMSC_INT_STS, &value);
		if (ret < 0 || (value & INT_STS_RX_STOP))
			break;
		sceKernelDelayThread(1000);
	}
	if (!(value & INT_STS_RX_STOP)) {
		ret = SMSC_ERROR_TIMEOUT;
		goto errorUnlock;
	}
	ret = writeRegister(a, SMSC_RX_CFG, RX_CFG_FIFO_FLUSH);
	if (ret < 0)
		goto errorUnlock;
	ret = readRegister(a, SMSC_RX_CFG, &value);
	if (ret < 0)
		goto errorUnlock;
	if (value & RX_CFG_FIFO_FLUSH) {
		ret = SMSC_ERROR_NOT_READY;
		goto errorUnlock;
	}
	sceKernelUnlockFastMutex(&a->mutex);
	a->driver.notify(&a->driver, (void *)a->driver.notify, NET_DRIVER_EVENT_STATUS, NET_DRIVER_STATUS_STOPPED, NULL, 0);
	return 0;
errorUnlock:
	sceKernelUnlockFastMutex(&a->mutex);
	a->driver.notify(&a->driver, (void *)a->driver.notify, NET_DRIVER_EVENT_STATUS, NET_DRIVER_STATUS_ERROR, &ret, 4);
	return ret;
}
static void startTx(struct SceUsbEtherSmscContext *a)
{
	unsigned char *data = a->txBuffer;
	unsigned char *end = data + a->bufferSize;
	unsigned int free = a->bufferSize, prefix = a->driver.prefix;
	unsigned int bytes = 0, packets = 0, broadcastBytes = 0, broadcasts = 0;
	unsigned int multicastBytes = 0, multicasts = 0;
	struct NetDriverPacket *packet;
	unsigned int *header;
	unsigned int amount;
	unsigned char *ether;
	int old, ret;

	old = __atomic_exchange_n(&a->pendingTx, 1, __ATOMIC_SEQ_CST);
	while (prefix + ETHER_MAX_FRAME <= free) {
		packet = a->driver.dequeueTx(&a->driver);
		if (!packet)
			break;
		if (free < prefix + packet->length) {
			a->driver.releaseTx(&a->driver, packet);
			break;
		}
		amount = packet->length + prefix;
		bytes += packet->length;
		packets++;
		header = (void *)(packet->buffer->data - prefix);
		header[0] = packet->length | TX_CMD_A_FIRST_SEG | TX_CMD_A_LAST_SEG;
		header[1] = packet->length;
		memcpy(data, header, packet->length + prefix);
		ether = packet->buffer->data;
		if (ether[0] == 0xff && ether[1] == 0xff && ether[2] == 0xff &&
		    ether[3] == 0xff && ether[4] == 0xff && ether[5] == 0xff) {
			broadcastBytes += packet->length;
			broadcasts++;
		} else if (ether[0] & 1) {
			multicastBytes += packet->length;
			multicasts++;
		}
		a->driver.releaseTx(&a->driver, packet);
		data = (void *)(((unsigned int)data + amount + 3) & ~3U);
		free = end - data;
	}
	if (packets) {
		ret = sendBuffer(a, a->txBuffer, data - a->txBuffer);
		if (ret >= 0) {
			a->driver.txBytes += bytes;
			a->driver.txPackets += packets;
			a->driver.txMulticastBytes += multicastBytes;
			a->driver.txMulticastPackets += multicasts;
			a->driver.txBroadcastBytes += broadcastBytes;
			a->driver.txBroadcastPackets += broadcasts;
			return;
		}
		a->driver.txErrors += packets;
	}
	old = __atomic_fetch_sub(&a->pendingTx, 1, __ATOMIC_SEQ_CST);
	if (old > 1)
		sceKernelSetEventFlag(a->workerEvent, WORKER_TX);
}
static void startRx(struct SceUsbEtherSmscContext *a)
{
	unsigned char *data = a->rxBuffer;
	unsigned char *end = data + a->rxCount;
	unsigned int status, length;
	int delivered = 0;
	struct NetDriverPacket *packet;

	while (data + 4 < end) {
		status = *(unsigned int *)data;
		length = RX_STS_LENGTH(status);
		if (length <= RX_HEADER)
			break;
		length -= RX_HEADER;
		if (status & RX_STS_ERROR) {
			a->driver.rxErrors++;
		} else if (end < data + RX_HEADER + length || !(packet = a->driver.allocateRx(&a->driver, length, 0))) {
			a->driver.rxDrops++;
		} else {
			memcpy(packet->buffer->end, data + RX_HEADER, length);
			packet->buffer->end += length;
			packet->length += length;
			status = *(unsigned int *)data;
			a->driver.rxBytes += length;
			a->driver.rxPackets++;
			if (status & RX_STS_BROADCAST) {
				a->driver.rxBroadcastBytes += length;
				a->driver.rxBroadcastPackets++;
			}
			if (status & RX_STS_MULTICAST) {
				a->driver.rxMulticastBytes += length;
				a->driver.rxMulticastPackets++;
			}
			a->driver.deliverRx(&a->driver, packet);
			delivered++;
		}
		data = (void *)(((unsigned int)data + length + 0xf) & ~3U);
	}
	if (delivered)
		a->driver.notify(&a->driver, (void *)a->driver.notify, NET_DRIVER_EVENT_STATUS, NET_DRIVER_STATUS_RECEIVED, NULL, 0);
	receiveBuffer(a, a->rxBuffer, a->bufferSize);
	sceKernelDelayThread(500);
}
static int workerEntry(SceSize arglen, void *argp)
{
	struct SceUsbEtherSmscContext *a = *(struct SceUsbEtherSmscContext **)argp;
	unsigned int bits;
	SceUInt timeout = 0;
	SceUInt64 last = 0, elapsed = 0, now;
	int ret, retries = 0, count;

	(void)arglen;
	a->proc.exit = NULL;
	a->proc.kill = NULL;
	a->proc.size = sizeof(a->proc);
	a->proc.notify = processEvent;
	a->proc.create = NULL;
	a->proc.suspend = NULL;
	a->proc.resume = NULL;
	ret = sceKernelRegisterProcEventHandler("SceNetDrvEther", &a->proc, (int)a);
	if (ret < 0) {
		if (a->procId > 0) {
			SceProcEventForDriver_3DED57CC(a->procId);
			a->procId = 0;
		}
		SceDebugForDriver_391B74B7("UsbEtherSmsc workerEntry Exit. ret=0x%08x\n", ret);
		sceKernelExitThread(0);
		return ret;
	}
	a->procId = ret;
	for (;;) {
		bits = 0;
		SceUInt *wait = NULL;

		if (a->flags & SMSC_FLAG_INITIALIZED) {
			timeout = SMSC_LINK_POLL;
			wait = &timeout;
		}
		ret = sceKernelWaitEventFlag(a->workerEvent, WORKER_EVENTS,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, wait);
		now = sceKernelGetSystemTimeWide();
		if (ret) {
			if (ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
				elapsed += SMSC_LINK_POLL - timeout;
				if (elapsed >= SMSC_LINK_POLL) {
					updateLink(a);
					elapsed = 0;
					last = now;
				}
			}
			continue;
		}
		if (bits & WORKER_DETACH) {
			a->active = 0;
			for (count = 0; count < 3000; count++) {
				if (!a->references)
					break;
				sceKernelDelayThread(1000);
			}
			if (a->flags & SMSC_FLAG_INITIALIZED)
				__atomic_fetch_and(&a->flags, ~(SMSC_FLAG_INITIALIZED | SMSC_FLAG_OPEN), __ATOMIC_SEQ_CST);
			retries = 0;
		}
		if (bits & WORKER_ATTACHED)
			initializeAdapter(a);
		if (bits & WORKER_OPEN)
			startInterface(a, &retries);
		if (bits & WORKER_CLOSE)
			stopInterface(a);
		if (bits & WORKER_TX)
			startTx(a);
		if (bits & WORKER_RX)
			startRx(a);
		if (now < last || now > last + SMSC_LINK_POLL) {
			updateLink(a);
			last = now;
		}
	}
}
int module_start(unsigned int arglen, void *argp)
{
	struct SceUsbEtherSmscContext *a = &s_context;
	unsigned int hardware;
	int ret;

	(void)arglen;
	(void)argp;
	hardware = SceSysconForDriver_CBD6D8BC();
	if (SceSblAIMgrForDriver_D78B04A2() && !(hardware & 0x80))
		return 0;
	memset(a, 0, sizeof(*a));
	ret = sceKernelInitializeFastMutex(&a->mutex, "SceUsbEtherSmscControl", 0x2000, NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelCreateEventFlag("SceUsbEtherSmscWorker", 0, 0, NULL);
	if (ret < 0)
		return ret;
	a->workerEvent = ret;
	ret = sceKernelCreateThread("SceNetDrvEther", workerEntry, 0x3e, 0x2000, 0, 0xf0000, NULL);
	if (ret < 0)
		return ret;
	a->thread = ret;
	ret = sceKernelStartThread(ret, 4, &a);
	if (ret < 0)
		return ret;
	ret = sceKernelRegisterSysEventHandler("SceUsbEtherSmsc", systemEvent, a);
	if (ret < 0)
		return ret;
	ret = initializeUsb(a);
	return ret < 0 ? ret : 0;
}
