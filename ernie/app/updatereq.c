/*
 * The firmware update's requests: host request 0x11 takes the update's commands, which the
 * host sends in this order: 0x1185 the image's signature (the syscon's version, board and
 * build date, masked), 0x1183 the run mode, then for each segment 0x1180 the segment, 0x1181
 * its data and 0x1182 its programming, and 0x1184 the digest of every segment programmed. Each
 * 0x80-byte block of a segment is AES-128-CBC decrypted with the updater key (the PS Dev
 * Wiki's "Ernie Updater AES128CBC Key") into the flash module's buffer, the IV chained from
 * the block's last ciphertext block, and its ciphertext summed for 0x1182's check. Run modes
 * 0x9a54 and 0x152e start the downloader (updaterStartDownloader, from the system mode's
 * transition 0x70), which updates the application itself.
 */
#pragma DI
#pragma EI

#include <string.h>

#include "ernie.h"
#include "aes.h"
#include "command.h"
#include "config.h"
#include "downloader.h"
#include "handshake.h"
#include "host.h"
#include "kernel.h"
#include "nvs.h"
#include "serial.h"
#include "sha1.h"
#include "sleepfactor.h"
#include "state.h"
#include "task.h"
#include "updatereq.h"
#include "version.h"


/* the flash self-programming library (0xf2b2-0xf6f5, not written yet) */
extern void FSL_Open(void);
extern unsigned char FSL_Init(unsigned char *descriptor);
extern unsigned char FSL_ModeCheck(void);
extern unsigned char FSL_GetSecurityFlags(unsigned char *flags);
extern unsigned char fslSetSecurityFlag(void);
extern void FSL_Close(void);
extern unsigned char fslDataBuffer[];		/* the library's descriptor */

/* the NVS byte of the downloader's flags (version.c's setDlFlags) */
#define UPDATER_NVS_DL_FLAGS	0x2d
#define UPDATER_NVS_AREA	1

static unsigned char updaterFlashPrepare(void);
static char updaterSegmentAllowed(unsigned char segment);
static char updaterSegmentReceived(unsigned char segment);
static void updaterSegmentSetReceived(unsigned char segment);
static unsigned char updaterCheckSignature(unsigned char *data);
static unsigned char updaterSetRunMode(unsigned int mode);
static unsigned char updaterSetSegment(unsigned char segment);
static unsigned char updaterExecProgramming(unsigned long sum);
static unsigned char updaterExecFinalize(unsigned char *data);
static unsigned char updaterWriteDlFlags(unsigned char flags);

/* the updater key and IV (the PS Dev Wiki's "Ernie Updater AES128CBC Key") */
const unsigned char updaterCryptoKey[16] = {
	0xea, 0xe4, 0x3a, 0x1c, 0x48, 0xcd, 0x32, 0xa5,
	0x65, 0xe2, 0xca, 0x7d, 0x8f, 0x90, 0x18, 0xdc,
};

const unsigned char updaterCryptoIv[AES_BLOCK_SIZE] = {
	0xc9, 0xd9, 0x61, 0x9c, 0xa1, 0x51, 0x34, 0x2d,
	0x04, 0x60, 0x2e, 0xcf, 0x0b, 0x8d, 0x6e, 0x33,
};

/* 0x1185's signature is the syscon's identity XORed with this */
const unsigned char updaterSignatureMask[24] = {
	0x46, 0xb5, 0x32, 0xe3, 0xf0, 0x12, 0xe6, 0x63,
	0xc0, 0x69, 0x4e, 0xca, 0x7c, 0x8c, 0x58, 0xb7,
	0x05, 0x02, 0x27, 0x9c, 0x0a, 0x91, 0xda, 0x21,
};

/* the segments run mode 0xc5e7 may program: 4-47 (0x1000-0xbfff) and 213-237 (0x35400-0x3b7ff) */
const unsigned char updaterSegments[UPDATER_SEGMENTS / 8] = {
	0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xe0, 0xff, 0xff, 0x3f, 0x00, 0x00,
};

void (*updaterDownloader)(void);		/* the downloader's entry */
struct HostRequest updaterRequest;
unsigned char updaterRequestAnswer[4];
unsigned char updaterRequestData[0x80];
struct Updater updater;

void updaterRequestInit(void)
{
	updaterRequest.id = 0x11;
	updaterRequest.data = updaterRequestData;
	updaterRequest.answer = updaterRequestAnswer;
	updaterRequest.size = sizeof(updaterRequestData);
	updaterRequest.notify = updaterRequestNotify;
	updaterRequest.state = HOST_REQUEST_RECEIVE;
	memset(&updater, 0, sizeof(updater));
	updater.output = (unsigned char *)0xFF706;
	updaterDownloader = (void (*)(void))DOWNLOADER_ENTRY;
}

/* 0x1100 (sceSysconGetErnieDLVersion) */
unsigned char hostCommandErnieDlVersion(unsigned char *out, unsigned char *length)
{
	getErnieDlVersion(out);
	*length = 4;
	return 0;
}

/* 0x1101: the update's progress in percent */
unsigned char hostCommandUpdaterProgress(unsigned char *out, unsigned char *length)
{
	if (updater.state == 0) {
		*length = 0;
		return 0x3e;
	}
	out[0] = updater.progress;
	out[1] = 0;
	out[2] = 0;
	out[3] = 0;
	*length = 4;
	return 0;
}

void updaterRequestNotify(void)
{
	switch (updaterRequest.event) {
	case HOST_EVENT_COMMAND:
		hostRequestSetState(&updaterRequest, HOST_REQUEST_BUSY);
		task2SetFlag(3, 0x0040);
		break;
	case HOST_EVENT_ANSWERED:
		updaterRequest.state = HOST_REQUEST_RECEIVE;
		if (updaterRequest.command == 0x1183 && updater.state == 0x13)
			hostIntCancel();
		break;
	}
}

/*
 * starts the downloader, with the host link's state in DOWNLOADER_START and the RAM its
 * version expects cleared; it does not return unless the flash library fails
 */
void updaterStartDownloader(void)
{
	unsigned char snapshot;
	unsigned long sleepFactor;
	unsigned long version;
	unsigned long hardware;
	unsigned char board;

	serialClose1();
	serialClose2();
	serialOpen3(3);
	if (configPrepareUpdate())
		;
	snapshot = sleepFactorSnapshot();
	sleepFactor = sleepFactorGet();
	getErnieDlVersion((unsigned char *)&version);
	version &= 0xffffff00;
	version = ((version << 24) & 0xff000000) | ((version << 8) & 0x00ff0000) |
		  ((version >> 8) & 0x0000ff00) | ((version >> 24) & 0x000000ff);
	getHardwareInfo((unsigned char *)&hardware);
	board = (unsigned char)((hardware >> 16) & 0xff);
	dis_dsp();
	DI();
	if (updaterFlashPrepare() == 0) {
		DOWNLOADER_START->snapshot = snapshot;
		memcpy(DOWNLOADER_START->sleepFactor, &sleepFactor, 4);
		DOWNLOADER_START->frameSize = hostLink.frameSize;
		if (version <= 0x000102ff)
			DOWNLOADER_START->link.size = hostLink.config.size;
		else
			DOWNLOADER_START->link.config = hostLink.config;
		if (version >= 0x00002000 && version <= 0x00002600)
			memset(DOWNLOADER_RAM_20, 0, DOWNLOADER_RAM_20_SIZE);
		if (board == 0x40 && (version == 0x00010000 || version == 0x00010800)) {
			memset(DOWNLOADER_RAM_40_BYTE, 0, 1);
			memset(DOWNLOADER_RAM_40_WORD, 0, 2);
		}
		if (board == 0x41 && version == 0x00010100) {
			memset(DOWNLOADER_RAM_41_BYTE, 0, 1);
			memset(DOWNLOADER_RAM_41_WORD, 0, 2);
		}
		(*updaterDownloader)();
	}
	ena_dsp();
	EI();
}

/* answers the request with no data */
void updaterRequestAnswerOk(void)
{
	hostRequestAnswer(&updaterRequest, HOST_REQUEST_ANSWER, 0, 0);
}

/* opens the flash library for the downloader; 0 when it may start */
static unsigned char updaterFlashPrepare(void)
{
	unsigned char result;
	unsigned int flags;

	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	if (result == 0) {
		result = FSL_ModeCheck();
		if (result == 0) {
			result = FSL_GetSecurityFlags((unsigned char *)&flags);
			if (result == 0 || (flags & 0x0e) == 0x0e)
				result = fslSetSecurityFlag();
		}
	}
	FSL_Close();
	return result;
}

/* event flag 3's 0x0040: the host's update command came in */
void updaterRequestEvent(FLGPTN ptn)
{
	unsigned char result;
	unsigned long sum;

	if (!(ptn & 0x0040))
		return;
	if (!handshakeUnlocked11()) {
		hostRequestAnswer(&updaterRequest, HOST_REQUEST_UNKNOWN, 0x3f, 0);
		handshakeRefuse();
		return;
	}
	result = 0x3b;
	switch (updaterRequest.command) {
	case 0x1185:
		if (updaterRequest.length == 0x18)
			result = updaterCheckSignature(updaterRequestData);
		hostRequestAnswer(&updaterRequest, HOST_REQUEST_ANSWER, result, 0);
		break;
	case 0x1183:
		if (updaterRequest.length == 2)
			result = updaterSetRunMode(updaterRequestData[0] |
						   (unsigned int)updaterRequestData[1] << 8);
		if (result != 0x80)
			hostRequestAnswer(&updaterRequest, HOST_REQUEST_ANSWER, result, 0);
		break;
	case 0x1180:
		if (updaterRequest.length == 1)
			result = updaterSetSegment(updaterRequestData[0]);
		hostRequestAnswer(&updaterRequest, HOST_REQUEST_ANSWER, result, 0);
		break;
	case 0x1181:
		if (updaterRequest.length >= 4)
			result = updaterDecryptData(updaterRequestData[0] |
						    (unsigned int)updaterRequestData[1] << 8,
						    updaterRequestData[2], &updaterRequestData[3]);
		hostRequestAnswer(&updaterRequest, HOST_REQUEST_ANSWER, result, 0);
		break;
	case 0x1182:
		if (updaterRequest.length == 4) {
			((unsigned char __near *)&sum)[0] = updaterRequestData[0];
			((unsigned char __near *)&sum)[1] = updaterRequestData[1];
			((unsigned char __near *)&sum)[2] = updaterRequestData[2];
			((unsigned char __near *)&sum)[3] = updaterRequestData[3];
			result = updaterExecProgramming(sum);
		}
		hostRequestAnswer(&updaterRequest, HOST_REQUEST_ANSWER, result, 0);
		break;
	case 0x1184:
		if (updaterRequest.length == SHA1HashSize) {
			result = updaterExecFinalize(updaterRequestData);
			if (result == 0x81) {
				task2SetFlag(3, 0x0040);
				break;
			}
		}
		hostRequestAnswer(&updaterRequest, HOST_REQUEST_ANSWER, result, 0);
		break;
	default:
		updaterRequest.state = HOST_REQUEST_ERROR;
		break;
	}
}

/* expands the updater key, copies its IV and resets the image's digest, for a new download */
unsigned char updaterDecryptInit(void)
{
	unsigned char result;

	memset(updater.received, 0, sizeof(updater.received));
	memset(updater.expected, 0, sizeof(updater.expected));
	result = (unsigned char)SHA1Reset((SHA1Context __near *)&updater.sha,
					  (unsigned char __near *)updater.shaBlock,
					  (unsigned long __near *)updater.shaW);
	if (result != 0)
		return result;
	updater.key.w = updater.roundKeys;
	updater.key.nb = 4;
	updater.key.nk = 4;
	result = (unsigned char)aesKeyExpansion((const unsigned long *)updaterCryptoKey,
				 (struct AesKey __near *)&updater.key);
	memcpy(updater.iv, updaterCryptoIv, AES_BLOCK_SIZE);
	updater.fill = 0;
	updater.progress = 0;
	updater.segmentCount = 0;
	updater.segmentDone = 0;
	updater.output = (unsigned char *)0xFF706;
	return result;
}

/* restarts the update: the IV back to the key's, the digest reset, the key schedule kept */
unsigned char updaterDecryptReset(void)
{
	unsigned char result = 0;

	updater.fill = 0;
	updater.progress = 0;
	updater.segmentCount = 0;
	updater.segmentDone = 0;
	memset(updater.received, 0, sizeof(updater.received));
	memset(updater.expected, 0, sizeof(updater.expected));
	memcpy(updater.iv, updaterCryptoIv, AES_BLOCK_SIZE);
	result = (unsigned char)SHA1Reset((SHA1Context __near *)&updater.sha,
					  (unsigned char __near *)updater.shaBlock,
					  (unsigned long __near *)updater.shaW);
	return result;
}

/* whether run mode 0xc5e7 may program the segment */
static char updaterSegmentAllowed(unsigned char segment)
{
	return (updaterSegments[segment >> 3] & (1 << (segment & 7))) != 0 ? 1 : 0;
}

/* whether the segment was programmed */
static char updaterSegmentReceived(unsigned char segment)
{
	return (updater.received[segment >> 3] & (1 << (segment & 7))) != 0 ? 1 : 0;
}

static void updaterSegmentSetReceived(unsigned char segment)
{
	updater.received[segment >> 3] |= (unsigned char)1 << (segment & 7);
}

/*
 * 0x1185: the signature, the syscon's version, its hardware info with 0xff for the first
 * byte, the build date in BCD (YYYYMMDDhhmm) and ten zeros, XORed with updaterSignatureMask
 */
static unsigned char updaterCheckSignature(unsigned char *data)
{
	unsigned char i;
	unsigned char signature[24];

	switch (updater.state) {
	case 0:
	case 1:
		break;
	default:
		return 1;
	}
	getErnieVersion(&signature[0]);
	getHardwareInfo(&signature[4]);
	signature[4] = 0xff;
	for (i = 0; i < 12; i += 2)
		signature[(i >> 1) + 8] =
			(unsigned char)((appTimeStamp[timeStampDigits[i]] - '0') << 4) |
			((appTimeStamp[timeStampDigits[i + 1]] - '0') & 0x0f);
	memset(&signature[14], 0, 10);
	for (i = 0; i < 24; i++)
		signature[i] = signature[i] ^ updaterSignatureMask[i];
	if (memcmp(data, signature, 24) == 0) {
		updater.state = 1;
		return 0;
	}
	return 0x21;
}

/*
 * 0x1183: the run mode. 0x9a54 and 0x152e start the downloader (bit 1 of the NVS flags, its
 * own update), 0xc5e7 and 0x3665 take segments (0x3665 through the flash module), 0x72ba
 * ends the update; 0x80 answers later (the system mode's transition answers)
 */
static unsigned char updaterSetRunMode(unsigned int mode)
{
	unsigned char result;
	unsigned char flags;

	result = nvsRead(UPDATER_NVS_DL_FLAGS, 1, &flags, UPDATER_NVS_AREA);
	if (result != 0)
		return 0x22;
	switch (mode) {
	case 0x9a54:
		if ((flags & 0x01) == 0)
			return 0x23;
		flags |= 0x02;
		result = updaterWriteDlFlags(flags);
		if (result != 0)
			return 1;
		updater.state = 0x13;
		stateTransition(0x70);
		return 0x80;
	case 0xc5e7:
		switch (updater.state) {
		case 0:
		case 1:
			if (updaterDecryptInit() != 0)
				return 1;
			result = updaterWriteDlFlags(flags & 0xfe);
			if (result == 0) {
				updater.state = 0x10;
				stateTransition(0x71);
				return 0x80;
			}
			return 0x22;
		case 0x10:
		case 0x12:
			result = updaterDecryptInit();
			return 0;
		default:
			return 1;
		}
	case 0x72ba:
		switch (updater.state) {
		case 0x22:
			result = configMergeUpdate();
			break;
		case 0:
		case 1:
		default:
			return 0;
		case 0x10:
		case 0x12:
		case 0x20:
			break;
		}
		updater.state = 0;
		stateTransition(0x72);
		return 0x80;
	case 0x152e:
		if (updater.state == 1) {
			if ((flags & 0x01) == 0)
				return 0x23;
			result = updaterWriteDlFlags(flags & 0xfd);
			if (result == 0) {
				updater.state = 0x13;
				stateTransition(0x70);
				return 0x80;
			}
			return 0x22;
		}
		return 0x21;
	case 0x3665:
		switch (updater.state) {
		case 0:
		case 1:
			result = updaterDecryptInit();
			if (result == 0) {
				updater.state = 0x20;
				configClose();
				stateTransition(0x71);
				return 0x80;
			}
			return 1;
		case 0x20:
			result = updaterDecryptInit();
			return 0;
		default:
			return 1;
		}
	default:
		return 1;
	}
}

/* 0x1180: the segment whose data comes next (in run mode 0x3665 the flash module's) */
static unsigned char updaterSetSegment(unsigned char segment)
{
	switch (updater.state) {
	case 0x10:
		if (!updaterSegmentAllowed(segment))
			return 1;
		break;
	case 0x20:
		segment = configUpdateSegment();
		break;
	default:
		return 1;
	}
	updater.segment = segment;
	updater.fill = 0;
	updater.sum = 0;
	memset(updater.block, 0, UPDATER_BLOCK_SIZE);
	memset(updater.output, 0, UPDATER_SEGMENT_SIZE);
	return 0;
}

/*
 * 0x1181: takes remaining bytes of the segment at data (its position in the segment is
 * length): fills the block, and for each full block AES-128-CBC decrypts it to the output,
 * chains the IV from its last ciphertext block and adds its ciphertext to the sum. 1 out of
 * phase, 0x3c too long.
 */
unsigned char updaterDecryptData(unsigned int length, unsigned char remaining,
				 unsigned char *data)
{
	unsigned char chunk;
	unsigned char result;
	unsigned char i;
	unsigned char *sum;
	unsigned int offset;

	result = 0;
	switch (updater.state) {
	case 0x10:
	case 0x20:
		break;
	default:
		return 1;
	}
	if (length >= UPDATER_SEGMENT_SIZE)
		return 0x3c;
	while (remaining != 0 && result == 0) {
		chunk = (unsigned char)(UPDATER_BLOCK_SIZE - updater.fill) > remaining ?
			remaining : UPDATER_BLOCK_SIZE - updater.fill;
		memcpy(&updater.block[updater.fill], data, chunk);
		updater.fill += chunk;
		if (updater.fill == UPDATER_BLOCK_SIZE) {
			offset = length & 0xff80;
			result = (unsigned char)aesCbcDecrypt(
				(const unsigned char __near *)updater.block,
				(unsigned char __near *)(updater.output + offset),
				(struct AesKey __near *)&updater.key, updater.iv, UPDATER_BLOCK_SIZE);
			memcpy(updater.iv, &updater.block[0x70], AES_BLOCK_SIZE);
			updater.fill = 0;
			sum = updater.block;
			for (i = 0; i < UPDATER_BLOCK_SIZE; i++)
				updater.sum += *sum++;
		}
		data += chunk;
		remaining -= chunk;
	}
	return result;
}

/* 0x1182: programs the segment, when sum is the complement of its ciphertext's sum */
static unsigned char updaterExecProgramming(unsigned long sum)
{
	unsigned char result;

	switch (updater.state) {
	case 0x10:
	case 0x20:
		break;
	default:
		return 0x3e;
	}
	if (~updater.sum != sum)
		return 3;
	result = configFlashWrite((unsigned long)updater.segment << 10);
	updaterSegmentSetReceived(updater.segment);
	return result;
}

/*
 * 0x1184: hashes the programmed segments, one each time (0x81: event flag 3's 0x0040 runs it
 * again), then compares the digest with data's and sets bit 0 of the NVS flags; 8 when the
 * digest differs
 */
static unsigned char updaterExecFinalize(unsigned char *data)
{
	unsigned char segment;
	unsigned char result;
	unsigned char flags;		/* first counts the segments (one slot in Sony's frame) */

	switch (updater.state) {
	case 0x10:
	case 0x20:
		updater.segment = 0;
		updater.state++;
		for (flags = 0; flags < 0xff; flags++)
			if (updaterSegmentReceived(flags))
				updater.segmentCount++;
		break;
	case 0x11:
		break;
	case 0x12:
	case 0x22:
		return 6;
	default:
		return 1;
	}
	while (updater.segment < 0xff) {
		segment = updater.segment++;
		if (updaterSegmentReceived(segment)) {
			result = (unsigned char)SHA1Input((SHA1Context __near *)&updater.sha,
					(const unsigned char *)((unsigned long)segment << 10),
					UPDATER_SEGMENT_SIZE);
			if (result != 0)
				return 1;
			updater.segmentDone++;
			updater.progress = (unsigned char)((unsigned int)updater.segmentDone * 100 /
					   (unsigned int)updater.segmentCount);
			break;
		}
	}
	if (updater.segmentDone == updater.segmentCount) {
		updater.state++;
		result = (unsigned char)SHA1Result((SHA1Context __near *)&updater.sha,
						   (unsigned char __near *)updater.hash);
		if (result != 0)
			return 1;
		result = (unsigned char)memcmp(data, updater.hash, SHA1HashSize);
		if (updater.state == 0x12 && result == 0) {
			result = nvsRead(UPDATER_NVS_DL_FLAGS, 1, &flags, UPDATER_NVS_AREA);
			if (result != 0)
				return 0x22;
			flags |= 0x01;
			result = updaterWriteDlFlags(flags);
			if (result != 0)
				return result;
		}
		updaterDecryptReset();
		if (result == 0)
			return 0;
		return 8;
	}
	return 0x81;
}

/* writes the downloader's flags to the NVS and to version.c's copy */
static unsigned char updaterWriteDlFlags(unsigned char flags)
{
	unsigned char result;

	result = nvsWrite(UPDATER_NVS_DL_FLAGS, 1, &flags, UPDATER_NVS_AREA);
	if (result != 0)
		return 0x22;
	setDlFlags(flags);
	return 0;
}
