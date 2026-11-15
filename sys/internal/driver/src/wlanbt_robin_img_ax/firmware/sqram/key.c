/*
 * The buffers of the keys the MAC installed (0xc000824c .. 0xc0008304): two pools of twelve
 * 112-byte buffers in DTCM, each a buffer header and a cipher_key_t, and the transmit pool when
 * both are used up. wlanKeyPoolsInit() also registers the start-up stage that resets the
 * defragmentation. The file name is inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/btrom.h"
#include "robin/buffer.h"
#include "robin/defrag.h"
#include "robin/esa.h"
#include "robin/rom.h"
#include "robin/txring.h"
#include "robin/wlantask.h"

#define WLAN_KEY_POOLS		2
#define WLAN_KEY_POOL_BLOCKS	12

/* Marks of a key buffer in its header while it is in use. */
#define WLAN_KEY_BUFFER_CLASS	2
#define WLAN_KEY_BUFFER_TAG	1

static int wlanDefragInitStage(void);

static struct WlanInitStage wlanDefragStage __attribute__((section(".dtcm.rw")));		/* 0x04000100 */

static struct OsaPoolConfig wlanKeyPoolConfig[WLAN_KEY_POOLS] __attribute__((section(".dtcm.rw"))) = {
	{ .blockSize = sizeof(struct WlanKeyBuffer), .blockCount = WLAN_KEY_POOL_BLOCKS },
	{ .blockSize = sizeof(struct WlanKeyBuffer), .blockCount = WLAN_KEY_POOL_BLOCKS },
};											/* 0x04000f54 */

static struct OsaBufferPool wlanKeyPool[WLAN_KEY_POOLS] __attribute__((section(".dtcm.rw"))) = {
	{ .words = { 1, 0, 0 } },
	{ .words = { 1, 0, 0 } },
};											/* 0x04000f64 */

/* The memory of each pool: its blocks, each with the allocator's word before it. */
static uint8_t wlanKeyPoolMemory[WLAN_KEY_POOLS][WLAN_KEY_POOL_BLOCKS * (sizeof(struct WlanKeyBuffer) + 4)]
	__attribute__((section(".dtcm.bss"), aligned(4)));				/* 0x0400395c */
static uint8_t wlanKeyPoolControl[WLAN_KEY_POOLS][WLAN_KEY_POOL_BLOCKS * 4]
	__attribute__((section(".dtcm.bss"), aligned(4)));				/* 0x0400894c */

/* A cleared key buffer: from the first pool with one free, else from the transmit pool (0xc000824c). */
struct WlanKeyBuffer *wlanKeyBufferAlloc(void)
{
	OsaBuffer *buffer;

	buffer = osa_BufferAlloc(&wlanKeyPool[0], sizeof(struct WlanKeyBuffer), 0);
	if (buffer == NULL) {
		buffer = osa_BufferAlloc(&wlanKeyPool[1], sizeof(struct WlanKeyBuffer), 0);
		if (buffer == NULL) {
			buffer = wlanTxBufferAlloc(&wlanBlockPool28, 0, 0, 3);
			if (buffer == NULL)
				return NULL;
		}
	}
	buffer->allocatorTag = WLAN_KEY_BUFFER_TAG;
	buffer->packetClass = WLAN_KEY_BUFFER_CLASS;
	__aeabi_memclr4(((struct WlanKeyBuffer *)buffer)->keyData, 84);
	return (struct WlanKeyBuffer *)buffer;
}

/* Clears the key and gives its buffer back (0xc0008292). */
void wlanKeyBufferFree(struct WlanKeyBuffer *key)
{
	if (key == NULL)
		return;
	__aeabi_memclr4(key->keyData, 84);
	key->buffer.allocatorTag = 0;
	key->buffer.packetClass = 1;
	osa_BufferFree(&key->buffer);
}

/* 0xc00082b2 */
static int wlanDefragInitStage(void)
{
	wlanDefragInit();
	return 0;
}

/* Creates the two key pools and registers wlanDefragInitStage(); 1 when a pool cannot be made (0xc00082bc). */
int wlanKeyPoolsInit(void)
{
	if (osa_BlockPoolCreate(&wlanKeyPool[0], &wlanKeyPoolConfig[0], wlanKeyPoolControl[0],
				wlanKeyPoolMemory[0], osaPoolEntryCreate) != 0)
		return 1;
	if (osa_BlockPoolCreate(&wlanKeyPool[1], &wlanKeyPoolConfig[1], wlanKeyPoolControl[1],
				wlanKeyPoolMemory[1], osaPoolEntryCreate) != 0)
		return 1;
	wlanDefragStage.run = wlanDefragInitStage;
	wlanInitStageRegister(&wlanDefragStage);
	return 0;
}
