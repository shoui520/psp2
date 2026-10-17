#ifndef RTL_INTERNAL_H
#define RTL_INTERNAL_H

#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/cpu/atomic.h>
#include <psp2kern/kernel/proc_event.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/aimgr.h>
#include <psp2kern/lowio/gpio.h>
#include <psp2kern/usbd.h>
#include <psp2kern/usbserv.h>
#include <psp2common/net.h>
#include <stddef.h>
#include <string.h>
#include <net_ps/netdrv.h>
#include "usb_ether_rtl.h"
#include <sysmem/debug.h>
#include "fw.h"

/* Error names and private structure names are unknown. */
#define RTL_ERROR(n) ((int)(0x80418000U | (n)))
struct RtlAdapter;
struct RtlAdapter {
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
	unsigned int txBufferSize;
	unsigned int rxBufferSize;
	unsigned char *rxBuffer;
	unsigned char *txBuffer;
	unsigned int macControl;
	SceKernelFastMutex mutex;
	int thread;
	unsigned int link;
	int references;
	int active;
	unsigned int rxCount;
	SceProcEventHandler proc;
	int procId;
	unsigned int state;
	unsigned int revision;
};
_Static_assert(sizeof(struct RtlAdapter) == 0x16c, "adapter size");
_Static_assert(offsetof(struct RtlAdapter, mutex) == 0xf0, "mutex offset");
extern struct RtlAdapter rtlAdapter;
/* name unknown: transfer completion and register-access helpers. */
void rtlControlDone(int result, int count, void *context);
int rtlReadRegister(struct RtlAdapter *a, unsigned short reg, unsigned short mask,
		    unsigned short length, unsigned int *value);
int rtlWriteRegister(struct RtlAdapter *a, unsigned short reg, unsigned short mask,
		     unsigned short length, unsigned int value);
int rtlReadPla(struct RtlAdapter *a, unsigned int reg, unsigned int length, void *data);
int rtlWritePla(struct RtlAdapter *a, unsigned int reg, unsigned int mask,
		 unsigned int length, const void *data);
int rtlReadPla8(struct RtlAdapter *a, unsigned int reg, unsigned char *value);
int rtlReadPla16(struct RtlAdapter *a, unsigned int reg, unsigned short *value);
int rtlReadPla32(struct RtlAdapter *a, unsigned int reg, unsigned int *value);
int rtlWritePla8(struct RtlAdapter *a, unsigned int reg, unsigned char value);
int rtlWritePla16(struct RtlAdapter *a, unsigned int reg, unsigned short value);
int rtlWritePla32(struct RtlAdapter *a, unsigned int reg, unsigned int value);
int rtlReadUsb(struct RtlAdapter *a, unsigned short reg, unsigned int length, void *data);
int rtlWriteUsb(struct RtlAdapter *a, unsigned short reg, unsigned short mask,
		 unsigned int length, const void *data);
int rtlReadUsb8(struct RtlAdapter *a, unsigned int reg, unsigned char *value);
int rtlWriteUsb8(struct RtlAdapter *a, unsigned int reg, unsigned char value);
int rtlWriteUsb16(struct RtlAdapter *a, unsigned int reg, unsigned short value);
int rtlWriteUsb32(struct RtlAdapter *a, unsigned int reg, unsigned int value);
int rtlEnableReceive(struct RtlAdapter *a);
int rtlUpdateLink(struct RtlAdapter *a);
int rtlWorker(SceSize length, void *arg);
int rtlProcessEvent(int event, int type, void *parameter, void *context);
void rtlPowerEvent(int event, int value);
int rtlSendBuffer(struct RtlAdapter *a, void *data, unsigned int length);
int rtlReceiveBuffer(struct RtlAdapter *a, void *data, unsigned int length);
int rtlWakeFeature(struct RtlAdapter *a);
void rtlPopulateInterface(struct RtlAdapter *a);

#endif
