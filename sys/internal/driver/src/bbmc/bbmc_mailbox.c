/* SceBbmc's single semaphore-backed intrusive message queue. */
#include <stddef.h>
#include <sysmem/cpu.h>
#include <threadmgr/threadmgr.h>
#include "bbmc_mailbox.h"

/* The slot flag immediately precedes the one mailbox returned by its owner. */
typedef struct BbmcMailboxSlot {
	int allocated;
	BbmcMailbox mailbox;
} BbmcMailboxSlot;

static BbmcMailboxSlot g_bbmcMailboxSlot;

_Static_assert(sizeof(BbmcMailbox) == 20, "BbmcMailbox size");
_Static_assert(sizeof(BbmcMailboxSlot) == 24, "BbmcMailboxSlot size");

/* name unknown: resets the whole single-slot mailbox owner. */
int bbmcInitializeMailbox(void)
{
	g_bbmcMailboxSlot.allocated = 0;
	g_bbmcMailboxSlot.mailbox.semaphore = 0;
	g_bbmcMailboxSlot.mailbox.lock = 0;
	g_bbmcMailboxSlot.mailbox.count = 0;
	g_bbmcMailboxSlot.mailbox.head = NULL;
	g_bbmcMailboxSlot.mailbox.tail = NULL;
	return 0;
}

/* name unknown: shutdown of the slot owner has no separate operation. */
int bbmcFinalizeMailbox(void)
{
	return 0;
}

/* name unknown: claims the one mailbox and returns its semaphore UID. */
int bbmcCreateMailbox(BbmcMailbox **mailbox)
{
	int ret;
	BbmcMailbox *box;

	if (mailbox == NULL)
		return 0x80330002;
	if (g_bbmcMailboxSlot.allocated != 0)
		return 0x80330005;
	g_bbmcMailboxSlot.allocated = 1;
	*mailbox = &g_bbmcMailboxSlot.mailbox;
	ret = sceKernelCreateSema("SceBbmcMbx", 0, 0, 0xffff, NULL);
	if (ret < 0) {
		g_bbmcMailboxSlot.allocated = 0;
	} else {
		box = *mailbox;
		box->head = NULL;
		box->semaphore = ret;
		box->lock = 0;
		box->count = 0;
		(*mailbox)->tail = NULL;
	}
	return ret;
}

/* name unknown: releases the single mailbox slot. */
unsigned int bbmcDeleteMailbox(BbmcMailbox *mailbox)
{
	if (mailbox != &g_bbmcMailboxSlot.mailbox)
		return 0x80330002;
	g_bbmcMailboxSlot.allocated = 0;
	sceKernelDeleteSema(g_bbmcMailboxSlot.mailbox.semaphore);
	return 0;
}

/* name unknown: queues an intrusive entry and wakes the mailbox receiver. */
int bbmcSendMailbox(BbmcMailbox *mailbox, BbmcQueueEntry *entry)
{
	SceKernelIntrStatus intr;

	if (entry == NULL)
		return 0x80330002;
	entry->next = NULL;
	intr = SceCpuForDriver_D32ACE9E(&mailbox->lock);
	if (mailbox->head != NULL) {
		mailbox->tail->next = entry;
		mailbox->tail = entry;
	} else {
		mailbox->head = entry;
		mailbox->tail = entry;
	}
	mailbox->count++;
	sceKernelSignalSema(mailbox->semaphore, 1);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&mailbox->lock, intr);
	return 0;
}

/* name unknown: waits for one entry, then removes its intrusive linkage. */
int bbmcReceiveMailbox(BbmcMailbox *mailbox, BbmcQueueEntry **entry, SceUInt *timeout)
{
	SceKernelIntrStatus intr;
	int ret;

	if (entry == NULL)
		return 0x80330002;
	ret = sceKernelWaitSema(mailbox->semaphore, 1, timeout);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_D32ACE9E(&mailbox->lock);
	*entry = mailbox->head;
	mailbox->head = mailbox->head->next;
	(*entry)->next = NULL;
	if (mailbox->head == NULL)
		mailbox->tail = NULL;
	mailbox->count--;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&mailbox->lock, intr);
	return ret;
}
