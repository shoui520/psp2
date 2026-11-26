/*
 * SceBeisobmf C API exports and module entry points.
 */

#include "be_isobmf_internal.h"
#include <scetypes.h>

extern "C" {

void SceBeisobmf_958E4F24(BeAllocFunc allocFn, BeReallocFunc reallocFn, BeFreeFunc freeFn)
{
	BeMemObject::setAllocator(allocFn, reallocFn, freeFn);
}

IsoParser *SceBeisobmf_82FDC702(void)
{
	return new IsoParser();
}

void SceBeisobmf_A70C1132(IsoParser *parser)
{
	if (parser != NULL)
		delete parser;
}

int SceBeisobmf_D310B27E(IsoParser *parser)
{
	if (parser != NULL)
		return parser->reset();
	return 0;
}

uint64_t SceBeisobmf_71AB7DEA(IsoParser *parser)
{
	if (parser != NULL)
		return parser->getNextExpectedOffset();
	return 0xFFFFFFFFULL;
}

int SceBeisobmf_D158B5D2(IsoParser *parser, const char *cmdLine)
{
	if (parser != NULL)
		return parser->executeCommand(cmdLine);
	return 0;
}

BeTree *SceBeisobmf_B66791B3(IsoParser *parser)
{
	if (parser != NULL)
		return parser->getBoxTree();
	return NULL;
}

uint32_t SceBeisobmf_D592745E(IsoParser *parser, const uint8_t *data, uint32_t size,
                              int unusedFlag, uint64_t fileOffset)
{
	if (parser != NULL)
		return parser->feedData(data, size, unusedFlag, fileOffset);
	return 0;
}

int SceBeisobmf_0A70CE8D(IsoParser *parser, void **outResult)
{
	if (parser == NULL || outResult == NULL)
		return 0;
	*outResult = parser->getQueryResult();
	return 1;
}

void *SceBeisobmf_B0208E0C(IsoBox *box, const char *fieldName)
{
	BeField *field = box->findField(fieldName);
	if (field != NULL)
		return field->getData();
	return NULL;
}

uint32_t SceBeisobmf_9722FE78(IsoBox *box, const char *fieldName)
{
	BeField *field = box->findField(fieldName);
	if (field != NULL)
		return field->getType();
	return 0;
}

uint32_t SceBeisobmf_1047C50A(IsoBox *box, const char *fieldName)
{
	BeField *field = box->findField(fieldName);
	if (field != NULL)
		return field->getSize();
	return 0;
}

IsoBox *SceBeisobmf_35EFA733(IsoBox *box)
{
	return static_cast<IsoBox *>(box->getFirstChild());
}

IsoBox *SceBeisobmf_D086C0EC(IsoBox *box)
{
	return static_cast<IsoBox *>(box->getNextSibling());
}

uint32_t SceBeisobmf_F9EA95DA(IsoBox *box)
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

uint32_t SceBeisobmf_72F99D98(IsoBox *box)
{
	return box->getParseState();
}

uint32_t SceBeisobmf_18B99048(IsoBox *box)
{
	return box->getBoxKindId();
}

uint64_t SceBeisobmf_D643E393(IsoBox *box)
{
	return box->getDiskOffset();
}

int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

} /* extern "C" */
