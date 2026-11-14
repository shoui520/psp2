/*
 * The WEP ICV error list (0x7a0c..0x7bf6): a station whose WEP frame fails the
 * ICV check is recorded with the key the decryption used and reported to the
 * host once (EVENT_WEP_ICV_ERR); it is forgotten when a frame from it
 * decrypts, or after WEP_ICV_ERR_AGE_TICKS ageing ticks.
 */
#include "robin/icverr.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"

struct WlanRomServices wlanRomServices __attribute__((section(".itcm.rw")));

ROBIN_PATCH_SLOT(wlanWepIcvErrInitOverride);
ROBIN_PATCH_SLOT(wlanWepIcvErrRemoveOverride);
ROBIN_PATCH_SLOT(wlanWepIcvErrForgetOverride);
ROBIN_PATCH_SLOT(wlanWepIcvErrRefreshOverride);
ROBIN_PATCH_SLOT(wlanWepIcvErrRecordOverride);
ROBIN_PATCH_SLOT(wlanWepIcvErrAgeOverride);

void wlanWepIcvErrInit(WepIcvErrEntry *entries, WepIcvErrList *list)
{
	int i;

	if (wlanWepIcvErrInitOverride(entries, list))
		return;

	*(uint32_t *)&entries[0] = 0;	/* the first entry's age and report flag */
	list->head = NULL;
	list->freeList = entries;
	list->tail = NULL;
	/* links all 15, so the last one points past the array */
	for (i = 0; i < WEP_ICV_ERR_ENTRIES; i++)
		entries[i].next = &entries[i + 1];
}

/* Unlinks entry (prev is the one before it, or NULL) and frees it; the tail is left as it is. */
void wlanWepIcvErrRemove(WepIcvErrEntry *prev, WepIcvErrEntry *entry, WepIcvErrList *list)
{
	if (wlanWepIcvErrRemoveOverride(prev, entry, list))
		return;

	entry->ageTicks = 0;
	entry->reported = 0;
	__aeabi_memclr4(&entry->event, 24);

	if (prev)
		prev->next = entry->next;
	else
		list->head = entry->next;

	entry->next = list->freeList;
	list->freeList = entry;
}

/* A frame from the transmitter decrypted: drop its entries. */
void wlanWepIcvErrForget(WlanWirelessFrame *frame, WepIcvErrList *list)
{
	WepIcvErrEntry *prev = NULL;
	WepIcvErrEntry *entry, *next;

	if (wlanWepIcvErrForgetOverride(frame, list))
		return;

	for (entry = list->head; entry; entry = next) {
		next = entry->next;
		if (memcmp(entry->event.src_mac_addr, frame->address2, 6) == 0)
			wlanWepIcvErrRemove(prev, entry, list);
		/* the removed entry stays the previous one */
		prev = entry;
	}
}

void wlanWepIcvErrRefresh(const uint8_t *address, WepIcvErrList *list)
{
	WepIcvErrEntry *entry;

	if (wlanWepIcvErrRefreshOverride(address, list))
		return;

	for (entry = list->head; entry; entry = entry->next) {
		if (memcmp(entry->event.src_mac_addr, address, 6) == 0)
			entry->ageTicks = WEP_ICV_ERR_AGE_TICKS;
	}
}

/* A WEP frame from the transmitter failed the ICV check with keyIndex. */
void wlanWepIcvErrRecord(struct WlanConnection *connection, WlanWirelessFrame *frame,
			 uint8_t keyIndex, int isWep104, const uint8_t *key,
			 WepIcvErrList *list)
{
	WepIcvErrEntry *prev = NULL;
	WepIcvErrEntry *entry;

	if (wlanWepIcvErrRecordOverride(connection, frame, keyIndex, isWep104, key, list))
		return;

	for (entry = list->head; entry; entry = entry->next) {
		if (memcmp(entry->event.src_mac_addr, frame->address2, 6) == 0) {
			if (entry->event.wep_key_index == keyIndex) {
				entry->ageTicks = WEP_ICV_ERR_AGE_TICKS;
				return;
			}
			/* another key: record it afresh */
			wlanWepIcvErrRemove(prev, entry, list);
			break;
		}
		prev = entry;
	}

	entry = list->freeList;
	if (!entry)
		return;

	list->freeList = entry->next;
	entry->next = NULL;
	entry->ageTicks = WEP_ICV_ERR_AGE_TICKS;
	entry->reported = 0;
	entry->event.reason_code = 0;
	entry->event.wep_key_index = keyIndex;
	entry->event.wep_key_length = isWep104 ? 13 : 5;
	__aeabi_memcpy(entry->event.key, key, entry->event.wep_key_length);
	__aeabi_memcpy(entry->event.src_mac_addr, frame->address2, 6);

	if (list->head) {
		list->tail->next = entry;
		list->tail = entry;
	} else {
		list->head = entry;
		entry->next = NULL;
		list->tail = list->head;
	}

	wlanRomServices.postConnectionEvent(connection, WLAN_EVENT_WEP_ICV_ERR_PENDING);
}

/* One ageing tick. */
void wlanWepIcvErrAge(WepIcvErrList *list)
{
	WepIcvErrEntry *prev = NULL;
	WepIcvErrEntry *entry, *next;

	if (wlanWepIcvErrAgeOverride(list))
		return;

	for (entry = list->head; entry; entry = next) {
		next = entry->next;
		if (entry->ageTicks != 0) {
			entry->ageTicks--;
			if (entry->ageTicks == 0)
				wlanWepIcvErrRemove(prev, entry, list);
		}
		/* the removed entry stays the previous one */
		prev = entry;
	}
}

/* Sends the host the events it does not have yet. */
void wlanWepIcvErrReport(struct WlanConnection *connection, WepIcvErrList *list)
{
	WepIcvErrEntry *entry;

	for (entry = list->head; entry; entry = entry->next) {
		if (entry->reported == 0) {
			entry->reported = 1;
			/* the event and its padding byte */
			wlanRomServices.sendHostEvent(connection, EVENT_WEP_ICV_ERR,
						      &entry->event, 24);
		}
	}
}
