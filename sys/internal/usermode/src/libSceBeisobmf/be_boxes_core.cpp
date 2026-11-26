/*
 * Core ISO 14496-12 box subclasses (IsoFullBox, IsoSampleEntryBox, and standard boxes).
 */

#include "be_isobmf_internal.h"

#define ISO_FOURCC(a, b, c, d) \
	((uint32_t)(uint8_t)(a) | \
	 ((uint32_t)(uint8_t)(b) << 8) | \
	 ((uint32_t)(uint8_t)(c) << 16) | \
	 ((uint32_t)(uint8_t)(d) << 24))

/* ChunkOffsetBox (stco) */

void ChunkOffsetBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("entry_count", BE_FIELD_UINT32, 4);
	}

	if (findField("entry_count") != NULL && findField("entry_count")->isIncomplete() == 0) {
		uint32_t entryCount = *static_cast<const uint32_t *>(findField("entry_count")->getData());
		addField("Struct_chunk_offset", BE_FIELD_UINT32_ARRAY, 4 * entryCount);
	}
}

ChunkOffsetBox::ChunkOffsetBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_STCO;
}

int ChunkOffsetBox::isContainer()
{
	return 0;
}

/* ChunkLargeOffsetBox (co64) */

void ChunkLargeOffsetBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("entry_count", BE_FIELD_UINT32, 4);
	}

	if (findField("entry_count") != NULL && findField("entry_count")->isIncomplete() == 0) {
		uint32_t entryCount = *static_cast<const uint32_t *>(findField("entry_count")->getData());
		addField("Struct_chunk_offset", BE_FIELD_UINT64_ARRAY, 8 * entryCount);
	}
}

ChunkLargeOffsetBox::ChunkLargeOffsetBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_CO64;
}

int ChunkLargeOffsetBox::isContainer()
{
	return 0;
}

/* CompactSampleSizeBox (stz2) */

void CompactSampleSizeBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("reserved", BE_FIELD_UINT24, 3);
		addField("field_size", BE_FIELD_UINT8, 1);
		addField("sample_count", BE_FIELD_UINT32, 4);
	}

	if (findField("sample_count") != NULL && findField("sample_count")->isIncomplete() == 0) {
		uint32_t sampleCount = *static_cast<const uint32_t *>(findField("sample_count")->getData());
		uint32_t fieldSize = *static_cast<const uint32_t *>(findField("field_size")->getData());
		addField("Struct_entry_size", BE_FIELD_BYTES, (sampleCount * fieldSize) >> 3);
	}
}

CompactSampleSizeBox::CompactSampleSizeBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_STZ2;
}

int CompactSampleSizeBox::isContainer()
{
	return 0;
}

/* CompositionOffsetBox (ctts) */

void CompositionOffsetBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("entry_count", BE_FIELD_UINT32, 4);
	}

	if (findField("entry_count") != NULL && findField("entry_count")->isIncomplete() == 0) {
		uint32_t entryCount = *static_cast<const uint32_t *>(findField("entry_count")->getData());
		addField("Struct_sample_count", BE_FIELD_UINT32_ARRAY, 8 * entryCount);
	}
}

CompositionOffsetBox::CompositionOffsetBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_CTTS;
}

int CompositionOffsetBox::isContainer()
{
	return 0;
}

/* DataInformationBox (dinf) */

DataInformationBox::DataInformationBox()
{
	m_boxKindId = ISO_BOX_DINF;
}

/* DataEntryUrlBox (url ) */

void DataEntryUrlBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		uint32_t remain = static_cast<uint32_t>(getBoxSize()) -
			static_cast<uint32_t>(getFullHeaderSize());
		addField("location", BE_FIELD_BYTES, remain);
	}
}

DataEntryUrlBox::DataEntryUrlBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_URL;
}

/* DataReferenceBox (dref) */

void DataReferenceBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("entry_count", BE_FIELD_UINT32, 4);
	}
}

DataReferenceBox::DataReferenceBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_DREF;
}

/* DegradationPriorityBox (stdp) */

void DegradationPriorityBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		uint32_t remain = static_cast<uint32_t>(getBoxSize()) -
			static_cast<uint32_t>(getFullHeaderSize());
		addField("Struct_priority", BE_FIELD_UINT16_ARRAY, remain);
	}
}

DegradationPriorityBox::DegradationPriorityBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_STDP;
}

int DegradationPriorityBox::isContainer()
{
	return 0;
}

/* EditBox (edts) */

EditBox::EditBox()
{
	m_boxKindId = ISO_BOX_EDTS;
}

/* EditListBox (elst) */

void EditListBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("entry_count", BE_FIELD_UINT32, 4);
	}

	if (findField("entry_count") != NULL && findField("entry_count")->isIncomplete() == 0) {
		uint8_t version = *static_cast<const uint8_t *>(findField("version")->getData());
		uint32_t entryCount = *static_cast<const uint32_t *>(findField("entry_count")->getData());
		if (version == 1)
			addField("Struct_segment_duration", BE_FIELD_BYTES, 20 * entryCount);
		else
			addField("Struct_segment_duration", BE_FIELD_BYTES, 12 * entryCount);
	}
}

void EditListBox::onBoxComplete()
{
	uint8_t *entries = static_cast<uint8_t *>(findField("Struct_segment_duration")->getData());
	uint32_t entryCount = *static_cast<const uint32_t *>(findField("entry_count")->getData());
	uint8_t version = *static_cast<const uint8_t *>(findField("version")->getData());

	if (version == 1) {
		for (uint32_t i = 0; i < entryCount; i++) {
			beSwapByteOrder(entries, 16, 8);
			beSwapByteOrder(entries + 16, 4, 2);
			entries += 20;
		}
	} else {
		for (uint32_t i = 0; i < entryCount; i++) {
			beSwapByteOrder(entries, 8, 4);
			beSwapByteOrder(entries + 8, 4, 2);
			entries += 12;
		}
	}
}

EditListBox::EditListBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_ELST;
}

int EditListBox::isContainer()
{
	return 0;
}

/* EsdsBox (esds) */

void EsdsBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		uint32_t remain = static_cast<uint32_t>(getBoxSize()) -
			static_cast<uint32_t>(getFullHeaderSize());
		addField("ES_Descriptor", BE_FIELD_BYTES, remain);
	}
}

EsdsBox::EsdsBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_ESDS;
}

/* FileTypeBox (ftyp) */

void FileTypeBox::onFieldParsed()
{
	IsoBox::onFieldParsed();
	if (m_bodyAdded == 0 && getBytesRead() >= getHeaderSize()) {
		m_bodyAdded = 1;
		addField("major_brand", BE_FIELD_BYTES, 4);
		addField("minor_version", BE_FIELD_UINT32, 4);
		uint64_t brandCount = (getBoxSize() - (getBytesRead() + 8)) / 4;
		for (uint64_t i = 0; i < brandCount; i++) {
			char brandName[64];
			beSnprintf(brandName, sizeof(brandName), "compatible_brands_%d", static_cast<int>(i));
			addField(brandName, BE_FIELD_BYTES, 4);
		}
	}
}

FileTypeBox::FileTypeBox()
{
	m_bodyAdded = 0;
	m_boxKindId = ISO_BOX_FTYP;
	m_hasExtendedHeader = 1;
}

/* HandlerBox (hdlr) */

void HandlerBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("pre_defined", BE_FIELD_UINT32, 4);
		addField("handler_type", BE_FIELD_BYTES, 4);
		addField("reserved", BE_FIELD_UINT32_ARRAY, 12);
		uint32_t remain = static_cast<uint32_t>(getBoxSize()) -
			static_cast<uint32_t>(getFullHeaderSize()) - 20;
		addField("name", BE_FIELD_BYTES, remain);
	}
}

HandlerBox::HandlerBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_HDLR;
}

/* MediaBox (mdia) */

MediaBox::MediaBox()
{
	m_boxKindId = ISO_BOX_MDIA;
}

/* MediaHeaderBox (mdhd) */

void MediaHeaderBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		if (*static_cast<const uint8_t *>(findField("version")->getData()) == 1) {
			addField("creation_time", BE_FIELD_UINT64, 8);
			addField("modification_time", BE_FIELD_UINT64, 8);
			addField("timescale", BE_FIELD_UINT32, 4);
			addField("duration", BE_FIELD_UINT64, 8);
		} else {
			addField("creation_time", BE_FIELD_UINT32, 4);
			addField("modification_time", BE_FIELD_UINT32, 4);
			addField("timescale", BE_FIELD_UINT32, 4);
			addField("duration", BE_FIELD_UINT32, 4);
		}
		addField("pad_language", BE_FIELD_UINT16_ARRAY, 2);
		addField("pre_defined", BE_FIELD_UINT16_ARRAY, 2);
	}
}

MediaHeaderBox::MediaHeaderBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_MDHD;
}

/* MediaInformationBox (minf) */

MediaInformationBox::MediaInformationBox()
{
	m_boxKindId = ISO_BOX_MINF;
}

/* VideoMediaHeaderBox (vmhd) */

void VideoMediaHeaderBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		addField("graphicsmode", BE_FIELD_UINT16, 2);
		addField("opcolor", BE_FIELD_UINT16_ARRAY, 6);
	}
}

VideoMediaHeaderBox::VideoMediaHeaderBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_VMHD;
}

int VideoMediaHeaderBox::isContainer()
{
	return 0;
}

/* SoundMediaHeaderBox (smhd) */

void SoundMediaHeaderBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		addField("balance", BE_FIELD_UINT16, 2);
		addField("reserved", BE_FIELD_UINT16_ARRAY, 2);
	}
}

SoundMediaHeaderBox::SoundMediaHeaderBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_SMHD;
}

int SoundMediaHeaderBox::isContainer()
{
	return 0;
}

/* HintMediaHeaderBox (hmhd) */

void HintMediaHeaderBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		addField("maxPDUsize", BE_FIELD_UINT16, 2);
		addField("avgPDUsize", BE_FIELD_UINT16, 2);
		addField("maxbitrate", BE_FIELD_UINT32, 4);
		addField("avgbitrate", BE_FIELD_UINT32, 4);
		addField("reserved", BE_FIELD_UINT32, 4);
	}
}

HintMediaHeaderBox::HintMediaHeaderBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_HMHD;
}

int HintMediaHeaderBox::isContainer()
{
	return 0;
}

/* NullMediaHeaderBox (nmhd) */

NullMediaHeaderBox::NullMediaHeaderBox()
{
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_NMHD;
}

int NullMediaHeaderBox::isContainer()
{
	return 0;
}

/* MovieBox (moov) */

MovieBox::MovieBox()
{
	m_boxKindId = ISO_BOX_MOOV;
}

/* MovieExtendsBox (mvex) */

MovieExtendsBox::MovieExtendsBox()
{
	m_boxKindId = ISO_BOX_MVEX;
}

/* MovieFragmentBox (moof) */

MovieFragmentBox::MovieFragmentBox()
{
	m_boxKindId = ISO_BOX_MOOF;
}

/* MovieFragmentHeaderBox (mfhd) */

void MovieFragmentHeaderBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("sequence_number", BE_FIELD_UINT32, 4);
	}
}

MovieFragmentHeaderBox::MovieFragmentHeaderBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_MFHD;
}

int MovieFragmentHeaderBox::isContainer()
{
	return 0;
}

/* MovieFragmentRandomAccessBox (mfra) */

MovieFragmentRandomAccessBox::MovieFragmentRandomAccessBox()
{
	m_boxKindId = ISO_BOX_MFRA;
}

/* MovieFragmentRandomAccessOffsetBox (mfro) */

void MovieFragmentRandomAccessOffsetBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("size2", BE_FIELD_UINT32, 4);
	}
}

MovieFragmentRandomAccessOffsetBox::MovieFragmentRandomAccessOffsetBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_MFRO;
}

int MovieFragmentRandomAccessOffsetBox::isContainer()
{
	return 0;
}

/* MovieHeaderBox (mvhd) */

void MovieHeaderBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		if (*static_cast<const uint8_t *>(findField("version")->getData()) == 1) {
			addField("creation_time", BE_FIELD_UINT64, 8);
			addField("modification_time", BE_FIELD_UINT64, 8);
			addField("timescale", BE_FIELD_UINT32, 4);
			addField("duration", BE_FIELD_UINT64, 8);
		} else {
			addField("creation_time", BE_FIELD_UINT32, 4);
			addField("modification_time", BE_FIELD_UINT32, 4);
			addField("timescale", BE_FIELD_UINT32, 4);
			addField("duration", BE_FIELD_UINT32, 4);
		}
		addField("rate", BE_FIELD_INT32, 4);
		addField("volume", BE_FIELD_INT16, 2);
		addField("bit", BE_FIELD_UINT16_ARRAY, 2);
		addField("reserved", BE_FIELD_UINT16_ARRAY, 8);
		addField("matrix", BE_FIELD_UINT32_ARRAY, 36);
		addField("pre_defined", BE_FIELD_UINT32_ARRAY, 24);
		addField("next_track_ID", BE_FIELD_UINT32, 4);
	}
}

MovieHeaderBox::MovieHeaderBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_MVHD;
}

/* HintTrackInfoBox (hnti) */

HintTrackInfoBox::HintTrackInfoBox()
{
	m_boxKindId = ISO_BOX_HNTI;
}

/* RtpMovieHintInformationBox (rtp ) */

void RtpMovieHintInformationBox::onFieldParsed()
{
	if (m_bodyAdded == 0 && getBytesRead() >= getHeaderSize()) {
		m_bodyAdded = 1;
		addField("descriptionformat", BE_FIELD_BYTES, 4);
		uint32_t remain = static_cast<uint32_t>(getBoxSize()) -
			static_cast<uint32_t>(getBytesRead()) - 4;
		addField("sdptext", BE_FIELD_UINT32_ARRAY, remain);
	}
}

RtpMovieHintInformationBox::RtpMovieHintInformationBox()
{
	m_boxKindId = ISO_BOX_RTP_HINT;
	m_hasExtendedHeader = 1;
	m_bodyAdded = 0;
}

/* RtpTrackSdpHintInformationBox (sdp ) */

void RtpTrackSdpHintInformationBox::onFieldParsed()
{
	if (m_bodyAdded == 0 && getBytesRead() >= getHeaderSize()) {
		m_bodyAdded = 1;
		uint32_t remain = static_cast<uint32_t>(getBoxSize()) -
			static_cast<uint32_t>(getBytesRead());
		addField("sdptext", BE_FIELD_UINT32_ARRAY, remain);
	}
}

RtpTrackSdpHintInformationBox::RtpTrackSdpHintInformationBox()
{
	m_boxKindId = ISO_BOX_SDP_HINT;
	m_hasExtendedHeader = 1;
	m_bodyAdded = 0;
}

/* SampleDependencyTypeBox (sdtp) */

void SampleDependencyTypeBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		uint32_t remain = static_cast<uint32_t>(getBoxSize()) -
			static_cast<uint32_t>(getFullHeaderSize());
		addField("Struct_reserved", BE_FIELD_BYTES, remain);
	}
}

SampleDependencyTypeBox::SampleDependencyTypeBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_SDTP;
}

int SampleDependencyTypeBox::isContainer()
{
	return 0;
}

/* SampleDescriptionBox (stsd) */

void SampleDescriptionBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("entry_count", BE_FIELD_UINT32, 4);
		return;
	}

	if (m_bodyAdded != 1 || findField("entry_count")->isIncomplete() != 0)
		return;

	IsoParser *parser = getParser();
	if (parser == NULL)
		return;

	IsoBox *hdlr = NULL;
	IsoBox *cursor = parser->getCurrentTreeBox();
	while (cursor != NULL && cursor->getBoxKindId() != ISO_BOX_MINF)
		cursor = static_cast<IsoBox *>(cursor->getParent());
	if (cursor != NULL) {
		for (cursor = static_cast<IsoBox *>(cursor->getFirstChild());
		     cursor != NULL;
		     cursor = static_cast<IsoBox *>(cursor->getNextSibling())) {
			if (cursor->getBoxKindId() == ISO_BOX_HDLR) {
				hdlr = cursor;
				break;
			}
		}
	}

	int checkedMdia = 0;
	for (;;) {
		if (hdlr == NULL) {
			IsoBox *mdia = parser->getCurrentTreeBox();
			while (mdia != NULL && mdia->getBoxKindId() != ISO_BOX_MDIA)
				mdia = static_cast<IsoBox *>(mdia->getParent());
			if (mdia == NULL)
				return;

			hdlr = static_cast<IsoBox *>(mdia->getFirstChild());
			while (hdlr != NULL && hdlr->getBoxKindId() != ISO_BOX_HDLR)
				hdlr = static_cast<IsoBox *>(hdlr->getNextSibling());
			if (hdlr == NULL)
				return;
		}

		uint32_t handlerType = *static_cast<const uint32_t *>(hdlr->findField("handler_type")->getData());
		switch (handlerType) {
		case ISO_FOURCC('h', 'i', 'n', 't'):
			parser->setSampleEntryContext(ISO_BOX_HINT_SAMPLE_ENTRY);
			return;
		case ISO_FOURCC('s', 'u', 'b', 't'):
			parser->setSampleEntryContext(ISO_BOX_XML_META_SAMPLE_ENTRY);
			return;
		case ISO_FOURCC('s', 'o', 'u', 'n'):
			parser->setSampleEntryContext(ISO_BOX_AUDIO_SAMPLE_ENTRY);
			return;
		case ISO_FOURCC('s', 'd', 's', 'm'):
		case ISO_FOURCC('o', 'd', 's', 'm'):
			parser->setSampleEntryContext(ISO_BOX_SAMPLE_ENTRY);
			return;
		case ISO_FOURCC('v', 'i', 'd', 'e'):
			parser->setSampleEntryContext(ISO_BOX_VISUAL_SAMPLE_ENTRY);
			return;
		case ISO_FOURCC('m', 'e', 't', 'a'):
			parser->setSampleEntryContext(ISO_BOX_META_SAMPLE_ENTRY);
			return;
		default:
			break;
		}

		if (checkedMdia != 0) {
			parser->setSampleEntryContext(ISO_BOX_GENERIC);
			return;
		}
		checkedMdia = 1;
		hdlr = NULL;
		parser->getCurrentTreeBox();
	}
}

void SampleDescriptionBox::onBoxComplete()
{
	if (getParser() != NULL)
		getParser()->setSampleEntryContext(ISO_BOX_GENERIC);
}

SampleDescriptionBox::SampleDescriptionBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_STSD;
}

SampleDescriptionBox::~SampleDescriptionBox()
{
}

/* IsoSampleEntryBox */

void IsoSampleEntryBox::onFieldParsed()
{
	IsoBox::onFieldParsed();
	if (m_entryHeaderAdded == 0 && getBytesRead() >= getHeaderSize()) {
		if (m_savedContext == 0 &&
		    getParser()->getSampleEntryContext() != 0 &&
		    getParser() != NULL) {
			m_savedContext = getParser()->getSampleEntryContext();
			getParser()->setSampleEntryContext(0);
		}
		m_entryHeaderAdded = 1;
		addField("reserved" "0", BE_FIELD_BYTES, 6);
		addField("data_reference_index", BE_FIELD_UINT16, 2);
	}
}

void IsoSampleEntryBox::onBoxComplete()
{
	if (getParser() != NULL)
		getParser()->setSampleEntryContext(m_savedContext);
}

IsoSampleEntryBox::IsoSampleEntryBox()
{
	m_savedContext = 0;
	m_entryHeaderAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_SAMPLE_ENTRY;
}

/* BitRateBox (btrt) */

void BitRateBox::onFieldParsed()
{
	IsoBox::onFieldParsed();
	if (m_bodyAdded == 0 && getBytesRead() >= getHeaderSize()) {
		m_bodyAdded = 1;
		addField("bufferSizeDB", BE_FIELD_UINT32, 4);
		addField("maxBitrate", BE_FIELD_UINT32, 4);
		addField("avgBitrate", BE_FIELD_UINT32, 4);
	}
}

BitRateBox::BitRateBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_BTRT;
}

/* PixelAspectRatioBox (pasp) */

void PixelAspectRatioBox::onFieldParsed()
{
	IsoBox::onFieldParsed();
	if (m_bodyAdded == 0 && getBytesRead() >= getHeaderSize()) {
		m_bodyAdded = 1;
		addField("hSpacing", BE_FIELD_UINT32, 4);
		addField("vSpacing", BE_FIELD_UINT32, 4);
	}
}

PixelAspectRatioBox::PixelAspectRatioBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_PASP;
}

/* VisualSampleEntryBox */

void VisualSampleEntryBox::onFieldParsed()
{
	IsoSampleEntryBox::onFieldParsed();
	if (m_bodyAdded == 0 && getBytesRead() >= getHeaderSize()) {
		m_bodyAdded = 1;
		if (m_subSavedContext == 0 &&
		    getParser()->getSampleEntryContext() != 0 &&
		    getParser() != NULL) {
			m_subSavedContext = getParser()->getSampleEntryContext();
			getParser()->setSampleEntryContext(0);
		}
		addField("pre_defined0", BE_FIELD_UINT16, 2);
		addField("reserved" "1", BE_FIELD_UINT16, 2);
		addField("pre_defined1", BE_FIELD_UINT32_ARRAY, 12);
		addField("width", BE_FIELD_UINT16, 2);
		addField("height", BE_FIELD_UINT16, 2);
		addField("horizresolution", BE_FIELD_UINT32, 4);
		addField("vertresolution", BE_FIELD_UINT32, 4);
		addField("reserved" "2", BE_FIELD_UINT32, 4);
		addField("frame_count", BE_FIELD_UINT16, 2);
		addField("compressorname", BE_FIELD_BYTES, 32);
		addField("depth", BE_FIELD_UINT16, 2);
		addField("pre_defined2", BE_FIELD_INT16, 2);
	}
}

void VisualSampleEntryBox::onBoxComplete()
{
	if (getParser() != NULL)
		getParser()->setSampleEntryContext(m_boxKindId);
}

VisualSampleEntryBox::VisualSampleEntryBox()
{
	m_subSavedContext = 0;
	m_hasExtendedHeader = 1;
	m_bodyAdded = 0;
	m_boxKindId = ISO_BOX_VISUAL_SAMPLE_ENTRY;
}

/* AvcConfigurationBox (avcC) */

void AvcConfigurationBox::onFieldParsed()
{
	IsoBox::onFieldParsed();
	if (m_parseStep == 0 && getBytesRead() >= getHeaderSize()) {
		m_parseStep++;
		if (m_savedContext == 0 &&
		    getParser()->getSampleEntryContext() != 0 &&
		    getParser() != NULL) {
			m_savedContext = getParser()->getSampleEntryContext();
			getParser()->setSampleEntryContext(0);
		}
		addField("configurationVersion", BE_FIELD_UINT8, 1);
		addField("AVCProfileIndication", BE_FIELD_UINT8, 1);
		addField("profile_compatibility", BE_FIELD_UINT8, 1);
		addField("AVCLevelIndication", BE_FIELD_UINT8, 1);
		addField("reserved_lengthSizeMinuxOne", BE_FIELD_UINT8, 1);
		addField("reserved_numOfSequenceParameterSets", BE_FIELD_UINT8, 1);
	}

	if (m_parseStep == 1 &&
	    findField("reserved_numOfSequenceParameterSets") != NULL &&
	    findField("reserved_numOfSequenceParameterSets")->isIncomplete() == 0) {
		m_spsCount = *static_cast<const uint8_t *>(findField("reserved_numOfSequenceParameterSets")->getData()) & 0x1F;
		m_parseStep++;
		if (m_spsCount != 0) {
			m_spsIndex = 0;
			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "sequenceParameterSetLength", 0);
			addField(m_nameBuf, BE_FIELD_UINT16, 2);
		}
	}

	if (m_spsCount != 0 && m_spsIndex < m_spsCount) {
		beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "sequenceParameterSetLength", m_spsIndex);
		BeField *spsLenField = findField(m_nameBuf);
		if (spsLenField != NULL && spsLenField->isIncomplete() == 0) {
			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "sequenceParameterSetNALUnit", m_spsIndex);
			uint16_t spsLen = *static_cast<const uint16_t *>(spsLenField->getData());
			addField(m_nameBuf, BE_FIELD_BYTES, spsLen);
		}

		beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "sequenceParameterSetNALUnit", m_spsIndex);
		BeField *spsNalField = findField(m_nameBuf);
		if (spsNalField != NULL && spsNalField->isIncomplete() == 0) {
			m_spsIndex++;
			if (m_spsIndex < m_spsCount) {
				beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "sequenceParameterSetLength", m_spsIndex);
				addField(m_nameBuf, BE_FIELD_UINT16, 2);
			}
		}
	}

	if (m_spsCount != 0 && m_spsIndex <= m_spsCount) {
		beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "sequenceParameterSetLength", m_spsIndex);
		if (findField(m_nameBuf) == NULL)
			addField("numOfPictureParameterSets", BE_FIELD_UINT8, 1);
	}

	if (findField("numOfPictureParameterSets") != NULL &&
	    findField("numOfPictureParameterSets")->isIncomplete() == 0) {
		m_ppsCount = *static_cast<const uint8_t *>(findField("numOfPictureParameterSets")->getData());
		if (m_spsCount != 0) {
			m_ppsIndex = 0;
			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "PictureParameterSetLength", 0);
			addField(m_nameBuf, BE_FIELD_UINT16, 2);
		}
	}

	if (m_ppsCount != 0 && m_ppsIndex < m_ppsCount) {
		beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "PictureParameterSetLength", m_ppsIndex);
		BeField *ppsLenField = findField(m_nameBuf);
		if (ppsLenField != NULL && ppsLenField->isIncomplete() == 0) {
			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "PictureParameterSetNALUnit", m_ppsIndex);
			uint16_t ppsLen = *static_cast<const uint16_t *>(ppsLenField->getData());
			addField(m_nameBuf, BE_FIELD_BYTES, ppsLen);
		}

		beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "PictureParameterSetLength", m_ppsIndex);
		BeField *ppsCheckField = findField(m_nameBuf);
		if (ppsCheckField != NULL && ppsCheckField->isIncomplete() == 0) {
			m_ppsIndex++;
			if (m_ppsIndex < m_ppsCount) {
				beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "PictureParameterSetLength", m_ppsIndex);
				addField(m_nameBuf, BE_FIELD_UINT16, 2);
			}
		}
	}
}

void AvcConfigurationBox::onBoxComplete()
{
	if (getParser() != NULL)
		getParser()->setSampleEntryContext(m_savedContext);

	if (findField("StructureSequenceParameterSet") != NULL) {
		uint32_t spsCount = *static_cast<const uint8_t *>(findField("reserved_numOfSequenceParameterSets")->getData()) & 0x1F;
		uint8_t *spsBytes = static_cast<uint8_t *>(findField("StructureSequenceParameterSet")->getData());
		for (uint32_t i = 0; i < 3 * spsCount; i += 3) {
			uint8_t swapped = spsBytes[0];
			spsBytes[0] = spsBytes[1];
			spsBytes[1] = swapped;
			spsBytes += 3;
		}
	}

	if (findField("StructurePictureParameterSet") != NULL) {
		uint32_t ppsCount = *static_cast<const uint8_t *>(findField("numOfPictureParameterSets")->getData());
		uint8_t *ppsBytes = static_cast<uint8_t *>(findField("StructurePictureParameterSet")->getData());
		for (uint32_t i = 0; i < 3 * ppsCount; i += 3) {
			uint8_t swapped = ppsBytes[0];
			ppsBytes[0] = ppsBytes[1];
			ppsBytes[1] = swapped;
			ppsBytes += 3;
		}
	}
}

AvcConfigurationBox::AvcConfigurationBox()
{
	m_spsCount = 0;
	m_ppsCount = 0;
	m_spsIndex = 0;
	m_ppsIndex = 0;
	m_parseStep = 0;
	m_savedContext = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_AVCC;
}

int AvcConfigurationBox::isContainer()
{
	return 0;
}

/* AudioSampleEntryBox */

void AudioSampleEntryBox::onFieldParsed()
{
	IsoSampleEntryBox::onFieldParsed();
	if (m_bodyAdded == 0 && getBytesRead() >= getHeaderSize()) {
		m_bodyAdded = 1;
		if (m_subSavedContext == 0 &&
		    getParser()->getSampleEntryContext() != 0 &&
		    getParser() != NULL) {
			m_subSavedContext = getParser()->getSampleEntryContext();
			getParser()->setSampleEntryContext(0);
		}
		addField("reserved" "1", BE_FIELD_UINT32_ARRAY, 8);
		addField("channelcount", BE_FIELD_UINT16, 2);
		addField("samplesize", BE_FIELD_UINT16, 2);
		addField("pre_defined", BE_FIELD_UINT16, 2);
		addField("reserved" "2", BE_FIELD_UINT16, 2);
		addField("samplerate", BE_FIELD_UINT32, 4);
	}

	if (findField("samplerate") != NULL && findField("samplerate")->isIncomplete() == 0) {
		m_parseState = ISO_BOX_STATE_HEADER_DONE;
		if (getParser() != NULL)
			getParser()->setSampleEntryContext(0);
	}
}

void AudioSampleEntryBox::onBoxComplete()
{
	if (getParser() != NULL)
		getParser()->setSampleEntryContext(m_boxKindId);
}

AudioSampleEntryBox::AudioSampleEntryBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_AUDIO_SAMPLE_ENTRY;
}

/* HintSampleEntryBox */

void HintSampleEntryBox::onFieldParsed()
{
	IsoSampleEntryBox::onFieldParsed();
	if (m_bodyAdded == 0 && getBytesRead() >= getHeaderSize()) {
		m_bodyAdded = 1;
		if (m_subSavedContext == 0 &&
		    getParser()->getSampleEntryContext() != 0 &&
		    getParser() != NULL) {
			m_subSavedContext = getParser()->getSampleEntryContext();
			getParser()->setSampleEntryContext(0);
		}
		addField("hinttrackversion", BE_FIELD_UINT16, 2);
		addField("highestcompatibleversion", BE_FIELD_UINT16, 2);
		addField("maxpacketsize", BE_FIELD_UINT32, 4);
	}

	if (findField("maxpacketsize") != NULL && findField("maxpacketsize")->isIncomplete() == 0) {
		m_parseState = ISO_BOX_STATE_HEADER_DONE;
		if (getParser() != NULL)
			getParser()->setSampleEntryContext(0);
	}
}

void HintSampleEntryBox::onBoxComplete()
{
	if (getParser() != NULL)
		getParser()->setSampleEntryContext(m_boxKindId);
}

HintSampleEntryBox::HintSampleEntryBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_HINT_SAMPLE_ENTRY;
}

/* XmlMetaSampleEntryBox */

void XmlMetaSampleEntryBox::onFieldParsed()
{
	IsoSampleEntryBox::onFieldParsed();
	if (m_bodyAdded == 0 && getBytesRead() >= getHeaderSize()) {
		if (m_subSavedContext == 0 &&
		    getParser()->getSampleEntryContext() != 0 &&
		    getParser() != NULL) {
			m_subSavedContext = getParser()->getSampleEntryContext();
			getParser()->setSampleEntryContext(0);
		}
		m_bodyAdded = 1;
		addField("namespace", BE_FIELD_CSTRING, 0);
	}

	if (findField("namespace") != NULL &&
	    findField("namespace")->isIncomplete() == 0 &&
	    findField("schema_location") == NULL &&
	    getBoxSize() > getBytesRead()) {
		addField("schema_location", BE_FIELD_CSTRING, 0);
	}

	if (findField("schema_location") != NULL &&
	    findField("schema_location")->isIncomplete() == 0 &&
	    getBoxSize() > getBytesRead()) {
		addField("image_mime_type", BE_FIELD_CSTRING, 0);
	}
}

void XmlMetaSampleEntryBox::onBoxComplete()
{
	if (getParser() != NULL)
		getParser()->setSampleEntryContext(m_boxKindId);
}

XmlMetaSampleEntryBox::XmlMetaSampleEntryBox()
{
	m_subSavedContext = 0;
	m_hasExtendedHeader = 1;
	m_bodyAdded = 0;
	m_boxKindId = ISO_BOX_XML_META_SAMPLE_ENTRY;
}

/* SampleSizeBox (stsz) */

void SampleSizeBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("sample_size", BE_FIELD_UINT32, 4);
		addField("sample_count", BE_FIELD_UINT32, 4);
	}

	if (findField("sample_count") != NULL && findField("sample_count")->isIncomplete() == 0) {
		uint32_t sampleCount = *static_cast<const uint32_t *>(findField("sample_count")->getData());
		addField("Struct_entry_size", BE_FIELD_UINT32_ARRAY, 4 * sampleCount);
	}
}

SampleSizeBox::SampleSizeBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_STSZ;
}

int SampleSizeBox::isContainer()
{
	return 0;
}

/* SampleTableBox (stbl) */

SampleTableBox::SampleTableBox()
{
	m_boxKindId = ISO_BOX_STBL;
}

/* SampleToChunkBox (stsc) */

void SampleToChunkBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("entry_count", BE_FIELD_UINT32, 4);
	}

	if (findField("entry_count") != NULL && findField("entry_count")->isIncomplete() == 0) {
		uint32_t entryCount = *static_cast<const uint32_t *>(findField("entry_count")->getData());
		addField("Struct_first_chunk", BE_FIELD_UINT32_ARRAY, 12 * entryCount);
	}
}

SampleToChunkBox::SampleToChunkBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_STSC;
}

int SampleToChunkBox::isContainer()
{
	return 0;
}

/* ShadowSyncSampleBox (stsh) */

void ShadowSyncSampleBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("entry_count", BE_FIELD_UINT32, 4);
	}

	if (findField("entry_count") != NULL && findField("entry_count")->isIncomplete() == 0) {
		uint32_t entryCount = *static_cast<const uint32_t *>(findField("entry_count")->getData());
		addField("Struct_shadow_sample_number", BE_FIELD_UINT32_ARRAY, 8 * entryCount);
	}
}

ShadowSyncSampleBox::ShadowSyncSampleBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_STSH;
}

int ShadowSyncSampleBox::isContainer()
{
	return 0;
}

/* SyncSampleBox (stss) */

void SyncSampleBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("entry_count", BE_FIELD_UINT32, 4);
	}

	if (findField("entry_count") != NULL && findField("entry_count")->isIncomplete() == 0) {
		uint32_t entryCount = *static_cast<const uint32_t *>(findField("entry_count")->getData());
		addField("Struct_sample_number", BE_FIELD_UINT32_ARRAY, 4 * entryCount);
	}
}

SyncSampleBox::SyncSampleBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_STSS;
}

int SyncSampleBox::isContainer()
{
	return 0;
}

/* TimeToSampleBox (stts) */

void TimeToSampleBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("entry_count", BE_FIELD_UINT32, 4);
	}

	if (findField("entry_count") != NULL && findField("entry_count")->isIncomplete() == 0) {
		uint32_t entryCount = *static_cast<const uint32_t *>(findField("entry_count")->getData());
		addField("Struct_sample_count", BE_FIELD_UINT32_ARRAY, 8 * entryCount);
	}
}

TimeToSampleBox::TimeToSampleBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_STTS;
}

int TimeToSampleBox::isContainer()
{
	return 0;
}

/* TrackBox (trak) */

TrackBox::TrackBox()
{
	m_boxKindId = ISO_BOX_TRAK;
}

/* TrackExtendsBox (trex) */

void TrackExtendsBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("track_ID", BE_FIELD_UINT32, 4);
		addField("default_sample_description_index", BE_FIELD_UINT32, 4);
		addField("default_sample_duration", BE_FIELD_UINT32, 4);
		addField("default_sample_size", BE_FIELD_UINT32, 4);
		addField("default_sample_flags", BE_FIELD_UINT32, 4);
	}
}

TrackExtendsBox::TrackExtendsBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_TREX;
}

int TrackExtendsBox::isContainer()
{
	return 0;
}

/* TrackFragmentBox (traf) */

TrackFragmentBox::TrackFragmentBox()
{
	m_boxKindId = ISO_BOX_TRAF;
}

/* TrackFragmentHeaderBox (tfhd) */

void TrackFragmentHeaderBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL &&
	    findField("flags")->getSkipAlloc() == 0) {
		m_bodyAdded = 1;
		addField("track_ID", BE_FIELD_UINT32, 4);
		uint32_t tfFlags = isoBoxReadFlags(this);
		if ((tfFlags & 0x01) != 0)
			addField("base_data_offset", BE_FIELD_UINT64, 8);
		if ((tfFlags & 0x02) != 0)
			addField("sample_description_index", BE_FIELD_UINT32, 4);
		if ((tfFlags & 0x08) != 0)
			addField("default_sample_duration", BE_FIELD_UINT32, 4);
		if ((tfFlags & 0x10) != 0)
			addField("default_sample_size", BE_FIELD_UINT32, 4);
		if ((tfFlags & 0x20) != 0)
			addField("default_sample_flags", BE_FIELD_UINT32, 4);
	}
}

TrackFragmentHeaderBox::TrackFragmentHeaderBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_TFHD;
}

int TrackFragmentHeaderBox::isContainer()
{
	return 0;
}

/* TrackFragmentRandomAccessBox (tfra) */

void TrackFragmentRandomAccessBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded++;
		addField("track_ID", BE_FIELD_UINT32, 4);
		addField("reserved", BE_FIELD_BYTES, 3);
		addField("reserved_length_sizeof", BE_FIELD_UINT8, 1);
		addField("number_of_entry", BE_FIELD_UINT32, 4);
	}

	if (m_bodyAdded == 1 &&
	    findField("version") != NULL &&
	    findField("version")->isIncomplete() == 0 &&
	    findField("number_of_entry") != NULL &&
	    findField("number_of_entry")->isIncomplete() == 0 &&
	    findField("reserved_length_sizeof") != NULL &&
	    findField("reserved_length_sizeof")->isIncomplete() == 0) {
		m_bodyAdded++;
		uint32_t timeOffsetSize = (*static_cast<const uint8_t *>(findField("version")->getData()) == 1) ? 16 : 8;
		uint32_t lengthSizes = *static_cast<const uint32_t *>(findField("reserved_length_sizeof")->getData());
		uint32_t entrySize = timeOffsetSize +
			((lengthSizes >> 4) & 3) +
			((lengthSizes >> 2) & 3) +
			(lengthSizes & 3) + 3;
		uint32_t entryCount = *static_cast<const uint32_t *>(findField("number_of_entry")->getData());
		addField("Struct_time", BE_FIELD_BYTES, entrySize * entryCount);
	}
}

void TrackFragmentRandomAccessBox::onBoxComplete()
{
	uint8_t *cursor = static_cast<uint8_t *>(findField("Struct_time")->getData());
	uint8_t version = *static_cast<const uint8_t *>(findField("version")->getData());
	uint32_t entryCount = *static_cast<const uint32_t *>(findField("number_of_entry")->getData());
	const uint32_t *lengthSizesPtr = static_cast<const uint32_t *>(findField("reserved_length_sizeof")->getData());

	for (uint32_t i = 0; i < entryCount; i++) {
		if (version == 1) {
			beSwapByteOrder(cursor, 16, 8);
			cursor += 16;
		} else {
			beSwapByteOrder(cursor, 8, 4);
			cursor += 8;
		}

		uint32_t trafNumSize = ((*lengthSizesPtr >> 4) & 3) + 1;
		beSwapByteOrder(cursor, trafNumSize, trafNumSize);
		cursor += ((*lengthSizesPtr >> 4) & 3) + 1;

		uint32_t trunNumSize = ((*lengthSizesPtr >> 2) & 3) + 1;
		beSwapByteOrder(cursor, trunNumSize, trunNumSize);
		cursor += ((*lengthSizesPtr >> 2) & 3) + 1;

		uint32_t sampleNumSize = (*lengthSizesPtr & 3) + 1;
		beSwapByteOrder(cursor, sampleNumSize, sampleNumSize);
		cursor += (*lengthSizesPtr & 3) + 1;
	}
}

TrackFragmentRandomAccessBox::TrackFragmentRandomAccessBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_TFRA;
}

int TrackFragmentRandomAccessBox::isContainer()
{
	return 0;
}

/* TrackHeaderBox (tkhd) */

void TrackHeaderBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
	}

	if (findField("version") != NULL && findField("version")->isIncomplete() == 0) {
		if (*static_cast<const uint8_t *>(findField("version")->getData()) == 1) {
			addField("creation_time", BE_FIELD_UINT64, 8);
			addField("modification_time", BE_FIELD_UINT64, 8);
			addField("track_ID", BE_FIELD_UINT32, 4);
			addField("reserved" "0", BE_FIELD_UINT32, 4);
			addField("duration", BE_FIELD_UINT64, 8);
		} else {
			addField("creation_time", BE_FIELD_UINT32, 4);
			addField("modification_time", BE_FIELD_UINT32, 4);
			addField("track_ID", BE_FIELD_UINT32, 4);
			addField("reserved" "0", BE_FIELD_UINT32, 4);
			addField("duration", BE_FIELD_UINT32, 4);
		}
		addField("reserved", BE_FIELD_UINT32_ARRAY, 8);
		addField("layer", BE_FIELD_UINT16, 2);
		addField("alternate_group", BE_FIELD_UINT16, 2);
		addField("volume", BE_FIELD_UINT16, 2);
		addField("reserved" "1", BE_FIELD_UINT16, 2);
		addField("matrix", BE_FIELD_UINT32_ARRAY, 36);
		addField("width", BE_FIELD_UINT32, 4);
		addField("height", BE_FIELD_UINT32, 4);
	}
}

TrackHeaderBox::TrackHeaderBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_TKHD;
}

int TrackHeaderBox::isContainer()
{
	return 0;
}

/* TrackReferenceTypeBox (tref) */

void TrackReferenceTypeBox::onFieldParsed()
{
	IsoBox::onFieldParsed();
	if (m_bodyAdded == 0 && getBytesRead() >= getHeaderSize()) {
		m_bodyAdded = 1;
		uint32_t remain = static_cast<uint32_t>(getBoxSize()) -
			static_cast<uint32_t>(getBytesRead());
		addField("track_IDs", BE_FIELD_UINT32_ARRAY, remain);
	}
}

TrackReferenceTypeBox::TrackReferenceTypeBox()
{
	m_bodyAdded = 0;
	m_boxKindId = ISO_BOX_TREF;
	m_hasExtendedHeader = 1;
}

/* TrackRunBox (trun) */

void TrackRunBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded++;
		addField("sample_count", BE_FIELD_UINT32, 4);
	}

	if (m_bodyAdded == 1 &&
	    findField("sample_count") != NULL &&
	    findField("sample_count")->isIncomplete() == 0 &&
	    findField("flags")->getSkipAlloc() == 0) {
		uint32_t trunFlags = isoBoxReadFlags(this);
		m_bodyAdded++;
		if ((trunFlags & 0x001) != 0)
			addField("data_offset", BE_FIELD_INT32, 4);
		if ((trunFlags & 0x004) != 0)
			addField("first_sample_flags", BE_FIELD_UINT32, 4);

		uint32_t sampleCount = *static_cast<const uint32_t *>(findField("sample_count")->getData());
		uint32_t perSampleBytes = 0;
		if ((trunFlags & 0x100) != 0)
			perSampleBytes += 4;
		if ((trunFlags & 0x200) != 0)
			perSampleBytes += 4;
		if ((trunFlags & 0x400) != 0)
			perSampleBytes += 4;
		if ((trunFlags & 0x800) != 0)
			perSampleBytes += 4;

		if (perSampleBytes != 0 && sampleCount != 0)
			addField("Struct_sample", BE_FIELD_UINT32_ARRAY, sampleCount * perSampleBytes);
	}
}

TrackRunBox::TrackRunBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_TRUN;
}

int TrackRunBox::isContainer()
{
	return 0;
}

/* UserDataBox (udta) */

UserDataBox::UserDataBox()
{
	m_boxKindId = ISO_BOX_UDTA;
}
