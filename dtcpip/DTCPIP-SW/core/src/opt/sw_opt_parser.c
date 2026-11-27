/*
 * MPEG-TS PAT/PMT section reassembler and DTCP descriptor parser
 * (DTCPIP-SW/core/src/opt/sw_opt_parser.c, from its trace strings): parses TS packet headers,
 * reassembles PAT and fragmented PMT sections, verifies MPEG-2 CRC-32, rewrites Move CCI bits
 * when requested, extracts PMT PID and embedded DTCP CCI descriptors.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SW_OPT_PARSER_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-SW\\core\\src\\opt\\sw_opt_parser.c"
#define SW_OPT_ERR_INVALID	11
#define SW_OPT_TS_PACKET_SIZE	188
#define SW_OPT_MAX_SECTION_LEN	1021	/* 0x3FD */
#define SW_OPT_MAX_DESC_LEN	1009	/* 0x3F1 */
#define SW_OPT_DTCP_TAG		136	/* 0x88 */

struct SwOptSectionBuffer {
	signed char tableId;
	unsigned char packetCount;
	unsigned char continuityCounter;
	unsigned char crcBytes[4];
	unsigned char pad;
	unsigned int sectionLength;
	unsigned char sectionData[1024];
};

struct SwOptParseResult {
	unsigned short pmtPid;
	unsigned short pad;
	int hasPmt;
	unsigned char cci;
	unsigned char epn;
	unsigned char dtcpDescriptor[6];
	unsigned int errorFlags;
};

/* external software-Wagner trace logger */
extern int swMwTrace(const char *format, ...);
#define swLogPrintf swMwTrace

/* trace verbosity level */
int g_swOptLogLevel;

/* fragmented PMT rewrite parser state */
static unsigned char *g_swOptScanPtr;
static unsigned char *g_swOptWritePtr;
static unsigned char *g_swOptEndPtr;
static unsigned char g_swOptScanPhase;
static unsigned int g_swOptProgramInfoRemain;
static unsigned int g_swOptEsInfoRemain;

/* standard MPEG-2 CRC-32 table, polynomial 0x04c11db7 */
static const unsigned int g_swOptCrc32Table[256] = {
	0x00000000u, 0x04c11db7u, 0x09823b6eu, 0x0d4326d9u,
	0x130476dcu, 0x17c56b6bu, 0x1a864db2u, 0x1e475005u,
	0x2608edb8u, 0x22c9f00fu, 0x2f8ad6d6u, 0x2b4bcb61u,
	0x350c9b64u, 0x31cd86d3u, 0x3c8ea00au, 0x384fbdbdu,
	0x4c11db70u, 0x48d0c6c7u, 0x4593e01eu, 0x4152fda9u,
	0x5f15adacu, 0x5bd4b01bu, 0x569796c2u, 0x52568b75u,
	0x6a1936c8u, 0x6ed82b7fu, 0x639b0da6u, 0x675a1011u,
	0x791d4014u, 0x7ddc5da3u, 0x709f7b7au, 0x745e66cdu,
	0x9823b6e0u, 0x9ce2ab57u, 0x91a18d8eu, 0x95609039u,
	0x8b27c03cu, 0x8fe6dd8bu, 0x82a5fb52u, 0x8664e6e5u,
	0xbe2b5b58u, 0xbaea46efu, 0xb7a96036u, 0xb3687d81u,
	0xad2f2d84u, 0xa9ee3033u, 0xa4ad16eau, 0xa06c0b5du,
	0xd4326d90u, 0xd0f37027u, 0xddb056feu, 0xd9714b49u,
	0xc7361b4cu, 0xc3f706fbu, 0xceb42022u, 0xca753d95u,
	0xf23a8028u, 0xf6fb9d9fu, 0xfbb8bb46u, 0xff79a6f1u,
	0xe13ef6f4u, 0xe5ffeb43u, 0xe8bccd9au, 0xec7dd02du,
	0x34867077u, 0x30476dc0u, 0x3d044b19u, 0x39c556aeu,
	0x278206abu, 0x23431b1cu, 0x2e003dc5u, 0x2ac12072u,
	0x128e9dcfu, 0x164f8078u, 0x1b0ca6a1u, 0x1fcdbb16u,
	0x018aeb13u, 0x054bf6a4u, 0x0808d07du, 0x0cc9cdcau,
	0x7897ab07u, 0x7c56b6b0u, 0x71159069u, 0x75d48ddeu,
	0x6b93dddbu, 0x6f52c06cu, 0x6211e6b5u, 0x66d0fb02u,
	0x5e9f46bfu, 0x5a5e5b08u, 0x571d7dd1u, 0x53dc6066u,
	0x4d9b3063u, 0x495a2dd4u, 0x44190b0du, 0x40d816bau,
	0xaca5c697u, 0xa864db20u, 0xa527fdf9u, 0xa1e6e04eu,
	0xbfa1b04bu, 0xbb60adfcu, 0xb6238b25u, 0xb2e29692u,
	0x8aad2b2fu, 0x8e6c3698u, 0x832f1041u, 0x87ee0df6u,
	0x99a95df3u, 0x9d684044u, 0x902b669du, 0x94ea7b2au,
	0xe0b41de7u, 0xe4750050u, 0xe9362689u, 0xedf73b3eu,
	0xf3b06b3bu, 0xf771768cu, 0xfa325055u, 0xfef34de2u,
	0xc6bcf05fu, 0xc27dede8u, 0xcf3ecb31u, 0xcbffd686u,
	0xd5b88683u, 0xd1799b34u, 0xdc3abdedu, 0xd8fba05au,
	0x690ce0eeu, 0x6dcdfd59u, 0x608edb80u, 0x644fc637u,
	0x7a089632u, 0x7ec98b85u, 0x738aad5cu, 0x774bb0ebu,
	0x4f040d56u, 0x4bc510e1u, 0x46863638u, 0x42472b8fu,
	0x5c007b8au, 0x58c1663du, 0x558240e4u, 0x51435d53u,
	0x251d3b9eu, 0x21dc2629u, 0x2c9f00f0u, 0x285e1d47u,
	0x36194d42u, 0x32d850f5u, 0x3f9b762cu, 0x3b5a6b9bu,
	0x0315d626u, 0x07d4cb91u, 0x0a97ed48u, 0x0e56f0ffu,
	0x1011a0fau, 0x14d0bd4du, 0x19939b94u, 0x1d528623u,
	0xf12f560eu, 0xf5ee4bb9u, 0xf8ad6d60u, 0xfc6c70d7u,
	0xe22b20d2u, 0xe6ea3d65u, 0xeba91bbcu, 0xef68060bu,
	0xd727bbb6u, 0xd3e6a601u, 0xdea580d8u, 0xda649d6fu,
	0xc423cd6au, 0xc0e2d0ddu, 0xcda1f604u, 0xc960ebb3u,
	0xbd3e8d7eu, 0xb9ff90c9u, 0xb4bcb610u, 0xb07daba7u,
	0xae3afba2u, 0xaafbe615u, 0xa7b8c0ccu, 0xa379dd7bu,
	0x9b3660c6u, 0x9ff77d71u, 0x92b45ba8u, 0x9675461fu,
	0x8832161au, 0x8cf30badu, 0x81b02d74u, 0x857130c3u,
	0x5d8a9099u, 0x594b8d2eu, 0x5408abf7u, 0x50c9b640u,
	0x4e8ee645u, 0x4a4ffbf2u, 0x470cdd2bu, 0x43cdc09cu,
	0x7b827d21u, 0x7f436096u, 0x7200464fu, 0x76c15bf8u,
	0x68860bfdu, 0x6c47164au, 0x61043093u, 0x65c52d24u,
	0x119b4be9u, 0x155a565eu, 0x18197087u, 0x1cd86d30u,
	0x029f3d35u, 0x065e2082u, 0x0b1d065bu, 0x0fdc1becu,
	0x3793a651u, 0x3352bbe6u, 0x3e119d3fu, 0x3ad08088u,
	0x2497d08du, 0x2056cd3au, 0x2d15ebe3u, 0x29d4f654u,
	0xc5a92679u, 0xc1683bceu, 0xcc2b1d17u, 0xc8ea00a0u,
	0xd6ad50a5u, 0xd26c4d12u, 0xdf2f6bcbu, 0xdbee767cu,
	0xe3a1cbc1u, 0xe760d676u, 0xea23f0afu, 0xeee2ed18u,
	0xf0a5bd1du, 0xf464a0aau, 0xf9278673u, 0xfde69bc4u,
	0x89b8fd09u, 0x8d79e0beu, 0x803ac667u, 0x84fbdbd0u,
	0x9abc8bd5u, 0x9e7d9662u, 0x933eb0bbu, 0x97ffad0cu,
	0xafb010b1u, 0xab710d06u, 0xa6322bdfu, 0xa2f33668u,
	0xbcb4666du, 0xb8757bdau, 0xb5365d03u, 0xb1f740b4u,
};

/* forward declarations */
void swOptSectionFini(struct SwOptSectionBuffer *section);
struct SwOptSectionBuffer *swOptSectionReset(struct SwOptSectionBuffer *section);
int swOptSectionPutPat(struct SwOptSectionBuffer *section, const unsigned char *packet,
		       unsigned int packetLen, int rewriteMove, int *complete,
		       unsigned char **outData, unsigned int *outLen);
int swOptSectionPutPmt(struct SwOptSectionBuffer *section, unsigned char *packet,
		       unsigned int packetLen, int rewriteMove, int *complete,
		       unsigned char **outData, unsigned int *outLen);
int swOptParseTsHeader(const unsigned char *packet, unsigned char *payloadOffset,
		       unsigned char *continuityCounter, unsigned char *pusiOut);
int swOptCheckPmtHeader(const unsigned char *section, unsigned int *outSectionLen);
int swOptScanFirstLoopDescriptors(const unsigned char *buf, unsigned int len,
				  const unsigned char **outCa, const unsigned char **outOther,
				  const unsigned char **outDtcp);
int swOptScanSecondLoopDescriptors(const unsigned char *buf, unsigned int len,
				   const unsigned char **outCa, const unsigned char **outOther,
				   const unsigned char **outDtcp);
int swOptCompareCciStrictness(unsigned char firstCci, unsigned char secondCci);

/* initialize a section reassembler buffer */
int swOptSectionInit(struct SwOptSectionBuffer *section)
{
	if (section == NULL)
		return SW_OPT_ERR_INVALID;
	swOptSectionReset(section);
	swOptSectionFini(section);
	return 0;
}

/* section teardown no-op */
void swOptSectionFini(struct SwOptSectionBuffer *section)
{
	(void)section;
}

/* clear a section reassembler buffer */
struct SwOptSectionBuffer *swOptSectionReset(struct SwOptSectionBuffer *section)
{
	if (section != NULL) {
		section->tableId = -1;
		section->packetCount = 0;
		section->continuityCounter = 0;
		section->sectionLength = 0;
		return (struct SwOptSectionBuffer *)memset(section->sectionData, 0,
							   sizeof(section->sectionData));
	}
	return section;
}

/* initialize a PAT/PMT parse result struct */
int swOptParseResultInit(struct SwOptParseResult *result)
{
	if (result != NULL) {
		result->pmtPid = 0xFFFFu;
		result->hasPmt = 0;
		result->cci = 3;
		result->epn = 1;
		result->errorFlags = 0;
		return 0;
	}
	return 0;
}

/* parse result teardown no-op */
void swOptParseResultFini(struct SwOptParseResult *result)
{
	(void)result;
}

/* ingest one 188-byte MPEG-TS packet into a PAT/PMT section buffer */
int sw_opt_section_put(struct SwOptSectionBuffer *section, unsigned char *packet,
		       unsigned int packetLen, int rewriteMove, int *complete,
		       unsigned char **outData, unsigned int *outLen)
{
	unsigned char payloadOffset;
	unsigned char pusi;

	if (section == NULL || packet == NULL || packetLen < SW_OPT_TS_PACKET_SIZE ||
	    complete == NULL || outData == NULL || outLen == NULL)
		return SW_OPT_ERR_INVALID;

	*complete = 0;
	*outData = NULL;
	*outLen = 0;

	if (swOptParseTsHeader(packet, &payloadOffset, &section->continuityCounter, &pusi) != 0) {
		if (g_swOptLogLevel >= 4)
			swLogPrintf("sw opt: %s:%s:%dinvalid TS heaer.\n",
				    "sw_opt_section_put", SW_OPT_PARSER_SOURCE, 162);
		return SW_OPT_ERR_INVALID;
	}

	if (pusi == 1) {
		swOptSectionReset(section);
		if (packet[4 + payloadOffset] == 0) {
			section->tableId = 0;
		} else if (packet[4 + payloadOffset] == 2) {
			section->tableId = 2;
		} else {
			swLogPrintf("sw opt: not PAT/PMT table id.\n");
			return SW_OPT_ERR_INVALID;
		}
	}

	if (section->tableId == 0) {
		if (swOptSectionPutPat(section, packet, packetLen, rewriteMove,
				       complete, outData, outLen) != 0)
			return SW_OPT_ERR_INVALID;
	} else if (section->tableId == 2) {
		if (swOptSectionPutPmt(section, packet, packetLen, rewriteMove,
				       complete, outData, outLen) != 0)
			return SW_OPT_ERR_INVALID;
	} else {
		section->tableId = -1;
		swLogPrintf("sw opt: unspecified fragmented section.\n");
		return SW_OPT_ERR_INVALID;
	}
	return 0;
}

/* map 2-bit DTCP CCI and EPN bit to DTCP E-EMI value */
int swOptCciToEemi(unsigned char cci, unsigned char epn)
{
	switch (cci) {
	case 0:
		return (epn != 0) ? 0 : 2;
	case 1:
		return 4;
	case 2:
		return 8;
	case 3:
		return 12;
	default:
		return 0xFF;
	}
}

/* verify that an embedded E-EMI is compatible with the stream E-EMI */
int swOptCheckEemiCompatibility(unsigned char streamEemi, unsigned char embeddedEemi)
{
	if (embeddedEemi == 6)
		return 0;
	switch (streamEemi) {
	case 0:
	case 12:
		return 1;
	case 2:
		return embeddedEemi != 12 && embeddedEemi != 4 &&
		       embeddedEemi != 8 && embeddedEemi != 10;
	case 4:
	case 6:
		return embeddedEemi != 12;
	case 8:
	case 10:
		return embeddedEemi != 12 && embeddedEemi != 4;
	default:
		return 0;
	}
}

/* compute big-endian MPEG-2 CRC-32 over a byte range */
int swOptCrc32(const unsigned char *bytes, unsigned int length, const unsigned int *table,
	       const unsigned int *seedIn, unsigned char useSeed, void *outCrc)
{
	unsigned int crc;
	unsigned int i;
	const unsigned int *crcTable;
	unsigned int swapped;

	if (bytes == NULL || length == 0 || outCrc == NULL)
		return SW_OPT_ERR_INVALID;
	if (useSeed != 0 && seedIn == NULL)
		return SW_OPT_ERR_INVALID;

	crcTable = (table != NULL) ? table : g_swOptCrc32Table;
	if (useSeed != 0) {
		unsigned int seed = *seedIn;

		crc = ((seed & 0x000000FFu) << 24) | ((seed & 0x0000FF00u) << 8) |
		      ((seed & 0x00FF0000u) >> 8) | ((seed & 0xFF000000u) >> 24);
	} else {
		crc = 0xFFFFFFFFu;
	}

	for (i = 0; i < length; ++i)
		crc = (crc << 8) ^ crcTable[(unsigned char)((crc >> 24) ^ bytes[i])];

	swapped = ((crc & 0x000000FFu) << 24) | ((crc & 0x0000FF00u) << 8) |
		  ((crc & 0x00FF0000u) >> 8) | ((crc & 0xFF000000u) >> 24);
	memcpy(outCrc, &swapped, 4);
	return 0;
}

/* parse a reassembled PAT section and extract the unique PMT PID */
int sw_opt_parser_parsepat(struct SwOptParseResult *out, const unsigned char *section,
			   unsigned int length)
{
	unsigned int cursor;
	unsigned int sectionLength;
	unsigned char crc[4];

	if (out == NULL || section == NULL || length < 12u)
		return SW_OPT_ERR_INVALID;

	out->pmtPid = 0xFFFFu;
	out->hasPmt = 0;
	out->cci = 0;
	out->epn = 1;
	out->errorFlags = 0;

	if (section[0] != 0) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: not PAT table id.\n");
		return SW_OPT_ERR_INVALID;
	}
	if ((section[1] >> 7) != 1) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: section syntax indicator is zero.\n");
		return SW_OPT_ERR_INVALID;
	}
	if (((section[1] >> 6) & 1u) != 0) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: not zero in fixed zero field.\n");
		return SW_OPT_ERR_INVALID;
	}

	sectionLength = (((unsigned int)(section[1] & 0x0Fu)) << 8) | section[2];
	if (sectionLength > SW_OPT_MAX_SECTION_LEN || sectionLength == 0 ||
	    sectionLength > length || sectionLength != length - 3u) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: invalid section length.\n");
		return SW_OPT_ERR_INVALID;
	}
	if ((section[5] & 1u) != 1u) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: current next indicator is zero.\n");
		return SW_OPT_ERR_INVALID;
	}
	if (section[6] != 0) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: section number isn't 0.\n");
		return SW_OPT_ERR_INVALID;
	}
	if (section[7] != 0) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: last section number isn't 0.\n");
		return SW_OPT_ERR_INVALID;
	}

	swOptCrc32(section, sectionLength - 1u, NULL, NULL, 0, crc);
	if (crc[0] != section[sectionLength - 1u] ||
	    crc[1] != section[sectionLength] ||
	    crc[2] != section[sectionLength + 1u] ||
	    crc[3] != section[sectionLength + 2u]) {
		out->errorFlags = 0x2000000u;
		swLogPrintf("sw opt: different from computed section crc.\n");
		return SW_OPT_ERR_INVALID;
	}

	for (cursor = 8; cursor < length - 4u; cursor += 4u) {
		unsigned short programNum = (unsigned short)((section[cursor] << 8) |
							     section[cursor + 1u]);

		if (programNum == 0)
			continue;
		if (out->pmtPid != 0xFFFFu)
			return SW_OPT_ERR_INVALID;
		out->pmtPid = (unsigned short)(((section[cursor + 2u] << 8) |
						section[cursor + 3u]) & 0x1FFFu);
		if (g_swOptLogLevel >= 4)
			swLogPrintf("sw opt: %s:%s:%dfound PMT pid  0x%04x.\n",
				    "sw_opt_parser_parsepat", SW_OPT_PARSER_SOURCE,
				    413, out->pmtPid);
	}
	return (out->pmtPid == 0xFFFFu) ? SW_OPT_ERR_INVALID : 0;
}

/* parse a reassembled PMT section and extract DTCP CCI/EPN descriptors */
int sw_opt_parser_parsepmt(struct SwOptParseResult *out, const unsigned char *section,
			   unsigned int length)
{
	unsigned char cci;
	unsigned char epn;
	const unsigned char *esCursor;
	unsigned int sectionLength;
	unsigned int esLoopLen;
	unsigned int programInfoLen;
	unsigned int esInfoLen;
	unsigned int esOffset;
	int hasDescriptor = 0;
	const unsigned char *caDesc;
	const unsigned char *otherDesc;
	const unsigned char *dtcpDesc;
	unsigned char crc[4];
	unsigned char chosenDesc[6];

	if (out == NULL || section == NULL || length < 16u)
		return SW_OPT_ERR_INVALID;

	out->hasPmt = 0;
	out->cci = 0;
	out->epn = 1;
	out->errorFlags = 0;

	if (section[0] != 2) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: not PAT table id.\n");
		return SW_OPT_ERR_INVALID;
	}
	if ((section[1] >> 7) != 1) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: section syntax indicator is zero.\n");
		return SW_OPT_ERR_INVALID;
	}
	if (((section[1] >> 6) & 1u) != 0) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: not zero in fixed zero field.\n");
		return SW_OPT_ERR_INVALID;
	}

	sectionLength = (((unsigned int)(section[1] & 0x0Fu)) << 8) | section[2];
	if (sectionLength > SW_OPT_MAX_SECTION_LEN || sectionLength == 0) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: invalid section length.\n");
		return SW_OPT_ERR_INVALID;
	}
	if ((section[5] & 1u) != 1u) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: current next indicator is zero.\n");
		return SW_OPT_ERR_INVALID;
	}
	if (section[6] != 0) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: section number isn't 0.\n");
		return SW_OPT_ERR_INVALID;
	}
	if (section[7] != 0) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: last section number isn't 0.\n");
		return SW_OPT_ERR_INVALID;
	}

	programInfoLen = (((unsigned int)(section[10] & 0x0Fu)) << 8) | section[11];
	if (programInfoLen > SW_OPT_MAX_DESC_LEN) {
		out->errorFlags = 0x1000000u;
		swLogPrintf("sw opt: invalid section length.\n");
		return SW_OPT_ERR_INVALID;
	}

	swOptCrc32(section, sectionLength - 1u, NULL, NULL, 0, crc);
	if (crc[0] != section[sectionLength - 1u] ||
	    crc[1] != section[sectionLength] ||
	    crc[2] != section[sectionLength + 1u] ||
	    crc[3] != section[sectionLength + 2u]) {
		out->errorFlags = 0x2000000u;
		swLogPrintf("sw opt: different from computed section crc.\n");
		return SW_OPT_ERR_INVALID;
	}

	caDesc = NULL;
	otherDesc = NULL;
	dtcpDesc = NULL;
	if (swOptScanFirstLoopDescriptors(section + 12, programInfoLen,
					  &caDesc, &otherDesc, &dtcpDesc) != 0) {
		out->errorFlags = 0x4000000u;
		swLogPrintf("sw opt: detected plural ccis in 1st loop.\n");
		return SW_OPT_ERR_INVALID;
	}

	if (dtcpDesc != NULL && dtcpDesc[0] == SW_OPT_DTCP_TAG) {
		if ((dtcpDesc[4] >> 7) == 0) {
			out->errorFlags = 0x8000000u;
			swLogPrintf("sw opt: detected DTCP audio descriptor in 1st loop.\n");
			return SW_OPT_ERR_INVALID;
		}
		if (((dtcpDesc[4] >> 6) & 1u) == 0 && (dtcpDesc[4] & 3u) == 2u) {
			out->errorFlags = 0x8000000u;
			swLogPrintf("sw opt: detected Move mode in 1st loop.\n");
			return SW_OPT_ERR_INVALID;
		}
		memcpy(chosenDesc, dtcpDesc, 6);
		hasDescriptor = 1;
	}

	esCursor = section + programInfoLen + 12;
	esLoopLen = sectionLength - programInfoLen - 13u;
	esOffset = 0;
	while (esOffset < esLoopLen) {
		esInfoLen = (((unsigned int)(esCursor[3] & 0x0Fu)) << 8) | esCursor[4];
		if (esInfoLen > SW_OPT_MAX_DESC_LEN) {
			out->errorFlags = 0x1000000u;
			swLogPrintf("sw opt: invalid section length.\n");
			return SW_OPT_ERR_INVALID;
		}
		caDesc = NULL;
		otherDesc = NULL;
		dtcpDesc = NULL;
		if (swOptScanSecondLoopDescriptors(esCursor + 5, esInfoLen,
						   &caDesc, &otherDesc, &dtcpDesc) != 0) {
			out->errorFlags = 0x4000000u;
			swLogPrintf("sw opt: detected plural ccis in 2nd loop.\n");
			return SW_OPT_ERR_INVALID;
		}
		if (dtcpDesc != NULL && dtcpDesc[0] == SW_OPT_DTCP_TAG) {
			if ((dtcpDesc[4] >> 7) == 0) {
				out->errorFlags = 0x8000000u;
				swLogPrintf("sw opt: detected DTCP audio descriptor in 2nd loop.\n");
				return SW_OPT_ERR_INVALID;
			}
			if (((dtcpDesc[4] >> 6) & 1u) == 0 && (dtcpDesc[4] & 3u) == 2u) {
				out->errorFlags = 0x8000000u;
				swLogPrintf("sw opt: detected Move mode in 2nd loop.\n");
				return SW_OPT_ERR_INVALID;
			}
		}
		if (hasDescriptor == 1 && chosenDesc[0] == SW_OPT_DTCP_TAG &&
		    dtcpDesc != NULL && dtcpDesc[0] == SW_OPT_DTCP_TAG &&
		    (swOptCompareCciStrictness(chosenDesc[4] & 3u, dtcpDesc[4] & 3u) == 2 ||
		     ((chosenDesc[4] & 3u) == 0 && (dtcpDesc[4] & 3u) == 0 &&
		      ((dtcpDesc[4] >> 2) & 1u) == 0))) {
			memcpy(chosenDesc, dtcpDesc, 6);
			hasDescriptor = 1;
		}
		esOffset += esInfoLen + 5u;
		esCursor += esInfoLen + 5u;
	}

	if (hasDescriptor == 1) {
		cci = chosenDesc[4] & 3u;
		epn = (chosenDesc[4] >> 2) & 1u;
	} else {
		cci = 0;
		epn = 1;
		chosenDesc[0] = 0x88;
		chosenDesc[1] = 0x04;
		chosenDesc[2] = 0x0F;
		chosenDesc[3] = 0xFF;
		chosenDesc[4] = 0xFC;
		chosenDesc[5] = 0xFC;
	}

	out->hasPmt = 1;
	out->cci = cci;
	out->epn = epn;
	memcpy(out->dtcpDescriptor, chosenDesc, 6);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dPMT parse completion\n",
			    "sw_opt_parser_parsepmt", SW_OPT_PARSER_SOURCE, 682);
	return 0;
}

/* reassemble a single-packet PAT section */
int swOptSectionPutPat(struct SwOptSectionBuffer *section, const unsigned char *packet,
		       unsigned int packetLen, int rewriteMove, int *complete,
		       unsigned char **outData, unsigned int *outLen)
{
	const unsigned char *payload;
	unsigned int sectionLength;
	unsigned char payloadOffset;
	unsigned char pusi;
	unsigned char crc[4];

	(void)rewriteMove;
	if (section == NULL || packet == NULL || packetLen < SW_OPT_TS_PACKET_SIZE ||
	    complete == NULL || outData == NULL || outLen == NULL)
		return SW_OPT_ERR_INVALID;

	*complete = 0;
	*outData = NULL;
	*outLen = 0;

	if (swOptParseTsHeader(packet, &payloadOffset, &section->continuityCounter, &pusi) != 0) {
		swLogPrintf("sw opt: invalid TS header.\n");
		return SW_OPT_ERR_INVALID;
	}
	if (pusi == 0) {
		swLogPrintf("sw opt: not found unit start indicator.\n");
		return SW_OPT_ERR_INVALID;
	}

	payload = packet + 4 + payloadOffset;
	if (payload[0] != 0) {
		swLogPrintf("sw opt: not PAT table id.\\n");
		return SW_OPT_ERR_INVALID;
	}
	if ((payload[1] >> 7) != 1) {
		swLogPrintf("sw opt: section syntax indicator is zero.\n");
		return SW_OPT_ERR_INVALID;
	}
	if (((payload[1] >> 6) & 1u) != 0) {
		swLogPrintf("sw opt: not zero in fixed zero field.\n");
		return SW_OPT_ERR_INVALID;
	}

	sectionLength = (((unsigned int)(payload[1] & 0x0Fu)) << 8) | payload[2];
	if (sectionLength > SW_OPT_MAX_SECTION_LEN || sectionLength == 0) {
		swLogPrintf("sw opt: invalid section length\n");
		return SW_OPT_ERR_INVALID;
	}
	if ((payload[5] & 1u) != 1u) {
		swLogPrintf("sw opt: current next indicator is zero.\n");
		return SW_OPT_ERR_INVALID;
	}
	if (payload[6] != 0) {
		swLogPrintf("sw opt: section number isn't 0.\n");
		return SW_OPT_ERR_INVALID;
	}
	if (payload[7] != 0) {
		swLogPrintf("sw opt: last section number isn't 0.\n");
		return SW_OPT_ERR_INVALID;
	}

	swOptCrc32(payload, sectionLength - 1u, NULL, NULL, 0, crc);
	if (crc[0] != payload[sectionLength - 1u] ||
	    crc[1] != payload[sectionLength] ||
	    crc[2] != payload[sectionLength + 1u] ||
	    crc[3] != payload[sectionLength + 2u]) {
		swLogPrintf("sw opt: different from computed section crc.\n");
		return SW_OPT_ERR_INVALID;
	}

	section->tableId = 0;
	section->packetCount = 1;
	section->sectionLength = sectionLength + 3u;
	memcpy(section->sectionData, payload, section->sectionLength);
	*complete = 1;
	*outData = section->sectionData;
	*outLen = section->sectionLength;
	return 0;
}

/* reassemble a single- or multi-packet PMT section and optionally rewrite CCI */
int swOptSectionPutPmt(struct SwOptSectionBuffer *section, unsigned char *packet,
		       unsigned int packetLen, int rewriteMove, int *complete,
		       unsigned char **outData, unsigned int *outLen)
{
	unsigned int pktNum;
	unsigned char *payload;
	unsigned int copyLen;
	unsigned char *copyStart;
	unsigned char *crcStart;
	unsigned char *scanLimit;
	int keepScanning;
	unsigned int crcBytesInPacket;
	unsigned int i;
	unsigned char payloadOffset;
	unsigned char pusi;
	unsigned int expectedLen;

	if (section == NULL || packet == NULL || packetLen < SW_OPT_TS_PACKET_SIZE ||
	    complete == NULL || outData == NULL || outLen == NULL)
		return SW_OPT_ERR_INVALID;

	*complete = 0;
	*outData = NULL;
	*outLen = 0;

	pktNum = ++section->packetCount;
	if (pktNum >= 5u || pktNum == 0) {
		swLogPrintf("sw opt: unit over run.\n");
		return SW_OPT_ERR_INVALID;
	}

	if (pktNum == 1u) {
		if (swOptParseTsHeader(packet, &payloadOffset, &section->continuityCounter,
				       &pusi) != 0) {
			swLogPrintf("sw opt: invalid TS header\n");
			return SW_OPT_ERR_INVALID;
		}
		payload = packet + 4 + payloadOffset;
		copyLen = 184u - payloadOffset;
		if (swOptCheckPmtHeader(payload, &expectedLen) != 0) {
			swLogPrintf("sw opt: invalid section length\n");
			return SW_OPT_ERR_INVALID;
		}
		expectedLen += 3u;
		if (copyLen > expectedLen)
			copyLen = expectedLen;
		g_swOptEndPtr = section->sectionData + expectedLen;
		g_swOptScanPtr = section->sectionData;
		g_swOptWritePtr = section->sectionData;
		g_swOptScanPhase = 2;
		section->tableId = 2;
	} else {
		unsigned int remain = (unsigned int)(g_swOptEndPtr - g_swOptWritePtr);

		payload = packet + 4;
		copyLen = (remain < 184u) ? remain : 184u;
	}

	memcpy(g_swOptWritePtr, payload, copyLen);
	copyStart = g_swOptWritePtr;
	g_swOptWritePtr += copyLen;
	section->sectionLength += copyLen;

	if (g_swOptWritePtr == g_swOptEndPtr) {
		*complete = 1;
		*outData = section->sectionData;
		*outLen = section->sectionLength;
	}

	if (rewriteMove != 0) {
		crcStart = g_swOptEndPtr - 4;
		scanLimit = (g_swOptWritePtr <= crcStart) ? g_swOptWritePtr : crcStart;
		keepScanning = 1;
		do {
			unsigned int avail = (unsigned int)(scanLimit - g_swOptScanPtr);

			if (g_swOptScanPhase == 2) {
				if (avail >= 12u) {
					g_swOptProgramInfoRemain =
						(((unsigned int)(g_swOptScanPtr[10] & 0x0Fu)) << 8) |
						g_swOptScanPtr[11];
					g_swOptScanPtr += 12;
					g_swOptScanPhase = 3;
				} else {
					keepScanning = 0;
				}
			} else if (g_swOptScanPhase == 3) {
				if (g_swOptProgramInfoRemain > 2u) {
					if (avail <= 2u) {
						keepScanning = 0;
					} else {
						unsigned int descLen;

						if (g_swOptScanPtr[0] == SW_OPT_DTCP_TAG &&
						    g_swOptScanPtr[1] >= 3u && avail >= 5u &&
						    g_swOptScanPtr + 4 >= copyStart &&
						    (g_swOptScanPtr[4] & 3u) == 2u) {
							size_t delta = (size_t)((g_swOptScanPtr + 4) -
										copyStart);
							payload[delta] = (payload[delta] & 0xFCu) | 1u;
						}
						descLen = (unsigned int)g_swOptScanPtr[1] + 2u;
						if (avail < descLen) {
							keepScanning = 0;
						} else {
							g_swOptScanPtr += descLen;
							g_swOptProgramInfoRemain -= descLen;
						}
					}
				} else {
					if (g_swOptProgramInfoRemain != 0 &&
					    avail >= g_swOptProgramInfoRemain) {
						g_swOptScanPtr += g_swOptProgramInfoRemain;
						g_swOptProgramInfoRemain = 0;
					}
					g_swOptScanPhase = 0;
				}
			} else if (g_swOptScanPhase == 0) {
				if (avail >= 5u) {
					g_swOptEsInfoRemain =
						(((unsigned int)(g_swOptScanPtr[3] & 0x0Fu)) << 8) |
						g_swOptScanPtr[4];
					g_swOptScanPtr += 5;
					g_swOptScanPhase = 1;
				} else {
					keepScanning = 0;
				}
			} else if (g_swOptScanPhase == 1) {
				if (g_swOptEsInfoRemain > 2u) {
					if (avail <= 2u) {
						keepScanning = 0;
					} else {
						unsigned int descLen;

						if (g_swOptScanPtr[0] == SW_OPT_DTCP_TAG &&
						    g_swOptScanPtr[1] >= 3u && avail >= 5u &&
						    g_swOptScanPtr + 4 >= copyStart &&
						    (g_swOptScanPtr[4] & 3u) == 2u) {
							size_t delta = (size_t)((g_swOptScanPtr + 4) -
										copyStart);
							payload[delta] = (payload[delta] & 0xFCu) | 1u;
						}
						descLen = (unsigned int)g_swOptScanPtr[1] + 2u;
						if (avail < descLen) {
							keepScanning = 0;
						} else {
							g_swOptScanPtr += descLen;
							g_swOptEsInfoRemain -= descLen;
						}
					}
				} else {
					if (g_swOptEsInfoRemain != 0 &&
					    avail >= g_swOptEsInfoRemain) {
						g_swOptScanPtr += g_swOptEsInfoRemain;
						g_swOptEsInfoRemain = 0;
					}
					g_swOptScanPhase = 0;
				}
			}
		} while (keepScanning != 0);

		crcBytesInPacket = 0;
		if (g_swOptWritePtr > crcStart)
			crcBytesInPacket = (unsigned int)(g_swOptWritePtr - crcStart);
		if (crcBytesInPacket >= copyLen) {
			for (i = 0; i < copyLen; ++i)
				payload[i] = section->crcBytes[copyLen - i - 1u];
		} else {
			unsigned int bodyLen = copyLen - crcBytesInPacket;
			unsigned int crcWord;

			memcpy(&crcWord, section->crcBytes, 4);
			swOptCrc32(payload, bodyLen, NULL, &crcWord,
				   (unsigned char)(pktNum - 1u), &crcWord);
			memcpy(section->crcBytes, &crcWord, 4);
			for (i = 0; i < crcBytesInPacket; ++i)
				payload[bodyLen + i] = section->crcBytes[i];
		}
	}
	return 0;
}

/* validate an MPEG-TS 4-byte header and optional pointer_field */
int swOptParseTsHeader(const unsigned char *packet, unsigned char *payloadOffset,
		       unsigned char *continuityCounter, unsigned char *pusiOut)
{
	unsigned char pointerField;

	*pusiOut = 0;
	if (packet[0] != 0x47)
		return SW_OPT_ERR_INVALID;
	if ((packet[1] >> 7) != 0)
		return SW_OPT_ERR_INVALID;
	if (((packet[1] >> 6) & 1u) == 1u) {
		pointerField = packet[4];
		if (pointerField > 0xAFu)
			return SW_OPT_ERR_INVALID;
		*payloadOffset = (unsigned char)(pointerField + 1u);
		*pusiOut = 1;
	} else {
		*payloadOffset = 0;
	}
	if ((packet[3] >> 6) != 0)
		return SW_OPT_ERR_INVALID;
	if (((packet[3] >> 4) & 3u) != 1u)
		return SW_OPT_ERR_INVALID;
	if (*pusiOut == 0 && (packet[3] & 0x0Fu) != ((*continuityCounter + 1u) & 0x0Fu))
		return SW_OPT_ERR_INVALID;
	*continuityCounter = packet[3] & 0x0Fu;
	return 0;
}

/* validate a PMT section header and return its 12-bit section_length */
int swOptCheckPmtHeader(const unsigned char *section, unsigned int *outSectionLen)
{
	unsigned int sectionLength;

	if (section[0] != 2)
		return SW_OPT_ERR_INVALID;
	if ((section[1] >> 7) != 1)
		return SW_OPT_ERR_INVALID;
	if (((section[1] >> 6) & 1u) != 0)
		return SW_OPT_ERR_INVALID;
	sectionLength = (((unsigned int)(section[1] & 0x0Fu)) << 8) | section[2];
	if (sectionLength > SW_OPT_MAX_SECTION_LEN || sectionLength < 5u)
		return SW_OPT_ERR_INVALID;
	if ((section[5] & 1u) != 1u)
		return SW_OPT_ERR_INVALID;
	if (section[6] != 0 || section[7] != 0)
		return SW_OPT_ERR_INVALID;
	*outSectionLen = sectionLength;
	return 0;
}

/* extract 12-bit ES_info_length from a PMT elementary stream entry */
int swOptReadEsInfoLength(const unsigned char *entry, unsigned int *outLength)
{
	*outLength = (((unsigned int)(entry[3] & 0x0Fu)) << 8) | entry[4];
	if (*outLength <= SW_OPT_MAX_DESC_LEN)
		return 0;
	return SW_OPT_ERR_INVALID;
}

/* scan PMT first-loop descriptors for tags 193, 222, 136 */
int swOptScanFirstLoopDescriptors(const unsigned char *buf, unsigned int len,
				  const unsigned char **outCa, const unsigned char **outOther,
				  const unsigned char **outDtcp)
{
	const unsigned char *cursor = buf;
	unsigned int offset;
	unsigned char seenCa = 0;
	unsigned char seenOther = 0;
	unsigned char seenDtcp = 0;

	for (offset = 0; offset < len; ) {
		unsigned char tag = cursor[0];
		unsigned int step = (unsigned int)cursor[1] + 2u;

		if (tag != 252) {
			switch (tag) {
			case 222:
				if (seenOther != 0)
					return SW_OPT_ERR_INVALID;
				*outOther = cursor;
				seenOther = 1;
				break;
			case 193:
				if (seenCa != 0)
					return SW_OPT_ERR_INVALID;
				*outCa = cursor;
				seenCa = 1;
				break;
			case SW_OPT_DTCP_TAG:
				if (seenDtcp != 0)
					return SW_OPT_ERR_INVALID;
				*outDtcp = cursor;
				seenDtcp = 1;
				break;
			default:
				break;
			}
		}
		offset += step;
		cursor += step;
	}
	return 0;
}

/* scan PMT second-loop descriptors for tags 193, 82, 136 */
int swOptScanSecondLoopDescriptors(const unsigned char *buf, unsigned int len,
				   const unsigned char **outCa, const unsigned char **outOther,
				   const unsigned char **outDtcp)
{
	const unsigned char *cursor = buf;
	unsigned int offset;
	unsigned char seenCa = 0;
	unsigned char seenOther = 0;
	unsigned char seenDtcp = 0;

	for (offset = 0; offset < len; ) {
		unsigned char tag = cursor[0];
		unsigned int step = (unsigned int)cursor[1] + 2u;

		if (tag != 253 && tag != 200) {
			switch (tag) {
			case 193:
				if (seenCa != 0)
					return SW_OPT_ERR_INVALID;
				*outCa = cursor;
				seenCa = 1;
				break;
			case SW_OPT_DTCP_TAG:
				if (seenDtcp != 0)
					return SW_OPT_ERR_INVALID;
				*outDtcp = cursor;
				seenDtcp = 1;
				break;
			case 82:
				if (seenOther != 0)
					return SW_OPT_ERR_INVALID;
				*outOther = cursor;
				seenOther = 1;
				break;
			default:
				break;
			}
		}
		offset += step;
		cursor += step;
	}
	return 0;
}

/* compare two 2-bit DTCP CCI values by bit-reversed strictness order */
int swOptCompareCciStrictness(unsigned char firstCci, unsigned char secondCci)
{
	unsigned int firstRank = (((unsigned int)firstCci << 1) & 2u) | ((firstCci >> 1) & 1u);
	unsigned int secondRank = (((unsigned int)secondCci << 1) & 2u) | ((secondCci >> 1) & 1u);

	return (firstRank < secondRank) ? 2 : 1;
}

/* dump PMT header fields when verbose logging is active */
int sw_opt_d_pmthdr1(const unsigned char *header)
{
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dtable_id = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1358, header[0]);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dcurrent_next_indicator = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1359, header[1] >> 7);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dzero_fixed = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1360, (header[1] >> 6) & 1u);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dreserved_1 = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1361, (header[1] >> 4) & 3u);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dlength_h = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1362, header[1] & 0x0Fu);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dsection_length_l = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1363, header[2]);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dprogram_number_h = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1364, header[3]);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dprogram_number_l = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1365, header[4]);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dreserved_2 = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1366, header[5] >> 6);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dversion_number = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1367, (header[5] >> 1) & 0x1Fu);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dcurrent_next_indicator = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1368, header[5] & 1u);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dsection_number = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1369, header[6]);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dlast_section_number = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1370, header[7]);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dreserved_3 = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1371, header[8] >> 5);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dPCR_PID_h = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1372, header[8] & 0x1Fu);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dPCR_PID_l = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1373, header[9]);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dreserved_4 = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1374, header[10] >> 4);
	if (g_swOptLogLevel >= 4)
		swLogPrintf("sw opt: %s:%s:%dprogram_info_length_h = 0x%02x\n",
			    "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1375, header[10] & 0x0Fu);
	if (g_swOptLogLevel >= 4)
		return swLogPrintf("sw opt: %s:%s:%dprogram_info_length_l = 0x%02x\n",
				   "sw_opt_d_pmthdr1", SW_OPT_PARSER_SOURCE, 1376, header[11]);
	return g_swOptLogLevel;
}
