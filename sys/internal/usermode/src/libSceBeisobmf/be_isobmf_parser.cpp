/*
 * ISO Base Media File Format box parser (IsoParser).
 */

#include "be_isobmf_internal.h"

#define ISO_FOURCC(a, b, c, d) \
	((uint32_t)(uint8_t)(a) | \
	 ((uint32_t)(uint8_t)(b) << 8) | \
	 ((uint32_t)(uint8_t)(c) << 16) | \
	 ((uint32_t)(uint8_t)(d) << 24))

int IsoParser::findByPath(const char *path)
{
	m_boxTree.moveToRoot();

	IsoBox *foundBox = NULL;
	if (beStrncmp(path, "/", 512) == 0) {
		foundBox = static_cast<IsoBox *>(m_boxTree.getCurrent());
	} else {
		IsoBox *cursor = static_cast<IsoBox *>(m_boxTree.getCurrent());
		IsoBox *matched = NULL;
		int pos = 0;

		do {
			char fourccBuf[8];
			char indexBuf[16];
			fourccBuf[0] = '\0';
			indexBuf[0] = '\0';

			char ch = path[pos];
			if (ch == '/')
				ch = path[++pos];

			int inIndex = 0;
			int fourccLen = 0;
			int indexLen = 0;
			while (ch != '/' && ch != '\0') {
				if (inIndex == 0 && ch != ':') {
					fourccBuf[fourccLen++] = ch;
				} else if (ch == ':') {
					inIndex = 1;
				} else {
					indexBuf[indexLen++] = ch;
				}
				ch = path[++pos];
			}
			fourccBuf[fourccLen] = '\0';
			indexBuf[indexLen] = '\0';

			int ordinal = 0;
			if (inIndex != 0)
				ordinal = static_cast<int>(beStrtoll(indexBuf, NULL, 10));

			int remaining = ordinal + 1;
			if (ordinal != -1) {
				while (cursor != NULL) {
					if (beStrncmp(cursor->getTypeStr(), fourccBuf, 4) == 0) {
						matched = cursor;
						remaining--;
					}
					cursor = static_cast<IsoBox *>(cursor->getNextSibling());
					if (remaining == 0)
						break;
				}
				if (remaining != 0)
					return 0;
			}

			cursor = matched;
			if (matched == NULL)
				return 0;

			while (path[pos] != '/' && path[pos] != '\0')
				pos++;

			if (path[pos] == '/')
				cursor = static_cast<IsoBox *>(matched->getFirstChild());
		} while (path[pos] == '/' && cursor != NULL);

		foundBox = cursor;
	}

	m_queryResultPtr = foundBox;
	m_queryResultType = (foundBox != NULL) ? BE_FIELD_BOX : BE_FIELD_NONE;
	return 1;
}

void IsoParser::findByOffset(IsoBox *node, uint64_t targetOffset)
{
	uint64_t startOffset = node->getDiskOffset();
	uint64_t endOffset = startOffset + node->getBoxSize();
	if (startOffset <= targetOffset && targetOffset <= endOffset)
		m_queryResultPtr = node;

	if (node->getFirstChild() != NULL)
		findByOffset(static_cast<IsoBox *>(node->getFirstChild()), targetOffset);
	if (node->getNextSibling() != NULL)
		findByOffset(static_cast<IsoBox *>(node->getNextSibling()), targetOffset);
}

IsoBox *IsoParser::createBox(uint32_t fourcc)
{
	if (fourcc == ISO_FOURCC('f', 'r', 'e', 'e'))
		return new IsoBox();
	if (fourcc == ISO_FOURCC('s', 'k', 'i', 'p'))
		return new IsoBox();

	if (m_sampleEntryContext != 0) {
		switch (m_sampleEntryContext) {
		case ISO_BOX_SAMPLE_ENTRY:
			return new IsoSampleEntryBox();
		case ISO_BOX_HINT_SAMPLE_ENTRY:
			if (fourcc == ISO_FOURCC('r', 't', 'p', ' '))
				return new HintSampleEntryBox();
			return new IsoBox();
		case ISO_BOX_VISUAL_SAMPLE_ENTRY:
			return new VisualSampleEntryBox();
		case ISO_BOX_AUDIO_SAMPLE_ENTRY:
			return new AudioSampleEntryBox();
		case ISO_BOX_XML_META_SAMPLE_ENTRY:
			return new XmlMetaSampleEntryBox();
		default:
			return new IsoBox();
		}
	}

	switch (fourcc) {
	case ISO_FOURCC('f', 't', 'y', 'p'): return new FileTypeBox();
	case ISO_FOURCC('m', 'o', 'o', 'v'): return new MovieBox();
	case ISO_FOURCC('m', 'v', 'h', 'd'): return new MovieHeaderBox();
	case ISO_FOURCC('t', 'r', 'a', 'k'): return new TrackBox();
	case ISO_FOURCC('t', 'k', 'h', 'd'): return new TrackHeaderBox();
	case ISO_FOURCC('t', 'r', 'e', 'f'): return new TrackReferenceTypeBox();
	case ISO_FOURCC('e', 'd', 't', 's'): return new EditBox();
	case ISO_FOURCC('e', 'l', 's', 't'): return new EditListBox();
	case ISO_FOURCC('m', 'd', 'i', 'a'): return new MediaBox();
	case ISO_FOURCC('m', 'd', 'h', 'd'): return new MediaHeaderBox();
	case ISO_FOURCC('h', 'd', 'l', 'r'): return new HandlerBox();
	case ISO_FOURCC('m', 'i', 'n', 'f'): return new MediaInformationBox();
	case ISO_FOURCC('v', 'm', 'h', 'd'): return new VideoMediaHeaderBox();
	case ISO_FOURCC('s', 'm', 'h', 'd'): return new SoundMediaHeaderBox();
	case ISO_FOURCC('h', 'm', 'h', 'd'): return new HintMediaHeaderBox();
	case ISO_FOURCC('n', 'm', 'h', 'd'): return new NullMediaHeaderBox();
	case ISO_FOURCC('d', 'i', 'n', 'f'): return new DataInformationBox();
	case ISO_FOURCC('d', 'r', 'e', 'f'): return new DataReferenceBox();
	case ISO_FOURCC('u', 'r', 'l', ' '): return new DataEntryUrlBox();
	case ISO_FOURCC('s', 't', 'b', 'l'): return new SampleTableBox();
	case ISO_FOURCC('s', 't', 't', 's'): return new TimeToSampleBox();
	case ISO_FOURCC('c', 't', 't', 's'): return new CompositionOffsetBox();
	case ISO_FOURCC('s', 't', 's', 's'): return new SyncSampleBox();
	case ISO_FOURCC('s', 't', 's', 'd'): return new SampleDescriptionBox();
	case ISO_FOURCC('s', 't', 's', 'z'): return new SampleSizeBox();
	case ISO_FOURCC('s', 't', 'z', '2'): return new CompactSampleSizeBox();
	case ISO_FOURCC('s', 't', 's', 'c'): return new SampleToChunkBox();
	case ISO_FOURCC('s', 't', 'c', 'o'): return new ChunkOffsetBox();
	case ISO_FOURCC('c', 'o', '6', '4'): return new ChunkLargeOffsetBox();
	case ISO_FOURCC('s', 't', 's', 'h'): return new ShadowSyncSampleBox();
	case ISO_FOURCC('s', 't', 'd', 'p'): return new DegradationPriorityBox();
	case ISO_FOURCC('s', 'd', 't', 'p'): return new SampleDependencyTypeBox();
	case ISO_FOURCC('u', 'd', 't', 'a'): return new UserDataBox();
	case ISO_FOURCC('m', 'v', 'e', 'x'): return new MovieExtendsBox();
	case ISO_FOURCC('m', 'e', 'h', 'd'): return new MovieExtendsHeaderBox();
	case ISO_FOURCC('t', 'r', 'e', 'x'): return new TrackExtendsBox();
	case ISO_FOURCC('m', 'o', 'o', 'f'): return new MovieFragmentBox();
	case ISO_FOURCC('m', 'f', 'h', 'd'): return new MovieFragmentHeaderBox();
	case ISO_FOURCC('t', 'r', 'a', 'f'): return new TrackFragmentBox();
	case ISO_FOURCC('t', 'f', 'h', 'd'): return new TrackFragmentHeaderBox();
	case ISO_FOURCC('t', 'r', 'u', 'n'): return new TrackRunBox();
	case ISO_FOURCC('m', 'f', 'r', 'a'): return new MovieFragmentRandomAccessBox();
	case ISO_FOURCC('t', 'f', 'r', 'a'): return new TrackFragmentRandomAccessBox();
	case ISO_FOURCC('m', 'f', 'r', 'o'): return new MovieFragmentRandomAccessOffsetBox();
	case ISO_FOURCC('e', 's', 'd', 's'): return new EsdsBox();
	case ISO_FOURCC('h', 'n', 't', 'i'): return new HintTrackInfoBox();
	case ISO_FOURCC('r', 't', 'p', ' '): return new RtpMovieHintInformationBox();
	case ISO_FOURCC('s', 'd', 'p', ' '): return new RtpTrackSdpHintInformationBox();
	case ISO_FOURCC('b', 't', 'r', 't'): return new BitRateBox();
	case ISO_FOURCC('p', 'a', 's', 'p'): return new PixelAspectRatioBox();
	case ISO_FOURCC('a', 'v', 'c', 'C'): return new AvcConfigurationBox();
	case ISO_FOURCC('s', 'i', 'n', 'f'): return new ProtectionSchemeInfoBox();
	case ISO_FOURCC('s', 'c', 'h', 'i'): return new SchemeInformationBox();
	case ISO_FOURCC('s', 'c', 'h', 'm'): return new SchemeTypeBox();
	case ISO_FOURCC('f', 'r', 'm', 'a'): return new OriginalFormatBox();
	case ISO_FOURCC('s', 'b', 'g', 'p'): return new SampleToGroupBox();
	case ISO_FOURCC('s', 'g', 'p', 'd'): return new SampleGroupDescriptionBox();
	case ISO_FOURCC('m', 'e', 't', 'a'): return new MetaBox();
	case ISO_FOURCC('x', 'm', 'l', ' '): return new XmlBox();
	case ISO_FOURCC('b', 'x', 'm', 'l'): return new BinaryXmlBox();
	case ISO_FOURCC('s', 'u', 'b', 's'): return new SubSampleInformationBox();
	case ISO_FOURCC('i', 'l', 'o', 'c'): return new ItemLocationBox();
	case ISO_FOURCC('s', 't', 'y', 'p'): return new SegmentTypeBox();
	case ISO_FOURCC('s', 'a', 'i', 'z'): return new SampleAuxInfoSizesBox();
	case ISO_FOURCC('s', 'a', 'i', 'o'): return new SampleAuxInfoOffsetsBox();
	case ISO_FOURCC('t', 'f', 'd', 't'): return new TrackFragmentDecodeTimeBox();
	case ISO_FOURCC('s', 'i', 'd', 'x'): return new SegmentIndexBox();
	case ISO_FOURCC('p', 's', 's', 'h'): return new ProtectionSystemSpecificHeaderBox();
	case ISO_FOURCC('t', 'e', 'n', 'c'): return new TrackEncryptionBox();
	case ISO_FOURCC('l', 'e', 'v', 'a'): return new LevelAssignmentBox();
	case ISO_FOURCC('s', 's', 'i', 'x'): return new SubsegmentIndexBox();
	case ISO_FOURCC('s', 't', 'h', 'd'): return new SubtitleMediaHeaderBox();
	default:
		return new IsoBox();
	}
}

int IsoParser::onCommand(int argc, const char **argv)
{
	if (argc == 0)
		return 0;

	m_boxTree.pushCursor();
	m_queryResultPtr = NULL;

	if (beStrncmp(argv[0], "GetBox", 128) == 0) {
		if (beStrncmp(argv[1], "disk_offset", 128) == 0) {
			uint64_t targetOffset = static_cast<uint64_t>(beStrtoll(argv[2], NULL, 10));
			m_boxTree.moveToRoot();
			IsoBox *root = static_cast<IsoBox *>(m_boxTree.getCurrent());
			findByOffset(root, targetOffset);
			m_queryResultType = (m_queryResultPtr != NULL) ? BE_FIELD_BOX : BE_FIELD_NONE;
		} else if (beStrncmp(argv[1], "path", 128) == 0) {
			m_boxTree.moveToRoot();
			findByPath(argv[2]);
			m_queryResultType = (m_queryResultPtr != NULL) ? BE_FIELD_BOX : BE_FIELD_NONE;
		} else {
			m_boxTree.popCursor();
			return 0;
		}
	} else if (beStrncmp(argv[0], "GetCount", 128) == 0) {
		const char *parentPath = argv[1];
		const char *boxType = argv[2];
		uint32_t matchCount = 0;

		if (findByPath(parentPath) != 0 && m_queryResultPtr != NULL) {
			IsoBox *node = static_cast<IsoBox *>(m_queryResultPtr);
			if (beStrncmp(parentPath, "/", 512) != 0)
				node = static_cast<IsoBox *>(node->getFirstChild());
			while (node != NULL) {
				if (beStrncmp(node->getTypeStr(), boxType, 4) == 0)
					matchCount++;
				node = static_cast<IsoBox *>(node->getNextSibling());
			}
		}

		m_queryCountResult = matchCount;
		m_queryResultType = BE_FIELD_UINT32;
		m_queryResultPtr = &m_queryCountResult;
	}

	m_boxTree.popCursor();
	return (m_queryResultPtr != NULL && m_queryResultType != BE_FIELD_NONE);
}

IsoParser::IsoParser()
{
	m_sampleEntryContext = 0;
	m_currentBox = NULL;
	m_skipRemaining = 0;
	m_skipKindCount = 0;
	m_headerBufUsed = 0;
	m_nextExpectedOffset = 0;
	m_reservedOffset = 0;
	m_queryCountResult = 0;
	m_pendingChildInsert = 0;

	for (int i = 0; i < 32; i++)
		m_skipKinds[i] = 0;

	uint32_t skipIdx = m_skipKindCount;
	m_skipKinds[skipIdx] = ISO_BOX_MDAT;
	m_queryResultType = 0;
	m_skipKinds[skipIdx + 1] = ISO_BOX_FREE;
	m_queryResultPtr = NULL;
	m_skipKinds[skipIdx + 2] = ISO_BOX_SKIP;
	m_skipKindCount = skipIdx + 3;

	registerCommand("GetBox", 2);
	registerCommand("GetCount", 2);
}

int IsoParser::reset()
{
	m_pendingChildInsert = 0;
	m_headerBufUsed = 0;
	m_skipRemaining = 0;

	if (m_currentBox != NULL &&
	    m_currentBox->getPrevSibling() == NULL &&
	    m_currentBox->getParent() == NULL) {
		delete m_currentBox;
		m_currentBox = NULL;
	}

	m_boxTree.clear();
	m_currentBox = NULL;
	m_queryResultPtr = NULL;
	return 0;
}

IsoParser::~IsoParser()
{
	reset();
}

IsoBox *IsoParser::getCurrentTreeBox() const
{
	return static_cast<IsoBox *>(m_boxTree.getCurrent());
}

uint64_t IsoParser::getNextExpectedOffset() const
{
	return m_nextExpectedOffset;
}

BeTree *IsoParser::getBoxTree()
{
	return &m_boxTree;
}

void IsoParser::setSampleEntryContext(uint32_t context)
{
	m_sampleEntryContext = context;
}

uint32_t IsoParser::getSampleEntryContext() const
{
	return m_sampleEntryContext;
}

uint32_t IsoParser::feedData(const uint8_t *data, uint32_t size, int unusedFlag, uint64_t fileOffset)
{
	(void)unusedFlag;

	if (m_skipRemaining != 0) {
		if (size < m_skipRemaining) {
			m_skipRemaining -= size;
			return 0;
		}
		fileOffset += m_skipRemaining;
		data += m_skipRemaining;
		size -= static_cast<uint32_t>(m_skipRemaining);
		m_skipRemaining = 0;
	}

	m_nextExpectedOffset = 0;
	uint32_t leftover = 0;

	if (m_currentBox != NULL) {
		leftover = static_cast<uint32_t>(m_currentBox->feedData(data, static_cast<int>(size)));
	} else {
		uint32_t need = 8 - m_headerBufUsed;
		uint32_t copyLen = (size < need) ? size : need;
		beMemcpy(&m_headerBuf[m_headerBufUsed], data, copyLen);

		if (m_headerBufUsed + size < 8) {
			m_headerBufUsed += size;
			return 0;
		}

		const uint8_t *restPtr = data + copyLen;
		if (fileOffset < m_headerBufUsed)
			return size;

		if (m_headerBuf[0] == 0 && m_headerBuf[1] == 0 &&
		    m_headerBuf[2] == 0 && m_headerBuf[3] == 0 && size >= 4) {
			m_currentBox = createBox(0);
			m_currentBox->setParser(this);
			m_currentBox->setDiskOffset(fileOffset - m_headerBufUsed);
			leftover = static_cast<uint32_t>(m_currentBox->feedData(m_headerBuf, static_cast<int>(size)));
		} else {
			uint32_t restSize = size - copyLen;
			uint32_t fourcc = static_cast<uint32_t>(m_headerBuf[4]) |
				(static_cast<uint32_t>(m_headerBuf[5]) << 8) |
				(static_cast<uint32_t>(m_headerBuf[6]) << 16) |
				(static_cast<uint32_t>(m_headerBuf[7]) << 24);
			m_currentBox = createBox(fourcc);
			m_currentBox->setParser(this);
			m_currentBox->setDiskOffset(fileOffset - m_headerBufUsed);
			m_currentBox->feedData(m_headerBuf, 8);
			if (m_currentBox->hasExtendedHeader() == 1) {
				leftover = static_cast<uint32_t>(m_currentBox->feedData(restPtr, static_cast<int>(restSize)));
			} else {
				leftover = restSize;
				m_currentBox->m_parseState = ISO_BOX_STATE_HEADER_DONE;
			}
		}
	}

	if (m_currentBox->getParseState() == ISO_BOX_STATE_HEADER_DONE ||
	    m_currentBox->getParseState() == ISO_BOX_STATE_COMPLETE) {
		int enterChildren = 0;

		if (m_currentBox->getParseState() == ISO_BOX_STATE_HEADER_DONE) {
			if (m_currentBox->isContainer() == 0) {
				m_skipRemaining = m_currentBox->getBoxSize() - m_currentBox->getDeclaredFieldsSize();
				if (m_pendingChildInsert != 0) {
					m_boxTree.insertChild(m_currentBox);
					m_pendingChildInsert = 0;
				} else {
					m_boxTree.insertSibling(m_currentBox);
				}
				m_nextExpectedOffset = 0;
			} else {
				m_skipRemaining = 0;
				int isSkipped = 0;
				for (uint32_t i = 0; i < 32; i++) {
					if (m_skipKinds[i] == 0)
						break;
					if (m_skipKinds[i] == m_currentBox->getBoxKindId()) {
						isSkipped = 1;
						break;
					}
				}
				if (isSkipped || m_currentBox->getBoxKindId() == ISO_BOX_GENERIC) {
					if (m_pendingChildInsert != 0) {
						m_boxTree.insertChild(m_currentBox);
						m_pendingChildInsert = 0;
					} else {
						m_boxTree.insertSibling(m_currentBox);
					}
					m_nextExpectedOffset = m_currentBox->getDiskOffset() + m_currentBox->getBoxSize();
				} else {
					if (m_pendingChildInsert != 0)
						m_boxTree.insertChild(m_currentBox);
					else
						m_boxTree.insertSibling(m_currentBox);
					m_nextExpectedOffset = 0;
					m_pendingChildInsert = 1;
					enterChildren = 1;
				}
			}
		} else if (m_currentBox->getParseState() == ISO_BOX_STATE_COMPLETE) {
			m_skipRemaining = 0;
			if (m_pendingChildInsert != 0) {
				m_boxTree.insertChild(m_currentBox);
				m_pendingChildInsert = 0;
			} else {
				m_boxTree.insertSibling(m_currentBox);
			}
			m_nextExpectedOffset = 0;
		}

		if (!enterChildren && m_pendingChildInsert == 0) {
			IsoBox *parent = static_cast<IsoBox *>(m_currentBox->getParent());
			while (parent != NULL) {
				IsoBox *child = static_cast<IsoBox *>(parent->getFirstChild());
				uint64_t totalChildBytes = parent->getBytesRead() - 8;
				while (child != NULL) {
					totalChildBytes += child->getBoxSize();
					if (child->getBoxSize() == 0)
						totalChildBytes += 4;
					child = static_cast<IsoBox *>(child->getNextSibling());
				}
				if (parent->getBoxSize() > totalChildBytes + 8)
					break;

				parent->m_parseState = ISO_BOX_STATE_COMPLETE;
				parent->onBoxComplete();
				m_boxTree.moveToParent();
				parent = static_cast<IsoBox *>(m_boxTree.getCurrent()->getParent());
			}
		}

		m_currentBox = NULL;
		m_headerBufUsed = 0;
	}

	return leftover;
}

void *IsoParser::getQueryResult() const
{
	return m_queryResultPtr;
}
