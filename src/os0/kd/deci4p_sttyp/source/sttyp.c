/*
 * SceDeci4pSTtyp: kernel printf output over DECI4P, one channel per CPU
 * (protocols 0x90030080..83, "STTYP0".."STTYP3"), through SceDeci4pSDfMgr
 * which works with interrupts off.
 *
 * module_bootstart runs on every CPU and opens that CPU's channel; CPU 0 also
 * installs the kprintf handler. The handler formats into the channel's
 * 255-byte line buffer and sends it (polling until the host took it) on a
 * newline or when the buffer is full. Output stops while suspended.
 */

#include "sce/cp15.h"
#include <stdarg.h>
#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/pm_mgr.h>
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sdfmgr.h"
#include "sce/suspend.h"
#include "sce/sysroot.h"

#define STTYP_CPUS		4
#define STTYP_BUF_SIZE		0x100
#define STTYP_LINE_MAX		0xff
#define STTYP_SUSPEND_IDX	0x17

/* SttypPutc pseudo characters */
#define STTYP_DISCARD		0x200
#define STTYP_FLUSH		0x201

/* printf flags */
#define F_LONG			0x01
#define F_QUAD			0x02
#define F_SHORT			0x04
#define F_ALT			0x08	/* '#' */
#define F_LEFT			0x10	/* '-' */
#define F_ZERO			0x20	/* '0' */
#define F_PREFIX		0x40	/* "0x" */

#if FW_DEBUG
#define STTYP_PRINTF(_line, _fmt, ...)					\
	SceDeci4pSDfMgrForKernel_501CE61D(0xf, SCE_DBGINFO(_line), _fmt, ##__VA_ARGS__)
#else
#define STTYP_PRINTF(_line, _fmt, ...)	do { } while (0)
#endif

typedef struct SttypPort {
	int handle;				/* SceDeci4pSDfMgrForKernel_A55F5F0D */
	SceSize len;				/* characters in buf */
	SceUInt32 suspended;
	SceSize sent;				/* bytes of the packet sent */
	TtypHeader hdr;				/* the packet: hdr .. buf[len] */
	char buf[STTYP_BUF_SIZE];
} SttypPort;

_Static_assert(sizeof(SttypPort) == 0x13c, "SttypPort layout");

static const SceSDfMgrProtocol s_protocol[STTYP_CPUS] = {
	{ sizeof(SceSDfMgrProtocol), 0x90030080, 0x80030090, 0x01000000, 0x00020001, 0x00030000, "STTYP0" },
	{ sizeof(SceSDfMgrProtocol), 0x90030081, 0x80030091, 0x01000000, 0x00020001, 0x00030000, "STTYP1" },
	{ sizeof(SceSDfMgrProtocol), 0x90030082, 0x80030092, 0x01000000, 0x00020001, 0x00030000, "STTYP2" },
	{ sizeof(SceSDfMgrProtocol), 0x90030083, 0x80030093, 0x01000000, 0x00020001, 0x00030000, "STTYP3" },
};

static SttypPort s_port[STTYP_CPUS];

/* name unknown: suspend handler */
static int SttypSuspend(void *args)
{
	s_port[SceCpuId()].suspended = 1;
	return 0;
}

/* name unknown: resume handler */
static int SttypResume(void *args)
{
	s_port[SceCpuId()].suspended = 0;
	return 0;
}

static void KpHandler(int event, void *arg, void *ctx)
{
	SttypPort *port = ctx;
	char data[16];
	int handle, ret;
	SceSize sent;

	switch (event) {
	case 1:					/* the host sent something: drop it */
		ret = sceSDfMgrExRecv(port->handle, data, sizeof(data));
		if (ret < 0) {
			STTYP_PRINTF(699, "STTYP read error (0x%08x)\n", ret);
			SCE_DBG_ASSERT_FAIL(700);
		}
		break;
	case 2:
	case 5:
	case 6:
		break;
	case 3:					/* send (the rest of) the packet */
		sent = port->sent;
		handle = port->handle;
		ret = sceSDfMgrExSend(handle, (SceUInt8 *)&port->hdr + sent,
				      SceDeci4pSDfMgrForKernel_68903C1D(&port->hdr.hdr) - port->sent);
		if (ret < 0)
			STTYP_PRINTF(687, "STTYP write error (0x%08x)\n", ret);
		else
			port->sent += ret;
		break;
	case 4:					/* sent: the line buffer is free */
		port->len = 0;
		break;
	default:
		STTYP_PRINTF(710, "KpHandler: unknown event 0x%x.\n", event);
		break;
	}
}

/* name unknown: send the line and poll until it is out */
static void SttypFlush(SttypPort *port)
{
	SceUInt64 t;

	if (port->suspended)
		return;
	SceDeci4pSDfMgrForKernel_FFC95B63(&port->hdr.hdr, sizeof(port->hdr) + port->len);
	port->hdr.len = port->len;
	t = SceSysrootForDriver_273EAE53();
	port->hdr.time[0] = t;
	port->hdr.time[1] = t >> 32;
	port->sent = 0;
	if (_threadSignal(port->handle) < 0)
		return;
	while (port->len != 0)
		sceSDfMgrPoll();
}

static void SttypFlushPending(SttypPort *port)
{
	if (port->len != 0)
		SttypFlush(port);
}

static void SttypPutc(SttypPort *port, int c)
{
	if (c == STTYP_DISCARD) {
		port->len = 0;
		return;
	}
	if (c == STTYP_FLUSH) {
		SttypFlushPending(port);
		return;
	}
	if (c == '\n')
		SttypPutc(port, '\r');
	port->buf[port->len++] = c;
	if (port->len == STTYP_LINE_MAX)
		SttypFlush(port);
}

/* whether a read of p would fault (address translation with all domains client) */
static inline int SttypBadAddr(const void *p)
{
	SceUInt32 dacr, par;

	dacr = readDacr();
	writeDacr(DACR_ALL_CLIENT);
	writeAts1cpr(p);
	par = readPar();
	writeDacr(dacr);
	return par & 1;
}

/* name unknown: the kprintf handler */
static int SttypKprintf(int unk, const char *fmt, va_list ap)
{
	static const char lower[] = "0123456789abcdef";
	static const char upper[] = "0123456789ABCDEF";
	const char *digits, *p, *start;
	char cbuf[1], nbuf[24], *q;
	SttypPort *port;
	SceUInt64 v64;
	SceUInt32 v, flags, base;
	int count, intr, width, prec, len, last, total, i;
	signed char c;
	char sign;

	intr = ksceKernelCpuSuspendIntr();
	if (fmt == NULL) {
		count = 0;
		goto out;
	}

	port = &s_port[SceCpuId()];
	port->len = 0;
	p = fmt;
	c = *p;
	if (c == '\0') {
		count = 0;
		goto done;
	}

	v64 = 0;
	count = 0;
	digits = lower;
	for (;;) {
		if (c != '%') {
			count++;
			SttypPutc(port, c);
			goto next;
		}

		width = 0;
		prec = -1;
		sign = 0;
		flags = 0;
		c = *++p;
again:
		switch (c) {
		case '\0':
			goto done;
		case '-':
			flags |= F_LEFT;
			c = *++p;
			goto again;
		case '+':
			sign = '+';
			c = *++p;
			goto again;
		case ' ':
			if (sign == 0)
				sign = ' ';
			c = *++p;
			goto again;
		case '#':
			flags |= F_ALT;
			c = *++p;
			goto again;
		case '0':
			flags |= F_ZERO;
			c = *++p;
			goto again;
		case '*':
			width = va_arg(ap, int);
			if (width < 0) {
				width = -width;
				flags |= F_LEFT;
			}
			c = *++p;
			goto again;
		case '.':
			c = *++p;
			if (c == '*') {
				prec = va_arg(ap, int);
				prec |= prec >> 31;
				c = *++p;
				goto again;
			}
			prec = 0;
			if ((unsigned char)(c - '0') > 9)
				goto again;
			do {
				prec = prec * 10 + c - '0';
				c = *++p;
			} while ((unsigned char)(c - '0') <= 9);
			prec |= prec >> 31;
			goto again;
		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':
			width = 0;
			do {
				width = width * 10 + c - '0';
				c = *++p;
			} while ((unsigned char)(c - '0') <= 9);
			goto again;
		case 'l':
			if (flags & F_LONG)
				flags = (flags & ~F_LONG) | F_QUAD;
			else
				flags |= F_LONG;
			c = *++p;
			goto again;
		case 'L':
			flags |= F_QUAD;
			c = *++p;
			goto again;
		case 'h':
			flags |= F_SHORT;
			c = *++p;
			goto again;

		case 'c':
			cbuf[0] = va_arg(ap, int);
			start = cbuf;
			len = 1;
			last = 0;
			prec = 0;
			sign = 0;
			goto emit;
		case 's':
			start = va_arg(ap, const char *);
			if (start == NULL)
				start = "(null)";
			else if (SttypBadAddr(start))
				start = "(access error)";
			len = strnlen(start, prec);
			last = len - 1;
			prec = 0;
			sign = 0;
			goto emit;

		case 'D':
			flags |= F_LONG;
			/* fall through */
		case 'd':
		case 'i':
			base = 10;
			if (flags & F_QUAD) {
				v64 = va_arg(ap, SceInt64);
				if ((SceInt64)v64 < 0) {
					v64 = -v64;
					sign = '-';
				}
				goto num64;
			}
			v = va_arg(ap, int);
			if ((int)v < 0) {
				sign = '-';
				v = -v;
			}
			goto num32;
		case 'U':
			flags |= F_LONG;
			/* fall through */
		case 'u':
			base = 10;
			goto unsigned_num;
		case 'O':
			flags |= F_LONG;
			/* fall through */
		case 'o':
			base = 8;
unsigned_num:
			sign = 0;
			if (flags & F_QUAD) {
				v64 = va_arg(ap, SceUInt64);
				goto num64;
			}
			v = va_arg(ap, SceUInt32);
			goto num32;
		case 'X':
			digits = upper;
			/* fall through */
		case 'x':
			base = 16;
			sign = 0;
			if (flags & F_QUAD) {
				v64 = va_arg(ap, SceUInt64);
				if ((flags & F_ALT) && v64 != 0)
					flags |= F_PREFIX;
				goto num64;
			}
			v = va_arg(ap, SceUInt32);
			if ((flags & F_ALT) && v != 0)
				flags |= F_PREFIX;
			goto num32;
		case 'p':
			if (width == 0)
				width = 10;
			v = va_arg(ap, SceUInt32);
			base = 16;
			sign = 0;
			i = flags & F_QUAD;
			flags |= F_PREFIX | F_ZERO;
			if (i)
				goto num64;		/* (prints the last 64-bit value) */
			goto num32;

		default:
			count++;
			SttypPutc(port, c);
			goto next;
		}

num32:
		if (prec != -1) {
			flags &= ~F_ZERO;
			if (v == 0 && prec == 0) {
				len = 0;
				last = -1;
				start = &nbuf[sizeof(nbuf) - 1];
				goto emit_sign;
			}
		}
		q = &nbuf[sizeof(nbuf) - 2];
		do {
			*q-- = digits[v % base];
			v /= base;
		} while (v != 0);
		goto digits_done;

num64:
		if (prec != -1) {
			flags &= ~F_ZERO;
			if (v64 == 0 && prec == 0) {
				len = 0;
				last = -1;
				start = &nbuf[sizeof(nbuf) - 1];
				v64 = 0;
				goto emit_sign;
			}
		}
		q = &nbuf[sizeof(nbuf) - 2];
		do {
			*q-- = digits[v64 % base];
			v64 /= base;
		} while (v64 != 0);
		v64 = 0;

digits_done:
		q++;
		if ((flags & F_ALT) && base == 8 && *q != '0')
			*--q = '0';
		start = q;
		len = &nbuf[sizeof(nbuf) - 1] - start;
		last = len - 1;
		digits = lower;

emit_sign:
		if (sign)
			len++;
emit:
		if (flags & F_PREFIX)
			len += 2;
		total = len >= prec ? len : prec;
		if (!(flags & (F_LEFT | F_ZERO)) && width != 0)
			for (i = total; i < width; i++)
				SttypPutc(port, ' ');
		if (sign)
			SttypPutc(port, sign);
		if (flags & F_PREFIX) {
			SttypPutc(port, '0');
			SttypPutc(port, *p == 'p' ? 'x' : *p);
		}
		if ((flags & (F_LEFT | F_ZERO)) == F_ZERO)
			for (i = total; i < width; i++)
				SttypPutc(port, '0');
		for (; len < prec; len++)
			SttypPutc(port, '0');
		for (i = 0; i <= last; i++)
			SttypPutc(port, start[i]);
		if (flags & F_LEFT)
			for (i = total; i < width; i++)
				SttypPutc(port, ' ');
		count += total >= width ? total : width;
next:
		c = *++p;
		if (c == '\0')
			break;
	}

done:
	SttypFlushPending(port);
out:
	ksceKernelCpuResumeIntr(intr);
	return count;
}

int module_start(SceSize args, void *argp)
{
	SceProductMode mode = 0;

	if (!ksceKernelCheckDipsw(0xc1) || !ksceKernelCheckDipsw(0xc6)) {
		if (SceCpuId() == 0)
			ksceKernelPrintf("STTYP is disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	if (kscePmMgrGetProductMode(&mode) < 0)
		return SCE_KERNEL_START_SUCCESS;
	return mode == 1 ? SCE_KERNEL_START_NO_RESIDENT : SCE_KERNEL_START_SUCCESS;
}

int module_bootstart(SceSize args, void *argp)
{
	SttypPort *port = &s_port[SceCpuId()];
	int handle;

	SceDeci4pSDfMgrForKernel_8A3B06F2(port, 0, sizeof(*port));
	port->hdr.hdr.version = SCE_DECI4P_VERSION;
	port->hdr.hdr.dst = 0;
	port->hdr.hdr.src = 0xa200 + (1 << SceCpuId());
	port->hdr.port = TTYP_PORT_KERNEL;
	port->hdr.hdr.protocol = s_protocol[SceCpuId()].protocol;
	handle = SceDeci4pSDfMgrForKernel_A55F5F0D(KpHandler, &s_protocol[SceCpuId()], port);
	if (handle < 0)
		return handle;
	port->handle = handle;

	if (SceCpuId() == 0) {
		ksceKernelRegisterKprintfHandler(SttypKprintf, NULL);
		SceKernelSuspendForDriver_CE7A2207(STTYP_SUSPEND_IDX, SttypSuspend, NULL);
		SceKernelSuspendForDriver_105C5752(STTYP_SUSPEND_IDX, SttypResume, NULL);
	}
	ksceKernelPrintf("STTYP%d/DECI4p is ready\n", SceCpuId());
	return 0;
}
