/*
 * SceBeisobmf (BEAV ISO Base Media File Format parser) internal C++ declarations.
 */

#ifndef BE_ISOBMF_INTERNAL_H
#define BE_ISOBMF_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

typedef void *(*BeAllocFunc)(size_t size);
typedef void *(*BeReallocFunc)(void *ptr, size_t size);
typedef void (*BeFreeFunc)(void *ptr);

/* C library wrappers (be_clib.cpp) */
void *beMemcpy(void *dst, const void *src, size_t size);
void *beMemset(void *dst, int value, size_t size);
int beMemcmp(const void *lhs, const void *rhs, size_t size);
int beSnprintf(char *buf, size_t size, const char *fmt, ...);
int beStrncmp(const char *lhs, const char *rhs, size_t maxLen);
char *beStrncpy(char *dst, const char *src, size_t maxLen);
size_t beStrlen(const char *str);
int64_t beStrtoll(const char *nptr, char **endptr, int base);

/* Endian swap utility (be_box.cpp) */
void beSwapByteOrder(void *buffer, uint32_t totalBytes, uint32_t elementSize);

enum BeFieldType {
	BE_FIELD_NONE = 0,
	BE_FIELD_INT8 = 1,
	BE_FIELD_UINT8 = 2,
	BE_FIELD_INT16 = 3,
	BE_FIELD_UINT16 = 4,
	BE_FIELD_INT24 = 5,
	BE_FIELD_UINT24 = 6,
	BE_FIELD_INT32 = 7,
	BE_FIELD_UINT32 = 8,
	BE_FIELD_INT64 = 9,
	BE_FIELD_UINT64 = 10,
	BE_FIELD_BYTES = 11,
	BE_FIELD_UINT16_ARRAY = 12,
	BE_FIELD_UINT32_ARRAY = 13,
	BE_FIELD_UINT64_ARRAY = 14,
	BE_FIELD_BOX = 15,
	BE_FIELD_CSTRING = 16
};

enum IsoBoxParseState {
	ISO_BOX_STATE_INIT = 0,
	ISO_BOX_STATE_PARSING = 1,
	ISO_BOX_STATE_HEADER_DONE = 2,
	ISO_BOX_STATE_COMPLETE = 3
};

enum IsoBoxKind {
	ISO_BOX_GENERIC = 0,
	ISO_BOX_SAMPLE_ENTRY = 1,
	ISO_BOX_HINT_SAMPLE_ENTRY = 2,
	ISO_BOX_BTRT = 3,
	ISO_BOX_META_SAMPLE_ENTRY = 4,
	ISO_BOX_PASP = 7,
	ISO_BOX_VISUAL_SAMPLE_ENTRY = 10,
	ISO_BOX_AVCC = 11,
	ISO_BOX_AUDIO_SAMPLE_ENTRY = 12,
	ISO_BOX_URL = 13,
	ISO_BOX_MOOV = 15,
	ISO_BOX_MVHD = 16,
	ISO_BOX_TRAK = 18,
	ISO_BOX_TKHD = 19,
	ISO_BOX_TREF = 20,
	ISO_BOX_EDTS = 21,
	ISO_BOX_ELST = 22,
	ISO_BOX_MDIA = 23,
	ISO_BOX_MDHD = 24,
	ISO_BOX_HDLR = 25,
	ISO_BOX_MINF = 26,
	ISO_BOX_VMHD = 27,
	ISO_BOX_SMHD = 28,
	ISO_BOX_HMHD = 29,
	ISO_BOX_NMHD = 30,
	ISO_BOX_DINF = 32,
	ISO_BOX_DREF = 33,
	ISO_BOX_STBL = 34,
	ISO_BOX_STTS = 35,
	ISO_BOX_CTTS = 36,
	ISO_BOX_STSS = 37,
	ISO_BOX_STSD = 38,
	ISO_BOX_STSZ = 39,
	ISO_BOX_STSC = 40,
	ISO_BOX_STCO = 41,
	ISO_BOX_CO64 = 42,
	ISO_BOX_STSH = 43,
	ISO_BOX_STDP = 44,
	ISO_BOX_MDAT = 45,
	ISO_BOX_FREE = 46,
	ISO_BOX_SKIP = 47,
	ISO_BOX_UDTA = 48,
	ISO_BOX_FTYP = 49,
	ISO_BOX_STZ2 = 50,
	ISO_BOX_MVEX = 52,
	ISO_BOX_MEHD = 53,
	ISO_BOX_TREX = 54,
	ISO_BOX_MOOF = 55,
	ISO_BOX_MFHD = 56,
	ISO_BOX_TRAF = 57,
	ISO_BOX_TFHD = 58,
	ISO_BOX_TRUN = 59,
	ISO_BOX_MFRA = 60,
	ISO_BOX_TFRA = 61,
	ISO_BOX_MFRO = 62,
	ISO_BOX_SDTP = 64,
	ISO_BOX_SBGP = 65,
	ISO_BOX_SGPD = 66,
	ISO_BOX_SUBS = 68,
	ISO_BOX_META = 69,
	ISO_BOX_ILOC = 71,
	ISO_BOX_SINF = 73,
	ISO_BOX_FRMA = 74,
	ISO_BOX_SCHM = 76,
	ISO_BOX_SCHI = 77,
	ISO_BOX_XML = 79,
	ISO_BOX_BXML = 80,
	ISO_BOX_ESDS = 82,
	ISO_BOX_HNTI = 83,
	ISO_BOX_RTP_HINT = 84,
	ISO_BOX_SDP_HINT = 85,
	ISO_BOX_SAIZ = 87,
	ISO_BOX_SAIO = 88,
	ISO_BOX_LEVA = 89,
	ISO_BOX_TFDT = 90,
	ISO_BOX_SIDX = 91,
	ISO_BOX_SSIX = 92,
	ISO_BOX_STYP = 93,
	ISO_BOX_PSSH = 95,
	ISO_BOX_TENC = 96,
	ISO_BOX_STHD = 102,
	ISO_BOX_XML_META_SAMPLE_ENTRY = 103
};

class IsoParser;

class BeMemObject {
public:
	BeMemObject();
	virtual ~BeMemObject();

	static void *operator new(size_t size);
	static void operator delete(void *ptr);
	static void *operator new[](size_t size);
	static void operator delete[](void *ptr);

	static void setAllocator(BeAllocFunc allocFn, BeReallocFunc reallocFn, BeFreeFunc freeFn);
};

class BeListNode : public BeMemObject {
public:
	BeListNode();
	virtual ~BeListNode();

	BeListNode *getNext() const;

	BeListNode *m_prev;
	BeListNode *m_next;
};

class BeList : public BeMemObject {
public:
	BeList();
	virtual ~BeList();

	void append(BeListNode *node);
	int moveFirst();
	int moveNext();
	BeListNode *getCurrent() const;

	uint32_t m_count;
	BeListNode *m_head;
	BeListNode *m_tail;
	BeListNode *m_current;
};

class BeField : public BeListNode {
public:
	BeField();
	virtual ~BeField();

	int isDynamicType() const;
	void *getData();
	const char *getName() const;
	uint32_t getType() const;
	uint32_t getSize() const;
	void setName(const char *name);
	int setType(uint32_t type);
	int setData(const void *src);
	int allocate(uint32_t size);
	int resize(uint32_t newSize);
	int writeAt(uint32_t offset, const void *src, uint32_t size);
	void swapEndian();
	uint32_t getSkipAlloc() const;
	uint32_t isIncomplete() const;

	char m_name[64];
	uint8_t m_inlineData[8];
	void *m_heapData;
	uint32_t m_dataSize;
	uint32_t m_dataType;
	uint64_t m_reservedValue;
	uint32_t m_skipAlloc;
	uint32_t m_incomplete;
};

class BeTreeNode : public BeMemObject {
public:
	BeTreeNode();
	virtual ~BeTreeNode();

	BeTreeNode *getFirstChild() const;
	BeTreeNode *getNextSibling() const;
	BeTreeNode *getPrevSibling() const;
	BeTreeNode *getParent() const;

	BeTreeNode *m_firstChild;
	BeTreeNode *m_parent;
	BeTreeNode *m_nextSibling;
	BeTreeNode *m_prevSibling;
};

class BeTree : public BeMemObject {
public:
	BeTree();
	virtual ~BeTree();

	int clear();
	void insertChild(BeTreeNode *node);
	void insertSibling(BeTreeNode *node);
	int moveToRoot();
	int moveToParent();
	int pushCursor();
	int popCursor();
	BeTreeNode *getCurrent() const;

	uint32_t m_stackDepth;
	BeTreeNode *m_root;
	BeTreeNode *m_current;
	BeTreeNode *m_savedStack[32];
};

class BeCommandEntry : public BeListNode {
public:
	BeCommandEntry() : BeListNode() {
		m_argCount = 0;
		m_name[0] = '\0';
	}
	virtual ~BeCommandEntry();

	char m_name[128];
	int m_argCount;
};

class BeCommandParser : public BeMemObject {
public:
	BeCommandParser();
	virtual ~BeCommandParser();

	void tokenize(const char *cmdLine);
	int registerCommand(const char *name, int argCount);
	int executeCommand(const char *cmdLine);
	virtual int onCommand(int argc, const char **argv);

	int m_tokenCount;
	char *m_tokens[16];
	BeList m_commands;
};

class IsoBox : public BeTreeNode {
public:
	IsoBox();
	virtual ~IsoBox();

	BeField *findField(const char *name);
	virtual void onFieldParsed();
	virtual void onBoxComplete();
	virtual int isContainer();
	virtual int feedData(const uint8_t *data, int size);

	uint32_t getParseState() const;
	uint64_t getBytesRead() const;
	uint32_t getBoxKindId() const;
	void setDiskOffset(uint64_t offset);
	uint64_t getDiskOffset() const;
	uint64_t getBoxSize();
	const char *getTypeStr();
	uint64_t getHeaderSize();
	uint64_t getDeclaredFieldsSize();
	uint32_t hasExtendedHeader() const;
	void addField(const char *name, uint32_t type, uint32_t size);
	void setParser(IsoParser *parser);
	IsoParser *getParser() const;

	BeList m_fields;
	BeField *m_currentField;
	char m_typeStr[8];
	uint32_t m_fieldIndex;
	uint32_t m_fieldBytesRead;
	uint64_t m_fallbackSize;
	uint64_t m_bytesRead;
	uint64_t m_diskOffset;
	uint32_t m_hasExtendedHeader;
	uint32_t m_parseState;
	uint32_t m_boxKindId;
	IsoParser *m_parser;
};

class IsoFullBox : public IsoBox {
public:
	IsoFullBox();
	virtual ~IsoFullBox() {}

	virtual void onFieldParsed();
	uint64_t getFullHeaderSize() const;

	uint64_t m_fullHeaderSize;
	uint32_t m_fullHeaderAdded;
};

class IsoSampleEntryBox : public IsoBox {
public:
	IsoSampleEntryBox();
	virtual ~IsoSampleEntryBox() {}

	virtual void onFieldParsed();
	virtual void onBoxComplete();

	uint32_t m_savedContext;
	uint32_t m_entryHeaderAdded;
};

class IsoParser : public BeCommandParser {
public:
	IsoParser();
	virtual ~IsoParser();

	int findByPath(const char *path);
	void findByOffset(IsoBox *node, uint64_t targetOffset);
	int reset();
	IsoBox *getCurrentTreeBox() const;
	uint64_t getNextExpectedOffset() const;
	BeTree *getBoxTree();
	void setSampleEntryContext(uint32_t context);
	uint32_t getSampleEntryContext() const;
	void *getQueryResult() const;

	virtual int onCommand(int argc, const char **argv);
	virtual IsoBox *createBox(uint32_t fourcc);
	virtual uint32_t feedData(const uint8_t *data, uint32_t size, int unusedFlag, uint64_t fileOffset);

	uint32_t m_skipKindCount;
	uint32_t m_headerBufUsed;
	uint64_t m_nextExpectedOffset;
	uint64_t m_reservedOffset;
	uint64_t m_skipRemaining;
	uint8_t m_headerBuf[584];
	uint32_t m_pendingChildInsert;
	uint32_t m_skipKinds[32];
	uint32_t m_sampleEntryContext;
	IsoBox *m_currentBox;
	BeTree m_boxTree;
	uint32_t m_queryCountResult;
	uint32_t m_queryResultType;
	void *m_queryResultPtr;
};

/* Extended ISO/DASH/CENC box subclasses (be_boxes_ext.cpp) */

class ItemLocationBox : public IsoFullBox {
public:
	ItemLocationBox();
	virtual ~ItemLocationBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class LevelAssignmentBox : public IsoFullBox {
public:
	LevelAssignmentBox();
	virtual ~LevelAssignmentBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	char m_nameBuf[64];
	uint32_t m_levelIndex;
	uint32_t m_bodyAdded;
};

class MetaBox : public IsoFullBox {
public:
	MetaBox();
	virtual ~MetaBox() {}
};

class MovieExtendsHeaderBox : public IsoFullBox {
public:
	MovieExtendsHeaderBox();
	virtual ~MovieExtendsHeaderBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class OriginalFormatBox : public IsoBox {
public:
	OriginalFormatBox();
	virtual ~OriginalFormatBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class ProtectionSchemeInfoBox : public IsoBox {
public:
	ProtectionSchemeInfoBox();
	virtual ~ProtectionSchemeInfoBox() {}
};

class ProtectionSystemSpecificHeaderBox : public IsoFullBox {
public:
	ProtectionSystemSpecificHeaderBox();
	virtual ~ProtectionSystemSpecificHeaderBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class SampleAuxInfoOffsetsBox : public IsoFullBox {
public:
	SampleAuxInfoOffsetsBox();
	virtual ~SampleAuxInfoOffsetsBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class SampleAuxInfoSizesBox : public IsoFullBox {
public:
	SampleAuxInfoSizesBox();
	virtual ~SampleAuxInfoSizesBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class SampleGroupDescriptionBox : public IsoFullBox {
public:
	SampleGroupDescriptionBox();
	virtual ~SampleGroupDescriptionBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_entryIndex;
	uint8_t m_isContainer;
	char m_nameBuf[64];
	uint32_t m_bodyAdded;
};

class SampleToGroupBox : public IsoFullBox {
public:
	SampleToGroupBox();
	virtual ~SampleToGroupBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class SchemeInformationBox : public IsoBox {
public:
	SchemeInformationBox();
	virtual ~SchemeInformationBox() {}
};

class SchemeTypeBox : public IsoFullBox {
public:
	SchemeTypeBox();
	virtual ~SchemeTypeBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class SegmentIndexBox : public IsoFullBox {
public:
	SegmentIndexBox();
	virtual ~SegmentIndexBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class SegmentTypeBox : public IsoBox {
public:
	SegmentTypeBox();
	virtual ~SegmentTypeBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class SubSampleInformationBox : public IsoFullBox {
public:
	SubSampleInformationBox();
	virtual ~SubSampleInformationBox() {}
	virtual void onFieldParsed();
	virtual void onBoxComplete();

	char m_nameBuf[64];
	uint32_t m_entryIndex;
	uint32_t m_bodyAdded;
};

class SubsegmentIndexBox : public IsoFullBox {
public:
	SubsegmentIndexBox();
	virtual ~SubsegmentIndexBox() {}
	virtual void onFieldParsed();
	virtual void onBoxComplete();

	char m_nameBuf[64];
	uint32_t m_subsegmentIndex;
	uint32_t m_bodyAdded;
};

class SubtitleMediaHeaderBox : public IsoFullBox {
public:
	SubtitleMediaHeaderBox();
	virtual ~SubtitleMediaHeaderBox() {}
};

class TrackEncryptionBox : public IsoFullBox {
public:
	TrackEncryptionBox();
	virtual ~TrackEncryptionBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class TrackFragmentDecodeTimeBox : public IsoFullBox {
public:
	TrackFragmentDecodeTimeBox();
	virtual ~TrackFragmentDecodeTimeBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class XmlBox : public IsoFullBox {
public:
	XmlBox();
	virtual ~XmlBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class BinaryXmlBox : public IsoFullBox {
public:
	BinaryXmlBox();
	virtual ~BinaryXmlBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

/* Core ISO 14496-12 box subclasses (be_boxes_core.cpp) */

class ChunkOffsetBox : public IsoFullBox {
public:
	ChunkOffsetBox();
	virtual ~ChunkOffsetBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class ChunkLargeOffsetBox : public IsoFullBox {
public:
	ChunkLargeOffsetBox();
	virtual ~ChunkLargeOffsetBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class CompactSampleSizeBox : public IsoFullBox {
public:
	CompactSampleSizeBox();
	virtual ~CompactSampleSizeBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class CompositionOffsetBox : public IsoFullBox {
public:
	CompositionOffsetBox();
	virtual ~CompositionOffsetBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class DataInformationBox : public IsoBox {
public:
	DataInformationBox();
	virtual ~DataInformationBox() {}
};

class DataEntryUrlBox : public IsoFullBox {
public:
	DataEntryUrlBox();
	virtual ~DataEntryUrlBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class DataReferenceBox : public IsoFullBox {
public:
	DataReferenceBox();
	virtual ~DataReferenceBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class DegradationPriorityBox : public IsoFullBox {
public:
	DegradationPriorityBox();
	virtual ~DegradationPriorityBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class EditBox : public IsoBox {
public:
	EditBox();
	virtual ~EditBox() {}
};

class EditListBox : public IsoFullBox {
public:
	EditListBox();
	virtual ~EditListBox() {}
	virtual void onFieldParsed();
	virtual void onBoxComplete();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class EsdsBox : public IsoFullBox {
public:
	EsdsBox();
	virtual ~EsdsBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class FileTypeBox : public IsoBox {
public:
	FileTypeBox();
	virtual ~FileTypeBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class HandlerBox : public IsoFullBox {
public:
	HandlerBox();
	virtual ~HandlerBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class MediaBox : public IsoBox {
public:
	MediaBox();
	virtual ~MediaBox() {}
};

class MediaHeaderBox : public IsoFullBox {
public:
	MediaHeaderBox();
	virtual ~MediaHeaderBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class MediaInformationBox : public IsoBox {
public:
	MediaInformationBox();
	virtual ~MediaInformationBox() {}
};

class VideoMediaHeaderBox : public IsoFullBox {
public:
	VideoMediaHeaderBox();
	virtual ~VideoMediaHeaderBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class SoundMediaHeaderBox : public IsoFullBox {
public:
	SoundMediaHeaderBox();
	virtual ~SoundMediaHeaderBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class HintMediaHeaderBox : public IsoFullBox {
public:
	HintMediaHeaderBox();
	virtual ~HintMediaHeaderBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class NullMediaHeaderBox : public IsoFullBox {
public:
	NullMediaHeaderBox();
	virtual ~NullMediaHeaderBox() {}
	virtual int isContainer();
};

class MovieBox : public IsoBox {
public:
	MovieBox();
	virtual ~MovieBox() {}
};

class MovieExtendsBox : public IsoBox {
public:
	MovieExtendsBox();
	virtual ~MovieExtendsBox() {}
};

class MovieFragmentBox : public IsoBox {
public:
	MovieFragmentBox();
	virtual ~MovieFragmentBox() {}
};

class MovieFragmentHeaderBox : public IsoFullBox {
public:
	MovieFragmentHeaderBox();
	virtual ~MovieFragmentHeaderBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class MovieFragmentRandomAccessBox : public IsoBox {
public:
	MovieFragmentRandomAccessBox();
	virtual ~MovieFragmentRandomAccessBox() {}
};

class MovieFragmentRandomAccessOffsetBox : public IsoFullBox {
public:
	MovieFragmentRandomAccessOffsetBox();
	virtual ~MovieFragmentRandomAccessOffsetBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class MovieHeaderBox : public IsoFullBox {
public:
	MovieHeaderBox();
	virtual ~MovieHeaderBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class HintTrackInfoBox : public IsoBox {
public:
	HintTrackInfoBox();
	virtual ~HintTrackInfoBox() {}
};

class RtpMovieHintInformationBox : public IsoBox {
public:
	RtpMovieHintInformationBox();
	virtual ~RtpMovieHintInformationBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class RtpTrackSdpHintInformationBox : public IsoBox {
public:
	RtpTrackSdpHintInformationBox();
	virtual ~RtpTrackSdpHintInformationBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class SampleDependencyTypeBox : public IsoFullBox {
public:
	SampleDependencyTypeBox();
	virtual ~SampleDependencyTypeBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class SampleDescriptionBox : public IsoFullBox {
public:
	SampleDescriptionBox();
	virtual ~SampleDescriptionBox();
	virtual void onFieldParsed();
	virtual void onBoxComplete();

	uint32_t m_bodyAdded;
};

class BitRateBox : public IsoBox {
public:
	BitRateBox();
	virtual ~BitRateBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class PixelAspectRatioBox : public IsoBox {
public:
	PixelAspectRatioBox();
	virtual ~PixelAspectRatioBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class VisualSampleEntryBox : public IsoSampleEntryBox {
public:
	VisualSampleEntryBox();
	virtual ~VisualSampleEntryBox() {}
	virtual void onFieldParsed();
	virtual void onBoxComplete();

	uint32_t m_subSavedContext;
	uint32_t m_bodyAdded;
};

class AvcConfigurationBox : public IsoBox {
public:
	AvcConfigurationBox();
	virtual ~AvcConfigurationBox() {}
	virtual void onFieldParsed();
	virtual void onBoxComplete();
	virtual int isContainer();

	char m_nameBuf[64];
	uint8_t m_spsCount;
	uint8_t m_ppsCount;
	uint8_t m_spsIndex;
	uint8_t m_ppsIndex;
	uint32_t m_savedContext;
	uint8_t m_parseStep;
};

class AudioSampleEntryBox : public IsoSampleEntryBox {
public:
	AudioSampleEntryBox();
	virtual ~AudioSampleEntryBox() {}
	virtual void onFieldParsed();
	virtual void onBoxComplete();

	uint32_t m_subSavedContext;
	uint32_t m_bodyAdded;
};

class HintSampleEntryBox : public IsoSampleEntryBox {
public:
	HintSampleEntryBox();
	virtual ~HintSampleEntryBox() {}
	virtual void onFieldParsed();
	virtual void onBoxComplete();

	uint32_t m_subSavedContext;
	uint32_t m_bodyAdded;
};

class XmlMetaSampleEntryBox : public IsoSampleEntryBox {
public:
	XmlMetaSampleEntryBox();
	virtual ~XmlMetaSampleEntryBox() {}
	virtual void onFieldParsed();
	virtual void onBoxComplete();

	uint32_t m_subSavedContext;
	uint32_t m_bodyAdded;
};

class SampleSizeBox : public IsoFullBox {
public:
	SampleSizeBox();
	virtual ~SampleSizeBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class SampleTableBox : public IsoBox {
public:
	SampleTableBox();
	virtual ~SampleTableBox() {}
};

class SampleToChunkBox : public IsoFullBox {
public:
	SampleToChunkBox();
	virtual ~SampleToChunkBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class ShadowSyncSampleBox : public IsoFullBox {
public:
	ShadowSyncSampleBox();
	virtual ~ShadowSyncSampleBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class SyncSampleBox : public IsoFullBox {
public:
	SyncSampleBox();
	virtual ~SyncSampleBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class TimeToSampleBox : public IsoFullBox {
public:
	TimeToSampleBox();
	virtual ~TimeToSampleBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class TrackBox : public IsoBox {
public:
	TrackBox();
	virtual ~TrackBox() {}
};

class TrackExtendsBox : public IsoFullBox {
public:
	TrackExtendsBox();
	virtual ~TrackExtendsBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class TrackFragmentBox : public IsoBox {
public:
	TrackFragmentBox();
	virtual ~TrackFragmentBox() {}
};

class TrackFragmentHeaderBox : public IsoFullBox {
public:
	TrackFragmentHeaderBox();
	virtual ~TrackFragmentHeaderBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class TrackFragmentRandomAccessBox : public IsoFullBox {
public:
	TrackFragmentRandomAccessBox();
	virtual ~TrackFragmentRandomAccessBox() {}
	virtual void onFieldParsed();
	virtual void onBoxComplete();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class TrackHeaderBox : public IsoFullBox {
public:
	TrackHeaderBox();
	virtual ~TrackHeaderBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class TrackReferenceTypeBox : public IsoBox {
public:
	TrackReferenceTypeBox();
	virtual ~TrackReferenceTypeBox() {}
	virtual void onFieldParsed();

	uint32_t m_bodyAdded;
};

class TrackRunBox : public IsoFullBox {
public:
	TrackRunBox();
	virtual ~TrackRunBox() {}
	virtual void onFieldParsed();
	virtual int isContainer();

	uint32_t m_bodyAdded;
};

class UserDataBox : public IsoBox {
public:
	UserDataBox();
	virtual ~UserDataBox() {}
};

/* C export helper for reading 24-bit flags from an IsoBox */
extern "C" uint32_t SceBeisobmf_F9EA95DA(IsoBox *box);

static inline uint32_t isoBoxReadFlags(IsoBox *box)
{
	if (box != NULL) {
		BeField *field = box->findField("flags");
		if (field != NULL) {
			const uint8_t *bytes = static_cast<const uint8_t *>(field->getData());
			return (static_cast<uint32_t>(bytes[2]) << 16) |
				(static_cast<uint32_t>(bytes[1]) << 8) |
				static_cast<uint32_t>(bytes[0]);
		}
	}
	return 0;
}

#endif /* BE_ISOBMF_INTERNAL_H */
