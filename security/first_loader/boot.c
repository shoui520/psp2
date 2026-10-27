/*
 * first_loader's program, run at 0x5c000 after reset copies it there: it brings up the
 * Cmep's keyrings, reads second_loader from the eMMC (Sdif) or the SD card, decrypts and
 * checks it, and starts it. It uses the Cmep control block (0xe0000000), the keyring
 * controller (0xe0030000), the bignum unit (0xe0040000), Bigmac (0xe0050000), Sdif
 * (0xe0b00000), GPIO 0 (0xe20a0000) and Pervasive (0xe3100000).
 *
 * The retail C reconstruction covers the observed boot paths and links with
 * the reset code. Its instruction-level match to the original is still under
 * review.
 */

#ifndef FL_BUILD_DATE
#error "FL_BUILD_DATE (build, YYYYMMDD in hex) not set"
#endif

/* code that differs between the builds: FL_SINCE(0x20110616) */
#define FL_SINCE(date)	(FL_BUILD_DATE >= (date))

/* GCC 4.5 predates C11; retain the shared headers' layout checks in C99. */
#define FL_JOIN_INNER(a, b) a##b
#define FL_JOIN(a, b) FL_JOIN_INNER(a, b)
#define _Static_assert(condition, message) \
	typedef char FL_JOIN(fl_static_assert_, __LINE__)[(condition) ? 1 : -1]
#include <lowio/pervasive_regs.h>
#include <cmep_regs.h>
#include <sdif_regs.h>
#include <stddef.h>
#undef _Static_assert
#undef FL_JOIN
#undef FL_JOIN_INNER

struct SecondLoaderHeader {
	unsigned int magic;
	unsigned int imageSize;
	unsigned int firstPartSize;
	unsigned int secondPartSize;
	unsigned int decodedSize;
	unsigned short keyIndex;
	unsigned short signingKeyIndex;
	unsigned char signedHeaderData[0x28];
};

struct BigmacRegisters {
	volatile unsigned int input;
	volatile unsigned int output;
	volatile unsigned int size;
	volatile unsigned int mode;
	volatile unsigned int keySlot;
	volatile unsigned int parameter;
	unsigned int reserved0;
	volatile unsigned int start;
	unsigned int reserved1;
	volatile unsigned int status;
	unsigned char reserved2[0x200 - 0x28];
	volatile unsigned int key[8];
};

struct BignumRegisters {
	volatile unsigned int work[66];
	volatile unsigned int input[66];
	unsigned char reserved0[0x400 - 0x210];
	volatile unsigned int modulus[66];
	volatile unsigned int result[66];
	unsigned char reserved1[0x800 - 0x610];
	volatile unsigned int command;
	volatile unsigned int status;
	volatile unsigned int exponent;
};

struct KeyPolicy {
	unsigned int bank2;
	unsigned int bank6;
	unsigned int reserved[2];
	unsigned char bank0[16];
	unsigned char bank1[16];
	unsigned char bank3[32];
	unsigned char bank4[16];
	unsigned char bank5[16];
	unsigned char bank7[32];
};

struct KeyringRegisters {
	volatile unsigned int bootParameters[8];
	volatile unsigned int select;
	volatile unsigned int command;
	volatile unsigned int probeSelect;
	volatile unsigned int probeStatus;
};

struct GpioRegisters {
	volatile unsigned int direction;
	volatile unsigned int input;
	volatile unsigned int set;
	volatile unsigned int clear;
};

#define BIGMAC ((struct BigmacRegisters *)0xe0050000UL)
#define BIGNUM ((struct BignumRegisters *)0xe0040000UL)
#define BIGNUM_ACK (*(volatile unsigned int *)0xe0020020UL)
#define KEYRING ((struct KeyringRegisters *)0xe0030000UL)
#define KEYRING_COMMAND (KEYRING->command)
#define CMEP ((SceCmepMailboxRegisters *)CMEP_MAILBOX_BASE)
#define GPIO ((struct GpioRegisters *)0xe20a0000UL)
#define BOOT_FUSE_WORDS ((volatile unsigned int *)0xe0062020UL)
#define CMEP_RESET_FLAGS (*(volatile unsigned int *)0xe0020000UL)
#define BOOT_SECURITY_COMMAND (*(volatile unsigned int *)0xe0070008UL)
#define BOOT_SECURITY_START (*(volatile unsigned int *)0xe0070000UL)
#define BIGMAC_RANDOM (*(volatile unsigned int *)0xe005003cUL)
#define BOOT_FUSE_FLAGS (*(volatile unsigned int *)0xe0064060UL)
#define BOOT_BRIDGE_STATUS (*(volatile unsigned int *)0x30000118UL)
#define BOOT_BRIDGE_CONTROL (*(volatile unsigned int *)0x30000208UL)
#define BIGMAC_SLOT_OUTPUT 0x10000000U
#define BIGMAC_INLINE_KEY  0x00000080U
#define BIGMAC_BUSY        0x00000001U
#define BIGMAC_ERROR_MASK  0x00078000U
#define BIGNUM_BUSY        0x80000000U
#define BIGNUM_ERROR       0x02000000U
#define BIGNUM_NEED_INPUT  0x04000000U
#define BIGNUM_INPUT_READY 0x08000000U

static unsigned char externalBootKey[32] __attribute__((section(".fl.externalBootKey"))) = {
	0xf4, 0x77, 0x16, 0xe6, 0xc5, 0x64, 0x9f, 0xd6,
	0x48, 0x53, 0x8f, 0xd9, 0x77, 0x3d, 0x12, 0xd1,
	0x22, 0x9e, 0x11, 0x87, 0x37, 0xb1, 0xd7, 0x82,
	0xd6, 0xa8, 0x0c, 0xdb, 0x72, 0xe4, 0xb9, 0xc3,
};

struct BootSector {
	unsigned char header[0x30];
	unsigned char firstBlock[4];
	unsigned char blockCount[4];
};

struct EmmcCard {
	unsigned int controllerAddress;
	unsigned int flags;
	unsigned char cid[16];
	unsigned char csd[16];
	unsigned char cardRegisters[0x70];
	unsigned char extendedCsd[512];
	unsigned char csdStructure;
	unsigned char specificationVersion;
	unsigned char reservedVersion[2];
	unsigned int accessTime;
	unsigned int accessClocks;
	unsigned int maximumClock;
	unsigned short commandClasses;
	unsigned short reservedClasses;
	unsigned int blockCount;
	unsigned char readBlockLength;
	unsigned char readBlockPartial;
	unsigned char writeBlockMisalign;
	unsigned char readBlockMisalign;
	unsigned char readToWriteFactor;
	unsigned char writeBlockLength;
	unsigned char writeBlockPartial;
	unsigned char reservedWrite;
	unsigned int sectorCount;
	unsigned int highSpeedClock;
};

typedef char checkEmmcCardSize[sizeof(struct EmmcCard) == 0x2c0 ? 1 : -1];

struct SdifCard {
	unsigned int controllerAddress;
	unsigned int flags;
	unsigned char cid[16];
	unsigned char csd[16];
	unsigned char cardRegisters[0xb0];
	unsigned char scr[8];
	unsigned char csdStructure;
	unsigned char reservedCsd[3];
	unsigned int maximumClock;
	unsigned short commandClasses;
	unsigned short reservedClasses;
	unsigned int sectorCount;
	unsigned char scrStructure;
	unsigned char sdSpecification;
	unsigned char dataStatusAfterErase;
	unsigned char securitySupport;
	unsigned char busWidthSupport;
	unsigned char reservedScr;
	unsigned short commandSupport;
};

typedef char checkSdifCardSize[sizeof(struct SdifCard) == 0xf8 ? 1 : -1];
typedef char checkSdifCardCapacityOffset[
	__builtin_offsetof(struct SdifCard, sectorCount) == 0xec ? 1 : -1];

union FirstLoaderCard {
	struct EmmcCard emmc;
	struct SdifCard sd;
};

struct FirstLoaderSdif {
	unsigned int flags;
	unsigned int baseClock;
	unsigned int maximumClock;
	unsigned short relativeAddress;
	unsigned char port;
	unsigned char commandComplete;
	unsigned int activeRequest;
	unsigned int activeTransfer;
	unsigned int registersAddress;
};

struct FirstLoaderSdifCommand {
	unsigned int size;
	unsigned int flags;
	unsigned int command;
	unsigned int argument;
	unsigned int response[4];
	unsigned int bufferAddress;
	unsigned int blockSize;
	unsigned int blockCount;
	int error;
};

static struct FirstLoaderSdif sdif;
static unsigned int sdifPowerState;

void delayMicroseconds(unsigned int microseconds);

static volatile unsigned int bootHardwareMode;

static union FirstLoaderCard firstLoaderCard;

static const unsigned int emmcTimeUnits[8] __attribute__((section(".fl.emmcTimeUnits"))) = {
	1, 10, 100, 1000, 10000, 100000, 1000000, 10000000,
};
static const unsigned char emmcTimeMultipliers[16] __attribute__((section(".fl.emmcTimeMultipliers"))) = {
	0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80,
};
static const unsigned int emmcClockUnits[8] __attribute__((section(".fl.emmcClockUnits"))) = {
	10000, 100000, 1000000, 10000000, 0, 0, 0, 0,
};
static const unsigned char emmcClockMultipliers[16] __attribute__((section(".fl.emmcClockMultipliers"))) = {
	0, 10, 12, 13, 15, 20, 26, 30, 35, 40, 45, 52, 55, 60, 70, 80,
};
/* The ROM keeps a separate pair for SD CSD clock decoding. */
static const unsigned int sdifClockUnits[8] __attribute__((section(".fl.sdifClockUnits"))) = {
	10000, 100000, 1000000, 10000000, 0, 0, 0, 0,
};
static const unsigned char sdifClockMultipliers[16] __attribute__((section(".fl.sdifClockMultipliers"))) = {
	0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80,
};

static unsigned char slot8Seed[32] __attribute__((section(".fl.slot8Seed"))) = {
	0x1e, 0xb2, 0x56, 0x43, 0x23, 0x4e, 0xf6, 0xef,
	0x38, 0x0f, 0x9e, 0x57, 0xa2, 0x16, 0xfa, 0x22,
	0xd8, 0x35, 0xff, 0x05, 0x08, 0xbe, 0xbb, 0xe9,
	0x76, 0x76, 0xfa, 0x47, 0x8c, 0x29, 0xc2, 0x1d,
};
static unsigned char slot9Seed[32] __attribute__((section(".fl.slot9Seed"))) = {
	0x50, 0x97, 0x62, 0x38, 0xee, 0xd2, 0x32, 0x34,
	0xf9, 0xc7, 0x46, 0xcc, 0xb0, 0x7d, 0x6d, 0x71,
	0x3f, 0x5b, 0xa6, 0x7b, 0xc8, 0xc0, 0x97, 0xda,
	0xe5, 0x2d, 0x85, 0x12, 0x08, 0x46, 0x83, 0xe7,
};
static unsigned char slot8Iv[16] __attribute__((section(".fl.slot8Iv"))) = {
	0xfd, 0x4f, 0xa8, 0xfa, 0x4f, 0xe7, 0x94, 0x30,
	0xa0, 0xca, 0x30, 0x5c, 0x88, 0xe5, 0x24, 0xdd,
};
static unsigned char slot9Iv[16] __attribute__((section(".fl.slot9Iv"))) = {
	0x98, 0x6d, 0x5e, 0xe1, 0xe7, 0x75, 0x9f, 0x4f,
	0x52, 0xdb, 0x43, 0xf3, 0x3f, 0xa5, 0x96, 0x0a,
};

/* The image payload IV is erased immediately after the Bigmac operation. */
static unsigned char payloadIv[16] __attribute__((section(".fl.payloadIv"))) = {
	0xaf, 0x5f, 0x2c, 0xb0, 0x4a, 0xc1, 0x75, 0x1a,
	0xbf, 0x51, 0xce, 0xf1, 0xc8, 0x09, 0x62, 0x10,
};

static unsigned char keyIvs[6][16] __attribute__((section(".fl.keyIvs"))) = {
	{ 0xb0, 0x7f, 0xeb, 0x18, 0xe4, 0x22, 0x16, 0x7a, 0xec, 0xdb, 0x60, 0xa8, 0xee, 0x0c, 0xa4, 0x27 },
	{ 0x69, 0x42, 0x55, 0x51, 0xe9, 0xae, 0x39, 0x45, 0xf3, 0x68, 0x73, 0xf1, 0x10, 0xfd, 0xa6, 0xfb },
	{ 0x1c, 0x48, 0xb1, 0xc8, 0xa5, 0x99, 0xcd, 0x59, 0xc7, 0xae, 0xcf, 0x8e, 0x46, 0x98, 0x30, 0xbe },
	{ 0x7c, 0x59, 0x8f, 0xb4, 0x5b, 0xcf, 0x00, 0xa9, 0xf4, 0x9e, 0x5a, 0xfa, 0xd3, 0x55, 0x47, 0x54 },
	{ 0x98, 0xe3, 0xf5, 0x08, 0xd1, 0x6e, 0xf7, 0x8c, 0x8f, 0xae, 0x96, 0xf4, 0x16, 0x8f, 0x20, 0xa6 },
	{ 0x76, 0x64, 0x10, 0xae, 0x51, 0x09, 0xf2, 0x20, 0x1d, 0xda, 0xe4, 0x1f, 0xb0, 0x7f, 0x6e, 0xf6 },
};
static unsigned char derivedKeyIvs[5][16] __attribute__((section(".fl.derivedKeyIvs"))) = {
	{ 0x09, 0x62, 0x93, 0x9d, 0xc5, 0xbe, 0x56, 0x0f, 0x52, 0xf6, 0xbb, 0x45, 0xb3, 0x4c, 0xe0, 0x97 },
	{ 0xd4, 0x6c, 0xf8, 0x2f, 0xc8, 0xba, 0x5d, 0x5b, 0x64, 0x36, 0xf9, 0xf7, 0xe4, 0xe2, 0x04, 0x34 },
	{ 0xea, 0x57, 0x4c, 0x6c, 0xb6, 0xb1, 0x15, 0x05, 0xd5, 0xda, 0x35, 0x44, 0x6d, 0x18, 0x78, 0x08 },
	{ 0x8f, 0x62, 0xaa, 0xed, 0xbd, 0x4e, 0x02, 0x5b, 0xe6, 0xe8, 0xff, 0x8e, 0x7d, 0x65, 0x47, 0x04 },
	{ 0xc8, 0x98, 0x61, 0x0b, 0xf8, 0xe6, 0xcc, 0xc1, 0x2c, 0x9b, 0xa4, 0x84, 0x14, 0xcc, 0x2b, 0xcf },
};
static unsigned char digestIv[16] __attribute__((section(".fl.digestIv"))) = {
	0xee, 0xb5, 0xec, 0x5c, 0xcf, 0x1c, 0xf4, 0x31,
	0x13, 0xf5, 0x21, 0x3a, 0xd5, 0xb8, 0xd6, 0xda,
};
static unsigned char expectedDigest[32];
static unsigned char actualDigest[32];

static unsigned char signatureSeed[32] __attribute__((section(".fl.signatureSeed"))) = {
	0x4a, 0x41, 0xde, 0xde, 0xff, 0xba, 0xc0, 0x4f,
	0x52, 0x60, 0x2d, 0x00, 0x34, 0xda, 0xd6, 0x66,
	0x16, 0x4e, 0x3c, 0xf1, 0xfc, 0x23, 0xbe, 0x4f,
	0x4d, 0x0f, 0x84, 0x71, 0xa7, 0xe6, 0x05, 0x63,
};
static const unsigned int signatureExponent[4] __attribute__((section(".fl.signatureExponent"))) = { 0x10001, 0, 0, 0 };
static const unsigned char signatureDigestPrefix[18] __attribute__((section(".fl.signatureDigestPrefix"), aligned(1))) = {
	0x30, 0x30, 0x30, 0x0c, 0x06, 0x08, 0x2a, 0x86, 0x48,
	0x86, 0xf7, 0x0d, 0x02, 0x09, 0x05, 0x00, 0x04, 0x20,
};
static const unsigned int fallbackModulus[64] __attribute__((section(".fl.fallbackModulus"))) = {
	0xe9d7d33bU, 0x59f3178aU, 0xf93a9ef1U, 0xe9952b80U,
	0xa86c94d6U, 0x58363190U, 0x889cfc2eU, 0x3d1ef0f4U,
	0xfae20131U, 0x231a5433U, 0xf0454e1eU, 0xc636855eU,
	0x0eb6694dU, 0x4d00b3f4U, 0xbf72ad23U, 0x95861b80U,
	0xbe43a8eeU, 0xa6625858U, 0xe72b6b3dU, 0xa85500f9U,
	0xcaae90c6U, 0xea853bd6U, 0xbb798a11U, 0x6f1510d5U,
	0x62b7d353U, 0x8fc11138U, 0x4a362908U, 0x6c1afbbaU,
	0x201f849dU, 0x963188a0U, 0x645b0d3cU, 0x470936fbU,
	0x8a746c20U, 0x1517dbefU, 0x4dca6b89U, 0x22ee8029U,
	0x3f24c3b0U, 0x2a3f3cb7U, 0x588c860aU, 0x7ab7577eU,
	0x982bbefeU, 0x42d64630U, 0x1923728cU, 0xc6a598a5U,
	0x4e9cfb0eU, 0x93f7dc9fU, 0xeecda3dfU, 0x49a2c8c6U,
	0x31256542U, 0x706d6d70U, 0x660c0b1bU, 0x41d2790eU,
	0xb8e1389dU, 0x9fbd76e9U, 0x1ecdd4b4U, 0x861e466fU,
	0xfac695b2U, 0x92c7d110U, 0xc066b27cU, 0xcc25308bU,
	0x7e83a476U, 0x8b20d8f0U, 0x595030baU, 0xbcda3de2U,
};
static unsigned int signatureDigest[8];



/* Forward declarations for the retail-ordered definitions. */
void __attribute__((noinline)) initializeBootSecurity(void);
int __attribute__((noinline)) detectExternalBoot(void);
void setBootParameters(unsigned int bootMode, unsigned int bootArgument);
int readSecondLoaderFromEmmc(unsigned char *destination);
int readSecondLoaderFromSd(unsigned char *destination);
struct EmmcCard *getEmmcCard(void);
struct SdifCard *getSdifCard(void);
int firstLoaderMain(unsigned char *destination, struct SecondLoaderHeader *header);
int checkSecondLoaderHeader(const struct SecondLoaderHeader *header);
int prepareSecondLoaderKeys(unsigned int bootMode, const struct SecondLoaderHeader *header,
			    unsigned char *authentication);
int verifySecondLoaderSignature(const struct SecondLoaderHeader *header,
				const unsigned char *authentication);
int verifySecondLoaderPayload(const struct SecondLoaderHeader *header,
			      const unsigned char *authentication,
			      const unsigned char *payload);
int applyKeyPolicy(const struct KeyPolicy *policy, const unsigned char *records,
		   unsigned int size);
int decryptSecondLoader(unsigned int output, unsigned int input, unsigned int size);
int processSecondLoader(unsigned int bootMode, unsigned char *destination,
			struct SecondLoaderHeader *header, unsigned int *bootArgument);
int bigmacRunMode301(unsigned int output, unsigned int input, unsigned int size,
		     unsigned int key);
int bigmacRunMode10a(unsigned int output, unsigned int input, unsigned int size,
		     unsigned int key, unsigned int parameter);
int bigmacRunMode30a(unsigned int output, unsigned int input, unsigned int size,
		     unsigned int key, unsigned int parameter);
int bigmacRunMode33(unsigned int output, unsigned int input, unsigned int size,
		    unsigned int key);
int bigmacClearMemory(unsigned int address, unsigned int value, unsigned int size);
__attribute__((noinline))
int bigmacRun(unsigned int output, unsigned int input, unsigned int size,
	      unsigned int key, unsigned int parameter, unsigned int mode);
int bigmacClearRegisters(void);
int bignumPowmod(unsigned int *output, const unsigned int *input,
		 const volatile unsigned int *modulus, const unsigned int *exponent,
		 unsigned int mode);
int initializeSdif(unsigned int port, unsigned int slowClock);
struct FirstLoaderSdif *getSdif(unsigned int port);
int resetSdif(struct FirstLoaderSdif *controller, unsigned int reset);
int powerOnSdif(struct FirstLoaderSdif *controller);
int powerOffSdif(struct FirstLoaderSdif *controller);
int isSdifCardPresent(struct FirstLoaderSdif *controller);
int setSdifClock(struct FirstLoaderSdif *controller, unsigned int frequency,
		 unsigned int highSpeed);
int setSdifBusWidth(struct FirstLoaderSdif *controller, unsigned int width);
void pollSdifInterrupt(unsigned int port, struct FirstLoaderSdif *controller,
		       unsigned int unused);
void collectSdifResponse(struct FirstLoaderSdif *controller,
			 struct FirstLoaderSdifCommand *command);
void resetSdifErrors(struct FirstLoaderSdif *controller, unsigned int errors);
void handleSdifCommandInterrupt(struct FirstLoaderSdif *controller,
				unsigned int normal, unsigned int errors);
void handleSdifDataInterrupt(struct FirstLoaderSdif *controller,
			     unsigned int normal, unsigned int errors);
int executeSdifCommand(struct FirstLoaderSdif *controller,
		       struct FirstLoaderSdifCommand *command);
int sendSdifCommand(struct FirstLoaderSdif *controller,
		    struct FirstLoaderSdifCommand *command, int retries);
int idleSdifCard(struct FirstLoaderSdif *controller);
int readSdifCid(struct FirstLoaderSdif *controller, unsigned int *cid);
int readSdifCsd(struct FirstLoaderSdif *controller, unsigned int *csd);
int readSdifCidWithRca(struct FirstLoaderSdif *controller,
		       unsigned int *cid);
int selectSdifCard(struct FirstLoaderSdif *controller, unsigned int select);
int stopSdifTransfer(struct FirstLoaderSdif *controller, unsigned int busyResponse);
int __attribute__((noinline)) decodeSdifCardStatus(unsigned int status);
int readSdifCardStatus(struct FirstLoaderSdif *controller, unsigned int *status);
int setSdifBlockLength(struct FirstLoaderSdif *controller, unsigned int length);
int setSdifBlockCount(struct FirstLoaderSdif *controller, unsigned int count);
int sendEmmcOperationConditions(struct EmmcCard *card, unsigned int conditions,
				unsigned int *response);
int readEmmcCid(struct EmmcCard *card, unsigned int *cid);
int setEmmcRelativeAddress(struct EmmcCard *card, unsigned int address);
int switchEmmcSetting(struct EmmcCard *card, unsigned int access,
		      unsigned int index, unsigned int value);
int selectEmmcCard(struct EmmcCard *card, unsigned int select);
int readEmmcExtendedCsd(struct EmmcCard *card, void *destination);
int readEmmcCsd(struct EmmcCard *card, unsigned int *csd);
int readEmmcCidWithRca(struct EmmcCard *card, unsigned int *cid);
int readEmmcCardStatus(struct EmmcCard *card, unsigned int *status);
int setEmmcBlockLength(struct EmmcCard *card, unsigned int length);
int setEmmcBlockCount(struct EmmcCard *card, unsigned int count);
int readEmmcBlocks(struct EmmcCard *card, unsigned int block,
		   void *destination, unsigned int count);
static unsigned int __attribute__((noinline, noclone))
readEmmcBits(const unsigned char *data, unsigned int first,
	     unsigned int count);
int openEmmc(unsigned int port, struct EmmcCard **resultCard);
int sendSdifInterfaceCondition(struct SdifCard *card,
			       unsigned int voltageFlags);
int readSdifCidForCard(struct SdifCard *card, unsigned int *cid);
int readSdifRelativeAddress(struct SdifCard *card,
			    unsigned short *relativeAddress);
int selectSdifCardForCard(struct SdifCard *card, unsigned int select);
int readSdifCsdForCard(struct SdifCard *card, unsigned int *csd);
int readSdifCidWithRcaForCard(struct SdifCard *card, unsigned int *cid);
int readSdifStatusForCard(struct SdifCard *card, unsigned int *status);
int setSdifBlockLengthForCard(struct SdifCard *card, unsigned int length);
int sendSdifApplicationCommand(struct SdifCard *card,
			       struct FirstLoaderSdifCommand *command,
			       int retries);
int requestSdifOperatingConditions(struct SdifCard *card,
				   unsigned int conditions,
				   unsigned int *response);
int setSdifCardDetectPullup(struct SdifCard *card,
			    unsigned int enable);
int readSdifScr(struct SdifCard *card, void *destination);
int readSdifBlocks(struct SdifCard *card, unsigned int block,
		   void *destination, unsigned int count);
unsigned int readBits(const unsigned char *data, unsigned int first,
		      unsigned int count);
int openSdifCard(unsigned int port, struct SdifCard **resultCard);
int probeExternalBoot(void);
unsigned int __attribute__((noinline))
copyWordsAndSum(unsigned int *dst, const unsigned int *src, unsigned int size);
void *__attribute__((noinline, noclone))
fillBytes(void *dst, int value, unsigned int size);
unsigned int compareBytes(const void *left, const void *right, unsigned int size);
static volatile SdifHostRegisters *sdifRegisters(const struct FirstLoaderSdif *controller);
static struct FirstLoaderSdif *emmcController(const struct EmmcCard *card);
static unsigned int readLittleEndianWord(const unsigned char *bytes);

void __attribute__((noinline)) initializeBootSecurity(void)
{
	KEYRING_COMMAND = 0x1c0f020eU;
	KEYRING_COMMAND = 0x1c0f020fU;
	BOOT_SECURITY_COMMAND = 0x020e020fU;
	BOOT_SECURITY_START = 1;
}

int __attribute__((noinline)) detectExternalBoot(void)
{
	KEYRING->probeSelect = 0x501;
	if ((KEYRING->probeStatus & 3) != 3)
		return -1;
	return (BOOT_FUSE_WORDS[0] & 1) ? 0x41 : 0x21;
}

void setBootParameters(unsigned int bootMode, unsigned int bootArgument)
{
	unsigned int parameters[8];
	unsigned int i;

	if (bootMode & 1) {
		for (i = 0; i < 8; i++)
			parameters[i] = BOOT_FUSE_WORDS[i];
		parameters[0] |= 4;
		parameters[7] = (parameters[7] & 0x0fffffffU) | (bootArgument << 28);
	} else {
		parameters[0] = (bootMode & 0x40 ? 3U : 0U) |
				(bootMode == 0x20 ? 1U : 0U) |
				(bootMode & 4 ? 8U : 0U);
		for (i = 1; i < 7; i++)
			parameters[i] = 0;
		parameters[7] = bootArgument << 28;
	}
	KEYRING->bootParameters[7] = parameters[7];
	for (i = 0; i < 7; i++)
		KEYRING->bootParameters[i] = parameters[i];
	KEYRING->select = 0x501;
	KEYRING_COMMAND = 0x08000501U;
}

int readSecondLoaderFromEmmc(unsigned char *destination)
{
	struct BootSector *sector = (struct BootSector *)destination;
	struct EmmcCard *card;
	unsigned int firstBlock;
	unsigned int blockCount;
	unsigned int i;
	int result;

	if (bootHardwareMode == 0) {
		PERVASIVE_MISC->sdifVoltage |= 1;
		PERVASIVE_RESET->bigmac = 1;
		PERVASIVE_GATE->sdif[0] = 1;
		while (PERVASIVE_GATE->sdif[0] == 0)
			;
		PERVASIVE_RESET->sdif[0] = 0;
		while (PERVASIVE_RESET->sdif[0] != 0)
			;
		PERVASIVE_RESET->bigmac = 0;
		while (PERVASIVE_RESET->bigmac != 0)
			;
	} else if (BOOT_BRIDGE_STATUS & 0x20000) {
		BOOT_BRIDGE_CONTROL = 0x80000001U;
	}
	initializeBootSecurity();
	result = initializeSdif(0, (BOOT_FUSE_FLAGS & 0x10000U) == 0);
	if (result < 0)
		return result;
	result = openEmmc(0, &card);
	if (result < 0)
		return result;
	result = readEmmcBlocks(card, 0, destination, 1);
	if (result < 0)
		return result;
	firstBlock = readLittleEndianWord(sector->firstBlock);
	blockCount = readLittleEndianWord(sector->blockCount);
	if (blockCount == 0 || blockCount >= 0xdf) {
		fillBytes(destination, 0, 0x200);
		return -1;
	}
	for (i = 0; i < blockCount; i++) {
		result = readEmmcBlocks(card, firstBlock + i, destination + i * 0x200, 1);
		if (result < 0)
			return result;
	}
	return 0;
}

int readSecondLoaderFromSd(unsigned char *destination)
{
	struct BootSector *sector = (struct BootSector *)destination;
	struct SdifCard *card;
	unsigned int port = 1;
	unsigned int firstBlock;
	unsigned int blockCount;
	unsigned int i;
	int result;

	if (bootHardwareMode == 0) {
		PERVASIVE_MISC->gameCard = 0;
		while (PERVASIVE_MISC->gameCard != 0)
			;
		PERVASIVE_GATE->sdif[1] = 1;
		while (PERVASIVE_GATE->sdif[1] == 0)
			;
		PERVASIVE_RESET->sdif[1] = 0;
		while (PERVASIVE_RESET->sdif[1] != 0)
			;
	} else if (BOOT_BRIDGE_STATUS & 0x20000U) {
		port = 0x101;
		BOOT_BRIDGE_CONTROL = 0;
	}
	result = initializeSdif(port, (BOOT_FUSE_FLAGS & 0x20000U) == 0);
	if (result < 0)
		return result;
	result = openSdifCard(port, &card);
	if (result < 0)
		return result;
	result = readSdifBlocks(card, 0, destination, 1);
	if (result < 0)
		return result;
	firstBlock = readLittleEndianWord(sector->firstBlock);
	blockCount = readLittleEndianWord(sector->blockCount);
	if (firstBlock >= card->sectorCount ||
	    card->sectorCount < firstBlock + blockCount ||
	    blockCount == 0 || blockCount >= 0xdf) {
		fillBytes(destination, 0, 0x200);
		return -1;
	}
	for (i = 0; i < blockCount; i++) {
		result = readSdifBlocks(card, firstBlock + i,
				       destination + i * 0x200, 1);
		if (result < 0)
			return result;
	}
	return 0;
}

struct EmmcCard *getEmmcCard(void)
{
	return &firstLoaderCard.emmc;
}

struct SdifCard *getSdifCard(void)
{
	return &firstLoaderCard.sd;
}

/* Keep the retail-ordered code within the ROM's fixed data window. */
__attribute__((optimize("Os")))
int firstLoaderMain(unsigned char *destination, struct SecondLoaderHeader *header)
{
	unsigned char *image = (unsigned char *)header;
	unsigned int bootMode;
	unsigned int bootArgument = 0;
	int result;

	initializeBootSecurity();
	bootMode = detectExternalBoot();
	if ((int)bootMode < 0) {
		unsigned int hardwareMode = bootHardwareMode;
		int externalBootResult;

		if (hardwareMode == 0) {
			PERVASIVE_BASECLK->cmepClock = 0x10004;
			PERVASIVE_BASECLK->busClock = 1;
			PERVASIVE_GATE->sdif[0] = 1;
			PERVASIVE_RESET->sdif[0] = 0;
			externalBootResult = probeExternalBoot();
		} else if (hardwareMode == 1) {
			externalBootResult = probeExternalBoot();
		} else {
			externalBootResult = hardwareMode != 2;
		}

		bootMode = 0x20;
		if (externalBootResult >= 0) {
			bootMode = 0x22;
			if (externalBootResult == 1)
				bootMode = 0x44;
		}
	}

	if (bootMode & 1U) {
		volatile unsigned int *bootSource =
			(volatile unsigned int *)0xe0000010UL;
		unsigned int sourceAddress;

		while ((*bootSource & 1U) == 0)
			;
		sourceAddress = *bootSource & 0xfffffffcU;
		copyWordsAndSum((unsigned int *)image,
				(const unsigned int *)(unsigned long)sourceAddress, 0x40);
		result = checkSecondLoaderHeader(header);
		if (result < 0)
			goto bootFailure;
		copyWordsAndSum((unsigned int *)(image + 0x40),
				(const unsigned int *)(unsigned long)(sourceAddress + 0x40),
				header->decodedSize + header->imageSize - 0x40);
	} else if (bootMode & 0x20U) {
		do {
			result = readSecondLoaderFromEmmc(image);
		} while (result < 0);
	} else if (bootMode & 0x40U) {
		do {
			result = readSecondLoaderFromSd(image);
		} while (result < 0);
	} else {
		goto bootFailure;
	}

	result = processSecondLoader(bootMode, destination, header, &bootArgument);
	if (result < 0)
		goto bootFailure;
	setBootParameters(bootMode, bootArgument);
	*(volatile unsigned int *)0xe0000000UL = 1;
	return 0;

bootFailure:
	delayMicroseconds(BIGMAC_RANDOM & 0xffffU);
	*(volatile unsigned int *)0xe0000000UL = 2;
	for (;;)
		;
}

/* Check the lengths and key selectors before processing the image. */
int checkSecondLoaderHeader(const struct SecondLoaderHeader *header)
{
	unsigned int size = header->imageSize;
	unsigned int decodedSize;

	if (header->magic != 0x64b2c8e5 || size < 0x2b0 || size >= 0x10000)
		return -1;
	if (header->firstPartSize >= size || header->secondPartSize >= size)
		return -1;
	if (header->firstPartSize + header->secondPartSize + 0x2b0 != size)
		return -1;
	decodedSize = header->decodedSize;
	if (FL_SINCE(0x20110616) && decodedSize >= 0x1bc00)
		return -1;
	if (decodedSize + size >= 0x1bc00 || header->signingKeyIndex >= 16 ||
	    header->keyIndex >= 6)
		return -1;
	return 0;
}

int prepareSecondLoaderKeys(unsigned int bootMode, const struct SecondLoaderHeader *header,
			    unsigned char *authentication)
{
	unsigned int address = (unsigned int)(unsigned long)authentication;
	int result;

	result = bigmacRunMode301(8, (unsigned int)(unsigned long)slot8Seed,
				  sizeof(slot8Seed), 0x206);
	fillBytes(slot8Seed, 0, sizeof(slot8Seed));
	if (result < 0)
		return result;
	if (bootMode & 0x20) {
		fillBytes(slot9Seed, 0, sizeof(slot9Seed));
		fillBytes(slot9Iv, 0, sizeof(slot9Iv));
		result = bigmacRunMode10a(address, address, header->decodedSize + 0x1e0,
					  8, (unsigned int)(unsigned long)slot8Iv);
		fillBytes(slot8Iv, 0, sizeof(slot8Iv));
	} else {
		fillBytes(slot8Iv, 0, sizeof(slot8Iv));
		result = bigmacRunMode301(9, (unsigned int)(unsigned long)slot9Seed,
					  sizeof(slot9Seed), 0x207);
		fillBytes(slot9Seed, 0, sizeof(slot9Seed));
		if (result < 0)
			return result;
		result = bigmacRunMode10a(address, address, header->decodedSize + 0x1e0,
					  9, (unsigned int)(unsigned long)slot9Iv);
		fillBytes(slot9Iv, 0, sizeof(slot9Iv));
	}
	if (result < 0)
		return result;
	KEYRING_COMMAND = 0x1c1e0008U;
	KEYRING_COMMAND = 0x1c1f0009U;
	KEYRING_COMMAND = 0x1c1f0206U;
	KEYRING_COMMAND = 0x1c1f0207U;
	return 0;
}

int verifySecondLoaderSignature(const struct SecondLoaderHeader *header,
				const unsigned char *authentication)
{
	const volatile unsigned int *modulus;
	unsigned int decodedWords[64];
	unsigned char *decoded = (unsigned char *)decodedWords;
	unsigned int keyMask;
	unsigned int i;
	int result;

	result = bigmacRunMode301(0x20, (unsigned int)(unsigned long)signatureSeed,
				  sizeof(signatureSeed), 0x344);
	fillBytes(signatureSeed, 0, sizeof(signatureSeed));
	if (result < 0)
		return result;
	KEYRING_COMMAND = 0x1c1f0344U;
	result = bigmacRunMode33((unsigned int)(unsigned long)signatureDigest,
				 (unsigned int)(unsigned long)header,
				 header->imageSize - 0x100, 0x20);
	if (result < 0)
		return result;
	keyMask = *(volatile unsigned int *)0xe0064060UL;
	if (keyMask & 0xffffU) {
		if (!(keyMask & (1U << header->signingKeyIndex)))
			return -1;
		modulus = (const volatile unsigned int *)(0xe0066000UL +
							 ((header->signingKeyIndex * 8U & 0xffU) * 32U));
	} else {
		modulus = fallbackModulus;
	}
	result = bignumPowmod(decodedWords, (const unsigned int *)(authentication + 0xe0),
			      modulus, signatureExponent, 1);
	if (result < 0 || decoded[0] != 0 || decoded[1] != 1)
		return -1;
	for (i = 2; i < 0xcd; i++) {
		if (decoded[i] != 0xff)
			return -1;
	}
	if (decoded[0xcd] != 0 ||
	    compareBytes(decoded + 0xce, signatureDigestPrefix, sizeof(signatureDigestPrefix)) ||
	    compareBytes(decoded + 0xe0, signatureDigest, sizeof(signatureDigest)))
		return -1;
	for (i = 0; i < 128; i++)
		KEYRING_COMMAND = 0x10000700U | i;
	return 0;
}

int verifySecondLoaderPayload(const struct SecondLoaderHeader *header,
			      const unsigned char *authentication,
			      const unsigned char *payload)
{
	unsigned int i;
	int result;

	result = bigmacRunMode30a(10, (unsigned int)(unsigned long)authentication, 32,
				  header->keyIndex + 0x208,
				  (unsigned int)(unsigned long)keyIvs[header->keyIndex]);
	fillBytes(keyIvs, 0, sizeof(keyIvs));
	if (result < 0)
		return result;
	for (i = 0; i < 5; i++) {
		result = bigmacRunMode30a(i + 11,
			(unsigned int)(unsigned long)(authentication + 0x20 + i * 32),
			32, header->keyIndex + 0x208,
			(unsigned int)(unsigned long)derivedKeyIvs[i]);
		fillBytes(derivedKeyIvs[i], 0, sizeof(derivedKeyIvs[i]));
		if (result < 0)
			return result;
	}
	for (i = 0; i < 6; i++)
		KEYRING_COMMAND = 0x1c1f0208U + i;
	result = bigmacRunMode30a((unsigned int)(unsigned long)expectedDigest,
				  (unsigned int)(unsigned long)(authentication + 0xc0),
				  32, 10, (unsigned int)(unsigned long)digestIv);
	fillBytes(digestIv, 0, sizeof(digestIv));
	if (result < 0)
		return result;
	result = bigmacRunMode33((unsigned int)(unsigned long)actualDigest,
				 (unsigned int)(unsigned long)payload, header->decodedSize, 0x20);
	if (result < 0)
		return result;
	KEYRING_COMMAND = 0x1c1f0020U;
	return compareBytes(expectedDigest, actualDigest, sizeof(expectedDigest)) ? -1 : 0;
}

/* Apply the image's key-slot masks and per-slot permission records. */
int applyKeyPolicy(const struct KeyPolicy *policy, const unsigned char *records,
		   unsigned int size)
{
	static const unsigned char permissionBits[8] __attribute__((section(".fl.permissionBits"), aligned(1))) = { 28, 27, 26, 20, 19, 18, 17, 16 };
	unsigned int i;
	unsigned int offset = 0;

	for (i = 0; i < 24; i++) {
		if (policy->bank2 & (1U << i))
			KEYRING_COMMAND = 0x1c1f0200U | i;
	}
	for (i = 0; i < 8; i++) {
		if (policy->bank6 & (1U << i))
			KEYRING_COMMAND = 0x1c1f0600U | i;
	}
	for (i = 0; i < 128; i++) {
		unsigned int bit = 1U << (i & 7);
		unsigned int byte = i >> 3;

		if ((policy->bank0[byte] & bit) && i != 10)
			KEYRING_COMMAND = 0x1c1f0000U | i;
		if (policy->bank1[byte] & bit)
			KEYRING_COMMAND = 0x1c1f0100U | i;
		if (policy->bank4[byte] & bit)
			KEYRING_COMMAND = 0x1c1f0400U | i;
		if (policy->bank5[byte] & bit)
			KEYRING_COMMAND = 0x1c1f0500U | i;
	}
	for (i = 0; i < 256; i++) {
		unsigned int bit = 1U << (i & 7);
		unsigned int byte = i >> 3;

		if (policy->bank3[byte] & bit)
			KEYRING_COMMAND = 0x1c1f0300U | i;
		if (policy->bank7[byte] & bit)
			KEYRING_COMMAND = 0x1c1f0700U | i;
	}
	while (offset + 2 < size) {
		unsigned int slot = records[offset] | ((unsigned int)records[offset + 1] << 8);
		unsigned int flags;
		unsigned int permissions = 0;

		if (slot == 0xffff)
			break;
		flags = records[offset + 2];
		offset += 3;
		if (slot == 10)
			continue;
		for (i = 0; i < 8; i++) {
			if (flags & (1U << i))
				permissions |= 1U << permissionBits[i];
		}
		KEYRING_COMMAND = (~permissions & 0x1c1f0000U) | slot;
	}
	return 0;
}

int decryptSecondLoader(unsigned int output, unsigned int input, unsigned int size)
{
	int result;

	result = bigmacRunMode10a(output, input, size, 10,
				 (unsigned int)(unsigned long)payloadIv);
	fillBytes(payloadIv, 0, sizeof(payloadIv));
	if (result < 0)
		return result;
	KEYRING_COMMAND = 0x1c1f000aU;
	return 0;
}

int processSecondLoader(unsigned int bootMode, unsigned char *destination,
			struct SecondLoaderHeader *header, unsigned int *bootArgument)
{
	unsigned char *image = (unsigned char *)header;
	struct KeyPolicy *policy = (struct KeyPolicy *)(image + sizeof(*header) +
						       header->firstPartSize);
	unsigned char *records = (unsigned char *)(policy + 1);
	unsigned char *authentication = records + header->secondPartSize;
	unsigned char *payload = authentication + 0x1e0;
	unsigned int keyIndex;
	unsigned int decodedSize;
	int result;

	result = checkSecondLoaderHeader(header);
	if (result < 0)
		return result;
	result = prepareSecondLoaderKeys(bootMode, header, authentication);
	if (result < 0)
		return result;
	result = verifySecondLoaderSignature(header, authentication);
	if (result < 0)
		return result;
	result = verifySecondLoaderPayload(header, authentication, payload);
	if (result < 0)
		return result;
	applyKeyPolicy(policy, records, header->secondPartSize);
	keyIndex = header->keyIndex;
	decodedSize = header->decodedSize;
	result = decryptSecondLoader((unsigned int)(unsigned long)destination,
				     (unsigned int)(unsigned long)payload, decodedSize);
	if (result < 0)
		return result;
	result = bigmacClearMemory((unsigned int)(unsigned long)(destination + decodedSize),
				   0, 0x1c000 - decodedSize);
	if (result < 0)
		return result;
	bigmacClearRegisters();
	*bootArgument = keyIndex;
	return 0;
}

int bigmacRunMode301(unsigned int output, unsigned int input, unsigned int size,
		     unsigned int key)
{
	return bigmacRun(output, input, size, key, 0, 0x301);
}

int bigmacRunMode10a(unsigned int output, unsigned int input, unsigned int size,
		     unsigned int key, unsigned int parameter)
{
	return bigmacRun(output, input, size, key, parameter, 0x10a);
}

int bigmacRunMode30a(unsigned int output, unsigned int input, unsigned int size,
		     unsigned int key, unsigned int parameter)
{
	return bigmacRun(output, input, size, key, parameter, 0x30a);
}

int bigmacRunMode33(unsigned int output, unsigned int input, unsigned int size,
		    unsigned int key)
{
	return bigmacRun(output, input, size, key, 0, 0x33);
}

int bigmacClearMemory(unsigned int address, unsigned int value, unsigned int size)
{
	(void)value;
	return bigmacRun(address, address, size, 0, 0, 0x0c);
}

/* Submit one operation. A value below 0x1000 identifies a hardware key slot;
 * otherwise key identifies 32 bytes to load into the engine.
 */
__attribute__((noinline))
int bigmacRun(unsigned int output, unsigned int input, unsigned int size,
	      unsigned int key, unsigned int parameter, unsigned int mode)
{
	unsigned int i;

	BIGMAC->output = output;
	BIGMAC->input = input;
	BIGMAC->size = size;
	if (parameter != 0)
		BIGMAC->parameter = parameter;
	if (output < 0x1000)
		mode |= BIGMAC_SLOT_OUTPUT;
	if (key < 0x1000) {
		BIGMAC->keySlot = key;
	} else {
		const unsigned char *bytes = (const unsigned char *)(unsigned long)key;

		for (i = 0; i < 8; i++) {
			BIGMAC->key[i] = (unsigned int)bytes[0] |
				((unsigned int)bytes[1] << 8) |
				((unsigned int)bytes[2] << 16) |
				((unsigned int)bytes[3] << 24);
			bytes += 4;
		}
		mode |= BIGMAC_INLINE_KEY;
	}
	BIGMAC->mode = mode;
	BIGMAC->start = 1;
	while (BIGMAC->status & BIGMAC_BUSY)
		;
	return (BIGMAC->status & BIGMAC_ERROR_MASK) ? -1 : 0;
}

int bigmacClearRegisters(void)
{
	BIGMAC->output = 0;
	BIGMAC->input = 0;
	BIGMAC->size = 0;
	BIGMAC->parameter = 0;
	BIGMAC->mode = 0;
	return 0;
}

/* Submit a 2048-bit modular operation and return its big-endian result. */
/* Keep the hardware status loop in the form the retail MeP code uses. */
__attribute__((optimize("O2", "no-tree-loop-optimize")))
int bignumPowmod(unsigned int *output, const unsigned int *input,
		 const volatile unsigned int *modulus, const unsigned int *exponent,
		 unsigned int mode)
{
	unsigned int i;
	int result = 0;

	for (i = 0; i < 64; i++)
		BIGNUM->input[i] = input[i];
	for (i = 0; i < 64; i++)
		BIGNUM->modulus[i] = modulus[i];
	BIGNUM->exponent = *exponent++;
	BIGNUM->command = 0x91000000U | (mode << 9);
	while (BIGNUM->status & BIGNUM_BUSY) {
		if (BIGNUM->status & BIGNUM_ERROR)
			break;
		if ((BIGNUM->status & BIGNUM_NEED_INPUT) &&
		    (BIGNUM->status & BIGNUM_INPUT_READY))
			BIGNUM->exponent = *exponent++;
	}
	if (BIGNUM->status & BIGNUM_ERROR) {
		result = -1;
	} else {
		for (i = 0; i < 64; i++) {
			unsigned int word = BIGNUM->result[i];

			output[63 - i] = (word << 24) | ((word & 0xff00U) << 8) |
					 ((word & 0xff0000U) >> 8) | (word >> 24);
		}
	}
	BIGNUM_ACK = BIGNUM_ACK;
	for (i = 0; i < 66; i++) {
		BIGNUM->input[i] = 0;
		BIGNUM->work[i] = 0;
		BIGNUM->result[i] = 0;
		BIGNUM->modulus[i] = 0;
	}
	return result;
}

int initializeSdif(unsigned int port, unsigned int slowClock)
{
	unsigned int physicalPort = port & 0xff;

	fillBytes(&sdif, 0, sizeof(sdif));
	sdif.port = physicalPort;
	sdif.baseClock = 48000000;
	sdif.maximumClock = slowClock ? 24000000 : 48000000;
	sdif.registersAddress = SDIF_REG_BASE(physicalPort);
	if (port == 0x101) {
		sdif.flags = 0x300000;
		sdif.registersAddress = SDIF_REG_BASE(0);
	} else if (port == 1) {
		sdif.flags = 0x300000;
	} else if (port == 0) {
		sdif.flags = 0x80;
	}
	resetSdif(&sdif, 3);
	return 0;
}

struct FirstLoaderSdif *getSdif(unsigned int port)
{
	(void)port;
	return &sdif;
}

int resetSdif(struct FirstLoaderSdif *controller, unsigned int reset)
{
	volatile SdifHostRegisters *registers = sdifRegisters(controller);

	if (reset & 1) {
		registers->softwareReset = 1;
		while (registers->softwareReset & 1)
			;
	}
	if (reset & 2) {
		registers->normalInterruptEnable = 0xf3;
		registers->errorInterruptEnable = 0x17f;
		registers->normalSignalEnable = 0xf3;
		registers->errorSignalEnable = 0x17f;
		registers->timeoutControl = 0x0e;
		while (!(registers->presentState & 0x20000U))
			;
		registers->normalInterruptStatus = registers->normalInterruptStatus;
		registers->errorInterruptStatus = registers->errorInterruptStatus;
		while (registers->normalInterruptStatus != 0)
			;
		while (registers->errorInterruptStatus != 0)
			;
	}
	return 0;
}

int powerOnSdif(struct FirstLoaderSdif *controller)
{
	volatile SdifHostRegisters *registers = sdifRegisters(controller);

	while (!(registers->presentState & 0x20000U))
		;
	if (registers->presentState & 0x10000U) {
		registers->powerControl |= 0x0f;
		registers->clockControl = 0;
		registers->clockControl = 0x8001;
		while (!(registers->clockControl & 2))
			;
		registers->clockControl |= 4;
		registers->hostControl = 0;
	}
	return 0;
}

int powerOffSdif(struct FirstLoaderSdif *controller)
{
	volatile SdifHostRegisters *registers = sdifRegisters(controller);

	registers->clockControl = 0;
	registers->hostControl = 0;
	registers->powerControl = 0;
	return 0;
}

int isSdifCardPresent(struct FirstLoaderSdif *controller)
{
	volatile SdifHostRegisters *registers = sdifRegisters(controller);
	unsigned int retries = 1000;

	while (!(registers->presentState & 0x20000U)) {
		if (retries == 0)
			return (int)0x80320002U;
		retries--;
		delayMicroseconds(1000);
	}
	return (registers->presentState & 0x10000U) != 0;
}

int setSdifClock(struct FirstLoaderSdif *controller, unsigned int frequency,
		 unsigned int highSpeed)
{
	volatile SdifHostRegisters *registers = sdifRegisters(controller);
	unsigned int divisor = 0;
	unsigned int retries = 10;
	unsigned short clock;
	unsigned char control;

	if (frequency > controller->maximumClock)
		frequency = controller->maximumClock;
	while (divisor < 8 && frequency < (controller->baseClock >> divisor))
		divisor++;
	registers->clockControl = 0;
	clock = registers->clockControl;
	clock |= (0x80U << divisor) & 0xff00U;
	registers->clockControl = clock | 1;
	while (!(registers->clockControl & 2)) {
		if (retries == 0)
			return (int)0x80320002U;
		retries--;
		delayMicroseconds(1000);
	}
	registers->clockControl |= 4;
	control = registers->hostControl;
	if (highSpeed)
		control |= 4;
	else
		control &= ~4;
	registers->hostControl = control;
	return 0;
}

int setSdifBusWidth(struct FirstLoaderSdif *controller, unsigned int width)
{
	volatile SdifHostRegisters *registers = sdifRegisters(controller);
	unsigned char control = registers->hostControl;

	if (width == 8)
		control = (control | 0x20) & ~2;
	else if (width == 4)
		control = (control & ~0x20) | 2;
	else
		control &= ~(0x20 | 2);
	registers->hostControl = control;
	return 0;
}

void pollSdifInterrupt(unsigned int port, struct FirstLoaderSdif *controller,
		       unsigned int unused)
{
	volatile SdifHostRegisters *registers = sdifRegisters(controller);
	unsigned int normal = registers->normalInterruptStatus & 0xfe3fU;
	unsigned int errors = registers->errorInterruptStatus;

	(void)port;
	(void)unused;
	(void)registers->autoCmd12Error;
	if ((normal & 1) || (errors & 0x0f)) {
		registers->normalInterruptStatus = 1;
		registers->errorInterruptStatus = 0x0f;
		while (registers->normalInterruptStatus & 1)
			;
		while (registers->errorInterruptStatus & 0x0f)
			;
		handleSdifCommandInterrupt(controller, normal, errors);
	}
	if ((normal & 0x32) || (errors & 0x70)) {
		registers->normalInterruptStatus = 0x32;
		registers->errorInterruptStatus = 0x70;
		while (registers->normalInterruptStatus & 0x32)
			;
		while (registers->errorInterruptStatus & 0x70)
			;
		handleSdifDataInterrupt(controller, normal, errors);
	}
}

void collectSdifResponse(struct FirstLoaderSdif *controller,
			 struct FirstLoaderSdifCommand *command)
{
	volatile SdifHostRegisters *registers = sdifRegisters(controller);
	unsigned int format = command->flags & 0xf8;
	unsigned int response0;
	unsigned int response1;
	unsigned int response2;
	unsigned int response3;

	command->error = 0;
	switch (format) {
	case 0x10:
	case 0x28:
		command->response[0] = registers->response[0];
		if (command->flags & 0x800)
			command->response[3] = registers->response[3];
		break;
	case 0x30:
		response0 = registers->response[0];
		response1 = registers->response[1];
		response2 = registers->response[2];
		response3 = registers->response[3];
		command->response[0] = response0 << 8;
		command->response[1] = (response1 << 8) | (response0 >> 24);
		command->response[2] = (response2 << 8) | (response1 >> 24);
		command->response[3] = (response3 << 8) | (response2 >> 24);
		break;
	case 0x40:
	case 0x50:
	case 0x60:
	case 0x78:
	case 0x80:
	case 0x90:
		command->response[0] = registers->response[0];
		break;
	default:
		break;
	}
	if (command->flags & 0x3000) {
		registers->softwareReset = 6;
		while (registers->softwareReset & 6)
			;
	}
}

void resetSdifErrors(struct FirstLoaderSdif *controller, unsigned int errors)
{
	volatile SdifHostRegisters *registers = sdifRegisters(controller);
	unsigned short enabled;

	if (errors == 0)
		return;
	enabled = registers->errorInterruptEnable;
	registers->errorInterruptEnable = 0;
	while ((short)registers->errorInterruptEnable != 0)
		;
	if (errors & 0x0f) {
		registers->softwareReset = 2;
		while (registers->softwareReset & 2)
			;
	}
	if (errors & 0x70) {
		registers->softwareReset = 4;
		while (registers->softwareReset & 4)
			;
	}
	registers->errorInterruptStatus = registers->errorInterruptStatus;
	while ((short)registers->errorInterruptStatus != 0)
		;
	registers->errorInterruptEnable = enabled;
	while (registers->errorInterruptEnable != enabled)
		;
}

void handleSdifCommandInterrupt(struct FirstLoaderSdif *controller,
				unsigned int normal, unsigned int errors)
{
	struct FirstLoaderSdifCommand *command =
		(struct FirstLoaderSdifCommand *)(unsigned long)controller->activeRequest;

	if (command == 0)
		return;
	if (errors != 0) {
		command->error = (int)(0x80320002U + ((errors & 1) == 0));
		command->flags |= 0x40000000U;
		resetSdifErrors(controller, errors);
		return;
	}
	if (!(normal & 1))
		return;
	controller->commandComplete = 1;
	collectSdifResponse(controller, command);
	if (controller->activeTransfer == 0 && !(command->flags & 8))
		command->flags |= 0x80000000U;
}

void handleSdifDataInterrupt(struct FirstLoaderSdif *controller,
			     unsigned int normal, unsigned int errors)
{
	struct FirstLoaderSdifCommand *command =
		(struct FirstLoaderSdifCommand *)(unsigned long)controller->activeRequest;
	volatile SdifHostRegisters *registers = sdifRegisters(controller);
	unsigned char *buffer;
	unsigned int i;

	if (command == 0)
		return;
	if (controller->activeTransfer == 0) {
		if ((normal & 2) && (command->flags & 8))
			command->flags |= 0x80000000U;
		return;
	}
	if (errors != 0) {
		controller->activeTransfer = 0;
		command->error = (int)(0x80320002U + ((errors & 0x10) == 0));
		command->flags |= 0x40000000U;
		resetSdifErrors(controller, errors);
		return;
	}
	if ((normal & 0x20) && (command->flags & 0x700) == 0x100 &&
	    (int)command->blockCount > 0) {
		command->blockCount--;
		while (!(registers->presentState & 0x800))
			;
		buffer = (unsigned char *)(unsigned long)command->bufferAddress;
		for (i = 0; (int)i < (int)command->blockSize;) {
			unsigned int remaining = command->blockSize - i;

			if (remaining == 1) {
				buffer[i++] = registers->buffer.byte;
			} else if (remaining < 4) {
				unsigned int value = registers->buffer.halfword;

				buffer[i++] = value;
				buffer[i++] = value >> 8;
			} else {
				unsigned int value = registers->buffer.word;

				if (command->bufferAddress & 3) {
					buffer[i] = value;
					buffer[i + 1] = value >> 8;
					buffer[i + 2] = value >> 16;
					buffer[i + 3] = value >> 24;
				} else {
					*(unsigned int *)(buffer + i) = value;
				}
				i += 4;
			}
		}
		command->bufferAddress += command->blockSize;
	}
	if ((normal & 0x10) && (command->flags & 0x700) == 0x200 &&
	    (int)command->blockCount > 0) {
		command->blockCount--;
		while (!(registers->presentState & 0x400))
			;
		buffer = (unsigned char *)(unsigned long)command->bufferAddress;
		for (i = 0; (int)i < (int)command->blockSize;) {
			unsigned int remaining = command->blockSize - i;
			unsigned int value;

			if (remaining == 1) {
				registers->buffer.byte = buffer[i++];
			} else if (remaining < 4) {
				value = buffer[i] | ((unsigned int)buffer[i + 1] << 8);
				i += 2;
				registers->buffer.halfword = value;
			} else {
				if (command->bufferAddress & 3)
					value = buffer[i] | ((unsigned int)buffer[i + 1] << 8) |
						((unsigned int)buffer[i + 2] << 16) |
						((unsigned int)buffer[i + 3] << 24);
				else
					value = *(unsigned int *)(buffer + i);
				i += 4;
				registers->buffer.word = value;
			}
		}
		command->bufferAddress += command->blockSize;
	}
	if (normal & 2) {
		if (controller->commandComplete)
			command->flags |= 0x80000000U;
		controller->activeTransfer = 0;
	}
}

int executeSdifCommand(struct FirstLoaderSdif *controller,
		       struct FirstLoaderSdifCommand *command)
{
	volatile SdifHostRegisters *registers = sdifRegisters(controller);
	unsigned int format;
	unsigned int responseMode;
	unsigned int kind;
	unsigned int transferMode;
	int result;

	command->error = 0;
	command->flags &= 0x3fffffffU;
	controller->commandComplete = 0;
	controller->activeTransfer = 0;
	controller->activeRequest = (unsigned int)(unsigned long)command;
	for (;;) {
		if (registers->presentState & 1)
			continue;
		if (command->command == 12)
			break;
		if ((command->flags & 7) != 4 && (command->flags & 0xf8) != 0x28)
			break;
		if (!(registers->presentState & 2))
			break;
	}
	format = command->flags & 0xf8;
	switch (format) {
	case 0x30: responseMode = 9; break;
	case 0x28: case 0x78: responseMode = 0x1b; break;
	case 0x10: case 0x60: case 0x80: case 0x90: responseMode = 0x1a; break;
	case 0x40: case 0x50: responseMode = 2; break;
	default: responseMode = 0; break;
	}
	kind = command->flags & 7;
	if (kind == 1 || kind == 2 || kind == 3) {
		registers->argument = command->argument;
		registers->transferMode = 0;
		registers->command = (command->command << 8) | responseMode;
	} else if (kind == 4) {
		controller->activeTransfer = (unsigned int)(unsigned long)command;
		registers->timeoutControl = 0x0e;
		registers->hostControl &= 0xe7;
		registers->blockSize = command->blockSize | 0x7000;
		registers->blockCount = command->blockCount;
		transferMode = (command->flags & 0x100) ? 0x10 : 0;
		if (command->flags & 0x800)
			transferMode |= 4;
		if (command->blockCount == 0)
			command->blockCount = 1;
		else if (command->blockCount == 1)
			transferMode |= 2;
		else if ((int)command->blockCount >= 2)
			transferMode |= 0x22;
		registers->argument = command->argument;
		registers->transferMode = transferMode;
		registers->command = (command->command << 8) | 0x20 | responseMode;
	}
	while (!(command->flags & 0xc0000000U)) {
		if ((short)registers->normalInterruptStatus != 0 ||
		    (short)registers->errorInterruptStatus != 0)
			pollSdifInterrupt(controller->port, controller, 0);
		delayMicroseconds(1000);
	}
	result = (command->flags & 0x40000000U) ? command->error : 0;
	controller->activeRequest = 0;
	return result;
}

int sendSdifCommand(struct FirstLoaderSdif *controller,
		    struct FirstLoaderSdifCommand *command, int retries)
{
	int result;

	do {
		result = executeSdifCommand(controller, command);
		if (result >= 0)
			break;
		retries--;
	} while (retries >= 1);
	return result;
}

int idleSdifCard(struct FirstLoaderSdif *controller)
{
	struct FirstLoaderSdifCommand command = { 0 };
	int result;

	command.size = sizeof(command);
	command.flags = 1;
	result = sendSdifCommand(controller, &command, 0);
	if (result < 0)
		return result;
	setSdifClock(controller, 400000, 0);
	setSdifBusWidth(controller, 1);
	return 0;
}

int readSdifCid(struct FirstLoaderSdif *controller, unsigned int *cid)
{
	struct FirstLoaderSdifCommand command;
	int result;

	command.size = sizeof(command);
	command.flags = 0x32;
	command.command = 2;
	command.argument = 0;
	command.bufferAddress = 0;
	result = sendSdifCommand(controller, &command, 3);
	if (result < 0)
		return result;
	copyWordsAndSum(cid, command.response, 16);
	return 0;
}

int readSdifCsd(struct FirstLoaderSdif *controller, unsigned int *csd)
{
	struct FirstLoaderSdifCommand command;
	int result;

	command.size = sizeof(command);
	command.flags = 0x33;
	command.command = 9;
	command.argument = (unsigned int)controller->relativeAddress << 16;
	command.bufferAddress = 0;
	result = sendSdifCommand(controller, &command, 3);
	if (result < 0)
		return result;
	copyWordsAndSum(csd, command.response, 16);
	return 0;
}

int readSdifCidWithRca(struct FirstLoaderSdif *controller,
		       unsigned int *cid)
{
	struct FirstLoaderSdifCommand command;
	int result;

	command.size = sizeof(command);
	command.flags = 0x33;
	command.command = 10;
	command.argument = (unsigned int)controller->relativeAddress << 16;
	command.bufferAddress = 0;
	result = sendSdifCommand(controller, &command, 3);
	if (result < 0)
		return result;
	copyWordsAndSum(cid, command.response, 16);
	return 0;
}

int selectSdifCard(struct FirstLoaderSdif *controller, unsigned int select)
{
	struct FirstLoaderSdifCommand command;
	int result;

	command.size = sizeof(command);
	command.flags = select ? 0x13 : 3;
	command.command = 7;
	command.argument = select ? (unsigned int)controller->relativeAddress << 16 : 0;
	result = sendSdifCommand(controller, &command, 3);
	return result < 0 ? result : 0;
}

int stopSdifTransfer(struct FirstLoaderSdif *controller, unsigned int busyResponse)
{
	struct FirstLoaderSdifCommand command = { 0 };
	int result;

	command.size = sizeof(command);
	command.flags = busyResponse ? 0x302b : 0x3013;
	command.command = 12;
	result = sendSdifCommand(controller, &command, 3);
	return result < 0 ? result : 0;
}

int __attribute__((noinline)) decodeSdifCardStatus(unsigned int status)
{
	unsigned int i;

	status &= 0xfdffe008U;
	if (status == 0)
		return 0;
	for (i = 0; i < 32; i++) {
		if (status & (1U << i))
			break;
	}
	return (int)(0x80320100U + i);
}

int readSdifCardStatus(struct FirstLoaderSdif *controller, unsigned int *status)
{
	struct FirstLoaderSdifCommand command;
	int result;

	command.size = sizeof(command);
	command.flags = 0x13;
	command.command = 13;
	command.argument = (unsigned int)controller->relativeAddress << 16;
	command.bufferAddress = 0;
	result = sendSdifCommand(controller, &command, 3);
	if (result < 0)
		return result;
	if (status != 0)
		*status = command.response[0];
	return decodeSdifCardStatus(command.response[0]);
}

int setSdifBlockLength(struct FirstLoaderSdif *controller, unsigned int length)
{
	struct FirstLoaderSdifCommand command;
	int result;

	command.size = sizeof(command);
	command.flags = 0x13;
	command.command = 16;
	command.argument = length;
	command.bufferAddress = 0;
	result = sendSdifCommand(controller, &command, 3);
	if (result < 0)
		return result;
	return decodeSdifCardStatus(command.response[0]);
}

int setSdifBlockCount(struct FirstLoaderSdif *controller, unsigned int count)
{
	struct FirstLoaderSdifCommand command;
	int result;

	command.size = sizeof(command);
	command.flags = 0x13;
	command.command = 23;
	command.argument = count;
	command.bufferAddress = 0;
	result = sendSdifCommand(controller, &command, 3);
	if (result < 0)
		return result;
	return decodeSdifCardStatus(command.response[0]);
}

int sendEmmcOperationConditions(struct EmmcCard *card, unsigned int conditions,
				unsigned int *response)
{
	struct FirstLoaderSdifCommand command = { 0 };
	unsigned int retries = 0;
	int result;

	command.size = sizeof(command);
	command.flags = 0x42;
	command.command = 1;
	command.argument = conditions;
	do {
		result = sendSdifCommand(emmcController(card), &command, 0);
		if (result < 0)
			return result;
		if (conditions == 0 || (command.response[0] & 0x80000000U)) {
			if (response != 0)
				*response = command.response[0];
			return 0;
		}
		delayMicroseconds(10000);
		retries++;
	} while (retries < 100);
	return (int)0x80320002U;
}

int readEmmcCid(struct EmmcCard *card, unsigned int *cid)
{
	return readSdifCid(emmcController(card), cid);
}

int setEmmcRelativeAddress(struct EmmcCard *card, unsigned int address)
{
	struct FirstLoaderSdifCommand command = { 0 };
	int result;

	command.size = sizeof(command);
	command.flags = 0x13;
	command.command = 3;
	command.argument = address << 16;
	result = sendSdifCommand(emmcController(card), &command, 3);
	return result < 0 ? result : decodeSdifCardStatus(command.response[0]);
}

int switchEmmcSetting(struct EmmcCard *card, unsigned int access,
		      unsigned int index, unsigned int value)
{
	struct FirstLoaderSdifCommand command = { 0 };
	int result;

	command.size = sizeof(command);
	command.flags = 0x2b;
	command.command = 6;
	command.argument = 0x03000000U | (access & 3) | ((index & 0xff) << 16) |
			   ((value & 0xff) << 8);
	result = sendSdifCommand(emmcController(card), &command, 3);
	return result < 0 ? result : 0;
}

int selectEmmcCard(struct EmmcCard *card, unsigned int select)
{
	return selectSdifCard(emmcController(card), select);
}

int readEmmcExtendedCsd(struct EmmcCard *card, void *destination)
{
	struct FirstLoaderSdifCommand command = { 0 };
	int result;

	command.size = sizeof(command);
	command.flags = 0x114;
	command.command = 8;
	command.bufferAddress = (unsigned int)(unsigned long)destination;
	command.blockSize = 512;
	result = sendSdifCommand(emmcController(card), &command, 3);
	return result < 0 ? result : 0;
}

int readEmmcCsd(struct EmmcCard *card, unsigned int *csd)
{
	return readSdifCsd(emmcController(card), csd);
}

int readEmmcCidWithRca(struct EmmcCard *card, unsigned int *cid)
{
	return readSdifCidWithRca(emmcController(card), cid);
}

int readEmmcCardStatus(struct EmmcCard *card, unsigned int *status)
{
	return readSdifCardStatus(emmcController(card), status);
}

int setEmmcBlockLength(struct EmmcCard *card, unsigned int length)
{
	return setSdifBlockLength(emmcController(card), length);
}

int setEmmcBlockCount(struct EmmcCard *card, unsigned int count)
{
	return setSdifBlockCount(emmcController(card), count);
}

int readEmmcBlocks(struct EmmcCard *card, unsigned int block,
		   void *destination, unsigned int count)
{
	struct FirstLoaderSdif *controller =
		(struct FirstLoaderSdif *)(unsigned long)card->controllerAddress;
	struct FirstLoaderSdifCommand command = { 0 };
	int result;

	command.size = sizeof(command);
	command.flags = 0x114;
	command.command = count == 1 ? 17 : 18;
	command.argument = (card->flags & 1) ? block : block << 9;
	command.bufferAddress = (unsigned int)(unsigned long)destination;
	command.blockSize = 512;
	command.blockCount = count;
	if (command.command == 18) {
		result = setEmmcBlockCount(card, count);
		if (result < 0)
			return result;
	}
	result = sendSdifCommand(controller, &command, 0);
	if (result < 0) {
		if (command.command == 18 && result != (int)0x8032011dU &&
		    result != (int)0x8032011eU && result != (int)0x8032011fU)
			stopSdifTransfer(controller, 0);
		return result;
	}
	result = readSdifCardStatus(controller, 0);
	return result < 0 ? result : 0;
}

static unsigned int __attribute__((noinline, noclone))
readEmmcBits(const unsigned char *data, unsigned int first,
	     unsigned int count)
{
	unsigned int result = 0;
	unsigned int i;

	data += first >> 3;
	first &= 7;
	for (i = 0; i < count; i++) {
		if (*data & (1U << first))
			result |= 1U << (i & 31);
		if (++first == 8) {
			first = 0;
			data++;
		}
	}
	return result;
}

int openEmmc(unsigned int port, struct EmmcCard **resultCard)
{
	struct FirstLoaderSdif *controller = getSdif(port);
	struct EmmcCard *card = getEmmcCard();
	const unsigned char *csd = card->csd;
	unsigned int response;
	unsigned int multiplier;
	unsigned int unit;
	int result;

	if (controller == 0)
		return -1;
	result = isSdifCardPresent(controller);
	if (result < 0)
		return result;
	if (result == 0)
		return -1;
	card->controllerAddress = (unsigned int)(unsigned long)controller;
	card->flags = 0;
	result = powerOnSdif(controller);
	if (result < 0)
		return result;
	result = idleSdifCard(controller);
	if (result < 0)
		return result;
	result = sendEmmcOperationConditions(card, controller->flags | 0x40000000U,
					     &response);
	if (result < 0)
		return result;
	result = readEmmcCid(card, (unsigned int *)card->cid);
	if (result < 0)
		return result;
	result = setEmmcRelativeAddress(card, 1);
	if (result < 0)
		return result;
	controller->relativeAddress = 1;
	result = readEmmcCsd(card, (unsigned int *)card->csd);
	if (result < 0)
		return result;
	card->csdStructure = readEmmcBits(csd, 0x7e, 2);
	card->specificationVersion = readEmmcBits(csd, 0x7a, 4);
	multiplier = readEmmcBits(csd, 0x73, 4);
	unit = readEmmcBits(csd, 0x70, 3);
	card->accessTime = emmcTimeMultipliers[multiplier] * emmcTimeUnits[unit] / 10;
	card->accessClocks = readEmmcBits(csd, 0x68, 8) * 100;
	multiplier = readEmmcBits(csd, 0x63, 4);
	unit = readEmmcBits(csd, 0x60, 3);
	card->maximumClock = emmcClockMultipliers[multiplier] * emmcClockUnits[unit];
	multiplier = readEmmcBits(csd, 0x2f, 3);
	card->blockCount = (readEmmcBits(csd, 0x3e, 12) + 1) <<
			   ((multiplier + 2) & 31);
	card->commandClasses = readEmmcBits(csd, 0x54, 12);
	card->readBlockLength = readEmmcBits(csd, 0x50, 4);
	card->readBlockPartial = readEmmcBits(csd, 0x4f, 1);
	card->writeBlockMisalign = readEmmcBits(csd, 0x4e, 1);
	card->readBlockMisalign = readEmmcBits(csd, 0x4d, 1);
	card->readToWriteFactor = readEmmcBits(csd, 0x1a, 3);
	card->writeBlockLength = readEmmcBits(csd, 0x16, 4);
	card->writeBlockPartial = readEmmcBits(csd, 0x15, 1);
	result = selectEmmcCard(card, 1);
	if (result < 0)
		return result;
	result = readEmmcExtendedCsd(card, card->extendedCsd);
	if (result < 0)
		return result;
	card->sectorCount = readLittleEndianWord(card->extendedCsd + 0xd4);
	card->flags |= 1;
	switch (card->extendedCsd[0xc4] & 3) {
	case 1: card->highSpeedClock = 26000000; break;
	case 2: case 3: card->highSpeedClock = 52000000; break;
	default: break;
	}
	result = setEmmcBlockLength(card, 512);
	if (result < 0)
		return result;
	if (card->highSpeedClock != 0) {
		result = switchEmmcSetting(card, 0, 0xb9, 1);
		if (result < 0)
			return result;
		result = setSdifClock(controller, card->highSpeedClock, 1);
	} else {
		result = setSdifClock(controller, card->maximumClock, 0);
	}
	if (result < 0)
		return result;
	if (resultCard != 0)
		*resultCard = card;
	return 0;
}

int sendSdifInterfaceCondition(struct SdifCard *card,
			       unsigned int voltageFlags)
{
	struct FirstLoaderSdif *controller =
		(struct FirstLoaderSdif *)(unsigned long)card->controllerAddress;
	struct FirstLoaderSdifCommand command;

	command.size = sizeof(command);
	command.flags = 0x92;
	command.command = 8;
	command.argument = (voltageFlags & 0xff8000U) ? 0x1aa : 0xaa;
	command.bufferAddress = 0;
	return sendSdifCommand(controller, &command, 3);
}

int readSdifCidForCard(struct SdifCard *card, unsigned int *cid)
{
	struct FirstLoaderSdif *controller =
		(struct FirstLoaderSdif *)(unsigned long)card->controllerAddress;

	return readSdifCid(controller, cid);
}

int readSdifRelativeAddress(struct SdifCard *card,
			    unsigned short *relativeAddress)
{
	struct FirstLoaderSdif *controller =
		(struct FirstLoaderSdif *)(unsigned long)card->controllerAddress;
	struct FirstLoaderSdifCommand command;
	int result;

	command.size = sizeof(command);
	command.flags = 0x82;
	command.command = 3;
	command.argument = 0;
	result = sendSdifCommand(controller, &command, 3);
	if (result < 0)
		return result;
	if (relativeAddress != 0)
		*relativeAddress = command.response[0] >> 16;
	return 0;
}

int selectSdifCardForCard(struct SdifCard *card, unsigned int select)
{
	struct FirstLoaderSdif *controller =
		(struct FirstLoaderSdif *)(unsigned long)card->controllerAddress;

	return selectSdifCard(controller, select);
}

int readSdifCsdForCard(struct SdifCard *card, unsigned int *csd)
{
	struct FirstLoaderSdif *controller =
		(struct FirstLoaderSdif *)(unsigned long)card->controllerAddress;

	return readSdifCsd(controller, csd);
}

int readSdifCidWithRcaForCard(struct SdifCard *card, unsigned int *cid)
{
	struct FirstLoaderSdif *controller =
		(struct FirstLoaderSdif *)(unsigned long)card->controllerAddress;

	return readSdifCidWithRca(controller, cid);
}

int readSdifStatusForCard(struct SdifCard *card, unsigned int *status)
{
	struct FirstLoaderSdif *controller =
		(struct FirstLoaderSdif *)(unsigned long)card->controllerAddress;

	return readSdifCardStatus(controller, status);
}

int setSdifBlockLengthForCard(struct SdifCard *card, unsigned int length)
{
	struct FirstLoaderSdif *controller =
		(struct FirstLoaderSdif *)(unsigned long)card->controllerAddress;

	return setSdifBlockLength(controller, length);
}

int sendSdifApplicationCommand(struct SdifCard *card,
			       struct FirstLoaderSdifCommand *command,
			       int retries)
{
	struct FirstLoaderSdif *controller =
		(struct FirstLoaderSdif *)(unsigned long)card->controllerAddress;
	struct FirstLoaderSdifCommand prefixCommand;
	int attempt;
	int result = -1;

	if (retries < 1)
		retries = 1;
	for (attempt = 0; attempt < retries; attempt++) {
		prefixCommand.size = sizeof(prefixCommand);
		prefixCommand.flags = 0x13;
		prefixCommand.command = 55;
		prefixCommand.argument =
			(unsigned int)controller->relativeAddress << 16;
		prefixCommand.bufferAddress = 0;
		result = sendSdifCommand(controller, &prefixCommand, 0);
		if (result < 0)
			continue;
		if ((prefixCommand.response[0] & 0x20U) == 0)
			return (int)0x80320105U;
		result = sendSdifCommand(controller, command, 0);
		if (result >= 0)
			return result;
	}
	return result;
}

int requestSdifOperatingConditions(struct SdifCard *card,
				   unsigned int conditions,
				   unsigned int *response)
{
	struct FirstLoaderSdifCommand command;
	unsigned int attempts = 0;
	int result;

	command.size = sizeof(command);
	command.flags = 0x42;
	command.command = 41;
	command.argument = conditions;
	command.bufferAddress = 0;
	do {
		result = sendSdifApplicationCommand(card, &command, 3);
		if (result < 0)
			return result;
		if (conditions == 0 || (command.response[0] & 0x80000000U))
			break;
		delayMicroseconds(10000);
		attempts++;
		if (attempts >= 100)
			return (int)0x80320002U;
	} while (1);
	if (response != 0)
		*response = command.response[0];
	return 0;
}

int setSdifCardDetectPullup(struct SdifCard *card,
			    unsigned int enable)
{
	struct FirstLoaderSdifCommand command;
	int result;

	command.size = sizeof(command);
	command.flags = 0x13;
	command.command = 42;
	command.argument = enable != 0;
	result = sendSdifApplicationCommand(card, &command, 3);
	if (result < 0)
		return result;
	return decodeSdifCardStatus(command.response[0]);
}

int readSdifScr(struct SdifCard *card, void *destination)
{
	struct FirstLoaderSdifCommand command;
	int result;

	command.size = sizeof(command);
	command.flags = 0x114;
	command.command = 51;
	command.argument = 0;
	command.bufferAddress = (unsigned int)(unsigned long)destination;
	command.blockSize = 8;
	command.blockCount = 0;
	result = sendSdifApplicationCommand(card, &command, 3);
	return result < 0 ? result : 0;
}

int readSdifBlocks(struct SdifCard *card, unsigned int block,
		   void *destination, unsigned int count)
{
	struct FirstLoaderSdif *controller =
		(struct FirstLoaderSdif *)(unsigned long)card->controllerAddress;
	struct FirstLoaderSdifCommand command = { 0 };
	int result;

	command.size = sizeof(command);
	command.flags = count == 1 ? 0x114 : 0x914;
	command.command = count == 1 ? 17 : 18;
	command.argument = (card->flags & 1) ? block : block << 9;
	command.bufferAddress = (unsigned int)(unsigned long)destination;
	command.blockSize = 0x200;
	command.blockCount = count;
	result = sendSdifCommand(controller, &command, 0);
	if (result < 0) {
		if (command.command == 18)
			stopSdifTransfer(controller, 1);
		return result;
	}
	result = readSdifCardStatus(controller, 0);
	return result < 0 ? result : 0;
}

/* Read bits in little-endian bit order, as used by the card registers. */
unsigned int readBits(const unsigned char *data, unsigned int first,
		      unsigned int count)
{
	unsigned int result = 0;
	unsigned int i;

	data += first >> 3;
	first &= 7;
	for (i = 0; i < count; i++) {
		if (*data & (1U << first))
			result |= 1U << (i & 31);
		if (++first == 8) {
			first = 0;
			data++;
		}
	}
	return result;
}

/* Keep SD initialization compact enough for the retail data addresses. */
__attribute__((optimize("Os")))
int openSdifCard(unsigned int port, struct SdifCard **resultCard)
{
	struct FirstLoaderSdif *controller = getSdif(port);
	struct SdifCard *card;
	unsigned short relativeAddress;
	unsigned int multiplier;
	unsigned int unit;
	unsigned int conditions;
	unsigned int csdStructure;
	int result;

	if (controller == 0)
		return -1;
	result = isSdifCardPresent(controller);
	if (result < 0)
		return result;
	if (result == 0)
		return (int)0x80320001U;
	card = getSdifCard();
	card->controllerAddress = (unsigned int)(unsigned long)controller;
	card->flags = 0;
	if (sdifPowerState != 0) {
		result = powerOffSdif(controller);
		if (result < 0)
			return result;
		delayMicroseconds(10000);
	}
	result = powerOnSdif(controller);
	if (result < 0)
		return result;
	sdifPowerState = 1;
	delayMicroseconds(10000);
	result = resetSdif(controller, 2);
	if (result < 0)
		return result;
	result = idleSdifCard(controller);
	if (result < 0)
		return result;

	conditions = controller->flags;
	result = sendSdifInterfaceCondition(card, conditions);
	if (result == 0)
		conditions |= 0x40000000U;
	controller->relativeAddress = 0;
	result = requestSdifOperatingConditions(card, conditions, 0);
	if (result < 0)
		return result;
	result = readSdifCidForCard(card, (unsigned int *)card->cid);
	if (result < 0)
		return result;
	result = readSdifRelativeAddress(card, &relativeAddress);
	if (result < 0)
		return result;
	if (relativeAddress == 0) {
		result = readSdifRelativeAddress(card, &relativeAddress);
		if (result < 0)
			return result;
	}
	controller->relativeAddress = relativeAddress;
	result = selectSdifCardForCard(card, 1);
	if (result < 0)
		return result;
	result = setSdifCardDetectPullup(card, 0);
	if (result < 0)
		return result;
	selectSdifCardForCard(card, 0);
	result = readSdifCsdForCard(card, (unsigned int *)card->csd);
	if (result < 0)
		return result;
	result = selectSdifCardForCard(card, 1);
	if (result < 0)
		return result;

	csdStructure = readBits(card->csd, 0x7e, 2);
	card->csdStructure = csdStructure;
	unit = readBits(card->csd, 0x60, 3);
	multiplier = readBits(card->csd, 99, 4);
	card->maximumClock = sdifClockMultipliers[multiplier] *
			     sdifClockUnits[unit];
	card->commandClasses = readBits(card->csd, 0x54, 12);
	if (csdStructure == 1) {
		card->sectorCount = (readBits(card->csd, 0x30, 0x16) + 1) * 0x400;
		card->flags |= 1;
	} else if (csdStructure == 0) {
		multiplier = readBits(card->csd, 0x2f, 3);
		card->sectorCount = (readBits(card->csd, 0x3e, 12) + 1) <<
				    (multiplier + 2);
	} else {
		return -1;
	}

	result = readSdifScr(card, card->scr);
	if (result < 0)
		return result;
	card->scrStructure = card->scr[0] >> 4;
	card->sdSpecification = card->scr[0] & 0xf;
	card->dataStatusAfterErase = card->scr[1] >> 7;
	card->securitySupport = (card->scr[1] >> 4) & 7;
	card->busWidthSupport = card->scr[1] & 0xf;
	if (card->sdSpecification == 2 && (card->scr[2] & 0x80))
		card->sdSpecification = 3;
	card->commandSupport = card->scr[3] & 2;
	result = setSdifBlockLengthForCard(card, 0x200);
	if (result < 0)
		return result;
	if (resultCard != 0)
		*resultCard = card;
	return 0;
}

int probeExternalBoot(void)
{
	unsigned int challenge[4];
	unsigned int response[4];
	unsigned int encrypted[4];
	int result;

	GPIO->direction = 8;
	if (!(GPIO->input & 0x10)) {
		fillBytes(externalBootKey, 0, sizeof(externalBootKey));
		return 0;
	}
	CMEP_RESET_FLAGS |= 0x10;
	challenge[0] = BIGMAC_RANDOM;
	challenge[1] = BIGMAC_RANDOM;
	challenge[2] = challenge[0];
	challenge[3] = challenge[1];
	CMEP->cry2Debugger[0] = challenge[0];
	CMEP->cry2Debugger[1] = challenge[1];
	GPIO->set = 8;
	while (GPIO->input & 0x10)
		;
	response[0] = CMEP->debugger2Cry[0];
	response[1] = CMEP->debugger2Cry[1];
	CMEP->debugger2Cry[0] = 0xffffffffU;
	CMEP->debugger2Cry[1] = 0xffffffffU;
	CMEP->debugger2Cry2[0] = 0xffffffffU;
	CMEP->debugger2Cry2[1] = 0xffffffffU;
	GPIO->clear = 8;
	/* Encrypt the 16-byte challenge; the reply contains only eight bytes. */
	result = bigmacRunMode301((unsigned int)(unsigned long)encrypted,
				  (unsigned int)(unsigned long)challenge, sizeof(challenge),
				  (unsigned int)(unsigned long)externalBootKey);
	if (result < 0 || compareBytes(response, encrypted, 8))
		result = -1;
	else
		result = 1;
	fillBytes(externalBootKey, 0, sizeof(externalBootKey));
	fillBytes(challenge, 0, sizeof(challenge));
	fillBytes(response, 0, sizeof(response));
	fillBytes(encrypted, 0, sizeof(encrypted));
	return result;
}

/* Copy whole words and return their sum. When size / 4 is zero the original
 * hardware loop counter wraps, so that case requires a full counter cycle.
 */
unsigned int __attribute__((noinline))
copyWordsAndSum(unsigned int *dst, const unsigned int *src, unsigned int size)
{
	unsigned int words = size >> 2;
	unsigned int sum = 0;

	do {
		unsigned int word = *src++;

		*dst++ = word;
		sum += word;
	} while (--words != 0);

	return sum;
}

/* Retail calls this shared helper from multiple boot paths. */
void *__attribute__((noinline, noclone))
fillBytes(void *dst, int value, unsigned int size)
{
	unsigned char *p = dst;
	unsigned int i;

	for (i = 0; i < size; i++)
		p[i] = value;
	return dst;
}

/* Returns the OR of all byte differences, not a lexicographic comparison. */
unsigned int compareBytes(const void *left, const void *right, unsigned int size)
{
	const unsigned char *a = left;
	const unsigned char *b = right;
	unsigned int difference = 0;
	unsigned int i;

	for (i = 0; i < size; i++)
		difference |= a[i] ^ b[i];
	return difference;
}

static volatile SdifHostRegisters *sdifRegisters(const struct FirstLoaderSdif *controller)
{
	return (volatile SdifHostRegisters *)(unsigned long)controller->registersAddress;
}

static struct FirstLoaderSdif *emmcController(const struct EmmcCard *card)
{
	return (struct FirstLoaderSdif *)(unsigned long)card->controllerAddress;
}

static unsigned int readLittleEndianWord(const unsigned char *bytes)
{
	return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8) |
	       ((unsigned int)bytes[2] << 16) | ((unsigned int)bytes[3] << 24);
}
